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

#include <boost/function.hpp>

template <typename T>
class splt_vector;
template <typename T>
class cont_vector;

namespace contentious
{

static constexpr uint16_t hwconc = 4;
extern std::mutex plck;

constexpr std::pair<const size_t, const size_t>
partition(const uint16_t p, const size_t sz)
{
    size_t chunk_sz = std::ceil( ((double)sz)/hwconc );
    return std::make_pair(chunk_sz * (p),
                          std::min(chunk_sz * (p+1), sz));
}



/* threadpool *************************************************************/

using closure = std::function<void (void)>;

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

using imap_fp = std::function<int64_t (int64_t)>;/*int64_t (*)(int64_t);*/

constexpr inline int64_t identity(const int64_t i) { return i; }
template <int64_t o>
constexpr inline int64_t offset(const int64_t i) { return i - o; }
template <int64_t i>
constexpr inline int64_t alltoone(const int64_t) { return i; }

std::pair<int64_t, int64_t> safe_mapping(const imap_fp &imap, size_t i,
                                         int64_t lo, int64_t hi);


/* comparing function *****************************************************/

// from: http://stackoverflow.com/q/18039723
template <typename T, typename... U>
size_t getAddress(std::function<T(U...)> &f)
{
    typedef T(fn_t)(U...);
    fn_t **fn_ptr = f.template target<fn_t *>();
    return (size_t)*fn_ptr;
}
template <typename Clazz, typename Ret, typename ...Args>
size_t getMemberAddress(std::function<Ret (Clazz::*)(Args...)> &executor)
{
    typedef Ret (Clazz::*fn_t)(Args...);
    fn_t **fn_ptr = executor.template target<fn_t *>();
    if (fn_ptr != nullptr) {
        return (size_t)*fn_ptr;
    }
    return 0;
}
inline bool same_imap(std::function<int(int)> &a,
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
using binary_fp = /*T (*)(T, T);*/boost::function<T (T, T)>;

template <typename T>
struct op
{
    const T identity;
    const binary_fp<T> f;
    const binary_fp<T> inv;
};

template <typename T>
constexpr inline T plus_fp(const T a, const T b)        { return a + b; }
template <typename T>
constexpr inline T minus_fp(const T a, const T b)       { return a - b; }
template <typename T>
constexpr inline T multiplies_fp(const T a, const T b)  { return a * b; }
template <typename T>
constexpr inline T divides_fp(const T a, const T b)     { return a / b; }

template <typename T>
constexpr inline T multplus_fp(const T a, const T b, const T c)
{
    return a + c*b;
}

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
    splt_vector<T> splt = cont.detach(dep, p);
    const auto &used = cont.tracker[&dep]._used[p];

    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    const binary_fp<T> fp = splt.ops[0].f;
    auto &target = *(splt._data.begin());
    T temp = target;
    auto end = used.cbegin() + b;
    for (auto it = used.cbegin() + a; it != end; ++it) {
        temp = fp(temp, *it);
        //temp += *it;
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

    splt_vector<T> splt = cont.detach(dep, p);

    const binary_fp<T> fp = splt.ops[0].f;
    auto end = splt._data.begin() + b;
    for (auto it = splt._data.begin() + a; it != end; ++it) {
        *it = fp(*it, val);
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
    const auto &tracker = other.get().tracker[&dep];

    int64_t adom, aran;
    std::tie(adom, aran) = safe_mapping(tracker.imaps[0], a, 0, cont.size());
    int64_t bdom, bran;
    std::tie(bdom, bran) = safe_mapping(tracker.imaps[0], b, 0, cont.size());

    splt_vector<T> splt = cont.detach(dep, p);

    const binary_fp<T> fp = splt.ops[0].f;
    auto trck = other.get()._data.cbegin() + adom;
    auto end = splt._data.begin() + bran;
    for (auto it = splt._data.begin() + aran; it != end; ++it, ++trck) {
        *it = fp(*it, *trck);
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

template <typename T, int... Offs>
void stencil_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                  const uint16_t p)
{
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    constexpr size_t offs_sz = sizeof...(Offs);
    std::array<std::function<int (int)>, offs_sz> offs{offset<Offs>...};

    size_t a, b;
    std::tie(a, b) = partition(p, cont.size());

    splt_vector<T> splt = cont.detach(dep, p);

    std::array<int64_t, offs_sz> adom, aran, bdom, bran, os, ioffs;
    const auto ops = cont.tracker[&dep].ops;
    std::array<binary_fp<T>, offs_sz> fs;
    const auto &used = cont.tracker[&dep]._used[p];
    for (size_t i = 0; i < offs_sz; ++i) {
        const auto op = cont.tracker[&dep].ops[i+1];
        std::tie(adom[i], aran[i]) = safe_mapping(offs[i], a, 0, cont.size());
        std::tie(bdom[i], bran[i]) = safe_mapping(offs[i], b, 0, cont.size());
        os[i] = a - offs[i](a);
        ioffs[i] = os[i] - os[0];
        fs[i] = ops[i+1].f;
    }

    int64_t ap = *std::max_element(aran.begin(), aran.end());
    int64_t bp = *std::min_element(bran.begin(), bran.end());
    auto trck = used.cbegin() + (ap+os[0]);
    auto end = splt._data.begin() + bp;
    for (auto it = splt._data.begin() + ap; it != end; ++it, ++trck) {
        T &target = *it;
        for (size_t i = 0; i < offs_sz; ++i) {
            target = fs[i](target, *(trck + ioffs[i]));
        }
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

}   // end namespace contentious

#endif  // CONT_CONTENTIOUS_H

