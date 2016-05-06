
#include <iostream>

#include "cont_vector-tests.h"
#include "cont_vector.h"

using namespace std;


/* old, but useful for testing ************************************************/

void cont_inc_reduce(cont_vector<double> &cont_ret,
              vector<double>::const_iterator a, vector<double>::const_iterator b)
{
    //chrono::time_point<chrono::system_clock> splt_start, splt_end;
    splt_vector<double> splt_ret = cont_ret.detach();
    double temp(0);
    //splt_start = chrono::system_clock::now();
    for (auto it = a; it != b; ++it) {
        temp += *it;
    }
    splt_ret.mut_comp(0, temp);

    /*
    double avx_ret(0);
    __m256d vals = _mm256_set_pd(0, 0, 0, 0);
    __m256d temps;
    for (int64_t i = 0; i < n; i += 4) {
        temps = _mm256_set_pd(1, 1, 1, 1);
        vals = _mm256_add_pd(vals, temps);
    }
    __m256d hsum = _mm256_hadd_pd(vals, vals);
    __m256d perm = _mm256_permute2f128_pd(hsum, hsum, 0x21);
    __m256d mret = _mm256_add_pd(hsum, perm);
    __m128d temp = _mm256_extractf128_pd(mret, 0);
    _mm_storel_pd(&avx_ret, temp);
    splt_ret.comp(0, avx_ret);
    */

    //splt_end = chrono::system_clock::now();
    //chrono::duration<double> splt_dur = splt_end - splt_start;
    //cout << "splt took: " << splt_dur.count() << " seconds; " << endl;
    //cout << "one cont_inc done: " << splt_ret.data.at(0) << endl;
    cont_vector<double> *next = new cont_vector<double>(cont_ret);
    cont_ret.register_dependent(next);
    cont_ret.reattach(splt_ret, *next);
}
double cont_reduce_manual(const vector<double> &test_vec)
{
    cont_vector<double> cont_ret(new Plus<double>());
    cont_ret.unprotected_push_back(0);
    vector<thread> cont_threads;
    int num_threads = thread::hardware_concurrency(); // * 16;
    size_t chunk_sz = test_vec.size()/num_threads;
    for (int i = 0; i < num_threads; ++i) {
        cont_threads.push_back(
          thread(cont_inc_reduce,
                 std::ref(cont_ret),
                 test_vec.begin() + chunk_sz * i,
                 test_vec.begin() + chunk_sz * (i+1)));
    }
    for (int i = 0; i < num_threads; ++i) {
        cont_threads[i].join();
    }
    return cont_ret.at_prescient(0);
}

/*
void cont_inc_foreach(cont_vector<double> &cont_ret,
              vector<double>::const_iterator a, vector<double>::const_iterator b)
{
    //chrono::time_point<chrono::system_clock> splt_start, splt_end;
    splt_vector<double> splt_ret = cont_ret.detach();
    //splt_start = chrono::system_clock::now();
    size_t chunk_sz = test_vec.size()/num_threads;

    // TODO: iterators, or at least all leaves at a time
    auto it_chunk_begin = splt_ret.begin() + chunk_sz * (i);
    auto it_chunk_end = splt_ret.begin() + chunk_sz * (i+1);
    for (auto it = it_chunk_begin; it != it_chunk_end; ++it) {
        splt_ret.mut_comp(*it, );
    }
    for (int i = chunk_sz * tid; i < chunk_sz * (tid+1); ++i) {
        cont_ret.mut_comp(i, 1);
    }
    cont_ret.join(splt_ret);
}
double cont_foreach(const vector<double> &test_vec)
{
    cont_vector<double> cont_ret(test_vec, new Plus<double>());
    vector<thread> cont_threads;
    int num_threads = thread::hardware_concurrency(); // * 16;
    size_t chunk_sz = test_vec.size()/num_threads;
    for (int i = 0; i < num_threads; ++i) {
        cont_threads.push_back(
          thread(cont_inc_foreach,
                 std::ref(cont_ret),
                 test_vec.begin() + chunk_sz * i,
                 test_vec.begin() + chunk_sz * (i+1)));
    }
    for (int i = 0; i < num_threads; ++i) {
        cont_threads[i].join();
    }
    return cont_ret.at_prescient(0);
}
*/


/* new, uses members of cont_vector *******************************************/

void cont_reduce(const vector<double> &test_vec)
{
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cout << test_vec[i] << " ";
    }
    cout << endl;
    cont_vector<double> cont_inp(new Plus<double>());
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    cout << cont_inp << endl;
    auto cont_ret = cont_inp.reduce(new Plus<double>());
    //auto cont_ret2 = cont_ret.foreach(new Plus<double>(), 2);
    cout << cont_ret << endl;
    /*
    for (size_t i = 0; i < cont_ret2.size(); ++i) {
        cout << cont_ret2[i] << " ";
    }
    cout << endl;
    */
}

void cont_foreach(const vector<double> &test_vec)
{
    /*
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cout << test_vec[i] << " ";
    }
    cout << endl;
    */
    cont_vector<double> cont_inp(new Multiply<double>());
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    cout << cont_inp << endl;
    auto cont_ret = cont_inp.foreach(new Multiply<double>(), 2);
    //cont_inp.resolve(cont_ret);
    //cont_inp.resolve(*cont_ret);
    //cout << cont_inp << endl;
    cout << *cont_ret << endl;

    /*
    cont_vector<double> cont_other(new Multiply<double>());
    cout << "cont_other: ";
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_other.unprotected_push_back(i);
        cout << cont_other[i] << " ";
    }
    cout << endl;
    */
    auto cont_ret2 = cont_ret->foreach(new Multiply<double>(), 3);
    cont_ret2->reset_latch(0);
    cont_inp.resolve(*cont_ret);
    cont_ret->resolve(*cont_ret2);
    cout << *cont_ret2 << endl;
    for (size_t i = 0; i < cont_inp.size(); ++i) {
        if (cont_inp[i] * 6 != cont_ret2->at(i)) {
            cout << "bad resolution at i: "
                 << cont_inp[i] << " "
                 << cont_ret2->at(i) << endl;
        }
    }
}

void cont_stencil(const vector<double> &test_vec)
{
    cont_vector<double> cont_other(new Multiply<double>());
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_other.unprotected_push_back(1);
    }
    cout << "cont_other: " << cont_other << endl;
    auto cont_ret = cont_other.stencil({-1, 1}, {2, 3});
    cout << "cont_ret: " << cont_ret << endl;
}


/* runner *********************************************************************/

void cont_testing()
{
    int64_t test_sz = 33; // numeric_limits<int64_t>::max() / pow(2,36);

    random_device rnd_device;
    mt19937 mersenne_engine(rnd_device());
    uniform_real_distribution<double> dist(-32.768, 32.768);

    auto gen = std::bind(dist, mersenne_engine);
    vector<double> test_vec(test_sz);
    generate(begin(test_vec), end(test_vec), gen);

    double answer_new = 0;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        answer_new += test_vec[i];
    }
    cout << answer_new << endl;
    //cont_reduce_new(test_vec);
    cont_foreach(test_vec);
    //cont_stencil(test_vec);
}

