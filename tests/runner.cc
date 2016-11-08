
#include "bp_vector-tests.h"
#include "reduce-tests.h"
#include "cont_vector-tests.h"

#include "contentious/cont_vector.h"

#include <iostream>
#include <iomanip>

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
         << thread::hardware_concurrency() 
         << endl;
    cout << "HWCONC: " << contentious::HWCONC << endl;
    cout << "BPBITS: " << static_cast<int>(BP_BITS) << endl;

#ifdef CTTS_TIMING
    for (uint16_t p = 0; p < contentious::HWCONC; ++p) {
        contentious::rslv_series[p].start(chrono::steady_clock::now());
    }
#endif
    int ret = 0;
    //ret += bp_vector_runner();
    //ret += reduce_runner();
    ret += cont_vector_runner();

    contentious::tp.stop();

#ifdef CTTS_STATS
    for (uint16_t p = 0; p < contentious::HWCONC; ++p) {
        cout << contentious::conflicted[p].back() << endl;
    }
#endif
#ifdef CTTS_TIMING
    {   using namespace fmt::literals;
    for (uint16_t p = 0; p < contentious::HWCONC; ++p) {
        ostringstream ss;
        ss << setw(2) << setfill('0') << contentious::HWCONC;
        string pstr(ss.str());
        ss.str("");
        ss << setw(3) << setfill('0') << 10;
        string ustr(ss.str());
        auto fname = "durs_mean.log";

        auto splt_data = slbench::compute_data(contentious::splt_durs[p]);
        fmt::print("{:>24s}{:>24s}{:>24s}{:>24s}{:>24s}\n",
                   "iterations", "avg", "min", "max", "var");
        cout << "splt_data[" << p << "]: " << splt_data << endl;
        
        auto splt_output = slbench::output<>{
            {"splt_data_{}_{}"_format(pstr, ustr), splt_data}
        };
        slbench::log_output(fname, splt_output);
        
        auto rslv_data = slbench::compute_data(contentious::rslv_durs[p]);
        cout << "rslv_data[" << p <<"]: " << rslv_data << endl;
        
        auto rslv_output = slbench::output<>{ 
            {"rslv_data_{}_{}"_format(pstr, ustr), rslv_data}
        };
        slbench::log_output(fname, rslv_output);
    }
    }
#endif
    return ret;
}
