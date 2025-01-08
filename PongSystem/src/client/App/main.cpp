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
#include <vector>
#include <cmath>
#include <functional>
#include <fstream>

#include "cryptolib.h"

#include "pingpong.grpc.pb.h"


using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using pingpong::Payload;
using pingpong::PayloadWithRes;
using pingpong::PingpongService;

LogHelper logging;

class PingpongClient {
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
    std::vector<std::unique_ptr<PingpongService::Stub>> connections_enode_;
    size_t recv_counter_ = 0;
    size_t id_;
    size_t usernum_;
    Limit limit_;

    std::mutex mtx_;
    std::deque<int64_t> latency_;
    std::deque<int64_t> oram_cost_;

public:
    explicit PingpongClient(size_t id, size_t user_num, size_t parallel_num, const std::string& network_filename, size_t enode_num, size_t bnode_num) {
        // Set id
        id_ = id;
        // Multi send-recv
        for (int k = parallel_num;k--;) {
            cqs_.push_back(new CompletionQueue());
        }
        // Load users
        usernum_ = user_num;
        // Load network info
        std::ifstream in_ncf(network_filename);
        if (in_ncf.fail()) {
            throw std::logic_error("File is not exist!");
        }
        nlohmann::json js_ncf;
        in_ncf >> js_ncf;
        // Add connect to load balancer
        size_t tmp_counter = 0;
        for (auto& addr : js_ncf["enode_addr"]) {
            add_connect2enode(addr);
            ++tmp_counter;
            if (tmp_counter == enode_num) {
                break;
            }
        }
    }

    std::vector<std::tuple<int, int, bool>> generateReadPairs(int usernum) {
        std::vector<std::tuple<int, int, bool>> pairs;
        int sender_range1 = std::ceil(usernum * 0.5);
        int sender_range2 = std::ceil(usernum * 0.9);
        int reciver_range1 = std::ceil(usernum * 0.2);
        int reciver_range2 = std::ceil(usernum * 0.3);
        int reciver_range3 = std::ceil(usernum * 0.95);
        // Generate read pairs
        for (int i = reciver_range3; i < usernum; i++) {
            for (int j = 0; j < 10; j++) {
                pairs.push_back(std::make_tuple(0 + (i-reciver_range3)*10 + j, i, true));
            }
        }
        for (int i = 0; i < reciver_range1; i++) {
            for (int j = 0; j < 2; j++) {
                pairs.push_back(std::make_tuple(sender_range1 + (i-0)*2 + j, i, true));
            }
        }
        for (int i = reciver_range1; i < reciver_range2; i++) {
            for (int j = 0; j < 1; j++) {
                pairs.push_back(std::make_tuple(sender_range2 + (i-reciver_range1)*1 + j, i, true));
            }
        }
        return pairs;
    }

    std::vector<std::tuple<int, int, bool>> generatePairs(int usernum) {
        std::vector<std::tuple<int, int, bool>> pairs;
        int sender_range1 = std::ceil(usernum * 0.5);
        int sender_range2 = std::ceil(usernum * 0.9);
        int reciver_range1 = std::ceil(usernum * 0.2);
        int reciver_range2 = std::ceil(usernum * 0.3);
        int reciver_range3 = std::ceil(usernum * 0.95);
        // Generate write pairs
        for (int i = reciver_range3; i < usernum; i++) {
            for (int j = 0; j < 10; j++) {
                pairs.push_back(std::make_tuple(0 + (i-reciver_range3)*10 + j, i, false));
            }
        }
        for (int i = 0; i < reciver_range1; i++) {
            for (int j = 0; j < 2; j++) {
                pairs.push_back(std::make_tuple(sender_range1 + (i-0)*2 + j, i, false));
            }
        }
        for (int i = reciver_range1; i < reciver_range2; i++) {
            for (int j = 0; j < 1; j++) {
                pairs.push_back(std::make_tuple(sender_range2 + (i-reciver_range1)*1 + j, i, false));
            }
        }
        return pairs;
    }

    uint64_t hashFunction(int sender, int receiver, size_t round_num) {
        std::hash<int> intHash;  // 使用 std::hash<int> 进行整数的哈希
        size_t combinedHash = intHash(sender) ^ intHash(receiver) ^ std::hash<size_t>{}(round_num);
        uint64_t result = static_cast<uint64_t>(combinedHash);
        return result;
    }

