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

#include "../folly/ProducerConsumerQueue.h"
#include "folly/LifoSem.h"

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
      : spin(true)
    {
        for (int p = 0; p < hwconc; ++p) {
            tasks.emplace_back(folly::ProducerConsumerQueue<closure>(4096));
            resns.emplace_back(folly::ProducerConsumerQueue<closure>(4096));
            threads[p] = std::thread(&threadpool::worker, this, p);
        }
    }

    ~threadpool()
    {
        for (int p = 0; p < hwconc; ++p) {
            threads[p].join();
        }
    }

    void submit(const closure &task, int p)
    {
        while (!tasks[p].write(task)) {
            continue;
        }
        sems[p].post();
    }

    void submitr(const closure &resn, int p)
    {
        while (!resns[p].write(resn)) {
            continue;
        }
        sems[p].post();
    }

    void finish()
    {
        std::unique_lock<std::mutex> lk(fin_m);
        bool done = false;
        while (!done) {
            done = fin_cv.wait_for(lk, std::chrono::milliseconds{1}, [this] {
                bool ret = true;
                for (int p = 0; p < hwconc; ++p) {
                    ret &= (tasks[p].isEmpty() && resns[p].isEmpty());
                }
                return ret;
            });
        }
    }

    void stop()
    {
        spin = false;
        for (int p = 0; p < hwconc; ++p) {
            sems[p].post();
        }
    }

private:
    void worker(int p)
    {
        closure *task;
        closure *resn;
        for (;;) {
            // wait until we have something to do
            sems[p].wait();
            // threadpool is done
            if (!spin) {
                assert(tasks[p].isEmpty());
                assert(resns[p].isEmpty());
                break;
            }
            if (tasks[p].isEmpty()) {
                // must resolve
                resn = resns[p].frontPtr();
                assert(resn);
                (*resn)();
                resns[p].popFront();
                fin_cv.notify_one();
            } else {
                // normal parallel processing
                task = tasks[p].frontPtr();
                assert(task);
                (*task)();
                tasks[p].popFront();
            }
        }
    }

private:
    std::array<std::thread, hwconc> threads;

    std::vector<folly::ProducerConsumerQueue<closure>> tasks;
    std::vector<folly::ProducerConsumerQueue<closure>> resns;

    std::atomic<bool> spin;

    std::array<folly::LifoSem, hwconc> sems;

    std::mutex fin_m;
    std::condition_variable fin_cv;

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
        //temp = fp(temp, *it);
        temp += *it;
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
        T &target = *it;
        //target = fp(target, val);
        target *= val;
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
        T &target = *it;
        //target = fp(target, *trck);
        target *= *trck;
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

    // get all the info for each part of the stencil
    std::array<int64_t, offs_sz> adom, aran, bdom, bran, os, ioffs;
    std::array<binary_fp<T>, offs_sz> fs;
    for (size_t i = 0; i < offs_sz; ++i) {
        std::tie(adom[i], aran[i]) = safe_mapping(offs[i], a, 0, cont.size());
        std::tie(bdom[i], bran[i]) = safe_mapping(offs[i], b, 0, cont.size());
        os[i] = a - offs[i](a);
        ioffs[i] = os[i] - os[0];
        fs[i] = cont.tracker[&dep].ops[i+1].f;
    }
    // we can only iterate over the narrowest valid range
    int64_t ap = *std::max_element(aran.begin(), aran.end());
    int64_t bp = *std::min_element(bran.begin(), bran.end());
    // iterate!
    auto trck = cont.tracker[&dep]._used[p].cbegin() + (ap+os[0]);
    auto end = splt._data.begin() + bp;
    for (auto it = splt._data.begin() + ap; it != end; ++it, ++trck) {
        T &target = *it;
        /*
        for (size_t i = 0; i < offs_sz; ++i) {
            target = fs[i](target, *(trck + ioffs[i]));
        }*/
        target += 0.2 * (*(trck) + -2 * *(trck+1) + *(trck+2));
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

