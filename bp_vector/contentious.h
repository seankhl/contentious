#ifndef CONT_CONTENTIOUS_H
#define CONT_CONTENTIOUS_H

#include <queue>

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
            threads.push_back(std::thread(&threadpool::worker_thread,
                                          this, std::ref(tasks[i]), std::ref(spin), i));
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
            lk.unlock();

            auto &task = tasks.get().front();
            task();
            tasks.get().pop();
            //std::cout << "only " << tasks.get().size()
            //          << " tasks left in thread " << tid << std::endl;
        }
    }

private:
    std::vector<std::thread> threads;
    std::vector<std::queue<closure>> tasks;
    std::vector<std::mutex> m;
    std::vector<std::condition_variable> cv;
    bool spin;
};


/* comparing function *****************************************************/

template <typename T, typename... U>
size_t getAddress(std::function<T(U...)> f) {
    typedef T(fnType)(U...);
    fnType ** fnPointer = f.template target<fnType*>();
    return (size_t) *fnPointer;
}

template<typename Clazz, typename Return, typename ...Arguments>
size_t getMemberAddress(std::function<Return (Clazz::*)(Arguments...)> & executor) {
    typedef Return (Clazz::*fnType)(Arguments...);
    fnType ** fnPointer = executor.template target<fnType *>();
    if (fnPointer != nullptr) {
        return (size_t) * fnPointer;
    }
    return 0;
}


/* index mappings *********************************************************/

constexpr inline int identity(const int i) { return i; }
template <int o>
constexpr inline int offset(const int i) { return i - o; }
constexpr inline int onetoall(const int) { return 0; }


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

    auto start = used.cbegin() + (a+1);
    auto end   = used.cbegin() + b;

    std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
    splt_start = std::chrono::system_clock::now();

    // once we mutate once, the vector is ours, and we can do unsafe writes
    splt.mut_comp(0, used[a]);
    T &valref = splt._data[0];
    for (auto it = start; it != end; ++it) {
        valref = splt.op.f(valref, *it);
        //for (size_t i = a; i < b; ++i) {
        //splt.mut_comp(0, *it);
        //splt.mut_comp(0, used[i]);
    }

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    std::cout << "splt took: " << splt_dur.count() << " seconds "
    << "for values " << a << " to " << b << "; " << std::endl;

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
    // TODO: iterators, or at least all leaves at a time
    /*for (size_t i = a; i < b; ++i) {
        splt.mut_comp(i, val);
    }*/
    auto end = splt._data.begin() + b;
    for (auto it = splt._data.begin() + a; it != end; ++it) {
        (*it) *= val;
    }
    cont.reattach(splt, dep, a, b);

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    std::cout << "splt took: " << splt_dur.count() << " seconds "
    << "for values " << a << " to " << b << "; " << std::endl;
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
    /*
    for (size_t i = a; i < b; ++i) {
        //std::cout << "i: " << i << ", index: " << index << std::endl;
        //if (index < 0 || index >= (int64_t)splt._data.size()) { continue; }
        splt.mut_comp(tracker.indexmap(i), *it);
        ++it;
    }
    */
    auto oit = tracker._used.cbegin() + a;
    auto it_end = splt._data.begin() + b;
    for (auto it = splt._data.begin() + a; it != it_end; ++it) {
        (*it) *= (*oit);
        ++oit;
    }

    int start = tracker.indexmap(a);
    if (start < 0) { start = 0; }
    int end = tracker.indexmap(b);
    if (end > (int64_t)splt._data.size()) { end = splt._data.size(); }

    cont.reattach(splt, dep, start, end);

    splt_end = std::chrono::system_clock::now();
    std::chrono::duration<double> splt_dur = splt_end - splt_start;
    std::cout << "splt took: " << splt_dur.count() << " seconds "
    << "for values " << a << " to " << b << "; " << std::endl;
}

template <typename T>
void foreach_splt_off(cont_vector<T> &cont, cont_vector<T> &dep,
                      const size_t a, const size_t b,
                      const std::reference_wrapper<cont_vector<T>> &other,
                      const int &off)
{
    size_t start = 0;
    size_t end = cont.size();
    if (off < 0) { start -= off; }
    else if (off > 0) { assert(end >= (size_t)off); end -= off; }
    start = std::max(start, a);
    end = std::min(end, b);

    splt_vector<T> splt = cont.detach(dep);
    for (auto & tracked: cont.tracker) {
        std::cout << "tracking" << std::endl;
    }
    for (size_t i = start; i < end; ++i) {
        splt.mut_comp(i, other.get().tracker[&dep]._used[i + off]);
    }

    cont.reattach(splt, dep, a, b);
}

}   // end namespace contentious

#endif  // CONT_CONTENTIOUS_H

