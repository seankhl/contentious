
#include <iostream>
#include <random>

#include "../bp_vector/cont_vector.h"

#include "cont_vector-tests.h"

using namespace std;


/* old, but useful for testing ************************************************/

void cont_inc_reduce(cont_vector<double> &cont_ret,
                     vector<double>::const_iterator a,
                     vector<double>::const_iterator b)
{
    //chrono::time_point<chrono::system_clock> splt_start, splt_end;
    //splt_start = chrono::system_clock::now();
    splt_vector<double> splt_ret = cont_ret.detach(cont_ret);
    double temp(0);
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
    cont_vector<double> next = cont_vector<double>(cont_ret);
    cont_ret.freeze(next);
    cont_ret.reattach(splt_ret, next, 0, next.size());
}
double cont_reduce_manual(const vector<double> &test_vec)
{
    cont_vector<double> cont_ret(contentious::plus);
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

    return cont_ret[0];
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
    cont_vector<double> cont_inp(contentious::plus);
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    auto cont_ret = cont_inp.reduce(contentious::plus);
    cont_inp.resolve(cont_ret);
    //auto cont_ret2 = cont_ret.foreach(new Plus<double>(), 2);
    cout << cont_ret[0] << endl;
}

void stdv_foreach(const vector<double> &test_vec)
{
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    vector<double> inp;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        inp.push_back(test_vec[i]);
    }
    auto ret = inp;
    for (size_t i = 0; i < inp.size(); ++i) {
        ret[i] = inp[i] * 2;
    }
    
    /*
    std::cout << "[";
    for (int i = 0; i < ret.size(); ++i) {
        std::cout << ret[i] << " ";
    }
    std::cout << "]" << std::endl;
    */
    
    vector<double> other;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        other.push_back(i);
    }
    auto ret2 = ret;
    for (size_t i = 0; i < inp.size(); ++i) {
        ret2[i] = ret[i] * other[i];
    }
    
    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    std::cout << "stdv_foreach_inner took: " << splt_dur.count() << " seconds "
              << std::endl;
    
    for (size_t i = 0; i < test_vec.size(); ++i) {
        if (test_vec[i] * 2 * i != ret2[i]) {
            cout << "bad resolution at ret2[" << i << "]: "
                 << test_vec[i] * 2 * i << " "
                 << ret2[i] << endl;
        }
    }
}

void cont_foreach(const vector<double> &test_vec)
{
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();
    
    cont_vector<double> cont_inp(contentious::mult);
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    auto cont_ret = cont_inp.foreach(contentious::mult, 2);
    
    cont_vector<double> cont_other(contentious::mult);
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_other.unprotected_push_back(i);
    }
    auto cont_ret2 = cont_ret.foreach(contentious::mult, cont_other);

    cont_inp.resolve(cont_ret);
    cont_ret.resolve(cont_ret2);
    
    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    std::cout << "cont_foreach_inner took: " << splt_dur.count() << " seconds "
              << std::endl;
    
    /*
    for (size_t i = 0; i < cont_inp.size(); ++i) {
        if (cont_inp[i] * 2 != cont_ret[i]) {
            cout << "bad resolution at cont_ret[" << i << "]: "
                 << cont_inp[i] * 2 << " " << cont_ret[i] << endl;
        }
    }
    */
    for (size_t i = 0; i < test_vec.size(); ++i) {
        if (test_vec[i] * 2 * i != cont_ret2[i]) {
            cout << "bad resolution at cont_ret[" << i << "]: "
                 << test_vec[i] * 2 * i << " "
                 << cont_ret2[i] << endl;
        }
    }
}

void cont_stencil(const vector<double> &test_vec)
{
    cont_vector<double> cont_inp(contentious::mult);
    for (size_t i = 0; i < 16 /*test_vec.size()*/; ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    cout << "cont_inp: " << cont_inp << endl;
    auto cont_ret = cont_inp.stencil({-1, 1}, {2, 3});
    for (size_t i = 0; i < cont_ret.size(); ++i) {
        double change = 1;
        if (i > 0) { change *= cont_inp[i-1]*2; }
        if (i < cont_ret.size()-1) { change *= cont_inp[i+1]*3; }
        // o the humanity
        if (std::abs(cont_inp[i] * change - cont_ret[i]) > 0.00000001) {
            cout << "bad resolution at cont_ret[" << i << "]: "
                 << cont_inp[i] * change << " "
                 << cont_ret[i] << endl;
        }
    }
    //cont_inp.resolve(cont_ret);
    //cout << "cont_ret: " << cont_ret << endl;
}


/* runner *********************************************************************/

int cont_vector_runner()
{
    int64_t test_sz = numeric_limits<int64_t>::max() / pow(2,39);
    cout << "foreach with size: " << test_sz << endl;
    if (test_sz < 0) return 1;

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
    //cont_reduce(test_vec);

    chrono::time_point<chrono::system_clock> stdv_start, stdv_end;
    stdv_start = chrono::system_clock::now();
    for (size_t i = 0; i < 1; ++i) {
        stdv_foreach(test_vec);
    }
    stdv_end = chrono::system_clock::now();
    chrono::duration<double> stdv_dur = stdv_end - stdv_start;

    chrono::time_point<chrono::system_clock> cont_start, cont_end;
    cont_start = chrono::system_clock::now();
    for (size_t i = 0; i < 1; ++i) {
        cont_foreach(test_vec);
    }
    cont_end = chrono::system_clock::now();
    chrono::duration<double> cont_dur = cont_end - cont_start;

    cout << "stdv took: " << stdv_dur.count() << " seconds; " << endl;
    cout << "cont took: " << cont_dur.count() << " seconds; " << endl;
    cout << "ratio: " << cont_dur.count() / stdv_dur.count() << endl;

    //cont_stencil(test_vec);
    cout << "DONE! " << test_sz << endl;

    return 0;
}