    uint64_t customHash(int sender, int receiver, size_t round_num) {
        const uint64_t multiplier = 31;
        uint64_t hashValue = 0;
        hashValue = hashValue * multiplier + static_cast<uint64_t>(sender);
        hashValue = hashValue * multiplier + static_cast<uint64_t>(receiver);
        hashValue = hashValue * multiplier + static_cast<uint64_t>(round_num);
        return hashValue;
    }   

    bool fileExists(const std::string& filename) {
        std::ifstream file(filename);
        return file.good(); 
    }


    void start_round(size_t round_num, size_t pkt_size) {
        // logging.info("Start loop");
        std::vector<std::tuple<int, int, bool>> pairs = generatePairs(usernum_);
        std::vector<std::tuple<int, int, bool>> read_pairs = generateReadPairs(usernum_);
        auto prev_tp = std::chrono::system_clock::now();
        std::deque<int64_t> round_cost;
        std::deque<int64_t> round_latency_99th;
        std::deque<int64_t> round_latency_100th;
        std::deque<int64_t> oram_round_latency_99th;
        std::deque<int64_t> oram_round_latency_100th;
        for (size_t r = 1;r <= 2*round_num;++r) {
            if (r > round_num) {
                if (r == round_num+1) {
                    round_latency_99th.clear();
                    round_latency_100th.clear();
                }
                logging.info("Start Read" + std::to_string(r-round_num) + " round [1.client send]");

                std::unordered_map<uint8_t, uint64_t> mapping_enodeid_counter;
                for (size_t enode_id = 0;enode_id < connections_enode_.size();++enode_id) {
                    mapping_enodeid_counter[enode_id] = 0;
                }
                std::mutex counter_mtx;

                size_t parallel_num = cqs_.size();
                float chip = 1.f * pairs.size() / parallel_num;
                std::vector<std::thread> threads;
                // size_t index_config = 0;
                for (size_t idx = 0;idx < parallel_num;++idx) {
                    threads.emplace_back([=, &mapping_enodeid_counter, &counter_mtx] {
                        int debug_cout = 0;
                        for (size_t k = std::floor(idx * chip);k < std::floor((idx + 1) * chip);++k) {
                            PktHead pkt_head;
                            uint8_t random_enode = k % connections_enode_.size();  // TODO
                            counter_mtx.lock();
                            mapping_enodeid_counter[random_enode] += 1;
                            counter_mtx.unlock();
                            pkt_head.enode_id = random_enode;
                            pkt_head.read_write = std::get<2>(read_pairs[k]);
                            pkt_head.signal = (uint64_t(std::get<0>(read_pairs[k])) << 16) + uint64_t(std::get<1>(read_pairs[k])) +(uint64_t(r-round_num) << 32);                            
                            pkt_head.is_dummy = false;
                            pkt_head.is_exceed = false;
                            uint8_t pkt_buffer[pkt_size]{};
                            memcpy(pkt_buffer, &pkt_head, sizeof(PktHead));
                            int64_t start_tp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                            memcpy(pkt_buffer + sizeof(PktHead), &start_tp, sizeof(int64_t));  // add timestamp
                            g_crypto_lib.encrypt(pkt_buffer, PKT_SIZE - MAC_SIZE, pkt_buffer);
                            send_pkt2enode(random_enode, pkt_head.signal, std::string(pkt_buffer, pkt_buffer + pkt_size), idx);
                            debug_cout++;
                        }
                        });
                }
                for (size_t idx = 0;idx < parallel_num;++idx) {
                    threads[idx].join();
                }
                for (size_t enode_id = 0;enode_id < connections_enode_.size();++enode_id) {
                    // std::cout << "debug end_note" << std::endl;
                    send_end_sig2enode(enode_id, mapping_enodeid_counter[enode_id]);  // sync-send makes sure send end singal after finishing async-send
                }
                limit_.P(usernum_);
                auto current_tp = std::chrono::system_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_tp - prev_tp);
                prev_tp = current_tp;
                round_cost.push_back(duration.count());

                std::sort(latency_.begin(), latency_.end());
                std::sort(oram_cost_.begin(), oram_cost_.end());
                size_t offset_99th = static_cast<size_t>(std::round(latency_.size() * 0.99));
                int64_t latency_sum_99th = std::accumulate(latency_.begin(), latency_.begin() + offset_99th, 0LL);
                int64_t latency_sum_100th = std::accumulate(latency_.begin(), latency_.end(), 0LL);
                logging.info("[read round"+ std::to_string(round_latency_99th.size()-round_num+1)+ "]99th latency: " + std::to_string(1.f * latency_sum_99th / offset_99th) + " ms");
                logging.info("[read round"+ std::to_string(round_latency_99th.size()-round_num+1)+" ]100th latency: " + std::to_string(1.f * latency_sum_100th / latency_.size()) + " ms");
                round_latency_99th.push_back(latency_sum_99th / offset_99th);
                round_latency_100th.push_back(latency_sum_100th / latency_.size());
                latency_.clear();
                size_t oram_offset_99th = static_cast<size_t>(std::round(oram_cost_.size() * 0.99));
                int64_t oram_latency_sum_99th = std::accumulate(oram_cost_.begin(), oram_cost_.begin() + offset_99th, 0LL);
                int64_t oram_latency_sum_100th = std::accumulate(oram_cost_.begin(), oram_cost_.end(), 0LL);
                oram_round_latency_99th.push_back(oram_latency_sum_99th / oram_offset_99th);
                oram_round_latency_100th.push_back(oram_latency_sum_100th / oram_cost_.size());
                oram_cost_.clear();
            } else {
                logging.info("Start Write" + std::to_string(r) + " round [1.client send]");
                std::unordered_map<uint8_t, uint64_t> mapping_enodeid_counter;
                for (size_t enode_id = 0;enode_id < connections_enode_.size();++enode_id) {
                    mapping_enodeid_counter[enode_id] = 0;
                }
                std::mutex counter_mtx;
                size_t parallel_num = cqs_.size();
                float chip = 1.f * pairs.size() / parallel_num;
                std::vector<std::thread> threads;
                for (size_t idx = 0;idx < parallel_num;++idx) {
                    threads.emplace_back([=, &mapping_enodeid_counter, &counter_mtx] {
                        int debug_cout = 0;
                        for (size_t k = std::floor(idx * chip);k < std::floor((idx + 1) * chip);++k) {
                            PktHead pkt_head;
                            uint8_t random_enode = k % connections_enode_.size();  // TODO
                            counter_mtx.lock();
                            mapping_enodeid_counter[random_enode] += 1;
                            counter_mtx.unlock();
                            pkt_head.enode_id = random_enode;
                            pkt_head.read_write = std::get<2>(pairs[k]);
                            pkt_head.signal = (uint64_t(std::get<0>(pairs[k])) << 16) + uint64_t(std::get<1>(pairs[k])) +(uint64_t(r) << 32);
                            pkt_head.is_dummy = false;
                            pkt_head.is_exceed = false;

                            uint8_t pkt_buffer[pkt_size]{};
                            memcpy(pkt_buffer, &pkt_head, sizeof(PktHead));
                            int64_t start_tp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                            memcpy(pkt_buffer + sizeof(PktHead), &start_tp, sizeof(int64_t));  // add timestamp
                            g_crypto_lib.encrypt(pkt_buffer, PKT_SIZE - MAC_SIZE, pkt_buffer);
                            send_pkt2enode(random_enode, pkt_head.signal, std::string(pkt_buffer, pkt_buffer + pkt_size), idx);
                            debug_cout++;
                        }
                        // std::cout << "debug count send: " + std::to_string(debug_cout) << std::endl;
                        });
                }
                for (size_t idx = 0;idx < parallel_num;++idx) {
                    threads[idx].join();
                }
                for (size_t enode_id = 0;enode_id < connections_enode_.size();++enode_id) {
                    send_end_sig2enode(enode_id, mapping_enodeid_counter[enode_id]);  // sync-send makes sure send end singal after finishing async-send
                }

                limit_.P(usernum_);
                auto current_tp = std::chrono::system_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_tp - prev_tp);
                prev_tp = current_tp;
                round_cost.push_back(duration.count());

                std::sort(latency_.begin(), latency_.end());
                std::sort(oram_cost_.begin(), oram_cost_.end());
                size_t offset_99th = static_cast<size_t>(std::round(latency_.size() * 0.99));
                int64_t latency_sum_99th = std::accumulate(latency_.begin(), latency_.begin() + offset_99th, 0LL);
                int64_t latency_sum_100th = std::accumulate(latency_.begin(), latency_.end(), 0LL);
                logging.info("[round"+ std::to_string(round_latency_99th.size()+1)+ "]99th latency: " + std::to_string(1.f * latency_sum_99th / offset_99th) + " ms");
                logging.info("[round"+ std::to_string(round_latency_99th.size()+1)+" ]100th latency: " + std::to_string(1.f * latency_sum_100th / latency_.size()) + " ms");
                round_latency_99th.push_back(latency_sum_99th / offset_99th);
                round_latency_100th.push_back(latency_sum_100th / latency_.size());
                latency_.clear();
                size_t oram_offset_99th = static_cast<size_t>(std::round(oram_cost_.size() * 0.99));
                int64_t oram_latency_sum_99th = std::accumulate(oram_cost_.begin(), oram_cost_.begin() + offset_99th, 0LL);
                int64_t oram_latency_sum_100th = std::accumulate(oram_cost_.begin(), oram_cost_.end(), 0LL);
                oram_round_latency_99th.push_back(oram_latency_sum_99th / oram_offset_99th);
                oram_round_latency_100th.push_back(oram_latency_sum_100th / oram_cost_.size());
                oram_cost_.clear();
            }
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
        if (round_latency_99th.size() != 0 && round_latency_100th.size()!=0) {
            logging.info("[total]99th latency: " + std::to_string(1.f * round_latency_sum_99th / std::chrono::milliseconds::period::den / round_latency_99th.size()) + " s");
            logging.info("[total]100th latency: " + std::to_string(1.f * round_latency_sum_100th / std::chrono::milliseconds::period::den / round_latency_100th.size()) + " s");
        }
        if (round_cost.size() > 3) {  // skip 3 rounds
            round_cost.pop_front();
            round_cost.pop_front();
            round_cost.pop_front();
        }
        uint64_t round_cost_sum = std::accumulate(round_cost.begin(), round_cost.end(), 0LL);
        if (round_cost.size() != 0) {
            uint64_t round_cost_mean = round_cost_sum / round_cost.size();
            logging.info("Mean round cost: " + std::to_string(1.f * round_cost_mean / std::chrono::milliseconds::period::den) + " s");
        }
        
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
    void add_connect2enode(const std::string& addr) {
        connections_enode_.emplace_back(PingpongService::NewStub(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));
    }

