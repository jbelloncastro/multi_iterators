
#include "MultiIterator.h"
#include <iostream>

int main() {
    int n0[] = {1,2,3,4};
    int n1[] = {5,6,7,8};
    int n2[] = {9,10,11,12};

    for( int v : util::iterate_over(n0, n1, n2) ) {
        std::printf("%d\n", v);
    }
    return 0;
}
