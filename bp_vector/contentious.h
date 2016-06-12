#ifndef CONT_CONTENTIOUS_H
#define CONT_CONTENTIOUS_H

#include <cassert>
#include <cmath>

#include <iostream>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

template <typename T>
class splt_vector;
template <typename T>
class cont_vector;

namespace contentious
{

static constexpr uint16_t hwconc = 1;
extern std::mutex plck;

constexpr std::pair<const size_t, const size_t>
partition(const uint16_t p, const size_t sz)
{
    size_t chunk_sz = std::ceil( ((double)sz)/hwconc );
    return std::make_pair(chunk_sz * (p),
                          std::min(chunk_sz * (p+1), sz));
}



/* threadpool *************************************************************/

using closure = std::function<void(void)>;

class threadpool
{
public:
    threadpool()
      : tasks(hwconc), taskm(hwconc),
        resolutions(hwconc), resm(hwconc),
        m(hwconc), cv(hwconc), spin(true),
        ntasksfin(0), nresolfin(0)
    {
        for (int p = 0; p < hwconc; ++p) {
            threads.push_back(std::thread(&threadpool::worker_thread, this,
                                          std::ref(tasks[p]),
                                          std::ref(resolutions[p]),
                                          std::ref(spin),
                                          p));
        }
    }

    ~threadpool()
    {
        for (int p = 0; p < hwconc; ++p) {
            threads[p].join();
        }
    }

    void submit(const closure &task, int p) {
        std::lock_guard<std::mutex> lk(taskm[p]);
        tasks[p].push(task);
        cv[p].notify_one();
    }

    void submitr(const closure &resolution, int p) {
        std::lock_guard<std::mutex> lk(resm[p]);
        resolutions[p].push(resolution);
        cv[p].notify_one();
    }

    void finish() {
        std::unique_lock<std::mutex> lk(mr);
        bool done = false;
        while (!done) {
            done = cvr.wait_for(lk, std::chrono::milliseconds(1), [this] {
                bool ret = true;
                for (int p = 0; p < hwconc; ++p) {
                    ret &= (tasks[p].size() == 0 &&
                            resolutions[p].size() == 0);
                }
                return ret;
            });
            for (int p = 0; p < hwconc; ++p) {
                cv[p].notify_one();
            }
        }
    }

    void stop() {
        spin = false;
        for (int p = 0; p < hwconc; ++p) {
            std::lock_guard<std::mutex> lk(m[p]);
            cv[p].notify_one();
        }
    }

private:
    void worker_thread(std::reference_wrapper<std::queue<closure>> tasks,
                       std::reference_wrapper<std::queue<closure>> resolutions,
                       std::reference_wrapper<bool> spin, int tid)
    {
        for (;;) {
            // wait until we have something to do
            std::unique_lock<std::mutex> lk(m[tid]);
            cv[tid].wait(lk, [this, tasks, resolutions, spin, tid] {
                std::lock_guard<std::mutex> tlck(taskm[tid]);
                std::lock_guard<std::mutex> rlck(resm[tid]);
                return (tasks.get().size() > 0) ||
                        resolutions.get().size() > 0 ||
                        !spin;
            });

            // threadpool is done
            if (!spin) {
                std::lock_guard<std::mutex> tlck(taskm[tid]);
                std::lock_guard<std::mutex> rlck(resm[tid]);
                assert(tasks.get().size() == 0);
                assert(resolutions.get().size() == 0);
                break;
            }
            if (tasks.get().size() == 0) {
                // must resolve
                std::lock_guard<std::mutex> rlck(resm[tid]);
                resolutions.get().front()();
                ++nresolfin;
                /*
                std::cout << "ntasksfin (r): " << ntasksfin
                          << "; nresolfin (r): " << nresolfin
                          << std::endl;
                          */
                resolutions.get().pop();
            } else {
                // normal parallel processing
                std::lock_guard<std::mutex> tlck(taskm[tid]);
                assert(tasks.get().size() != 0);
                tasks.get().front()();
                ++ntasksfin;
                /*
                std::cout << "ntasksfin (t): " << ntasksfin
                          << "; nresolfin (t): " << nresolfin
                          << std::endl;
                          */
                tasks.get().pop();
            }
            cvr.notify_one();

            lk.unlock();
        }
    }

private:
    std::vector<std::thread> threads;

    std::vector<std::queue<closure>> tasks;
    std::vector<std::mutex> taskm;

    std::vector<std::queue<closure>> resolutions;
    std::vector<std::mutex> resm;

    std::vector<std::mutex> m;
    std::vector<std::condition_variable> cv;

    bool spin;
    std::atomic<int> ntasksfin;
    std::atomic<int> nresolfin;

