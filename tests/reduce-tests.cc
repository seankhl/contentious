
#include "reduce-tests.h"
#include "timing.h"
#include "../bp_vector/cont_vector.h"

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


using namespace std;

double seq_reduce(const vector<double> &seq_vec)
{
    double seq_ret(0);
    for (auto it = seq_vec.begin(); it != seq_vec.end(); ++it) {
        seq_ret += *it;
    }
    return seq_ret;
}

double vec_reduce(const vector<double> &test_vec)
{
    vector<double> vec_ret{0};
    for (auto it = test_vec.begin(); it != test_vec.end(); ++it) {
        vec_ret[0] += *it;
    }
    return vec_ret[0];
}

void locked_inc(double &locked_ret,
                vector<double>::const_iterator a,
                vector<double>::const_iterator b,
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
                vector<double>::const_iterator a,
                vector<double>::const_iterator b)
{
    for (auto it = a; it != b; ++it) {
        auto cur = atomic_ret.load();
        while (!atomic_ret.compare_exchange_weak(cur, cur + *it)) {
            continue;
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

double async_inc(vector<double>::const_iterator a,
                 vector<double>::const_iterator b)
{
    double async_ret = 0;
    for (auto it = a; it != b; ++it) {
        async_ret += *it;
    }
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

int reduce_runner()
{
    constexpr int64_t test_sz = std::pow(2,24);
    static_assert(test_sz > 0, "Must run with test size > 0");

    cout << "**** Testing reduce with size: " << test_sz << endl;

    random_device rnd_device;
    mt19937 mersenne_engine(rnd_device());
    uniform_real_distribution<double> dist(-32.768, 32.768);
    auto gen = std::bind(dist, mersenne_engine);

    // make std::vector with vals in it
    vector<double> test_vec(test_sz);
    generate(begin(test_vec), end(test_vec), gen);

    // make cont_vector with vals in it
    cont_vector<double> test_cvec;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        test_cvec.unprotected_push_back(test_vec[i]);
    }

    // compute reference answer
    double answer = 0;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        answer += test_vec[i];
    }
    // create runner for all the variations of reduce
    slbench::suite<double> reduce_suite {
        { "async",  slbench::make_bench<16>(async_reduce, test_vec)           },
        { "avx",    slbench::make_bench<16>(avx_reduce, test_vec)             },
        { "omp",    slbench::make_bench<16>(omp_reduce, test_vec)             },
        { "seq",    slbench::make_bench<16>(seq_reduce, test_vec)             },
        { "vec",    slbench::make_bench<16>(vec_reduce, test_vec)             },
        { "cont",   slbench::make_bench<16>(
                        *[](cont_vector<double> &v) {
                            auto cont_ret = v.reduce(contentious::plus<double>);
                            contentious::tp.finish();
                            return cont_ret[0];
                        },
                        test_cvec)                                      }
    };
    auto reduce_output = slbench::run_suite(reduce_suite);
    cout << reduce_output << endl;

    /*
    for (const auto &test : runner) {
        auto output = test.second();
        cout << test.first << endl
             << "  diff: " << output.res - answer << endl
             << "  took: " << output.min.count() << " seconds; " << endl;
        if      (test.first == "cont")  { cont_dur = output.min.count(); }
        else if (test.first == "vec")   { vec_dur  = output.min.count(); }
    }
    cout << "ratio is: " << cont_dur/vec_dur << endl;
    */

    return 0;
}

