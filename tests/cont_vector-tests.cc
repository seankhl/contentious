
#include "cont_vector-tests.h"
#include "../bp_vector/cont_vector.h"

#include <iostream>
#include <random>


using namespace std;

/* old, but useful for testing ************************************************/

void cont_inc_reduce(cont_vector<double> &cont_ret, uint16_t p,
                     vector<double>::const_iterator a,
                     vector<double>::const_iterator b)
{
    //chrono::time_point<chrono::system_clock> splt_start, splt_end;
    //splt_start = chrono::system_clock::now();
    splt_vector<double> splt_ret = cont_ret.detach(cont_ret, p);
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
    cont_ret.freeze(next, true, contentious::identity, contentious::plus<double>);
    cont_ret.reattach(splt_ret, next, p, 0, next.size());
}
double cont_reduce_manual(const vector<double> &test_vec)
{
    cont_vector<double> cont_ret;
    cont_ret.unprotected_push_back(0);
    vector<thread> cont_threads;
    uint16_t nthreads = hwconc; // * 16;
    size_t chunk_sz = test_vec.size()/nthreads;
    for (uint16_t p = 0; p < nthreads; ++p) {
        cont_threads.push_back(
          thread(cont_inc_reduce,
                 std::ref(cont_ret), p,
                 test_vec.begin() + chunk_sz * (p),
                 test_vec.begin() + chunk_sz * (p+1)));
    }
    for (int p = 0; p < nthreads; ++p) {
        cont_threads[p].join();
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
    cont_vector<double> cont_ret(test_vec);
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
    cont_vector<double> cont_inp;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    auto cont_ret = cont_inp.reduce(contentious::plus<double>);
    contentious::tp.finish();
    cout << cont_ret[0] << endl;
}

void stdv_foreach(const vector<double> &test_vec)
{
    vector<double> inp;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        inp.push_back(test_vec[i]);
    }
    vector<double> other;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        other.push_back(i);
    }

    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    auto ret = inp;
    for (size_t i = 0; i < inp.size(); ++i) {
        ret[i] = inp[i] * 2;
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
        if (test_vec[i] * 2 != ret[i]) {
            cout << "bad resolution at ret2[" << i << "]: "
                 << test_vec[i] * 2 << " "
                 << ret[i] << endl;
        }
    }
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
    cont_vector<double> cont_inp;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    cont_vector<double> cont_other;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_other.unprotected_push_back(i+1);
    }

    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    auto cont_ret = cont_inp.foreach(contentious::mult<double>, 2);
    auto cont_ret2 = cont_ret.foreach(contentious::mult<double>, cont_other);
    contentious::tp.finish();

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    std::cout << "cont_foreach_inner took: " << splt_dur.count() << " seconds "
              << std::endl;
    
    size_t bad = 0;
    for (size_t i = 0; i < cont_inp.size(); ++i) {
        if (cont_inp[i] * 2 != cont_ret[i]) {
            /*cout << "bad resolution at cont_ret[" << i << "]: "
                 << cont_inp[i] * 2 << " " << cont_ret[i] << endl;*/
            ++bad;
        }
    }

    for (size_t i = 0; i < cont_inp.size(); ++i) {
        if (test_vec[i] * 2 * (i+1) != cont_ret2[i]) {
            ++bad;
            /*cout << "bad resolution at cont_ret2[" << i << "]: "
                 << test_vec[i] * 2 * (i+1) << " "
                 << cont_ret2[i] << endl;*/
        }
    }

    if (bad != 0) {
        std::cout << "bad resolutions for foreach: " << bad << std::endl;
    }
}

