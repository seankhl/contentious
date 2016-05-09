
#ifndef CONT_VECTOR_H
#define CONT_VECTOR_H

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <boost/thread/latch.hpp>

#include "bp_vector.h"


template <typename T>
class splt_vector;
template <typename T>
class cont_vector;


namespace contentious
{
    template <typename T>
    void reduce_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                     const size_t a, const size_t b)
    {
        std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
        splt_start = std::chrono::system_clock::now();
        
        /*
        {
            std::lock_guard<std::mutex> lock(dep.data_lock);
            std::cout << "dep in reduce_splt: " << dep << std::endl;
        }
        */
        splt_vector<T> splt = cont.detach(dep);
        /*
        {
            std::lock_guard<std::mutex> lock(dep.data_lock);
            std::cout << "cont in reduce_splt (after detach): " << cont << std::endl;
            std::cout << "splt[0] for " << splt._data.get_id()
                      << ": " << splt._data[0] << std::endl;
        }
        {
            std::cout << "splt[0] for " << splt._data.get_id()
                      << ": " << splt._data[0] << std::endl;
            std::cout << "cont is:" << cont << std::endl;
            std::cout << "dep is: " << dep << std::endl;
        }*/
        /*
        {
        std::lock_guard<std::mutex> lock(cont.data_lock);
        //std::cout << cont << std::endl;
        std::cout << "range is: " << a << " to " << b << " with size "
                  << b - a << std::endl;
        }
        */
        auto start = cont._data.begin() + (a+1);
        auto end   = cont._data.begin() + b;

        splt._data.mut_set(0, splt._data[0] + cont[a]);
        T &valref = splt._data[0];
        
        for (auto it = start; it != end; ++it) {
            // TODO: right now, reduces happen at index 0, which probably isn't
            // exactly right
            //splt.mut_comp(0, cont[i]);
            valref += *it;
            //std::cout << "*it: " << *it << std::endl;
        }
        /*
        T &valref = splt._data[0];
        for (size_t i = a+1; i < b; ++i) {
            // TODO: right now, reduces happen at index 0, which probably isn't
            // exactly right
            //splt.mut_comp(0, cont[i]);
            valref += cont[i];
        }
        */
        /*
        }
        */

        //std::cout << "one cont_inc done: " << splt_ret._data[0] << std::endl;
        cont.reattach(splt, dep, 0, 1);
        
        splt_end = std::chrono::system_clock::now();
        std::chrono::duration<double> splt_dur = splt_end - splt_start;
        std::cout << "splt took: " << splt_dur.count() << " seconds; " << std::endl;
    }

    template <typename T>
    void foreach_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                      const size_t a, const size_t b,
                      const T &val)
    {
        //std::cout << "other tracking (foreach_splt, top) " << std::endl;
        splt_vector<T> splt = cont.detach(dep);
        /* TODO: iterators, or at least all leaves at a time
        auto it_chunk_begin = splt_ret.begin() + chunk_sz * (i);
        auto it_chunk_end = splt_ret.begin() + chunk_sz * (i+1);
        for (auto it = it_chunk_begin; it != it_chunk_end; ++it) {
            splt_ret.mut_comp(*it, );
        }
        */
        for (size_t i = a; i < b; ++i) {
            splt.mut_comp(i, val);
        }
        /*
        const uint16_t sid = splt._data.get_id();
        std::cout << std::endl << std::endl << sid << std::endl << std::endl;
        if (sid == 25) {
            std::this_thread::yield();
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(1s);
            std::this_thread::yield();
        }
        */
        cont.reattach(splt, dep, a, b);
    }

    template <typename T>
    void foreach_splt_cvec(cont_vector<T> &cont, cont_vector<T> &dep,
                           const size_t a, const size_t b,
                           const std::reference_wrapper<const cont_vector<T>> &other)
    {
        splt_vector<T> splt = cont.detach(dep);

        // TODO: see other foreach
        /*
        for (size_t i = a; i < b; ++i) {
            splt.mut_comp(i, other.get()[i]);
        }
        */
        auto start = other.get()._data.begin() + a;
        auto end = other.get()._data.begin() + b;
        size_t i = a;
        for (auto it = start; it != end; ++it) {
            splt.mut_comp(i++, *it);
        }

        cont.reattach(splt, dep, a, b);
    }

    template <typename T>
    void foreach_splt_off(cont_vector<T> &cont, cont_vector<T> &dep,
                          const size_t a, const size_t b,
                          const std::reference_wrapper<const cont_vector<T>> &other, 
                          const int &off)
    {
        size_t start = 0;
        size_t end = cont.size();
        if (off < 0) { start -= off; }
        else if (off > 0) { assert(end >= (size_t)off); end -= off; }
        start = std::max(start, a);
        end = std::min(end, b);

        splt_vector<T> splt = cont.detach(dep);
        // TODO: see other foreach
        for (size_t i = start; i < end; ++i) {
            splt.mut_comp(i, other.get()[i + off]);
        }
        cont.reattach(splt, dep, a, b);
    }

}   // end namespace contentious


