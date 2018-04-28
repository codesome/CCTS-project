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

struct event {
    bool is_write;
    int object_id;
    
    event(bool is_write, int object_id): is_write(is_write), object_id(object_id) {}
    
    void print_event_message(int tid, int trans_id, FILE *fp) {
        if(is_write) {
            fprintf(fp, "Writing %c by transaction %d which is invoked by thread %d\n", object_id, trans_id, tid );
            fflush(fp);
        } else {
            fprintf(fp, "Reading %c by transaction %d which is invoked by thread %d\n", object_id, trans_id, tid );
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

    void commit(FILE *fp) {
        fprintf(fp, "Commit transaction %d\n", tid);
        fflush(fp);
    }

    void abort(FILE *fp) {
        fprintf(fp, "Abort transaction %d\n", tid);
        fflush(fp);
    }

};


#endif