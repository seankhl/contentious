#ifndef BP_REDUCE_TESTS_H
#define BP_REDUCE_TESTS_H

#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <limits>

#include <cmath>

#include <immintrin.h>

#include "cont_vector.h"

double seq_reduce(int64_t test_sz);

void locked_inc(double &locked_ret, int64_t n, std::mutex &ltm);
double locked_reduce(int64_t test_sz);

void atomic_inc(std::atomic<double> &atomic_ret, int64_t n);
double atomic_reduce(int64_t test_sz);

double async_inc(int64_t n);
double async_reduce(int64_t test_sz);

double avx_reduce(int64_t test_sz);

double omp_reduce(int64_t test_sz);

void cont_inc(cont_vector<double> &cont_ret, int64_t n);
double cont_reduce(int64_t test_sz);

void reduce_timing();

#endif // BP_REDUCE_TESTS_H
