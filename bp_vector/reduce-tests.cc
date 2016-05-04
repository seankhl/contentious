
#include "reduce-tests.h"

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


void cont_inc(cont_vector<double> &cont_ret,
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
    cont_ret.join(splt_ret);
}
double cont_reduce(const vector<double> &test_vec)
{
    cont_vector<double> cont_ret(new Plus<double>());
    cont_ret.unprotected_push_back(0);
    vector<thread> cont_threads;
    int num_threads = thread::hardware_concurrency(); // * 16;
    size_t chunk_sz = test_vec.size()/num_threads;
    for (int i = 0; i < num_threads; ++i) {
        cont_threads.push_back(
          thread(cont_inc,
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
void cont_inc(cont_vector<double> &cont_ret,
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
          thread(cont_inc,
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

void cont_reduce_new(const vector<double> &test_vec)
{
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cout << test_vec[i] << " ";
    }
    cout << endl;
    cont_vector<double> cont_inp(new Multiply<double>());
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    auto cont_ret = cont_inp.reduce(new Plus<double>());
    //auto cont_ret2 = cont_ret.foreach(new Plus<double>(), 2);
    for (size_t i = 0; i < cont_ret.size(); ++i) {
        cout << cont_ret[i] << " ";
    }
    cout << endl;
    /*
    for (size_t i = 0; i < cont_ret2.size(); ++i) {
        cout << cont_ret2[i] << " ";
    }
    cout << endl;
    */
}

void cont_foreach(const vector<double> &test_vec)
{
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cout << test_vec[i] << " ";
    }
    cout << endl;
    cont_vector<double> cont_inp(new Multiply<double>());
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    auto cont_ret = cont_inp.foreach(new Multiply<double>(), 2);

    cont_vector<double> cont_other(new Multiply<double>());
    cout << "cont_other: ";
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_other.unprotected_push_back(i);
        cout << cont_other[i] << " ";
    }
    cout << endl;
    auto cont_ret2 = cont_ret.foreach(new Plus<double>(), cont_other);
    for (size_t i = 0; i < cont_ret.size(); ++i) {
        cout << cont_ret[i] << " ";
    }
    cout << endl;
    for (size_t i = 0; i < cont_ret2.size(); ++i) {
        cout << cont_ret2[i] << " ";
    }
    cout << endl;
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

void reduce_timing()
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
    cout << cont_reduce(test_vec) << " " << cont_reduce(test_vec) - answer_new << endl;
    //cont_reduce_new(test_vec);
    //cont_foreach(test_vec);
    cont_stencil(test_vec);

    /*
    chrono::time_point<chrono::system_clock> locked_start, locked_end;
    locked_start = chrono::system_clock::now();
    double locked_test = locked_reduce(test_vec);
    locked_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> atomic_start, atomic_end;
    atomic_start = chrono::system_clock::now();
    double atomic_test = atomic_reduce(test_vec);
    atomic_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> async_start, async_end;
    async_start = chrono::system_clock::now();
    double async_test = async_reduce(test_vec);
    async_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> avx_start, avx_end;
    avx_start = chrono::system_clock::now();
    double avx_test = avx_reduce(test_vec);
    avx_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> cont_start, cont_end;
    cont_start = chrono::system_clock::now();
    double cont_test = cont_reduce(test_vec);
    cont_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> omp_start, omp_end;
    omp_start = chrono::system_clock::now();
    double omp_test = omp_reduce(test_vec);
    omp_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> seq_start, seq_end;
    seq_start = chrono::system_clock::now();
    double seq_test = seq_reduce(test_vec);
    seq_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> vec_start, vec_end;
    vec_start = chrono::system_clock::now();
    double vec_test = vec_reduce(test_vec);
    vec_end = chrono::system_clock::now();
    */

    /*
    if (seq_test != locked_test) {
        cout << "error: seq_test is " << seq_test
             << " and locked_test is " << locked_test << endl;
    } else if (seq_test != atomic_test) {
        cout << "error: seq_test is " << seq_test
             << " and atomic_test is " << atomic_test << endl;
    } else if (seq_test != async_test) {
        cout << "error: seq_test is " << seq_test
             << " and async_test is " << async_test << endl;
    } else if (seq_test != avx_test) {
        cout << "error: seq_test is " << seq_test
             << " and avx_test is " << avx_test << endl;
    } else if (seq_test != omp_test) {
        cout << "error: seq_test is " << seq_test
             << " and omp_test is " << omp_test << endl;
    } else if (seq_test != cont_test) {
        cout << "error: seq_test is " << seq_test
             << " and cont_test is " << cont_test << endl;
    }
    */

    /*
    cout << "seq_test: " << seq_test << endl;
    cout << "locked_test: " << locked_test << endl;
    cout << "atomic_test: " << atomic_test << endl;
    cout << "async_test: " << async_test << endl;
    cout << "avx_test: " << avx_test << endl;
    cout << "omp_test: " << omp_test << endl;
    cout << "vec_test: " << vec_test << endl;
    cout << "cont_test: " << cont_test << endl;
    */

    /*
    chrono::duration<double> seq_dur = seq_end - seq_start;
    chrono::duration<double> locked_dur = locked_end - locked_start;
    chrono::duration<double> atomic_dur = atomic_end - atomic_start;
    chrono::duration<double> async_dur = async_end - async_start;
    chrono::duration<double> avx_dur = avx_end - avx_start;
    chrono::duration<double> omp_dur = omp_end - omp_start;
    chrono::duration<double> vec_dur = vec_end - vec_start;
    chrono::duration<double> cont_dur = cont_end - cont_start;
    */

    /*
    cout << "seq took: " << seq_dur.count() << " seconds; " << endl;
    cout << "locked took: " << locked_dur.count() << " seconds; " << endl;
    cout << "atomic took: " << atomic_dur.count() << " seconds; " << endl;
    cout << "async took: " << async_dur.count() << " seconds; " << endl;
    cout << "avx took: " << avx_dur.count() << " seconds; " << endl;
    cout << "omp took: " << omp_dur.count() << " seconds; " << endl;
    cout << "vec took: " << vec_dur.count() << " seconds; " << endl;
    cout << "cont took: " << cont_dur.count() << " seconds; " << endl;
    */
}

