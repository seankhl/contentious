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

#include <CL/cl.h>

#include "cont_vector.h"

double seq_reduce(const std::vector<double> &test_vec);
double locked_reduce(const std::vector<double> &test_vec);
double atomic_reduce(const std::vector<double> &test_vec);
double async_reduce(const std::vector<double> &test_vec);
double avx_reduce(const std::vector<double> &test_vec);
double omp_reduce(const std::vector<double> &test_vec);

void reduce_timing();

#endif // BP_REDUCE_TESTS_H

