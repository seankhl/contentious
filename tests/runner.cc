
#include "bp_vector-tests.h"
#include "reduce-tests.h"
#include "cont_vector-tests.h"

#include "contentious/cont_vector.h"

#include <iostream>

using namespace std;

int main()
{
#ifdef DEBUG
    cout << "Debugging..." << endl;
#endif
#ifdef RELEASE
    cout << "Benchmarking..." << endl;
#endif
    int ret = 0;

    cout << "HWCONC: " << contentious::HWCONC << endl;
    cout << "BPBITS: " << static_cast<int>(BP_BITS) << endl;

    //ret += bp_vector_runner();
    //ret += reduce_runner();
    ret += cont_vector_runner();

    contentious::tp.stop();
    return ret;
}
