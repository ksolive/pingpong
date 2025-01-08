#include "../../common/json.hpp"
#include "../../common/ds.hpp"
#include "../../common/utils.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <thread>
#include <chrono>
#include <bitset>
#include <deque>

#include "cryptolib.h"

#include "notify.grpc.pb.h"


using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using notify::Payload;
using notify::PayloadWithRes;
using notify::NotifyService;

LogHelper logging;

class NotifyClient {
private:
    // gRPC correlation
    struct AsyncClientCall {
        Payload reply;
        ClientContext context;
        Status status;
        std::unique_ptr<ClientAsyncResponseReader<Payload>> response_reader;
    };

    std::vector<CompletionQueue*> cqs_;
    std::vector<std::thread> recv_pkts_from_enode_threads_;
    std::mutex recv_mtx_;

    std::vector<std::unique_ptr<NotifyService::Stub>> connections_enode_;
    size_t recv_counter_ = 0;
    size_t id_;
    std::vector<uint128_t> dd_ids_;

    Limit limit_;

    std::mutex mtx_;
    std::deque<int64_t> latency_;
    // from, to, notf, is_sentail
    // std::vector<std::tuple<uint32_t,uint32_t,uint32_t,bool>> pkt_configs;
    std::vector<PktConfig> pkt_configs;

public:
    explicit NotifyClient(size_t id, size_t user_num, size_t parallel_num, const std::string& network_filename, size_t enode_num, size_t bnode_num) {
        // Set id
        id_ = id;
        // Multi send-recv
        for (int k = parallel_num;k--;) {
            cqs_.push_back(new CompletionQueue());
        }        
        std::default_random_engine e(1234);  // TODO
        std::uniform_int_distribution<uint64_t> unidis;
        uint32_t i_ = 0;
        pkt_configs = generate_pkt_configs(user_num);
        logging.info("pkt_configs.size() = " + std::to_string(pkt_configs.size()));
        std::ifstream in_ncf(network_filename);
        if (in_ncf.fail()) {
            throw std::logic_error("File is not exist!");
        }
        nlohmann::json js_ncf;
        in_ncf >> js_ncf;
        size_t tmp_counter = 0;
        for (auto& addr : js_ncf["enode_addr"]) {
            add_connect2enode(addr);
            ++tmp_counter;
            if (tmp_counter == enode_num) {
                break;
            }
        }
    }