    std::mutex mr;
    std::condition_variable cvr;
};

extern contentious::threadpool tp;

/* index mappings *********************************************************/

constexpr inline int identity(const int i) { return i; }
template <int o>
constexpr inline int offset(const int i) { return i - o; }
template <int i>
constexpr inline int alltoone(const int) { return i; }


/* comparing function *****************************************************/

// from: http://stackoverflow.com/q/18039723
template <typename T, typename... U>
size_t getAddress(std::function<T(U...)> &f)
{
    typedef T(fn_t)(U...);
    fn_t **fn_ptr = f.template target<fn_t *>();
    return (size_t) *fn_ptr;
}
template <typename Clazz, typename Ret, typename ...Args>
size_t getMemberAddress(std::function<Ret (Clazz::*)(Args...)> &executor)
{
    typedef Ret (Clazz::*fn_t)(Args...);
    fn_t **fn_ptr = executor.template target<fn_t *>();
    if (fn_ptr != nullptr) {
        return (size_t) *fn_ptr;
    }
    return 0;
}
inline bool same_indexmap(std::function<int(int)> &a,
                          std::function<int(int)> &b)
{
    return getAddress(a) == getAddress(b);
}

// TODO: fix obvious hack
inline bool is_monotonic(const std::function<int(int)> &imap)
{
    return imap(1) > imap(0);
}


/* operators **************************************************************/

template <typename T>
using binary_fp = T (*)(T,T);

template <typename T>
struct op
{
    const T identity;
    const binary_fp<T> f;
    const binary_fp<T> inv;
};

template <typename T>
constexpr inline T plus_fp(const T a, const T b) { return a + b; }
template <typename T>
constexpr inline T minus_fp(const T a, const T b) { return a - b; }
template <typename T>
constexpr inline T multiplies_fp(const T a, const T b) { return a * b; }
template <typename T>
constexpr inline T divides_fp(const T a, const T b) { return a / b; }

template <typename T>
const op<T> plus { 0, plus_fp<T>, minus_fp<T> };
template <typename T>
const op<T> mult { 1, multiplies_fp<T>, divides_fp<T> };


/* parallelism helpers ****************************************************/

template <typename T>
void reduce_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                 const uint16_t p)
{
    size_t a, b;
    std::tie(a, b) = partition(p, cont.size());
    splt_vector<T> splt = cont.detach(dep);
    const auto &used = cont.tracker[&dep]._used;

    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    auto &target = *(splt._data.begin());
    T temp = target;
    auto end = used.cbegin() + b;
    for (auto it = used.cbegin() + a; it != end; ++it) {
        temp = splt.op.f(temp, *it);
    }
    target = temp;

    cont.reattach(splt, dep, p, 0, 1);

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    /*{
        std::lock_guard<std::mutex> lock(contentious::plck);
        std::cout << "splt took: " << splt_dur.count() << " seconds "
        << "for values " << a << " to " << b << "; " << std::endl;
    }*/

}

template <typename T>
void foreach_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                  const uint16_t p, const T &val)
{
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    size_t a, b;
    std::tie(a, b) = partition(p, cont.size());

    splt_vector<T> splt = cont.detach(dep, a, b);

    auto end = splt._data.begin() + b;
    for (auto it = splt._data.begin() + a; it != end; ++it) {
        *it = splt.op.f(*it, val);
    }

    cont.reattach(splt, dep, p, a, b);

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    /*{
        std::lock_guard<std::mutex> lock(contentious::plck);
        std::cout << "splt took: " << splt_dur.count() << " seconds "
                  << "for values " << a << " to " << b << "; " << std::endl;
    }*/
}

template <typename T>
void foreach_splt_cvec(cont_vector<T> &cont, cont_vector<T> &dep,
                       const uint16_t p,
                       const std::reference_wrapper<cont_vector<T>> &other)
{
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    size_t a, b;
    std::tie(a, b) = partition(p, cont.size());
    //if (p == 0) { ++a; }
    //if (p == hwconc-1) { --b; }
    const auto &tracker = other.get().tracker[&dep];

    int o;
    int amap = tracker.indexmap(a);// int aoff = 0;
    o = a - amap;
    int adom = a + o;
    int aran = a;
    if (adom < 0) {
        aran += (0 - adom);
        adom += (0 - adom);
        //assert(adom == (int64_t)a);
    }
    int bdom = b + o;
    int bran = b;
    if (bdom >= (int64_t)cont.size()) {
        bran -= (bdom - (int64_t)cont.size());
        bdom -= (bdom - (int64_t)cont.size());
        //assert(bdom == (int64_t)b);
    }
    assert(bran - aran == bdom - adom);

    splt_vector<T> splt = cont.detach(dep, a, b);
    other.get().refresh(dep, adom, bdom);

    auto trck = tracker._used.cbegin() + adom;
    auto end = splt._data.begin() + bran;
    for (auto it = splt._data.begin() + aran; it != end; ++it, ++trck) {
        *it = splt.op.f(*it, *trck);
    }
    //assert(trck == tracker._used.cbegin() + bdom);

    cont.reattach(splt, dep, p, a, b);

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    /*{
        std::lock_guard<std::mutex> lock(contentious::plck);
        std::cout << "splt took: " << splt_dur.count() << " seconds "
                  << "for values " << a << " to " << b << "; " << std::endl;
    }*/
}

}   // end namespace contentious

#endif  // CONT_CONTENTIOUS_H

