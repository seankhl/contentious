
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <cassert>
#include <thread>
#include "boost/coroutine/asymmetric_coroutine.hpp" 

#include "trie.h"
#include "cont_vec.h"

using namespace std;

int test_simple()
{
    PS_Trie<double> test = PS_Trie<double>();
    int test_sz = 129;
    for (int i = 0; i < test_sz; ++i) {
        test.push_back(1000 + test_sz - i);
    }
    for (int i = 0; i < test_sz; ++i) {
        if ((1000 + test_sz - i) != test[i]) {
            cerr << "! test_simple failed: at index " << i 
                 << " got " << test[i] 
                 << ", expected " << 1000 + test_sz - i
                 << endl;
            return 1;
        }
    }
    for (int i = 0; i < test_sz; ++i) {
        test.at(i) = 1000 + i;
    }
    for (int i = 0; i < test_sz; ++i) {
        if ((1000 + i) != test[i]) {
            cerr << "! test_simple failed: at index " << i 
                 << " got " << test[i] 
                 << ", expected " << 1000 + i
                 << endl;
            return 1;
        }
    }
    cerr << "+ test_simple passed" << endl;
    return 0;
}

int test_insert()
{
    // insert random reals into the trie and a reference vec
    random_device r;
    default_random_engine e_val(r());
    uniform_real_distribution<double> dist_real;
    auto rand_val = std::bind(dist_real, e_val);
    vector<double> test_vec;
    PS_Trie<double> test_trie;
    size_t test_sz = 400000;
    for (size_t i = 0; i < test_sz; ++i) {
        double val = rand_val();
        test_vec.push_back(val);
        test_trie.push_back(val);
    }
    // check random values
    //default_random_engine e_ind(r());
    //uniform_int_distribution<size_t> dist_int(0, test_sz);
    //auto rand_ind = std::bind(dist_int, e_ind);
    for (size_t i = 0; i < test_sz; ++i) {
        if ((test_vec.at(i) != test_trie.at(i))) {
            cerr << "test_insert failed: at index " << i 
                 << "; got " << test_trie.at(i) 
                 << ", expected " << test_vec.at(i);
            return 1;
        }
    }
    cerr << "+ test_insert passed" << endl;
    return 0;
}

int test_pers()
{
    vector<PS_Trie<double>> test;
    int test_sz = 1048;
    test.push_back(PS_Trie<double>());
    for (int i = 1; i < test_sz; ++i) {
        test.push_back(test[i-1].pers_push_back(1000 + test_sz - i));
        //cout << i << "- " << test[i-1] << endl;
        //cout << i << "  " << test[i] << endl;
    }
    for (int i = 1; i < test_sz; ++i) {
        for (int j = 0; j < i; ++j) {
            test[i] = test[i].pers_set(j, i);
        }
       //cout << i << "- " << test[i-2] << endl;
       //cout << i << "  " << test[i] << endl;
    }
    vector<double> ref;
    if (!test[0].empty()) {
        cerr << "! test_pers failed: first vector not empty" << endl;
        return 1;
    }
    //cout << "0 " << test[0] << endl;
    for (int i = 1; i < test_sz; ++i) {
        ref.push_back(1000 + test_sz - i);
        for (int j = 0; j < i; ++j) {
            if (i != test[i].at(j)) {
                cerr << "! test_pers failed: at copy " << i 
                     << ", index " << j 
                     << "; got " << test[i].at(j) 
                     << ", expected " << i
                     << endl;
            return 1;
            }
        }
       //cout << i << " " << test[i] << endl;
    }
    cerr << "+ test_pers passed" << endl;
    return 0;
}

int test_coroutine()
{
    stringstream ss;
    const int num=5, width=15;
    boost::coroutines::asymmetric_coroutine<string>::push_type writer(
        [&](boost::coroutines::asymmetric_coroutine<string>::pull_type& in){
            // pull values from upstream, lay them out 'num' to a line
            for (;;){
                for(int i=0;i<num;++i){
                    // when we exhaust the input, stop
                    if(!in) return;
                    ss << std::setw(width) << in.get();
                    // now that we've handled this item, advance to next
                    in();
                }
                // after 'num' items, line break
                ss << endl;
            }
        });

    vector<string> words{   "peas", "porridge", "hot", "peas", "porridge", 
                            "cold", "peas", "porridge", "in", "the", 
                            "pot", "nine", "days", "old"    };

    std::copy(boost::begin(words),boost::end(words),boost::begin(writer));
    string expected = \
"           peas       porridge            hot           peas       porridge\n"
"           cold           peas       porridge             in            the\n"
"            pot           nine           days            old";
    if (ss.str() != expected) {
        cerr << "! test_coroutine failed: wrong output" << endl;
        return 1;
    }
    cerr << "+ test_coroutine passed" << endl;
    return 0;
}

void my_accumulate(Cont_Vec<double> &test, size_t index)
{
    Splinter_Vec<double> mine = test.detach(new Plus<double>());
    for (int i = 0; i < 10; ++i) {
        mine = mine.comp(index, i);
    }
    test.reattach(mine);
}

int test_cvec()
{
    Cont_Vec<double> test;
    test.push_back(0);
    test.push_back(1);
    test.push_back(2);
    test.push_back(3);
    uint16_t num_threads = thread::hardware_concurrency() * 100;
    cout << num_threads << endl;
    vector<thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(thread(my_accumulate, std::ref(test), 1));
    }
    for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
    }

    cout << test << endl;
    test.print_unresolved_info();
    test.resolve();
    cout << test << endl;

    return 0;
}

int main()
{
#ifdef DEBUG
    cout << "debugging" << endl;
#endif
#ifdef RELEASE
    cout << "ooh" << endl;
#endif
    vector<function<int()>> runner;
    runner.push_back(test_simple);
    runner.push_back(test_insert);
    runner.push_back(test_pers);
    runner.push_back(test_coroutine);
    runner.push_back(test_cvec);
    int num_tests = runner.size();
    int ret = 0;
    for (int i = 0; i < num_tests; ++i) {
        ret += runner[i]();
    }
    cout << num_tests-ret << "/" << num_tests;
    if (ret == 0) {
        cout << " all tests passed!" << endl;
    }
    return ret;
}

