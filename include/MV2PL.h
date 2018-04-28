#ifndef _MV2PL_H_
#define _MV2PL_H_

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_set>
#include "boost/thread/shared_mutex.hpp"
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

// TODO: add t0 value by default

// object id -> its object pointer
std::map<int, object*> object_map;

// object id -> list of version in the order that it is created
std::map<int, std::vector<int>> object_version_list;
// To protect addition in object_version_list
// Read should hold this in shared mode
// std::map<int, boost::shared_mutex> object_version_list_mtx;
std::map<int, std::mutex> object_version_list_mtx;

// You should add yourself in the list when you read a particular version
std::map<int, std::mutex> i_read_this_mtx;
std::map<int, std::vector<int>> i_read_this;

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

    // used to get a transaction
    std::atomic_int p(0);

    // times for all the transactions in a thread
    std::vector<double> times(n_threads);
    for (int i=0; i<n_threads; i++) {
        times[i] = 0;
    }

    // Vector telling whether a transaction is committed
    std::vector<std::atomic_bool> committed(trans.size()+1);
    committed[0] = true;
    for(int i=1; i<committed.size(); i++) {
        committed[i].store(false);
    }

    FILE *fp = fopen ("MV2PL-log.txt", "w+");

    auto task = [this, &p, &fp, &times, &committed](int thread_id) {
        int my_ptr = p++;
        while(my_ptr < this->trans.size()) {
            auto start_time = std::chrono::high_resolution_clock::now(); // start time
            transaction t = this->trans[my_ptr]; // current transaction

            std::unordered_set<int> current_versions_written;
            std::unordered_set<int> versions_read;

            int size = t.size();
            for(int i=0; i<size; i++) {
                auto e = t.at(i);

                if(i==size-1) {
                    // last step
                }

                if(e.is_write) {
                    // WRITE

                    // This is the write lock
                    object_map[e.object_id]->write_lock.lock();

                    object_version_list_mtx[e.object_id].lock();

                    // Current version that it writes
                    current_versions_written.emplace(object_version_list[e.object_id].back());

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
                    
                    // I read this current version
                    // uncommitted version will wait for me
                    i_read_this_mtx[read_version].lock();
                    i_read_this[read_version].push_back(t.getid());
                    i_read_this_mtx[read_version].unlock();
                    
                    // object_version_list_mtx[e.object_id].unlock_shared();
                }
            }

            // TODO: handle last step

            // Releasing all write locks
            for(auto o: t.get_write_set()) {
                object_map[o]->write_lock.unlock();
            }

            committed[t.getid()].store(true);
        
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