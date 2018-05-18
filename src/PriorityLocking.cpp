#include <iostream>
#include <vector>
#include <algorithm>

#include "PriorityLocking.h"

int main(int argc, char const *argv[]) {

    int size, n_threads, ne, lambda;
    std::cin >> size >> n_threads >> ne >> lambda;
    srand(lambda);
    PriorityLocking sched(n_threads);
    char c;

    srand(lambda);
    
    sched.priorities.push_back(0);
    std::vector<int> pri(size);
    for (int i = 0; i < size; ++i) {
        pri[0] = (i * 1000) / size;
    }
    std::random_shuffle(pri.begin(), pri.end());
    for (int i = 0; i < size; ++i) {
        transaction t(i+1);
        int n_ops;
        bool is_write;
        std::cin >> n_ops;
        for (int j = 0; j < n_ops ; ++j) {
            std::cin >> is_write;
            std::cin >> c;
            if(object_map.find(c)==object_map.end()) {
                object_map.emplace(c, new_object_ptr(c));
            }
            t.add_event(is_write, c);
        }
        sched.add_transaction(t);
        sched.priorities.push_back(pri[i]);            
    }

    double avg_time = sched.simulate();
    
    FILE *fp = fopen ("Average_time_PriorityLocking.txt", "w+");
    fprintf(fp, "Average Time: %.9lf seconds\n", avg_time/1e6);
    fclose(fp);
    printf("Avg. Time: %.9lf\n", avg_time/1e6);
    printf("Committed %d\n", sched.commit_count());
    printf("Aborted: %d\n", sched.abort_count());

    return 0;
}