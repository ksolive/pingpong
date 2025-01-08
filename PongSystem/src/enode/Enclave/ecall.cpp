#include "Enclave_t.h"

#include "sgx_trts.h"
#include <sgx_thread.h>
#include <sgx_spinlock.h>

#include <stdlib.h>
#include <string.h>
#include <vector>
#include <deque>
#include <map>
#include <assert.h>
#include <math.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <functional>

#include "utils_t.h"
#include "cryptolib.h"
#include "../../../thirdparty/rise/par_obl_primitives.h"
#include "../../common/ds.hpp"

void fun();

/* Attributes */
class Enclave
{
private:
    bool is_first_round_ = true;
    std::vector<Pkt> pkts_;
    size_t recv_counter_ = 0;
    size_t send_counter_ = 0;

    size_t worker_thread_num_ = 0;
    enum class WorkerFn {
        bnodeid_sort,
        toR_sort,
        stop,
    };
    struct worker_state {
        WorkerFn fn;
        int n_done;
        int curr_iter = 0;
    };
    std::mutex mtx_;
    std::condition_variable cv_;
    worker_state state_;


public:
    Enclave() {
    }
    ~Enclave() {
    }

    struct BnodeDummySorter {
        bool operator()(const Pkt& a, const Pkt& b) {
            uint16_t a_16 = (static_cast<uint16_t>(a.head.from) << 8) | a.head.is_dummy;
            uint16_t b_16 = (static_cast<uint16_t>(b.head.from) << 8) | b.head.is_dummy;
            return ObliviousLess(a_16, b_16);
        }
    };

    struct BackipSorter {
        bool operator()(const Pkt& a, const Pkt& b) {
            bool is_equal = ObliviousEqual(a.head.signal, b.head.signal);

            return ObliviousChoose<bool>(is_equal, ObliviousLess(a.head.enode_id, b.head.enode_id), ObliviousLess(a.head.signal, b.head.signal));
        }
    };

    struct TestSorter {
        bool operator()(const std::string& a, const std::string& b) {
            return true;
        }
    };

    std::string uninitialized_string(size_t size) {
        std::string ret;
        ret.resize(size);
        return ret;
    }

    void DDR(uint8_t** in_data, size_t batch_num, size_t pkt_size, std::vector<Pkt>& pkts) {
        pkts.clear();
        uint8_t pkt_buffer[pkt_size - MAC_SIZE];
        std::unordered_set<uint64_t> dedup_set;
        for (size_t i = 0;i < batch_num;++i) {
            // cal pkt digest
            uint64_t digest = 0;
            uint64_t tmp = 0;
            for (size_t j = 0;j < pkt_size;++j) {
                tmp |= in_data[i][j];
                tmp = tmp << 8;
                if (j % 8 == 0 || j == pkt_size - 1) {
                    digest ^= tmp;
                    tmp = 0;
                }
            }

            // decrypt and add pkt that is not in the set
            if (dedup_set.find(digest) == dedup_set.end()) {
                dedup_set.insert(digest);
                g_crypto_lib.decrypt(in_data[i], pkt_size, pkt_buffer);
                pkts.push_back(Pkt(pkt_buffer, pkt_size - MAC_SIZE));
            }
            else {
                eprintf("dedup+1 ");
            }
        }
    }

