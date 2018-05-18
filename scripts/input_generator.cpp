#include <iostream>
#include <random>
#include <unordered_set>
using namespace std;


int main(int argc, char const *argv[]) {

	int n_trans = atoi(argv[1]);
    int n_threads = atoi(argv[2]);
    int max_ops = atoi(argv[3]);
    int lambda = atoi(argv[4]);
	
	random_device rd;
    mt19937 engine(rd());
    uniform_int_distribution<> ops(max_ops/3,max_ops);

    uniform_int_distribution<> objsr('a','a'+max_ops);

    printf("%d %d %d %d\n", n_trans, n_threads, max_ops, lambda);

    for (int i = 0; i < n_trans; ++i) {
    	int n_ops = ops(engine);
    	unordered_set<int> objs;

    	while(objs.size()<n_ops) {
    		int ele = objsr(engine);
    		objs.insert(ele);
    	}
	    
	    // uniform_int_distribution<> uidx(1,n_ops/2);
	    int pivot = n_ops/2;
		if(n_ops + n_ops - pivot -1 <= 0) {
			i--;
			continue;
		}
    	printf("%d ", n_ops + n_ops - pivot -1 );
    	for(int i: objs) {
    		printf("0 %c ",i );
    	}

	    int cnt = 0;
    	for(int i: objs) {
    		if(cnt > pivot) {
    			printf("1 %c ",i );
    		}
    		cnt++;
    	}
    	printf("\n");

    }

	return 0;
}