    void send_pkt2enode(size_t id, uint64_t ip, const std::string& pkt, size_t cq_id) {
        PayloadWithRes request;
        if (ip == 0) {
            std::cout << "debug bad 0" << std::endl;
        }
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
                // logging.info("client recv size 0");
                delete call;
                continue;
            }
            if (call->reply.data().size() == 1) { // dedup pkt
                // logging.info("client recv size 1");
                limit_.V();
                delete call;
                continue;
            }
            uint8_t* p = reinterpret_cast<uint8_t*>(const_cast<char*>(call->reply.data().data()));
            g_crypto_lib.decrypt(p, PKT_SIZE, pkt_buffer);
            // logging.info("debug real pkt");
            Pkt pkt = Pkt(pkt_buffer, PKT_SIZE - MAC_SIZE);
            int64_t start_tp;
            memcpy(&start_tp, pkt_buffer + sizeof(PktHead), sizeof(int64_t));
            int64_t in_oram_tp;
            memcpy(&in_oram_tp, pkt_buffer + sizeof(PktHead) + sizeof(int64_t), sizeof(int64_t));
            int64_t out_oram_tp;
            memcpy(&out_oram_tp, pkt_buffer + sizeof(PktHead)+ 2*sizeof(int64_t), sizeof(int64_t));
            int64_t end_tp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            mtx_.lock();
            if (start_tp != 0) {
                latency_.push_back(end_tp - start_tp);
            }
            oram_cost_.push_back(out_oram_tp-in_oram_tp);
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
      -u --user-num <user_num>            Num of simulated users [default: 100]
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
        "Message Client");  // version string

    logging.set_id_prefix(docopt_args["--id"].asLong(), "Client");

    logging.info("Input args:");
    std::string args_str = "";
    for (auto arg : docopt_args) {
        args_str += arg.first + " " + arg.second.asString() + "; ";
    }
    logging.info(args_str);

    PingpongClient Pingpong_client(docopt_args["--id"].asLong(), docopt_args["--user-num"].asLong(), docopt_args["--parallel-num"].asLong(), docopt_args["--config-path"].asString(), docopt_args["--enode-num"].asLong(), docopt_args["--bnode-num"].asLong());

    Pingpong_client.recv_pkts_from_enode_mt();
    Pingpong_client.start_round(docopt_args["--round-num"].asLong(), PKT_SIZE);

    Pingpong_client.join();  //blocks forever
    return 0;
}