    void handle_batch(uint8_t** in_data, size_t batch_num, size_t pkt_size, uint8_t* out_data, size_t out_size, size_t* lens_data, size_t lens_size, size_t bnode_num, size_t B, uint8_t enode_id, uint64_t round_num, bool use_B_padding) {
        if (is_first_round_) {
            size_t max_size = batch_num + (batch_num > B * bnode_num) ? batch_num : (B * bnode_num);
            pkts_.reserve(static_cast<size_t>(1.1 * max_size));
            is_first_round_ = false;
        }
        // ocall_set_time_point();
        try
        {   
            // Dedup, decrypt and restore pkt
            DDR(in_data, batch_num, pkt_size, pkts_);
            // Set random bnode_id
            std::hash<uint64_t> hash_f;
            std::vector<size_t> lens(bnode_num, 0);
            for (auto& pkt : pkts_) {
                if (use_B_padding) {
                    pkt.head.from = pkt.head.signal % bnode_num;
                }
                else {
                    // one-time addr and mininum padding
                    size_t hash_value = hash_f(round_num + round_num ^ pkt.head.signal);
                    pkt.head.from = hash_value % bnode_num;
                }
                lens[pkt.head.from] += 1;
            }
            // Padding
            for (size_t i = 0;i < bnode_num;++i) {
                for (size_t j = 0;j < B;++j) {
                    pkts_.push_back(Pkt());
                    draw_rand(&pkts_.back(), pkt_size - MAC_SIZE);
                    pkts_.back().head.enode_id = enode_id;
                    pkts_.back().head.from = i;
                    pkts_.back().head.is_dummy = true;  // tag=is_dummy, 0 donate non-empty packet, 1 is empty
                }
            }
            // Osort
            if (worker_thread_num_ == 1) {
                ObliviousSort(pkts_.begin(), pkts_.end(), BnodeDummySorter());
            }
            else {
                notify_workers(WorkerFn::bnodeid_sort);
                ObliviousSortParallelNonAdaptive(pkts_.begin(), pkts_.end(), BnodeDummySorter(), worker_thread_num_, 0);
                wait_for_workers();
            }
            // Compact
            uint8_t* tags = new uint8_t[pkts_.size()]{};
            uint8_t reserved = 0x01;
            uint8_t discard = 0x00;
            size_t si = 0;
            for (size_t k = 0;k < lens.size();++k) {
                size_t padding_len = B;
                memset(tags + si, reserved, padding_len);
                si += lens[k] + B;
                lens[k] = padding_len;
            }
            ObliviousCompact(pkts_.begin(), pkts_.end(), tags);
            size_t lens_sum = std::accumulate(lens.begin(), lens.end(), static_cast<size_t>(0));
            pkts_.resize(lens_sum);
            delete[] tags;
            uint8_t pkt_buffer[pkt_size - MAC_SIZE]{};
            uint8_t* out_data_p = out_data;
            for (size_t k = 0;k < pkts_.size();++k) {
                memcpy(pkt_buffer, &pkts_[k], pkt_size - MAC_SIZE);
                g_crypto_lib.encrypt(pkt_buffer, pkt_size - MAC_SIZE, out_data_p);
                out_data_p += pkt_size;
            }
            memcpy(lens_data, lens.data(), lens.size() * sizeof(size_t));
        }
        catch (const std::exception& e)
        {
            eprintf("Enclave error: %s\n", e.what());
        }
    }

    void handle_blocks(uint8_t** in_data, size_t pkt_num, size_t pkt_size, uint8_t* out_data, size_t out_size, size_t batch_num, const uint64_t** to_R_list, size_t* valid_out_data_size) {
        try {
            // Dedup, decrypt and restore pkt
            DDR(in_data, pkt_num, pkt_size, pkts_);
            // 检查下这里解密出来的是什么样子的
            eprintf("pkt recive size: %d", pkts_.size());
            int64_t intime;
            int64_t outtime;
            size_t unique_pkts_num = pkts_.size();
            // Padding for everyuser
            for (size_t k = 0;k < batch_num;++k) {
                pkts_.push_back(Pkt());
                draw_rand(&pkts_.back(), pkt_size - MAC_SIZE);
                pkts_.back().head.enode_id = 255;
                pkts_.back().head.signal = *(to_R_list[k]);
                int64_t zero_time = 0;
                memcpy(pkts_.back().content, &zero_time, sizeof(int64_t));  // add timestamp
            }
            // Osort
            if (worker_thread_num_ == 1) {
                ObliviousSort(pkts_.begin(), pkts_.end(), BackipSorter());
            }
            else {
                notify_workers(WorkerFn::toR_sort);
                ObliviousSortParallelNonAdaptive(pkts_.begin(), pkts_.end(), BackipSorter(), worker_thread_num_, 0);
                wait_for_workers();
            }
            // Compact
            uint8_t reserved = 0x01;
            uint8_t discard = 0x00;
            uint8_t real_pkt_tag = 0x00;
            uint8_t dummy_tag = 0x01;
            uint8_t padding_pkt_tag = 0x02;
            uint8_t* tags = new uint8_t[pkts_.size()]{};  // default is discard

            tags[0] = ObliviousChoose(ObliviousEqual(pkts_[0].head.enode_id, real_pkt_tag), reserved, tags[0]);
            int count = 0;
            for (size_t k = 1;k < pkts_.size();++k) {
                bool is_real_pkt = (!pkts_[k].head.is_dummy) && (!pkts_[k].head.is_exceed);
                count += ObliviousChoose(is_real_pkt, 1, 0);
                bool is_same_as_prev = ObliviousEqual<uint64_t>(pkts_[k].head.from, pkts_[k - 1].head.from);
                tags[k] = ObliviousChoose((is_real_pkt && (!is_same_as_prev)), reserved, discard);
            }
            ObliviousCompact(pkts_.begin(), pkts_.end(), tags);
            pkts_.resize(batch_num);
            delete[] tags;
            // Copy out
            uint8_t pkt_buffer[pkt_size - MAC_SIZE]{};
            uint8_t* out_data_p = out_data;
            uint64_t a;
            uint64_t b;
            for (size_t k = 0;k < pkts_.size();++k) {
                memcpy(pkt_buffer, &pkts_[k], pkt_size - MAC_SIZE);
                a = *(uint64_t*)pkt_buffer;
                b = pkts_[k].head.signal;
                memcpy(out_data_p, pkt_buffer, sizeof(uint64_t));
                g_crypto_lib.encrypt(pkt_buffer, pkt_size - MAC_SIZE, out_data_p + sizeof(uint64_t));
                out_data_p += pkt_size + sizeof(uint64_t);
            }
            *valid_out_data_size = pkts_.size();
            eprintf("debug valid_out_data_size:%d ", pkts_.size());
        }
        catch (const std::exception& e)
        {
            eprintf("Enclave error: %s\n", e.what());
        }
    }

