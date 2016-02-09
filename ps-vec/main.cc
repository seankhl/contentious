#include "trie.h"

#include <iostream>
#include <chrono>
#include <random>
#include <assert.h>

using namespace std;

int test_simple()
{
    PS_Trie<double> test = PS_Trie<double>();
    int test_sz = 128;
    for (int i = 0; i < test_sz; ++i) {
        test.push_back(1000 + test_sz - i);
    }
    for (int i = 0; i < test_sz; ++i) {
        if ((1000 + test_sz - i) != test.get(i)) {
            cerr << "test_simple failed: at index " << i 
                 << " got " << test.get(i) 
                 << ", expected " << 1000 + test_sz - i;
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
    default_random_engine e_ind(r());
    uniform_int_distribution<size_t> dist_int(0, test_sz);
    auto rand_ind = std::bind(dist_int, e_ind);
    for (size_t i = 0; i < test_sz; ++i) {
        if ((test_vec.at(i) != test_trie.get(i))) {
            cerr << "test_insert failed: at index " << i 
                 << " got " << test_trie.get(i) 
                 << ", expected " << test_vec.at(i);
            return 1;
        }
    }
    cerr << "+ test_insert passed" << endl;
    return 0;
}

int main()
{
    int ret = 0;
    ret &= test_simple();
    ret &= test_insert();
    if (ret == 0) {
        cout << "all tests passed!" << endl;
    }
    return ret;
}
