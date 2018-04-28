#ifndef _MV2PL_H_
#define _MV2PL_H_

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_set>
// #include "boost/thread/shared_mutex.hpp"
#include "common.h"

std::atomic_uint timestamp_provider(1);

class object {
public:
    int id;

    // Should lock this while writing
    std::mutex lock;

};

object* new_object_ptr(int id) {
    object* obj = new object;
    obj->id = id;
    return obj;
}


class MVTO {

    int n_threads;
    std::vector<transaction> trans;

public:
    void add_transaction(transaction t) {
        trans.emplace_back(std::move(t));
    }

    MVTO(int n_threads) : n_threads(n_threads) {}

    double simulate();

};


struct op_ts {
    int my_tid;
    int version;
    op_ts(){}
    op_ts(int my_tid, int version): my_tid(my_tid), version(version) {}
};

// object id -> its object pointer
std::map<int, object*> object_map;

double MVTO::simulate() {

    // DS!

    // TODO: add t0 value by default

    // object id -> all transaction that read it
    std::map<int, std::vector<op_ts>> obj_read_ops;
    // object id -> all transaction that wrote it
    std::map<int, std::vector<op_ts>> obj_write_ops;

    std::vector<unsigned int> trans_timestamps(trans.size()+1);

    // used to get a transaction
    std::atomic_int p(0);

    // times for all the transactions in a thread
    std::vector<double> times(n_threads);
    for (int i=0; i<n_threads; i++) {
        times[i] = 0;
    }

    // Vector telling whether a transaction is transaction_state
    std::vector<std::atomic<state>> transaction_state(trans.size()+1);
    transaction_state[0].store(COMMITTED);
    for(int i=1; i<transaction_state.size(); i++) {
        transaction_state[i].store(RUNNING);
    }

    FILE *fp = fopen ("MVTO-log.txt", "w+");

    auto task = [this, &p, &fp, &times, &transaction_state, &trans_timestamps, &obj_write_ops, &obj_read_ops](int thread_id) {
        int my_ptr = p++;
        while(my_ptr < this->trans.size()) {
            auto start_time = std::chrono::high_resolution_clock::now(); // start time
            transaction t = this->trans[my_ptr]; // current transaction
            auto my_ts = timestamp_provider++;
            trans_timestamps[t.getid()] = my_ts;

            std::unordered_set<int> versions_read;

            int size = t.size();
            for(int i=0; i<size; i++) {
                auto e = t.at(i);

                object_map[e.object_id]->lock.lock();
                
                if(e.is_write) {
                    // (2) in book
                    // WRITE

                    for(auto ops: obj_read_ops[e.object_id]) {
                        int j = ops.my_tid;
                        int k = ops.version;
                        if(trans_timestamps[k] < trans_timestamps[t.getid()] && trans_timestamps[t.getid()] < trans_timestamps[j]) {
                            transaction_state[t.getid()].store(ABORTED);
                            break;
                        }
                    }

                    obj_write_ops[e.object_id].emplace_back(t.getid(), t.getid());
                } else {
                    // (1) in book
                    // READ
                    int version_read = 0;
                    unsigned int curr_ts = 0;
                    for(auto ops: obj_write_ops[e.object_id]) {
                        if (trans_timestamps[ops.my_tid] < my_ts && trans_timestamps[ops.my_tid] > curr_ts) {
                            curr_ts = trans_timestamps[ops.my_tid];
                            version_read = ops.my_tid;
                        }
                    }

                    versions_read.emplace(version_read);

                    obj_read_ops[e.object_id].emplace_back(t.getid(), version_read);

                }
                
                object_map[e.object_id]->lock.unlock();
            }

            bool end = false;
            // (3) in book
            while(!end) {
                for(auto tid: versions_read) {
                    if(transaction_state[tid]==ABORTED) {
                        transaction_state[t.getid()] == ABORTED;
                        end = true;
                        break;
                    }
                    if(transaction_state[tid]==RUNNING) {
                        break;
                    }
                }
            }

            if(transaction_state[t.getid()].load() == RUNNING) {
                transaction_state[t.getid()].store(COMMITTED);
            }
        
            auto stop_time = std::chrono::high_resolution_clock::now(); // end time
            double micro_sec = std::chrono::duration_cast<std::chrono::microseconds>(stop_time-start_time).count();
            times[thread_id] += micro_sec;
            my_ptr = p++;

        }
    };

    std::vector<std::thread> threads;
    for (int i=0; i<n_threads; i++) {
        threads.emplace_back([](auto f, int thread_id){
            f(thread_id);
        }, task, i);
    }

    for (int i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    fclose(fp);

    double total_time = 0;
    for (int i = 0; i < n_threads; ++i) {
        total_time += times[i];
    }

    return total_time/trans.size();

}

#endif