    void start_round(size_t round_num, size_t pkt_size) {
        auto prev_tp = std::chrono::system_clock::now();
        std::deque<int64_t> round_cost;
        std::deque<int64_t> round_latency_99th;
        std::deque<int64_t> round_latency_100th;
        for (size_t r = 1;r <= round_num;++r) {
            logging.info("Start " + std::to_string(r) + " round [1.client send]");
            std::unordered_map<uint8_t, uint64_t> mapping_enodeid_counter;
            for (size_t enode_id = 0;enode_id < connections_enode_.size();++enode_id) {
                mapping_enodeid_counter[enode_id] = 0;
            }
            std::mutex counter_mtx;
            size_t parallel_num = cqs_.size();
            // float chip = 1.f * users_.size() / parallel_num;
            float chip = 1.f * pkt_configs.size() / parallel_num;
            std::vector<std::thread> threads;
            for (size_t idx = 0;idx < parallel_num;++idx) {
                threads.emplace_back([=, &mapping_enodeid_counter, &counter_mtx] {
                    // Random setting
                    std::random_device r;
                    std::default_random_engine e{ r() };
                    std::uniform_int_distribution<uint8_t> unidis_enode(0, connections_enode_.size() - 1);
                    std::uniform_int_distribution<size_t> unidis_tp(0, 3);  // TODO
                    e.seed(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + idx * (idx + 1) + idx);
                    for (size_t k = std::floor(idx * chip);k < std::floor((idx + 1) * chip);++k) {
                        Pkt package;
                        PktHead pkt_head;
                        uint8_t random_enode = k % connections_enode_.size();  // TODO
                        PktConfig pkt_config = pkt_configs[k];
                        counter_mtx.lock();
                        mapping_enodeid_counter[random_enode] += 1;
                        counter_mtx.unlock();
                        pkt_head.enode_id = random_enode;
                        pkt_head.set_dd_id(pkt_config.to);
                        pkt_head.from_S = pkt_config.from;
                        pkt_head.is_sential = (uint8_t)pkt_config.is_sentail;
                        package.head = pkt_head;
                        package.set_notf(pkt_config.notf);
                        int64_t start_tp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                        memcpy(&(package.content), &start_tp, sizeof(int64_t));
                        uint8_t* pkt_buffer = new uint8_t[PKT_SIZE];
                        memset(pkt_buffer, 0, sizeof(pkt_buffer));
                        memcpy(pkt_buffer, (uint8_t*)(&package), sizeof(Pkt));
                        g_crypto_lib.encrypt(pkt_buffer, PKT_SIZE - MAC_SIZE, pkt_buffer);
                        std::string str = std::string(pkt_buffer, pkt_buffer + PKT_SIZE);
                        send_pkt2enode(random_enode, (uint64_t)pkt_config.from + 1, str, idx);
                    }
                    });
            }
            for (size_t idx = 0;idx < parallel_num;idx++) {
                threads[idx].join();
            }
            for (size_t enode_id = 0;enode_id < connections_enode_.size();++enode_id) {
                send_end_sig2enode(enode_id, mapping_enodeid_counter[enode_id]);  // sync-send makes sure send end singal after finishing async-send
            }
            limit_.P(pkt_configs.size());
            auto current_tp = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_tp - prev_tp);
            prev_tp = current_tp;
            round_cost.push_back(duration.count());
            std::sort(latency_.begin(), latency_.end());
            size_t offset_99th = static_cast<size_t>(std::round(latency_.size() * 0.99));
            int64_t latency_sum_99th = std::accumulate(latency_.begin(), latency_.begin() + offset_99th, 0LL);
            int64_t latency_sum_100th = std::accumulate(latency_.begin(), latency_.end(), 0LL);
            round_latency_99th.push_back(latency_sum_99th / offset_99th);
            round_latency_100th.push_back(latency_sum_100th / latency_.size());
            latency_.clear();
        }
        // Print report
        mtx_.lock();
        if (round_latency_99th.size() > 3) {
            round_latency_99th.erase(round_latency_99th.begin(), round_latency_99th.begin() + 3);  // skip 3 rounds
        }
        if (round_latency_100th.size() > 3) {
            round_latency_100th.erase(round_latency_100th.begin(), round_latency_100th.begin() + 3);  // skip 3 rounds
        }
        int64_t round_latency_sum_99th = std::accumulate(round_latency_99th.begin(), round_latency_99th.end(), 0LL);
        int64_t round_latency_sum_100th = std::accumulate(round_latency_100th.begin(), round_latency_100th.end(), 0LL);
        logging.info("99th latency: " + std::to_string(1.f * round_latency_sum_99th / std::chrono::milliseconds::period::den / round_latency_99th.size()) + " s");
        logging.info("100th latency: " + std::to_string(1.f * round_latency_sum_100th / std::chrono::milliseconds::period::den / round_latency_100th.size()) + " s");
        if (round_cost.size() > 3) {  // skip 3 rounds
            round_cost.pop_front();
            round_cost.pop_front();
            round_cost.pop_front();
        }
        uint64_t round_cost_sum = std::accumulate(round_cost.begin(), round_cost.end(), 0LL);
        uint64_t round_cost_mean = round_cost_sum / round_cost.size();
        logging.info("Mean round cost: " + std::to_string(1.f * round_cost_mean / std::chrono::milliseconds::period::den) + " s");
        mtx_.unlock();
        std::cout << "Finish exp" << std::endl;  // end signal for scripts! important!
    }

    void recv_pkts_from_enode_mt() {
        for (size_t k = 0;k < cqs_.size();++k) {
            recv_pkts_from_enode_threads_.emplace_back([this, k] { this->recv_pkts_from_enode_st(k); });
        }
    }

    void join() {
        for (auto& th : recv_pkts_from_enode_threads_) {
            th.join();
        }
    }


private:
    std::vector<PktConfig> generate_pkt_configs(size_t user_num){
        std::vector<PktConfig> pkt_configs;
        PktConfig pkt_config;
        for (size_t i = 0;i < user_num;++i) {
            pkt_config.from = i;
            pkt_config.to = get_sent_target(i, 1);
            pkt_config.notf = get_my_index(i, pkt_config.to);
            pkt_config.is_sentail = false;
            pkt_configs.emplace_back(pkt_config);
        }
        return pkt_configs;
    }

    uint32_t get_my_index(uint32_t my_id, uint32_t frd_id){
        return (frd_id + MAX_BUDDY - my_id) % MAX_BUDDY;
    }

