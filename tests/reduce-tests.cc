
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <limits>
#include <random>

#include <cmath>

#include <immintrin.h>

#include <CL/cl.h>

#include "../bp_vector/cont_vector.h"

#include "reduce-tests.h"
#include "timing.h"

using namespace std;

double seq_reduce(const vector<double> &seq_vec)
{
    double seq_ret(0);
    for (auto it = seq_vec.begin(); it != seq_vec.end(); ++it) {
        seq_ret += *it;
    }
    return seq_ret;
}

void locked_inc(double &locked_ret,
                vector<double>::const_iterator a, vector<double>::const_iterator b,
                mutex &ltm)
{
    for (auto it = a; it != b; ++it) {
        lock_guard<mutex> lock(ltm);
        locked_ret += *it;
    }
}
double locked_reduce(const vector<double> &test_vec)
{
    double locked_ret(0);
    mutex ltm;
    vector<thread> locked_threads;
    int num_threads = thread::hardware_concurrency();
    size_t chunk_sz = test_vec.size()/num_threads;
    for (int i = 0; i < num_threads; ++i) {
        locked_threads.push_back(
          thread(locked_inc,
                 std::ref(locked_ret),
                 test_vec.begin() + chunk_sz * i,
                 test_vec.begin() + chunk_sz * (i+1),
                 std::ref(ltm)));
    }
    for (int i = 0; i < num_threads; ++i) {
        locked_threads[i].join();
    }
    return locked_ret;
}

void atomic_inc(atomic<double> &atomic_ret,
                vector<double>::const_iterator a, vector<double>::const_iterator b)
{
    for (auto it = a; it != b; ++it) {
        auto cur = atomic_ret.load();
        while (!atomic_ret.compare_exchange_weak(cur, cur + *it)) {
            ;
        }
    }
}
double atomic_reduce(const vector<double> &test_vec)
{
    atomic<double> atomic_ret(0);
    vector<thread> atomic_threads;
    int num_threads = thread::hardware_concurrency();
    size_t chunk_sz = test_vec.size()/num_threads;
    for (int i = 0; i < num_threads; ++i) {
        atomic_threads.push_back(
          thread(atomic_inc, std::ref(atomic_ret),
                 test_vec.begin() + chunk_sz * i,
                 test_vec.begin() + chunk_sz * (i+1)));
    }
    for (int i = 0; i < num_threads; ++i) {
        atomic_threads[i].join();
    }
    return atomic_ret;
}

double async_inc(vector<double>::const_iterator a, vector<double>::const_iterator b)
{
    //chrono::time_point<chrono::system_clock> async_piece_start, async_piece_end;
    //async_piece_start = chrono::system_clock::now();
    double async_ret = 0;
    for (auto it = a; it != b; ++it) {
        async_ret += *it;
    }
    //async_piece_end = chrono::system_clock::now();
    //chrono::duration<double> async_piece_dur = async_piece_end - async_piece_start;
    //cout << "async took: " << async_piece_dur.count() << " seconds; " << endl;
    return async_ret;
}
double async_reduce(const vector<double> &test_vec)
{
    double async_ret(0);
    vector<future<double>> async_pieces;
    int16_t num_threads = thread::hardware_concurrency();
    size_t chunk_sz = test_vec.size()/num_threads;
    for (int i = 0; i < num_threads; ++i) {
        async_pieces.push_back(
                async(launch::async, &async_inc,
                      test_vec.begin() + chunk_sz * i,
                      test_vec.begin() + chunk_sz * (i+1)));
    }
    for (int i = 0; i < num_threads; ++i) {
        async_ret += async_pieces[i].get();
    }
    return async_ret;
}

double avx_reduce(const vector<double> &test_vec)
{
    double avx_ret(0);
    __m256d vals = _mm256_set_pd(0, 0, 0, 0);
    __m256d temps;
    for (auto it = test_vec.begin(); it != test_vec.end(); it += 4) {
        temps = _mm256_set_pd(*it, *(it+1), *(it+2), *(it+3));
        vals = _mm256_add_pd(vals, temps);
    }
    __m256d hsum = _mm256_hadd_pd(vals, vals);
    __m256d perm = _mm256_permute2f128_pd(hsum, hsum, 0x21);
    __m256d mret = _mm256_add_pd(hsum, perm);
    __m128d temp = _mm256_extractf128_pd(mret, 0);
    _mm_storel_pd(&avx_ret, temp);
    return avx_ret;
}