template <typename T>
class Operator
{
public:
    T identity;
    Operator(T identity_in)
      : identity(identity_in) {}
    virtual T f(const T &lhs, const T &rhs) const = 0;
    virtual T inv(const T &lhs, const T &rhs) const = 0;
    virtual ~Operator() {}
};

template <typename T>
class Plus : public Operator<T>
{
public:
    Plus()
      : Operator<T>(0) {}
    inline T f(const T &lhs, const T &rhs) const
    {
        return lhs + rhs;
    }
    inline T inv(const T &lhs, const T &rhs) const
    {
        return lhs - rhs;
    }
    virtual ~Plus() {}
};

template <typename T>
class Multiply : public Operator<T>
{
public:
    Multiply()
      : Operator<T>(1) {}
    inline T f(const T &lhs, const T &rhs) const
    {
        return lhs * rhs;
    }
    inline T inv(const T &lhs, const T &rhs) const
    {
        return lhs / rhs;
    }
    virtual ~Multiply() {}
};


template <typename T>
class splt_vector
{
    friend void contentious::reduce_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t);

    friend void contentious::foreach_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t, const T &);

    friend void contentious::foreach_splt_cvec<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t,
                    const std::reference_wrapper<const cont_vector<T>> &);

    friend void contentious::foreach_splt_off<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t,
                    const std::reference_wrapper<const cont_vector<T>> &,
                    const int &);

    friend class cont_vector<T>;

private:
    tr_vector<T> _data;
    const Operator<T> *op;

public:
    splt_vector() = delete;

    splt_vector(const cont_vector<T> &trunk)
      : _data(trunk._used.new_id()), op(trunk.op)
    {   /* nothing to do here */ }

    inline splt_vector<T> comp(const size_t i, const T &val)
    {
        _data.mut_set(i, op->f(_data.at(i), val));
        return *this;
    }

    inline void mut_comp(const size_t i, const T &val)
    {
        _data.mut_set(i, op->f(_data[i], val));
    }

};

template <typename T>
class cont_vector
{
    friend void contentious::reduce_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t);

    friend void contentious::foreach_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t, const T &);

    friend void contentious::foreach_splt_cvec<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t,
                    const std::reference_wrapper<const cont_vector<T>> &);

    friend void contentious::foreach_splt_off<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t,
                    const std::reference_wrapper<const cont_vector<T>> &,
                    const int &);

    friend class splt_vector<T>;

private:
    tr_vector<T> _data;
    tr_vector<T> _used;

    std::set<uint16_t> splinters;
    std::map<uint16_t, bool> resolved;

    std::mutex data_lock;
    boost::latch resolve_latch;

    std::vector<cont_vector<T> *> dependents;

    volatile bool unsplintered = true;
    volatile size_t resolved_size_shouldbe = std::thread::hardware_concurrency();

    // TODO: op in cont? stencil? both? what's the deal
    //       requires determining if a single cont can have multiple ops
    //       (other than stencils, which as for now have exactly 2)
    const Operator<T> *op;

    // TODO: timestamps for reading and writing?
    // if read <= write:
    //     no dependents
    // if read > write:
    //     register dependents
    //std::atomic<uint16_t> ts_r;
    //std::atomic<uint16_t> ts_w;

    // user can tick the read counter (obsolete)
    //void tick_r() { ++tick_r; }

    // tids to lookahead if prescient (obsolete)
    //std::map<std::thread::id, bool> prescient;

    template <typename... U>
    void exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                         const size_t, const size_t, const U &...),
                  cont_vector<T> &dep, const U &... args);

