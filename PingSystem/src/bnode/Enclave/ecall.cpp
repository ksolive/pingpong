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
        ddid_sort,
        enodeid_sort,
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
    struct DDID_Sentinel_Sorter{
        bool operator()(const Pkt& a, const Pkt& b){
            bool pred = ObliviousEqual(a.head.dd_id.highest, b.head.dd_id.highest);
            uint64_t a_64 = ObliviousChoose(pred, a.head.dd_id.midhigh, b.head.dd_id.midhigh);
            uint64_t b_64 = ObliviousChoose(pred, b.head.dd_id.midhigh, a.head.dd_id.midhigh);
            pred = ObliviousEqual(a_64, b_64);
            a_64 = ObliviousChoose(pred, a.head.dd_id.midlow, a_64);
            b_64 = ObliviousChoose(pred, b.head.dd_id.midlow, b_64);
            pred = ObliviousEqual(a_64, b_64);
            a_64 = ObliviousChoose(pred, a.head.dd_id.lowest, a_64);
            b_64 = ObliviousChoose(pred, b.head.dd_id.lowest, b_64);
            pred = ObliviousEqual(a_64, b_64);
            a_64 = ObliviousChoose(pred, (uint64_t)a.head.is_sential, a_64);
            b_64 = ObliviousChoose(pred, (uint64_t)b.head.is_sential, b_64);
            return ObliviousLess(a_64, b_64);
        }
    };

    struct LBIDSorter {
        bool operator()(const Pkt& a, const Pkt& b) {
            return ObliviousLess(a.head.enode_id, b.head.enode_id);
        }
    };

    std::string uninitialized_string(size_t size) {
        std::string ret;
        ret.resize(size);
        return ret;
    }

    inline bool ObliviousEqual128(uint128_t a, uint128_t b) {
        return ObliviousEqual(a.high, b.high) && ObliviousEqual(a.low, b.low);
    }

    inline bool ObliviousEqual256(uint256_t a, uint256_t b) {
        return ObliviousEqual(a.highest, b.highest) && ObliviousEqual(a.midhigh, b.midhigh) && ObliviousEqual(a.midlow, b.midlow) && ObliviousEqual(a.lowest, b.lowest);
    }

    void DDR(uint8_t** in_data, size_t batch_num, size_t pkt_size, std::vector<Pkt>& pkts) {
        // for
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
                // eprintf("dedup+1 ");
            }
        }
    }

    void handle_blocks(uint8_t** in_data, size_t pkt_num, size_t pkt_size, uint8_t* out_data, size_t out_size, size_t* lens_data, size_t lens_size) {
        if (is_first_round_) {
            pkts_.reserve(static_cast<size_t>(1.2 * pkt_num));
            is_first_round_ = false;
        }
        // ocall_set_time_point();
        try {
            // Dedup, decrypt and restore pkt
            DDR(in_data, pkt_num, pkt_size, pkts_);
            // ocall_probe_time();
            // Osort
            if (worker_thread_num_ == 1) {
                ObliviousSort(pkts_.begin(), pkts_.end(), DDID_Sentinel_Sorter());
            }
            else {
                notify_workers(WorkerFn::ddid_sort);
                ObliviousSortParallelNonAdaptive(pkts_.begin(), pkts_.end(), DDID_Sentinel_Sorter(), worker_thread_num_, 0);
                wait_for_workers();
            }
            // ocall_probe_time();
            // Dedup, set to_R, correct and swap       
            for (size_t k = 0;k < pkts_.size();++k) {
                pkts_[k].head.to_R = pkts_[k].head.from_S;
            }
            pkts_.push_back(Pkt());
            for (size_t k = 0;k < pkts_.size() - 1;++k) {
                bool is_same_as_prev = ObliviousEqual256(pkts_[k].head.dd_id, pkts_[k - 1].head.dd_id);
                for(size_t notf_i = 0; notf_i < sizeof(pkts_[k].notf); notf_i++){
                    pkts_[k].notf[notf_i] = ObliviousChoose<uint8_t>(is_same_as_prev, pkts_[k].notf[notf_i] + pkts_[k-1].notf[notf_i], pkts_[k].notf[notf_i]);
                }
            }
            pkts_.pop_back();
            // Osort
            if (worker_thread_num_ == 1) {
                ObliviousSort(pkts_.begin(), pkts_.end(), LBIDSorter());
            }
            else {
                notify_workers(WorkerFn::enodeid_sort);
                ObliviousSortParallelNonAdaptive(pkts_.begin(), pkts_.end(), LBIDSorter(), worker_thread_num_, 0);
                wait_for_workers();
            }
            // Recount the lens
            std::vector<size_t> lens(lens_size, 0);
            int idx = 0;
            int counter = 0;
            for (int k = 0;k < pkts_.size();++k) {
                ++counter;

                if (k == pkts_.size() - 1) {
                    lens[idx] = counter;
                    break;
                }
                if (pkts_[k].head.enode_id != pkts_[k + 1].head.enode_id) {
                    lens[idx++] = counter;
                    counter = 0;
                }
            }
            memcpy(lens_data, lens.data(), lens.size() * sizeof(size_t));
            if(out_size < pkts_.size() * (sizeof(Pkt)+16)){
                pkts_.resize(out_size / (sizeof(Pkt)+16));
            }
            if(lens_size < lens.size()) {
                lens.resize(lens_size);
            }
            // Copy out
            uint8_t pkt_buffer[pkt_size - MAC_SIZE];
            uint8_t* out_data_p = out_data;
            for (size_t k = 0;k < pkts_.size();++k) {
                memcpy(pkt_buffer, &pkts_[k], pkt_size - MAC_SIZE);
                g_crypto_lib.encrypt(pkt_buffer, pkt_size - MAC_SIZE, out_data_p);
                out_data_p += pkt_size;
            }
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
            if (state_.fn == WorkerFn::ddid_sort) {
                ObliviousSortParallelNonAdaptive(pkts_.begin(), pkts_.end(), DDID_Sentinel_Sorter(), worker_thread_num_, thread_id);
            }
            else if (state_.fn == WorkerFn::enodeid_sort) {
                ObliviousSortParallelNonAdaptive(pkts_.begin(), pkts_.end(), LBIDSorter(), worker_thread_num_, thread_id);
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

void ecall_handle_blocks(uint8_t** in_data, size_t pkt_num, size_t pkt_size, uint8_t* out_data, size_t out_size, size_t* lens_data, size_t lens_size) {
    enclave.handle_blocks(in_data, pkt_num, pkt_size, out_data, out_size, lens_data, lens_size);
}

void ecall_set_worker_thread_num(size_t worker_thread_num) {
    enclave.set_worker_thread_num(worker_thread_num);
}

void ecall_start_worker_loop(size_t thread_id) {
    enclave.start_worker_loop(thread_id);
}