    void set_worker_thread_num(size_t worker_thread_num) {
        worker_thread_num_ = worker_thread_num;
    }

    void start_worker_loop(size_t thread_id) {
        int next_iter = 1;
        while (true) {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [&, this, next_iter, thread_id] {
                bool ready = state_.curr_iter == next_iter;
                return ready;
                });
            lk.unlock();
            if (state_.fn == WorkerFn::bnodeid_sort) {
                ObliviousSortParallelNonAdaptive(pkts_.begin(), pkts_.end(), BnodeDummySorter(), worker_thread_num_, thread_id);
            }
            else if (state_.fn == WorkerFn::toR_sort) {
                ObliviousSortParallelNonAdaptive(pkts_.begin(), pkts_.end(), BackipSorter(), worker_thread_num_, thread_id);
            }
            else if (state_.fn == WorkerFn::stop) {
                return;
            }

            next_iter++;
            lk.lock();
            if (++state_.n_done == worker_thread_num_) {
                lk.unlock();
                cv_.notify_all();
            }

        }
    }

    void notify_workers(WorkerFn fn) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            state_.fn = fn;
            state_.curr_iter++;
            state_.n_done = 1;
        }
        cv_.notify_all();
    }

    void wait_for_workers() {
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [&, this] {
                bool done = state_.n_done == worker_thread_num_;
                return done;
                });
        }
    }

};
Enclave enclave;


void ecall_handle_batch(uint8_t** in_data, size_t batch_num, size_t pkt_size, uint8_t* out_data, size_t out_size, size_t* lens_data, size_t lens_size, size_t bnode_num, size_t B, uint8_t enode_id, uint64_t round_num, size_t use_B_padding) {
    enclave.handle_batch(in_data, batch_num, pkt_size, out_data, out_size, lens_data, lens_size, bnode_num, B, enode_id, round_num, use_B_padding == 1 ? true : false);
}

void ecall_handle_blocks(uint8_t** in_data, size_t pkt_num, size_t pkt_size, uint8_t* out_data, size_t out_size, size_t batch_num, const uint64_t** to_R_list, size_t* valid_out_data_size) {
    enclave.handle_blocks(in_data, pkt_num, pkt_size, out_data, out_size, batch_num, to_R_list, valid_out_data_size);
}

void ecall_set_worker_thread_num(size_t worker_thread_num) {
    enclave.set_worker_thread_num(worker_thread_num);
}

void ecall_start_worker_loop(size_t thread_id) {
    enclave.start_worker_loop(thread_id);
}