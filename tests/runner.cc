
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

#ifdef CTTS_STATS
    cout << contentious::conflicted << endl;
#endif
#ifdef CTTS_TIMING
    for (int p = 0; p < contentious::HWCONC; ++p) {
        auto splt_data = slbench::compute_data(contentious::splt_durs[p]);
        cout << "splt_data[" << p << "]: " << splt_data << endl;
        if (!contentious::rslv_durs[p].durs.empty()) {
            auto rslv_data = slbench::compute_data(contentious::rslv_durs[p]);
            cout << "rslv_data[" << p <<"]: " << rslv_data << endl;
        } else {
            cout << "No rslv_data vals at " << p << endl;
        }
    }
#endif
    return ret;
}
