#ifndef _COMMON_H_
#define _COMMON_H_

#include <iostream>
#include <vector>
#include <set>

enum state {
    RUNNING=0, 
    COMMITTED=1, 
    ABORTED=2
};

enum phase {
    READ=0, 
    WAIT=1, 
    WRITE=2
};

struct event {
    bool is_write;
    int object_id;
    
    event(bool is_write, int object_id): is_write(is_write), object_id(object_id) {}
    
    void print_event_message(int tid, int trans_id, int version, FILE *fp) {
        if(is_write) {
            fprintf(fp, "W%d(%c_%d) by thread %d\n", trans_id, object_id, trans_id, tid);
            fflush(fp);
        } else {
            fprintf(fp, "R%d(%c_%d) by thread %d\n", trans_id, object_id, version, tid);
            fflush(fp);
        }
    }
    void print_mono_event_message(int tid, int trans_id, FILE *fp) {
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

    std::vector<event> events;
    std::set<int> unique_objs, read_set, write_set;
    int tid;


public:
    transaction(int tid): tid(tid) {}

    void add_event(bool is_write, int object_id) {
        events.emplace_back(is_write, object_id);
        unique_objs.insert(object_id);
        if(is_write) {
            write_set.insert(object_id);
        } else {
            read_set.insert(object_id);
        }
    }
    std::set<int> get_unique_objs() {
        return unique_objs;
    }
    std::set<int> get_read_set() {
        return read_set;
    }
    std::set<int> get_write_set() {
        return write_set;
    }
    int getid() {
        return tid;
    }
    int size() {
        return events.size();
    }
    event& at(int i) {
        return events[i];
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


#endif