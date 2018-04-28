#include <iostream>
#include <vector>

#include "MVTO.h"

int main(int argc, char const *argv[]) {

    int size, n_threads, ne, lambda;
    std::cin >> size >> n_threads >> ne >> lambda;
    srand(lambda);
    MVTO sched(n_threads);
    char c;

    // reading the input
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
    }

    double avg_time = sched.simulate();
    
    FILE *fp = fopen ("Average_time_MVTO.txt", "w+");
    fprintf(fp, "Average Time: %.9lf seconds\n", avg_time/1e6);
    fclose(fp);
    printf("Average Time: %.9lf seconds\n", avg_time/1e6);

    return 0;
}