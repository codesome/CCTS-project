#ifndef _2PL_H_
#define _2PL_H_

#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <deque>
#include <atomic>
#include <unordered_set>
#include "common.h"

class object {
public:
    int id;

    std::mutex _2pl_mtx;

    // Should lock this while writing
    std::mutex mtx;
    std::unordered_set<int> wlock_list;
    std::unordered_set<int> rlock_list;

};

object* new_object_ptr(int id) {
    object* obj = new object;
    obj->id = id;
    return obj;
}

// object id -> its object pointer
std::map<int, object*> object_map;

class _2PL{

	int n_threads;
	std::vector<transaction> trans;

    

public:

	_2PL(int n_threads) : n_threads(n_threads) {}

	void add_transaction(transaction t) {
		trans.emplace_back(std::move(t));
	}

	double simulate();

    // For the lock

    std::vector<int> priorities;
    std::vector<phase> in_phase;

    std::vector<std::unordered_set<int>> before_trans_set;
    std::vector<std::unordered_set<int>> after_trans_set;



    bool rlock(int T, int x, std::vector<std::atomic<state>> &transaction_state, std::vector<std::atomic_int> &before_count) {
        object *obj = object_map[x];
        obj->mtx.lock();
        
        obj->rlock_list.emplace(T);

        for(auto t: obj->wlock_list) {
            if(priorities[t] > priorities[T] || in_phase[t]==WRITE) {
                obj->mtx.unlock();
                return false;
            }
        }

        for(auto t: obj->wlock_list) {
            if(before_trans_set[T].find(t)!=before_trans_set[T].end()) {
                transaction_state[t].store(ABORTED);
            } else if (after_trans_set[T].find(t) == after_trans_set[T].end()) {
                after_trans_set[T].emplace(t);
                before_count[t]++;
            }
        }

        obj->mtx.unlock();
        return true;
    }

    void remove_from_rlock_list(int T, int x) {
        object *obj = object_map[x];
        obj->mtx.lock();
        obj->rlock_list.erase(T);
        obj->mtx.unlock();
    }

    void remove_from_wlock_list(int T, int x) {
        object *obj = object_map[x];
        obj->mtx.lock();
        obj->wlock_list.erase(T);
        obj->mtx.unlock();
    }

    bool wlock(int T, int x, std::vector<std::atomic<state>> &transaction_state, std::vector<std::atomic_int> &before_count) {
        object *obj = object_map[x];
        obj->mtx.lock();
        
        obj->wlock_list.emplace(T);

        for(auto t: obj->rlock_list) {
            if(priorities[t] > priorities[T]) {
                if (after_trans_set[t].find(T) == after_trans_set[t].end()) {
                    after_trans_set[t].emplace(T);
                    before_count[T]++;                
                }
            } else {
                if(in_phase[t]==WAIT) {
                    if(after_trans_set[T].find(t)!=after_trans_set[T].end()) {
                        transaction_state[t].store(ABORTED);
                    } else {
                        after_trans_set[T].emplace(t);
                    }
                } else if(in_phase[t] == READ) {
                    transaction_state[t].store(ABORTED);
                }
            }
        }

        obj->mtx.unlock();
        return true;
    }

};


double _2PL::simulate() {

    std::vector<std::atomic<state>> transaction_state(trans.size()+1);
    std::vector<std::atomic_int> before_count(trans.size()+1);
    
    after_trans_set.resize(trans.size()+1);
    before_trans_set.resize(trans.size()+1);
    

    before_count[0].store(0);
    transaction_state[0].store(COMMITTED);
    for(int i=1; i<transaction_state.size(); i++) {
        transaction_state[i].store(RUNNING);
        before_count[i].store(0);
    }

    in_phase.resize(trans.size()+1);
    in_phase[0] = WRITE;
    for(int i=0; i<trans.size(); i++) {
        in_phase[i+1] = READ;
    }

    std::atomic_int p(0);

    FILE *fp = fopen ("2PL-log.txt", "w+");

    std::vector<double> times(n_threads);
    for (int i=0; i<n_threads; i++) {
        times[i] = 0;
    }

    auto task = [this, &p, &fp, &times, &transaction_state, &before_count](int thread_id) {

        int my_ptr = p++;
        while(my_ptr < this->trans.size()) {
            auto start_time = std::chrono::high_resolution_clock::now();
            transaction t = this->trans[my_ptr];

            for(event e: t) {
                if(e.is_write) {
                    while(!wlock(t.getid(), e.object_id, transaction_state, before_count)) {}
                } else {
                    while(!rlock(t.getid(), e.object_id, transaction_state, before_count)) {}
                }
                if(transaction_state[t.getid()].load()==ABORTED) {
                    break;
                }

                if(!e.is_write) {
                    e.print_event_message(thread_id, t.getid(), 0, fp);
                }
            }

            in_phase[t.getid()] = WAIT;
            printf("w %d\n", t.getid());
            while(before_count[t.getid()].load() && transaction_state[t.getid()].load()!=ABORTED) {}

            if(transaction_state[t.getid()].load()==ABORTED) {
                printf("a %d\n", t.getid());
                // remove myself from all read
                for(auto o: t.get_read_set()) {
                    remove_from_rlock_list(t.getid(), o);
                }
                for(auto o: t.get_write_set()) {
                    remove_from_wlock_list(t.getid(), o);
                }
                t.abort(thread_id, fp);
            } else {
                printf("c %d\n", t.getid());

                // remove myself from all read
                for(auto o: t.get_read_set()) {
                    remove_from_rlock_list(t.getid(), o);
                }

                in_phase[t.getid()] = WRITE;
                for(event e: t) {
                    if(e.is_write) {
                        e.print_event_message(thread_id, t.getid(), 0, fp);                    
                    }
                }


                for(auto o: t.get_write_set()) {
                    remove_from_wlock_list(t.getid(), o);
                }

                t.commit(thread_id, fp);
            }

            for(auto tt: after_trans_set[t.getid()]) {
                before_count[tt]--;
            }

            // for(int uo: t.get_unique_objs()) {
            //     object_map[uo]->_2pl_mtx.unlock();
            // }

            auto stop_time = std::chrono::high_resolution_clock::now();
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