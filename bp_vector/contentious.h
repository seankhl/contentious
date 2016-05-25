#ifndef CONT_CONTENTIOUS_H
#define CONT_CONTENTIOUS_H

#include <cassert>

#include <iostream>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

template <typename T>
class splt_vector;
template <typename T>
class cont_vector;

namespace contentious
{
static constexpr uint16_t hwconc = 2;


/* threadpool *************************************************************/

using closure = std::function<void(void)>;

class threadpool
{
public:
    threadpool()
      : tasks(hwconc), m(hwconc), cv(hwconc), spin(true)
    {
        for (int i = 0; i < hwconc; ++i) {
            threads.push_back(std::thread(&threadpool::worker_thread, this,
                                          std::ref(tasks[i]), std::ref(spin),
                                          i));
        }
    }

    ~threadpool()
    {
        for (int i = 0; i < hwconc; ++i) {
            threads[i].join();
        }
    }

    void submit(const closure &task, int i) {
        std::lock_guard<std::mutex> lk(m[i]);
        tasks[i].push(task);
        cv[i].notify_one();
    }

    void stop() {
        spin = false;
        for (int i = 0; i < hwconc; ++i) {
            std::lock_guard<std::mutex> lk(m[i]);
            cv[i].notify_one();
        }
    }

private:
    void worker_thread(std::reference_wrapper<std::queue<closure>> tasks,
                       std::reference_wrapper<bool> spin, int tid)
    {
        for (;;) {
            // Wait until main() sends data
            std::unique_lock<std::mutex> lk(m[tid]);
            cv[tid].wait(lk, [tasks, spin]{
                return (tasks.get().size() > 0) || !spin;
            });
            if (!spin) {
                assert(tasks.get().size() == 0);
                break;
            }

            tasks.get().front()();
            tasks.get().pop();

            lk.unlock();
        }
    }

private:
    std::vector<std::thread> threads;
    std::vector<std::queue<closure>> tasks;
    std::vector<std::mutex> m;
    std::vector<std::condition_variable> cv;
    bool spin;
};

extern contentious::threadpool tp;


/* comparing function *****************************************************/

// from: http://stackoverflow.com/q/18039723
template <typename T, typename... U>
size_t getAddress(std::function<T(U...)> f)
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


/* index mappings *********************************************************/

constexpr inline int identity(const int i) { return i; }
template <int o>
constexpr inline int offset(const int i) { return i - o; }
template <int i>
constexpr inline int alltoone(const int) { return i; }


/* operators **************************************************************/

template <typename T>
struct op
{
    T identity;
    std::function<T(T,T)> f;
    std::function<T(T,T)> inv;
};

const op<double> plus { 0, std::plus<double>(), std::minus<double>() };
const op<double> mult { 1, std::multiplies<double>(), std::divides<double>() };


/* parallelism helpers ****************************************************/

template <typename T>
void reduce_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                 const size_t a, const size_t b)
{
    splt_vector<T> splt = cont.detach(dep);
    const auto &used = cont.tracker[&dep]._used;

    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    auto &target = *(splt._data.begin());
    auto end = used.cbegin() + b;
    for (auto it = used.cbegin() + a; it != end; ++it) {
        target = splt.op.f(target, *it);
    }

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    //std::cout << "splt took: " << splt_dur.count() << " seconds "
    //          << "for values " << a << " to " << b << "; " << std::endl;

    cont.reattach(splt, dep, 0, 1);

}

template <typename T>
void foreach_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                  const size_t a, const size_t b,
                  const T &val)
{
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    splt_vector<T> splt = cont.detach(dep);
    auto end = splt._data.begin() + b;
    for (auto it = splt._data.begin() + a; it != end; ++it) {
        (*it) = splt.op.f(*it, val);
    }
    cont.reattach(splt, dep, a, b);

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    //std::cout << "splt took: " << splt_dur.count() << " seconds "
    //          << "for values " << a << " to " << b << "; " << std::endl;
}

template <typename T>
void foreach_splt_cvec(cont_vector<T> &cont, cont_vector<T> &dep,
                       const size_t a, const size_t b,
                       const std::reference_wrapper<cont_vector<T>> &other)
{
    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    splt_vector<T> splt = cont.detach(dep);

    const auto &tracker = other.get().tracker[&dep];
    auto oit = tracker._used.cbegin() + a;

    int amap = tracker.indexmap(a);
    if (amap < 0) { ++oit; amap = 0; }
    int bmap = tracker.indexmap(b);
    if (bmap > (int64_t)splt._data.size()) { bmap = splt._data.size(); }

    auto end = splt._data.begin() + bmap;
    for (auto it = splt._data.begin() + amap; it != end; ++it, ++oit) {
        (*it) = splt.op.f(*it, *oit);
    }

    cont.reattach(splt, dep, amap, bmap);

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    //std::cout << "splt took: " << splt_dur.count() << " seconds "
    //          << "for values " << a << " to " << b << "; " << std::endl;
}

}   // end namespace contentious

#endif  // CONT_CONTENTIOUS_H

