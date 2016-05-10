
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

// locks in:
//   * move constructor (other)
//   * destructor (*this)
//   * freeze (*this)
//   * detach (*this, dep)
//   * reattach (*this, dep)
//   * resolve (dep)
//
//   _data/_used
//   splinters
//   resolved

namespace contentious
{
    template <typename T>
    void reduce_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                     const size_t a, const size_t b)
    {
        //std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
        //splt_start = std::chrono::system_clock::now();

        splt_vector<T> splt = cont.detach(dep);

        auto start = cont._data.begin() + (a+1);
        auto end   = cont._data.begin() + b;

        // once we mutate once, the vector is ours, and we can do unsafe writes
        splt._data.mut_set(0, splt._data[0] + cont[a]);
        T &valref = splt._data[0];
        for (auto it = start; it != end; ++it) {
            valref += *it;
        }

        cont.reattach(splt, dep, 0, 1);

        //splt_end = std::chrono::system_clock::now();
        //std::chrono::duration<double> splt_dur = splt_end - splt_start;
        //std::cout << "splt took: " << splt_dur.count() << " seconds; " << std::endl;
    }

    template <typename T>
    void foreach_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                      const size_t a, const size_t b,
                      const T &val)
    {
        splt_vector<T> splt = cont.detach(dep);

        // TODO: iterators, or at least all leaves at a time
        for (size_t i = a; i < b; ++i) {
            splt.mut_comp(i, val);
        }

        cont.reattach(splt, dep, a, b);
    }

    template <typename T>
    void foreach_splt_cvec(
                cont_vector<T> &cont, cont_vector<T> &dep,
                const size_t a, const size_t b,
                const std::reference_wrapper<const cont_vector<T>> &other)
    {
        splt_vector<T> splt = cont.detach(dep);

        auto start = other.get()._data.begin() + a;
        auto end   = other.get()._data.begin() + b;
        size_t i = a;
        for (auto it = start; it != end; ++it) {
            splt.mut_comp(i++, *it);
        }

        cont.reattach(splt, dep, a, b);
    }

    template <typename T>
    void foreach_splt_off(
                cont_vector<T> &cont, cont_vector<T> &dep,
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
      : Operator<T>(0)
    {   /* nothing to do here */ }
    inline T f(const T &lhs, const T &rhs) const
    {   return lhs + rhs; }
    inline T inv(const T &lhs, const T &rhs) const
    {   return lhs - rhs; }
    virtual ~Plus()
    {   /* nothing to do here */ }
};

template <typename T>
class Multiply : public Operator<T>
{
public:
    Multiply()
      : Operator<T>(1)
    {   /* nothing to do here */ }
    inline T f(const T &lhs, const T &rhs) const
    {   return lhs * rhs; }
    inline T inv(const T &lhs, const T &rhs) const
    {   return lhs / rhs; }
    virtual ~Multiply()
    {   /* nothing to do here */ }
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
        _data = _data.set(i, op->f(_data.at(i), val));
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

    uint16_t hwconc = std::thread::hardware_concurrency();

    // TODO: op in cont? stencil? both? what's the deal
    //       requires determining if a single cont can have multiple ops
    //       (other than stencils, which as for now have exactly 2)
    const Operator<T> *op;

    template <typename... U>
    void exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                         const size_t, const size_t, const U &...),
                  cont_vector<T> &dep, const U &... args);

public:
    cont_vector()
      : resolve_latch(0), op(nullptr)
    {   /* nothing to do here */ }
    cont_vector(const Operator<T> *_op)
      : resolve_latch(0), op(_op)
    {   /* nothing to do here */ }

    cont_vector(const cont_vector<T> &other)
      : _data(other._data.new_id()), resolve_latch(0), op(other.op)
    {   /* nothing to do here */ }
    cont_vector<T> &operator=(cont_vector<T> other)
    {
        _data = other._data.new_id();
        resolve_latch.reset(0);
        std::swap(op, other.op);
        return *this;
    }

    /*
    cont_vector(cont_vector<T> &&other)
      : _data(std::move(other._data)),
        _used(std::move(other._used)),
        splinters(std::move(other.splinters)),
        resolve_latch(0),
        dependents(std::move(other.dependents)),
        unsplintered(std::move(other.unsplintered)),
        op(std::move(other.op))
    {   // locked (TODO: this is really a terrible idea)
        std::lock_guard<std::mutex> lock(other.data_lock);

        resolved = (std::move(other.resolved));
        other.unsplintered = true;
        std::cout << "RPOBLESM" << std::endl;
    }
    */

    /*
    cont_vector(const std::vector<T> &other)
      : _data(other), resolve_latch(0), op(nullptr)
    {   // nothing to do here
    }
    */

    ~cont_vector()
    {
        // finish this round of resolutions to avoid segfaulting on ourselves
        resolve_latch.wait();
        // make sure I am resolved, to avoid segfaulting in the cont_vector I
        // depend on
        // TODO: BAD SPINLOCK! BAD!!!
        if (!unsplintered) {
            while (resolved.size() != hwconc) {
                std::this_thread::yield();
            }
        }
        bool flag = false;
        while (!flag) {
            std::this_thread::yield();
            flag = true;
            for (const auto &it : resolved) {
                std::lock_guard<std::mutex> lock(data_lock);
                flag &= it.second;
            }
        }
        // should be okay now
    }

    inline size_t size() const  {  return _data.size(); }

    // internal passthroughs
    inline const T &operator[](size_t i) const  { return _data[i]; }
    inline T &operator[](size_t i)              { return _data[i]; }

    inline const T &at(size_t i) const  { return _data.at(i); }
    inline T &at(size_t i)              { return _data.at(i); }

    inline void unprotected_set(const size_t i, const T &val)
    {
        _data.mut_set(i, val);
    }
    inline void unprotected_push_back(const T &val)
    {
        _data.mut_push_back(val);
    }

    void freeze(cont_vector<T> &dep, bool nocopy = false);
    splt_vector<T> detach(cont_vector &dep);
    void reattach(splt_vector<T> &splinter, cont_vector<T> &dep,
                  size_t a, size_t b);
    void resolve(cont_vector<T> &dep);

    cont_vector<T> reduce(const Operator<T> *op);
    cont_vector<T> foreach(const Operator<T> *op, const T &val);
    cont_vector<T> foreach(const Operator<T> *op, const cont_vector<T> &other);
    cont_vector<T> stencil(const std::vector<int> &offs,
                           const std::vector<T> &coeffs,
                           const Operator<T> *op1 = new Multiply<T>,
                           const Operator<T> *op2 = new Plus<T>());

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

