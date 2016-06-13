
#include "bp_vector-tests.h"
#include "reduce-tests.h"
#include "cont_vector-tests.h"

#include "../bp_vector/cont_vector.h"

#include <iostream>

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

    //ret += bp_vector_runner();
    //ret += reduce_runner();
    ret += cont_vector_runner();

    contentious::tp.stop();
    return ret;
}
