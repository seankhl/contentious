
#include "reduce-tests.h"

using namespace std;

double seq_reduce(int64_t test_sz)
{
    double seq_ret(0);
    for (int64_t i = 0; i < test_sz; ++i) {
        seq_ret += 1;
    }
    return seq_ret;
}

void locked_inc(double &locked_ret, int64_t n, std::mutex &ltm)
{
    for (int64_t i = 0; i < n; ++i) {
        std::lock_guard<std::mutex> lock(ltm);
        locked_ret += 1;
    }
}
double locked_reduce(int64_t test_sz)
{
    double locked_ret(0);
    std::mutex ltm;
    vector<thread> locked_threads;
    int num_threads = thread::hardware_concurrency();
    for (int i = 0; i < num_threads; ++i) {
        locked_threads.push_back(
          thread(locked_inc, 
                 std::ref(locked_ret), 
                 test_sz/num_threads, 
                 std::ref(ltm)));
    }
    for (int i = 0; i < num_threads; ++i) {
        locked_threads[i].join();
    }
    return locked_ret;
}

void atomic_inc(atomic<double> &atomic_ret, int64_t n)
{
    for (int64_t i = 0; i < n; ++i) {
        auto cur = atomic_ret.load();
        while (!atomic_ret.compare_exchange_weak(cur, cur + 1.0)) { 
            ;
        }
    }
}
double atomic_reduce(int64_t test_sz)
{
    atomic<double> atomic_ret(0);
    vector<thread> atomic_threads;
    int num_threads = thread::hardware_concurrency();
    for (int i = 0; i < num_threads; ++i) {
        atomic_threads.push_back(
          thread(atomic_inc, std::ref(atomic_ret), test_sz/num_threads));
    }
    for (int i = 0; i < num_threads; ++i) {
        atomic_threads[i].join();
    }
    return atomic_ret;
}

double async_inc(int64_t n)
{
	//chrono::time_point<chrono::system_clock> async_piece_start, async_piece_end;
	//async_piece_start = chrono::system_clock::now();
    double async_ret = 0;
    for (int64_t i = 0; i < n; ++i) {
        async_ret += 1;
    }
	//async_piece_end = chrono::system_clock::now();
    //chrono::duration<double> async_piece_dur = async_piece_end - async_piece_start;
    //cout << "async took: " << async_piece_dur.count() << " seconds; " << endl;
    return async_ret;
}
double async_reduce(int64_t test_sz)
{
    double async_ret(0);
    vector<future<double>> async_pieces;
    int16_t num_threads = thread::hardware_concurrency();
    for (int i = 0; i < num_threads; ++i) {
        async_pieces.push_back(
                async(launch::async, &async_inc, test_sz/num_threads));
    }
    for (int i = 0; i < num_threads; ++i) {
        async_ret += async_pieces[i].get();
    }
    return async_ret;
}

double avx_reduce(int64_t test_sz)
{
    double avx_ret(0);
    double res[4];
    __m256d vals = _mm256_set_pd(0, 0, 0, 0);
    __m256d temps;
    for (int64_t i = 0; i < test_sz; i += 4) {
        temps = _mm256_set_pd(1, 1, 1, 1);
        vals = _mm256_add_pd(vals, temps);
    }
    __m256d hsum = _mm256_hadd_pd(vals, vals);
    __m256d perm = _mm256_permute2f128_pd(hsum, hsum, 0x21);
    __m256d mret = _mm256_add_pd(hsum, perm);
    __m128d temp = _mm256_extractf128_pd(mret, 0);
    _mm_storel_pd(&avx_ret, temp);
    return avx_ret;
}

double omp_reduce(int64_t test_sz)
{
    double omp_ret(0);
#pragma omp parallel for        \
  reduction(+:omp_ret)  
/*
  default(shared) private(i)    \
  schedule(static, chunk)       \
*/
    for (int64_t i = 0; i < test_sz; ++i) {
        omp_ret += 1;
    }
    return omp_ret;
}

void reduce_timing()
{
    int64_t test_sz = std::numeric_limits<int64_t>::max() / pow(2,34);


	chrono::time_point<chrono::system_clock> seq_start, seq_end;
	seq_start = chrono::system_clock::now();
    double seq_test = seq_reduce(test_sz);
	seq_end = chrono::system_clock::now();
	
    /*
    chrono::time_point<chrono::system_clock> locked_start, locked_end;
	locked_start = chrono::system_clock::now();
    double locked_test = locked_reduce(test_sz);
	locked_end = chrono::system_clock::now();
	
    chrono::time_point<chrono::system_clock> atomic_start, atomic_end;
	atomic_start = chrono::system_clock::now();
    double atomic_test = atomic_reduce(test_sz);
	atomic_end = chrono::system_clock::now();
    */

    chrono::time_point<chrono::system_clock> async_start, async_end;
	async_start = chrono::system_clock::now();
    double async_test = async_reduce(test_sz);
	async_end = chrono::system_clock::now();
    
	chrono::time_point<chrono::system_clock> avx_start, avx_end;
	avx_start = chrono::system_clock::now();
    double avx_test = avx_reduce(test_sz);
	avx_end = chrono::system_clock::now();
	
	chrono::time_point<chrono::system_clock> omp_start, omp_end;
	omp_start = chrono::system_clock::now();
    double omp_test = omp_reduce(test_sz);
	omp_end = chrono::system_clock::now();
    
    /*
    if (seq_test != locked_test) {
        cout << "error: seq_test is " << seq_test
             << " and locked_test is " << locked_test << endl;
    } else if (seq_test != atomic_test) {
        cout << "error: seq_test is " << seq_test
             << " and atomic_test is " << atomic_test << endl;
    } else */if (seq_test != async_test) {
        cout << "error: seq_test is " << seq_test
             << " and async_test is " << async_test << endl;
    } else if (seq_test != avx_test) {
        cout << "error: seq_test is " << seq_test
             << " and avx_test is " << avx_test << endl;
    } else if (seq_test != omp_test) {
        cout << "error: seq_test is " << seq_test
             << " and omp_test is " << omp_test << endl;
    }
    
    std::cout << "seq_test: " << seq_test << endl;
    /*std::cout << "locked_test: " << locked_test << endl;
    std::cout << "atomic_test: " << atomic_test << endl;*/
    std::cout << "async_test: " << async_test << endl;
    std::cout << "avx_test: " << avx_test << endl;
    std::cout << "omp_test: " << omp_test << endl;
	
    chrono::duration<double> seq_dur = seq_end - seq_start;
	/*chrono::duration<double> locked_dur = locked_end - locked_start;
	chrono::duration<double> atomic_dur = atomic_end - atomic_start;*/
	chrono::duration<double> async_dur = async_end - async_start;
    chrono::duration<double> avx_dur = avx_end - avx_start;
	chrono::duration<double> omp_dur = omp_end - omp_start;
    
    cout << "seq took: " << seq_dur.count() << " seconds; " << endl;
    /*cout << "locked took: " << locked_dur.count() << " seconds; " << endl;
    cout << "atomic took: " << atomic_dur.count() << " seconds; " << endl;*/
    cout << "async took: " << async_dur.count() << " seconds; " << endl;
    cout << "avx took: " << avx_dur.count() << " seconds; " << endl;
    cout << "omp took: " << omp_dur.count() << " seconds; " << endl;
}