double omp_reduce(const vector<double> &test_vec)
{
    double omp_ret(0);
#pragma omp parallel for reduction(+:omp_ret)
/*
  default(shared) private(i)    \
  schedule(static, chunk)       \
*/
    for (size_t i = 0; i < test_vec.size(); ++i) {
        omp_ret += test_vec[i];
    }
    return omp_ret;
}

double vec_reduce(const vector<double> &test_vec)
{
    vector<double> vec_ret{0};
    for (auto it = test_vec.begin(); it != test_vec.end(); ++it) {
        vec_ret[0] += *it;
    }
    return vec_ret[0];
}

double cont_reduce_dup(const cont_vector<double> &cont_arg)
{
    auto cont_inp(cont_arg);
    //cont_vector<double> cont_inp(new Plus<double>());
    //for (size_t i = 0; i < test_vec.size(); ++i) {
    //    cont_inp.unprotected_push_back(test_vec[i]);
    //}
    //cout << cont_inp << endl;
    chrono::time_point<chrono::system_clock> cont_piece_start, cont_piece_end;
    cont_piece_start = chrono::system_clock::now();
    auto cont_ret = cont_inp.reduce(new Plus<double>());
    cont_inp.resolve(cont_ret);
    cont_piece_end = chrono::system_clock::now();
    chrono::duration<double> cont_piece_dur = cont_piece_end - cont_piece_start;
    cout << "cont took: " << cont_piece_dur.count() << " seconds; " << endl;
    //auto cont_ret2 = cont_ret.foreach(new Plus<double>(), 2);
    //cout << cont_ret[0] << endl;
    /*
    for (size_t i = 0; i < cont_ret2.size(); ++i) {
        cout << cont_ret2[i] << " ";
    }
    cout << endl;
    */
    return cont_ret[0];
}


int reduce_runner()
{
    int64_t test_sz = numeric_limits<int64_t>::max() / pow(2,42);

    random_device rnd_device;
    mt19937 mersenne_engine(rnd_device());
    uniform_real_distribution<double> dist(-32.768, 32.768);
    auto gen = std::bind(dist, mersenne_engine);
    
    // make regular vector with vals in it
    vector<double> test_vec(test_sz);
    generate(begin(test_vec), end(test_vec), gen);

    // make cont_vector with vals in it
    cont_vector<double> test_cvec(new Plus<double>());
    for (size_t i = 0; i < test_vec.size(); ++i) {
        test_cvec.unprotected_push_back(test_vec[i]);
    }
    //cout << test_cvec << endl;
    
    // compute reference answer
    double answer = 0;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        answer += test_vec[i];
    }
    cout << "reference: size is " << test_vec.size() 
         << " and reduce with addition gives " << answer << endl;

    // create runner for all the variations of reduce
    map< string, function< pair<double, chrono::duration<double>>() >> runner {
        //bind(timetest<double, vector<double>>, &locked_reduce, test_vec),
        //bind(timetest<double, vector<double>>, &atomic_reduce, test_vec),
        { "async",  bind(timetest<double, vector<double>>, 
                            &async_reduce, test_vec)                },
        /*{ "avx",    bind(timetest<double, vector<double>>, 
                            &avx_reduce, test_vec)                  },*/
        { "cont",   bind(timetest<double, cont_vector<double>>, 
                            &cont_reduce_dup, std::cref(test_cvec)) },
        { "omp",    bind(timetest<double, vector<double>>, 
                            &omp_reduce, test_vec)                  },
        { "seq",    bind(timetest<double, vector<double>>, 
                            &seq_reduce, test_vec)                  },
        { "vec",    bind(timetest<double, vector<double>>, 
                            &vec_reduce, test_vec)                  }
    };

    double out, cont_dur = 0, vec_dur = 0;
    chrono::duration<double> dur;
    for (const auto &test : runner) {
        tie(out, dur) = test.second();
        cout << test.first << endl
             << "  diff: " << out - answer << endl
             << "  took: " << dur.count() << " seconds; " << endl;
        if      (test.first == "cont")  { cont_dur = dur.count(); } 
        else if (test.first == "vec")   { vec_dur  = dur.count(); }
    }
    cout << "ratio is: " << cont_dur/vec_dur << endl;

    return 0;
}

