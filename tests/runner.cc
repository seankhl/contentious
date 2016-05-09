
#include <iostream>

#include "bp_vector-tests.h"
#include "reduce-tests.h"
#include "cont_vector-tests.h"

using namespace std;

int main()
{
#ifdef DEBUG
    cout << "debugging" << endl;
#endif
#ifdef RELEASE
    cout << "profiling" << endl;
#endif
    int ret = 0;

    ret += bp_vector_runner();
    ret += reduce_runner();
    ret += cont_vector_runner();

    return ret;
}
