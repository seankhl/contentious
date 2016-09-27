
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
    cout << "# logical procs: "
         << std::thread::hardware_concurrency() 
         << std::endl;
    cout << "HWCONC: " << contentious::HWCONC << endl;
    cout << "BPBITS: " << static_cast<int>(BP_BITS) << endl;

    int ret = 0;
    //ret += bp_vector_runner();
    //ret += reduce_runner();
    ret += cont_vector_runner();

    contentious::tp.stop();
    return ret;
}
