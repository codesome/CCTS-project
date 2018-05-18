#ifndef _MV2PL_H_
#define _MV2PL_H_

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_set>
#include <unistd.h>
// #include "boost/thread/shared_mutex.hpp"
#include "common.h"

class object {
public:
    int id;

    // Should lock this while writing
    std::mutex write_lock;

};

object* new_object_ptr(int id) {
    object* obj = new object;
    obj->id = id;
    return obj;
}

// object id -> its object pointer
std::map<int, object*> object_map;

class MV2PL {

    int n_threads;
    std::vector<transaction> trans;

public:
    void add_transaction(transaction t) {
        trans.emplace_back(std::move(t));
    }

    MV2PL(int n_threads) : n_threads(n_threads) {}

    double simulate();

};



double MV2PL::simulate() {

    // object id -> list of version in the order that it is created
    std::map<int, std::vector<int>> object_version_list;
    // To protect addition in object_version_list
    // Read should hold this in shared mode
    // std::map<int, boost::shared_mutex> object_version_list_mtx;
    std::map<int, std::mutex> object_version_list_mtx;
    for(auto& p: object_map) {
        std::vector<int> v;
        v.emplace_back(0);
        object_version_list.emplace(p.first, std::move(v));
    }

    // You should add yourself in the list when you read a particular version
    // list[i] means ith version - 0,1,2,....,n
    std::vector<std::mutex> i_read_this_mtx(trans.size()+1);
    std::vector<std::vector<int>> i_read_this(trans.size()+1);


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

    FILE *fp = fopen ("MV2PL-log.txt", "w+");

    auto task = [this, &p, &fp, &times, &transaction_state, &object_version_list, &object_version_list_mtx, &i_read_this, &i_read_this_mtx](int thread_id) {
        int my_ptr = p++;
        while(my_ptr < this->trans.size()) {
            auto start_time = std::chrono::high_resolution_clock::now(); // start time
            transaction t = this->trans[my_ptr]; // current transaction

            std::unordered_set<int> current_versions_written;
            std::unordered_set<int> versions_read;

            for(auto o: t.get_write_set()) {
                object_map[o]->write_lock.lock();
            }

            int size = t.size();
            for(int i=0; i<size; i++) {
                auto e = t.at(i);

                if(i==size-1) {
                    // last step
                    startover:
                    while(true) {

                        // 2 (a) from book

                        // "current data item written by ti"
                        for(auto i_wrote_this_current: current_versions_written) {
                            i_read_this_mtx[i_wrote_this_current].lock();

                            // "that tj read"
                            for(auto this_read_current /*tj*/: i_read_this[i_wrote_this_current]) {
                                if(this_read_current != t.getid() && transaction_state[this_read_current].load() != COMMITTED) {
                                    i_read_this_mtx[i_wrote_this_current].unlock();
                                    goto startover;
                                }
                            }

                            i_read_this_mtx[i_wrote_this_current].unlock();
                        }

                        // 2 (b) from book
                        for(auto i_read /*tj*/: versions_read) {
                            if(transaction_state[i_read].load() != COMMITTED) {
                                goto startover;
                            }
                        }

                        break;

                    }
                }

                if(e.is_write) {
                    // WRITE

                    // This is the write lock
                    object_map[e.object_id]->write_lock.lock();

                    object_version_list_mtx[e.object_id].lock();

                    // Current version that it writes
                    current_versions_written.emplace(object_version_list[e.object_id].back());
                    e.print_event_message(thread_id, t.getid(), t.getid(), fp);

                    // Adding uncommitted version
                    object_version_list[e.object_id].push_back(t.getid());

                    object_version_list_mtx[e.object_id].unlock();

                } else {
                    // READ
                    // object_version_list_mtx[e.object_id].lock_shared();
                    object_version_list_mtx[e.object_id].lock();
                    
                    auto &lst = object_version_list[e.object_id];
                    // read here
                    int read_version = lst[lst.size()-1];
                    object_version_list_mtx[e.object_id].unlock();
                    
                    // I have read from this version
                    // I should wait for this version to finish
                    versions_read.emplace(read_version);
                    
                    e.print_event_message(thread_id, t.getid(), read_version, fp);
                    

                    // I read this current version
                    // uncommitted version will wait for me
                    i_read_this_mtx[read_version].lock();
                    i_read_this[read_version].push_back(t.getid());
                    i_read_this_mtx[read_version].unlock();
                    
                    // object_version_list_mtx[e.object_id].unlock_shared();
                }

                usleep(100*(rand()%100));
                
            }


            // Releasing all write locks
            for(auto o: t.get_write_set()) {
                object_map[o]->write_lock.unlock();
            }

            t.commit(thread_id, fp);
            transaction_state[t.getid()].store(COMMITTED);
        
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
