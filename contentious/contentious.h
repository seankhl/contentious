#ifndef CONT_CONTENTIOUS_H
#define CONT_CONTENTIOUS_H

#include <cassert>
#include <cmath>

#include <iostream>
#include <functional>
#include <mutex>
#include <atomic>

#include <boost/function.hpp>

#include "../tests/slbench.h"

#include "contentious_constants.h"
#include "threadpool.h"

template <typename T>
class splt_vector;
template <typename T>
class cont_vector;

namespace contentious {

#ifdef CTTS_STATS
extern std::atomic<uint64_t> conflicted;
#endif
#ifdef CTTS_TIMING
extern std::array<slbench::stopwatch, contentious::HWCONC> splt_durs;
extern std::array<slbench::stopwatch, contentious::HWCONC> rslv_durs;
#endif
extern std::mutex plck;

static inline std::pair<const size_t, const size_t>
partition(const uint16_t p, const size_t sz)
{
    size_t chunk_sz = std::ceil( static_cast<double>(sz)/HWCONC );
    return std::make_pair(chunk_sz * (p),
                          std::min(chunk_sz * (p+1), sz));
}


/* threadpool *************************************************************/

extern threadpool tp;


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

/* from: http://stackoverflow.com/q/18039723 */
template <typename T, typename... U>
size_t getAddress(std::function<T (U...)> &f)
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
inline bool same_imap(contentious::imap_fp &a,
                      contentious::imap_fp &b)
{
    return getAddress(a) == getAddress(b);
}

// TODO: fix obvious hack
inline bool is_monotonic(const contentious::imap_fp &imap)
{
    return imap(0) < imap(1);
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
    //const auto &used = cont.tracker[&dep]._used[p];
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    const auto &used = cont.tracker.find(dkey)->second._used[p];
#ifdef CTTS_TIMING
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();
#endif
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
#ifdef CTTS_TIMING
    splt_end = std::chrono::system_clock::now();
    splt_durs[p].add(splt_start, splt_end);
#endif
}

template <typename T>
void foreach_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                  const uint16_t p, const T &val)
{
#ifdef CTTS_TIMING
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();
#endif
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
#ifdef CTTS_TIMING
    splt_end = std::chrono::system_clock::now();
    splt_durs[p].add(splt_start, splt_end);
#endif
    cont.reattach(splt, dep, p, a, b);
}

template <typename T>
void foreach_splt_cvec(cont_vector<T> &cont, cont_vector<T> &dep,
                       const uint16_t p,
                       const std::reference_wrapper<cont_vector<T>> &other)
{
#ifdef CTTS_TIMING
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();
#endif
    size_t a, b;
    std::tie(a, b) = partition(p, cont.size());
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    const auto &imaps = other.get().tracker.find(dkey)->second.imaps;

    int64_t adom, aran, bdom, bran;
    std::tie(adom, aran) = safe_mapping(imaps[0], a, 0, cont.size());
    std::tie(bdom, bran) = safe_mapping(imaps[0], b, 0, cont.size());

    splt_vector<T> splt = cont.detach(dep, p);

    const binary_fp<T> fp = splt.ops[0].f;
    auto trck = other.get()._data.cbegin() + adom;
    auto end = splt._data.begin() + bran;
    for (auto it = splt._data.begin() + aran; it != end; ++it, ++trck) {
        T &target = *it;
        //target = fp(target, *trck);
        target *= *trck;
    }
#ifdef CTTS_TIMING
    splt_end = std::chrono::system_clock::now();
    splt_durs[p].add(splt_start, splt_end);
#endif
    cont.reattach(splt, dep, p, a, b);
}

template <typename T, size_t NS>
void stencil_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                  const uint16_t p)
{
#ifdef CTTS_TIMING
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();
#endif
    size_t a, b;
    std::tie(a, b) = partition(p, cont.size());
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    const auto &tracker = cont.tracker.find(dkey)->second;

    // get all the info for each part of the stencil
    std::array<int64_t, NS> adom, aran, bdom, bran, os, ioffs;
    std::array<binary_fp<T>, NS> fs;
    const auto &imaps = tracker.imaps;
    for (size_t i = 0; i < NS; ++i) {
        std::tie(adom[i], aran[i]) = safe_mapping(imaps[i+1], a, 0, cont.size());
        std::tie(bdom[i], bran[i]) = safe_mapping(imaps[i+1], b, 0, cont.size());
        os[i] = a - imaps[i+1](a);
        ioffs[i] = os[i] - os[0];
        fs[i] = tracker.ops[i+1].f;
    }
    // we can only iterate over the narrowest valid range
    int64_t ap = *std::max_element(aran.begin(), aran.end());
    int64_t bp = *std::min_element(bran.begin(), bran.end());

    // iterate!
    splt_vector<T> splt = cont.detach(dep, p);
    auto trck = tracker._used[p].cbegin() + (ap+os[0]);
    auto end = splt._data.begin() + bp;
    for (auto it = splt._data.begin() + ap; it != end; ++it, ++trck) {
        T &target = *it;
        /*for (size_t i = 0; i < NS; ++i) {
            target = fs[i](target, *(trck + ioffs[i]));
        }*/
        target += 0.4 * (*(trck) + -2 * *(trck+1) + *(trck+2));
    }
#ifdef CTTS_TIMING
    splt_end = std::chrono::system_clock::now();
    splt_durs[p].add(splt_start, splt_end);
#endif
    cont.reattach(splt, dep, p, a, b);
}

}   // end namespace contentious

#endif  // CONT_CONTENTIOUS_H