int cont_stencil(const vector<double> &test_vec)
{
    cont_vector<double> cont_inp;

    for (size_t i = 0; i < 64/*(test_vec.size()*/; ++i) {
        cont_inp.unprotected_push_back(/*test_vec[i]*/1);
    }
    //cout << "cont_inp: " << cont_inp << endl;
    auto cont_ret = cont_inp.stencil3<-1, 1>({0.5, 0.5});
    auto cont_ret2 = cont_ret->stencil3<-1, 1>({0.5, 0.5});
    auto cont_ret3 = cont_ret2->stencil3<-1, 1>({0.5, 0.5});
    auto cont_ret4 = cont_ret3->stencil3<-1, 1>({0.5, 0.5});
    auto cont_ret5 = cont_ret4->stencil3<-1, 1>({0.5, 0.5});
    auto cont_ret6 = cont_ret5->stencil3<-1, 1>({0.5, 0.5});
    auto cont_ret7 = cont_ret6->stencil3<-1, 1>({0.5, 0.5});
    auto cont_ret8 = cont_ret7->stencil3<-1, 1>({0.5, 0.5});
    auto cont_ret9 = cont_ret8->stencil3<-1, 1>({0.5, 0.5});
    //std::cout << cont_ret4 << std::endl;
    contentious::tp.finish();

    std::cout << *cont_ret << std::endl;
    std::cout << *cont_ret2 << std::endl;
    std::cout << *cont_ret3 << std::endl;
    std::cout << *cont_ret9 << std::endl;
    /*int bad = 0;
    for (size_t i = 0; i < cont_ret.size(); ++i) {
        double change = 1;
        if (i > 0) { change += cont_inp[i-1]*0.5; }
        if (i < cont_ret.size()-1) { change += cont_inp[i+1]*0.5; }
        // o the humanity
        if (std::abs(cont_inp[i] + change - (cont_ret)[i]) > 0.00000001) {
            ++bad;
            cout << "bad resolution at cont_ret[" << i << "]: "
                 << cont_inp[i] * change << " "
                 << cont_ret[i] << endl;
            return 1;
        }
    }
    if (bad != 0) {
        std::cout << "bad resolutions for stencil: " << bad << std::endl;
    }*/
    //cont_inp.resolve(cont_ret);
    //cout << "cont_ret: " << cont_ret << endl;
    return 0;
}

constexpr double ipow(double base, int exp, double result = 1)
{
    return exp < 1 ? result : \
               ipow(base*base, exp/2, (exp % 2) ? result*base : result);
}
void cont_heat()
{
    constexpr double dt = 0.00001;
    constexpr double dy = 0.0001;
    constexpr double viscosity = 2.0 * 1.0/ipow(10,4);
    constexpr double y_max = 4000;
    constexpr double t_max = 0.0001;
    constexpr double V0 = 10;

    constexpr double s = viscosity * dt/ipow(dy,2);
    constexpr int64_t r = (t_max + dt) / dt;
    constexpr int64_t c = (y_max + dy) / dy;
    std::cout << s << std::endl;

    cont_vector<double> cont_inp;
    cont_inp.unprotected_push_back(V0);
    for (int64_t i = 1; i < c; ++i) {
        cont_inp.unprotected_push_back(0.0);
    }
    /*
    cont_vector<double> *curr = &cont_inp;
    cont_vector<double> *next;
    */
    std::array<std::shared_ptr<cont_vector<double>>, r> grid;
    grid[0] = make_shared<cont_vector<double>>(cont_inp);
    for (int t = 1; t < r; ++t) {
        //std::cout << "whut" << std::endl;
        grid[t] = grid[t-1]->stencil3<-1, 0, 1>({1.0*s, -2.0*s, 1.0*s});
    }
    contentious::tp.finish();
    for (size_t i = 0; i < 32; ++i) {
        std::cout << (*grid[r-1])[i] << " ";
    }
    std::cout << std::endl;
}


/* runner *********************************************************************/

int cont_vector_runner()
{
    /*
    int64_t test_sz = pow(2,23);
    cout << "cont testing with size: " << test_sz << endl;
    if (test_sz < 0) return 1;

    random_device rnd_device;
    mt19937 mersenne_engine(rnd_device());
    uniform_real_distribution<double> dist(-32.768, 32.768);

    auto gen = std::bind(dist, mersenne_engine);
    vector<double> test_vec(test_sz);
    generate(begin(test_vec), end(test_vec), gen);
    */

    /*
    double answer_new = 0;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        answer_new += test_vec[i];
    }
    cout << answer_new << endl;
    cont_reduce(test_vec);

    chrono::time_point<chrono::system_clock> stdv_start, stdv_end;
    stdv_start = chrono::system_clock::now();
    for (size_t i = 0; i < 8; ++i) {
        stdv_foreach(test_vec);
    }
    stdv_end = chrono::system_clock::now();
    chrono::duration<double> stdv_dur = stdv_end - stdv_start;

    chrono::time_point<chrono::system_clock> cont_start, cont_end;
    cont_start = chrono::system_clock::now();
    for (size_t i = 0; i < 8; ++i) {
        cont_foreach(test_vec);
    }
    cont_end = chrono::system_clock::now();
    chrono::duration<double> cont_dur = cont_end - cont_start;

    cout << "stdv took: " << stdv_dur.count() << " seconds; " << endl;
    cout << "cont took: " << cont_dur.count() << " seconds; " << endl;
    cout << "ratio: " << cont_dur.count() / stdv_dur.count() << endl;
    for (int i = 0; i < 8; ++i) {
        cont_stencil(test_vec);
    }
    */

    for (int i = 0; i < 1; ++i) {
        cont_heat();
    }
    
    //cout << "DONE! " << test_sz << endl;

    return 0;
}

