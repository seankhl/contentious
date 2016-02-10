#include "trie.h"

#include <iostream>
#include <chrono>
#include <random>
#include <assert.h>

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
    int test_sz = 33;
    test.push_back(PS_Trie<double>());
    for (int i = 1; i < test_sz; ++i) {
        test.push_back(test[i-1].pers_push_back(1000 + test_sz - i));
    }
    vector<double> ref;
    if (!test[0].empty()) {
        cerr << "! test_pers failed: first vector not empty" << endl;
        return 1;
    }
    //cout << test[0] << endl;
    for (int i = 1; i < test_sz; ++i) {
        ref.push_back(1000 + test_sz - i);
        for (int j = 0; j < i; ++j) {
            if (ref[j] != test[i].at(j)) {
                cerr << "! test_pers failed: at copy " << i 
                     << ", index " << j 
                     << "; got " << test[i].at(j) 
                     << ", expected " << 1000 + test_sz - i
                     << endl;
            return 1;
            }
        }
       //cout << test[i] << endl;
    }
    cerr << "+ test_pers passed" << endl;
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
    int num_tests = 3;
    int ret = 0;
    ret += test_simple();
    ret += test_insert();
    ret += test_pers();
    cout << num_tests-ret << "/" << num_tests;
    if (ret == 0) {
        cout << " all tests passed!" << endl;
    }
    return ret;
}