public:
    cont_vector() = delete;
    cont_vector(const Operator<T> *_op)
      : resolve_latch(0), op(_op)
    {   /* nothing to do here */ }

    cont_vector(const cont_vector<T> &other)
      : _data(other._data.new_id()), resolve_latch(0), op(other.op)
    {   /* nothing to do here */
        std::cout << "cvec with id " << _data.get_id()
                  << " copied from cvec with id " << other._data.get_id()
                  << std::endl;
        assert(dependents.size() == 0);
    }
    cont_vector(cont_vector<T> &&other)
      : _data(std::move(other._data)), 
        _used(std::move(other._used)),
        splinters(std::move(other.splinters)),
        resolve_latch(0),
        dependents(std::move(other.dependents)),
        unsplintered(std::move(other.unsplintered)),
        resolved_size_shouldbe(std::move(other.resolved_size_shouldbe)),
        op(std::move(other.op))
    {   /* nothing to do here */
        std::cout << "cvec with id " << _data.get_id()
                  << " moved from cvec with id " << other._data.get_id()
                  << std::endl;
        {
            std::lock_guard<std::mutex> lock(other.data_lock);
            
            resolved = (std::move(other.resolved));
            
            other.resolved_size_shouldbe = 0;
            other.reset_latch(0);
            other.unsplintered = true;
        }
    }

    cont_vector(const std::vector<T> &other)
      : _data(other), resolve_latch(0), op(nullptr)
    {   /* nothing to do here */ }

    ~cont_vector()
    {
        // finish this round of resolutions to avoid segfaulting on ourselves
        std::cout << "destructor: cvec with id " << _data.get_id()
                  << " waiting on latch" << std::endl;
        resolve_latch.wait();

        // make sure I am resolved, to avoid segfaulting in the cont_vector I
        // depend on
        std::cout << "destructor: cvec with id " << _data.get_id()
                  << " has resolved size: " << resolved.size()
                  << " but should be: " << resolved_size_shouldbe << std::endl;
        if (!unsplintered) {
            while (resolved.size() != resolved_size_shouldbe) {
                std::this_thread::yield();
            }
        }
        //std::cout << "id " << _data.get_id() << "'s resolved.size(): " << resolved.size()
        //          << "should be: " << resolved_size_shouldbe << std::endl;
        bool flag = false;
        auto lastresolved = resolved;
        //std::cout << "lastresolved: " << std::endl;
        //for (const auto &it : lastresolved) {
        //    std::cout << it.first << ", " << it.second << "; ";
        //}
        //std::cout << std::endl;
        while (!flag) {
            flag = true;
            for (const auto &it : resolved) {
                std::lock_guard<std::mutex> lock(data_lock);
                flag &= it.second;
                if (lastresolved[it.first] != it.second) {
                    lastresolved[it.first] = it.second;
                    std::cout << "resolved at " << it.first
                              << "changed to " << it.second << std::endl;
                }
            }
        }

        std::cout << "id " << _data.get_id() << " finished destructor" << std::endl;

    }

    inline size_t size() const {  return _data.size(); }

    // internal passthroughs
    inline const T &operator[](size_t i) const {    return _data[i]; }
    inline T &operator[](size_t i) {                return _data[i]; }

    inline const T &at(size_t i) const {    return _data.at(i); }
    inline T &at(size_t i) {                return _data.at(i); }

    // TODO: probably need to remove this junk
    inline const T &at_prescient(size_t i) const {  return dependents[0]->at(i); }
    inline T &at_prescient(size_t i) {              return dependents[0]->at(i); }

    inline void unprotected_set(const size_t i, const T &val)
    {
        _data.mut_set(i, val);
    }
    inline void unprotected_push_back(const T &val)
    {
        _data.mut_push_back(val);
    }

    inline void reset_latch(const size_t count)
    {
        resolve_latch.reset(count);
    }
    inline void register_dependent(cont_vector<T> *dep)
    {
        {   // locked
            std::lock_guard<std::mutex> lock(data_lock);
            dependents.push_back(dep);
            dep->unsplintered = false;
            //std::cout << "registered dependent: " << dep << std::endl;
        }
    }

    splt_vector<T> detach(cont_vector &dep);
    void reattach(splt_vector<T> &splinter, cont_vector<T> &dep, size_t a, size_t b);
    void resolve(cont_vector<T> &dep);

    cont_vector<T> reduce(const Operator<T> *op);
    cont_vector<T> foreach(const Operator<T> *op, const T &val);
    cont_vector<T> foreach(const Operator<T> *op, const cont_vector<T> &other);
    cont_vector<T> stencil(const std::vector<int> &offs,
                           const std::vector<T> &coeffs,
                           const Operator<T> *op1 = new Multiply<T>,
                           const Operator<T> *op2 = new Plus<T>());

    /* obsolete
    inline cont_vector<T> pull()
    {
        resolve_latch.wait();
        return *next;
        delete other.op;
    }

    inline void sync()
    {
        reattach();
        pull();
    }
    */

    friend std::ostream &operator<<(std::ostream &out,
                                    const cont_vector<T> &cont)
    {
        out << "cont_vector{" << std::endl;
        out << "  id: " << cont._data.get_id() << std::endl;
        out << "  data: " << cont._data << std::endl;
        out << "  used: " << cont._used << std::endl;
        out << "  splt: ";
        for (const auto &i : cont.splinters) { out << i << " "; }
        out << std::endl << "}/cont_vector" << std::endl;
        return out;
    }

};

#include "cont_vector-impl.h"

#endif  // CONT_VECTOR_H