uint32_t get_sent_target(uint32_t my_id, uint32_t sent_mode=1){
    uint32_t frd_id;
    uint32_t my_id_in_group = my_id % MAX_BUDDY;
    uint32_t group_base = (my_id / MAX_BUDDY) * MAX_BUDDY;
    switch (sent_mode)
    {
    case 1:
        if(my_id_in_group < MAX_BUDDY * 0.5){
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.05) +(int)(MAX_BUDDY*0.95) + group_base;
        } else if(my_id_in_group < MAX_BUDDY * 0.9) {
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.2) + group_base;
        } else {
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.1) +(int)(MAX_BUDDY*0.2) + group_base;
        }
        break;
    case 2:
        if(my_id_in_group < MAX_BUDDY * 0.6){
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.2) +(int)(MAX_BUDDY*0.8) + group_base;
        } else if(my_id_in_group < MAX_BUDDY * 0.8) {
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.1) + group_base;
        } else {
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.2) +(int)(MAX_BUDDY*0.1) + group_base;
        }
        break;
    case 3:
        if(my_id_in_group < MAX_BUDDY * 0.6){
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.3) +(int)(MAX_BUDDY*0.7) + group_base;
        } else if(my_id_in_group < MAX_BUDDY * 0.8) {
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.2) + group_base;
        } else {
            frd_id = my_id;
        }
        break;
    case 4:
        if(my_id_in_group < MAX_BUDDY * 0.5){
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.25) +(int)(MAX_BUDDY*0.75) + group_base;
        } else {
            frd_id = my_id_in_group % (int)(MAX_BUDDY*0.25) + group_base;
        }
        break;
    default:
        frd_id = my_id;
        break;
    }
    return frd_id;
}

    void add_connect2enode(const std::string& addr) {
        connections_enode_.emplace_back(NotifyService::NewStub(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));
        // logging.info("Add load balancer connection " + addr);
    }

    uint32_t retrieve_friend_id(uint32_t my_id, uint32_t notf){
        return (my_id - notf + MAX_BUDDY) % MAX_BUDDY + ((my_id / MAX_BUDDY) * MAX_BUDDY);
    }

    void send_pkt2enode(size_t id, uint64_t ip, const std::string& pkt, size_t cq_id) {
        PayloadWithRes request;
        request.set_reserved(ip);
        request.set_data(pkt);
        AsyncClientCall* call = new AsyncClientCall;
        call->response_reader = connections_enode_[id]->PrepareAsyncSendMsg(&call->context, request, &(*cqs_[cq_id]));
        call->response_reader->StartCall();
        call->response_reader->Finish(&call->reply, &call->status, (void*)call);
    }

    void send_end_sig2enode(size_t id, size_t counter) {
        PayloadWithRes request;
        Payload response;
        request.set_reserved(0);
        request.set_data(std::to_string(counter));
        AsyncClientCall* call = new AsyncClientCall;
        call->response_reader = connections_enode_[id]->PrepareAsyncSendMsg(&call->context, request, &(*cqs_[0]));
        call->response_reader->StartCall();
        call->response_reader->Finish(&call->reply, &call->status, (void*)call);
    }

    void recv_pkts_from_enode_st(size_t cq_id) {
        void* got_tag;
        bool ok = false;
        bool failed_flag = false;
        uint8_t pkt_buffer[PKT_SIZE - MAC_SIZE];
        while ((*cqs_[cq_id]).Next(&got_tag, &ok)) {
            AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);
            GPR_ASSERT(ok);
            if (!call->status.ok()) {
                if (!failed_flag) {
                    logging.error(call->status.error_message() + " : " + std::to_string(call->status.error_code()));
                    logging.error("RPC failed");
                }
                failed_flag = true;
                delete call;
                continue;
            }
            if (call->reply.data().size() == 0) {  // skip end signal return pkt
                delete call;
                continue;
            }
            if (call->reply.data().size() == 1) { // dedup pkt
                limit_.V();
                delete call;
                continue;
            }
            uint8_t* p = reinterpret_cast<uint8_t*>(const_cast<char*>(call->reply.data().data()));
            g_crypto_lib.decrypt(p, PKT_SIZE, pkt_buffer);
            int64_t start_tp;
            memcpy(&start_tp, pkt_buffer + sizeof(PktHead) + (MAX_BUDDY / 8 + 1), sizeof(int64_t));
            int64_t end_tp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            mtx_.lock();
            if (start_tp != 0) {
                latency_.push_back(end_tp - start_tp);
            }
            mtx_.unlock();
            limit_.V();
            delete call;
        }
    }

};

#include "docopt.h"
static const char USAGE[] =
R"(
    Usage:
      exec [--id <id>] [-u <user_num>] [-r <round_num>] [-p <parallel_num>] [-c <config_path>] [--bnode-num <bnode_num>] [--enode-num <enode_num>]

    Options:
      -h --help                           Show this screen.
      --version                           Show version.
      --id <id>                           Id of client [default: 0]
      -u --user-num <user_num>            Num of simulated users [default: 10]
      -r --round-num <round_num>          Num of synchronized rounds [default: 10]
      -p --parallel-num <parallel_num>    Parallel num of grpc recv [default: 1]
      -c --config-path <config_path>      Path of the network config file [default: ../config/config_local.json]
      --bnode-num <bnode_num>             Num of all bnodes [default: 1]
      --enode-num <enode_num>             Num of all enodes [default: 1]
)";

#include <unistd.h>  // usleep

int main(int argc, char** argv)
{
    std::map<std::string, docopt::value> docopt_args = docopt::docopt(
        USAGE,
        { argv + 1, argv + argc },
        true,  // show help if requested
        "Notify Client");  // version string
    logging.set_id_prefix(docopt_args["--id"].asLong(), "Client");
    logging.info("Input args:");
    std::string args_str = "";
    for (auto arg : docopt_args) {
        args_str += arg.first + " " + arg.second.asString() + "; ";
    }
    logging.info(args_str);
    NotifyClient notify_client(docopt_args["--id"].asLong(), docopt_args["--user-num"].asLong(), docopt_args["--parallel-num"].asLong(), docopt_args["--config-path"].asString(), docopt_args["--enode-num"].asLong(), docopt_args["--bnode-num"].asLong());
    notify_client.recv_pkts_from_enode_mt();
    notify_client.start_round(docopt_args["--round-num"].asLong(), PKT_SIZE);
    notify_client.join();  //blocks forever
    return 0;
}