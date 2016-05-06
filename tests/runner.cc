
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

    vector<function<int()>> runner;
    runner.push_back(test_simple);
    runner.push_back(test_insert);
    runner.push_back(test_pers);
    runner.push_back(test_pers_alternate);
    //runner.push_back(test_pers_iter);
    runner.push_back(test_trans);
    runner.push_back(test_make);
    runner.push_back(test_coroutine);
    //runner.push_back(test_coroutine_practical);
    runner.push_back(test_cvec);
    int num_tests = runner.size();
    for (int i = 0; i < num_tests; ++i) {
        ret += runner[i]();
    }
    cout << num_tests-ret << "/" << num_tests;
    if (ret == 0) {
        cout << " all tests passed!" << endl;
    }
    else {
        cout << " " << ret << " tests failed!" << endl;
    }
    //vec_timing();
    reduce_timing();
    cont_testing();

    return ret;
}
