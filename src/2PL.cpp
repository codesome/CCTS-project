#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <memory>
#include <functional>
#include <deque>
#include <queue>
#include <atomic>
#include <cstdlib>
#include <unistd.h>
#include <condition_variable>
using namespace std;

struct event {
	bool is_write;
	int object_id;
	
	event(bool is_write, int object_id): is_write(is_write), object_id(object_id) {}
	
    void print_event_message(int tid, int trans_id, FILE *fp) {
        if(is_write) {
            fprintf(fp, "W%d(%c) by thread %d\n", trans_id, object_id, tid);
            fflush(fp);
        } else {
            fprintf(fp, "R%d(%c) by thread %d\n", trans_id, object_id, tid);
            fflush(fp);
        }
    }
};


class transaction {

	vector<event> events;
	set<int> unique_objs;
	int tid;

public:
	transaction(int tid): tid(tid) {}

	void add_event(bool is_write, int object_id) {
		events.emplace_back(is_write, object_id);
		unique_objs.insert(object_id);
	}
	set<int> get_unique_objs() {
		return unique_objs;
	}
	int getid() {
		return tid;
	}
	auto begin() {
		return events.begin();
	}
	auto end() {
		return events.end();
	}

    void commit(int thid, FILE *fp) {
        fprintf(fp, "c%d by thread %d\n", tid, thid);
        fflush(fp);
    }

    void abort(int thid, FILE *fp) {
        fprintf(fp, "a%d by thread %d\n", tid, thid);
        fflush(fp);
    }


};

class scheduler {

protected:
	vector<transaction> trans;

public:
	void add_transaction(transaction t) {
		trans.emplace_back(move(t));
	}

	virtual double simulate() = 0;

};

class _2PL: public scheduler {

	int n_threads;

public:

	_2PL(int n_threads) : n_threads(n_threads) {}

	double simulate() {

		atomic_int p(0);

		deque<mutex> obj_locks;
		map<int, int > lock_index;
		for(transaction t: trans) {
			for(int uo: t.get_unique_objs()) {
				if(lock_index.find(uo) == lock_index.end()) {
					lock_index[uo] = obj_locks.size();
					obj_locks.emplace_back();
				}
			}
		}

		FILE *fp = fopen ("2PL-log.txt", "w+");

		vector<double> times(n_threads);
		for (int i=0; i<n_threads; i++) {
			times[i] = 0;
		}

		function<void(int)> task = [this, &p, &fp, &obj_locks, &lock_index, &times](int thread_id) {

			int my_ptr = p++;
			while(my_ptr < this->trans.size()) {
		    	auto start_time = std::chrono::high_resolution_clock::now();
				transaction t = this->trans[my_ptr];

				for(int uo: t.get_unique_objs()) {
					obj_locks[lock_index[uo]].lock();
				}

				for(event e: t) {
					e.print_event_message(thread_id, t.getid(), fp);
                    usleep(100*(rand()%100));
				}

				for(int uo: t.get_unique_objs()) {
					obj_locks[lock_index[uo]].unlock();
				}

				t.commit(thread_id, fp);

		    	auto stop_time = std::chrono::high_resolution_clock::now();
    			double micro_sec = std::chrono::duration_cast<std::chrono::microseconds>(stop_time-start_time).count();
    			times[thread_id] += micro_sec;
				my_ptr = p++;

			}

		};

		vector<thread> threads;
		for (int i=0; i<n_threads; i++) {
			threads.emplace_back([](function<void(int)> f, int thread_id){
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

};

int main(int argc, char const *argv[]) {

	int size, n_threads, ne, lambda;
	cin >> size >> n_threads >> ne >> lambda;
	srand(lambda);
	scheduler* sched = new _2PL(n_threads);
	char c;
	for (int i = 0; i < size; ++i) {
		transaction t(i+1);
		int n_ops;
		bool is_write;
		cin >> n_ops;
		for (int j = 0; j < n_ops ; ++j) {
			cin >> is_write;
			cin >> c;
			t.add_event(is_write, c);
		}
		sched->add_transaction(move(t));
	}

	double avg_time = sched->simulate();
	
	FILE *fp = fopen ("Average_time_2PL.txt", "w+");
	fprintf(fp, "Average Time: %.9lf seconds\n", avg_time/1e6);
	fclose(fp);
	cout << "Avg. Time: " << avg_time/1e6 << endl;
	return 0;
}