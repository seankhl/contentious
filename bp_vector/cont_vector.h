
#ifndef CONT_VECTOR_H
#define CONT_VECTOR_H

#include "bp_vector.h"
#include "contentious.h"

#include <cmath>

#include <iostream>
#include <memory>
#include <functional>
#include <algorithm>
#include <iterator>
#include <utility>
#include <tuple>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <thread>
#include <mutex>

#include <boost/thread/latch.hpp>
#include <boost/bind.hpp>

template <typename T>
class splt_vector;
template <typename T>
class cont_vector;

using contentious::hwconc;

// locks in:
//   * move constructor (other)
//   * destructor (*this)
//   * freeze (*this)
//   * detach (*this, dep)
//   * reattach (*this, dep)
//   * resolve (dep)
//
//   _data/_used
//   resolved
//
// TODO
//   * multiple dependents (one->many and many->one)
//   * correctly resolve many->one value deps (non identity index deps)
//   * efficient mutable iterators for tr_vector
//   * lifetimes
//   * efficient/correct dependent vector shapes (reduce should be just single
//     val; not all deps are necessarily the same shape as their originators
//   * cheap attachment/resolution for identity index deps


template <typename T>
class splt_vector
{
    friend void contentious::reduce_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const uint16_t);

    friend void contentious::foreach_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const uint16_t, const T &);

    friend void contentious::foreach_splt_cvec<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const uint16_t,
                    const std::reference_wrapper<cont_vector<T>> &);

    template <typename U, int... Offs>
    friend void contentious::stencil_splt(
                    cont_vector<U> &, cont_vector<U> &,
                    const uint16_t);

    friend class cont_vector<T>;

private:
    tr_vector<T> _data;
    std::vector<contentious::op<T>> ops;

public:
    splt_vector() = delete;

    splt_vector(const tr_vector<T> &_used,
                std::vector<contentious::op<T>> &_ops)
      : _data(_used.new_id()), ops(_ops)
    {   /* nothing to do here */ }

    inline splt_vector<T> comp(const size_t i, const T &val)
    {
        _data = _data.set(i, ops[0].f(_data.at(i), val));
        return *this;
    }

    inline void mut_comp(const size_t i, const T &val)
    {
        _data.mut_set(i, ops[0].f(_data[i], val));
    }

};

template <typename T>
class cont_vector
{
    friend void contentious::reduce_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const uint16_t);

    friend void contentious::foreach_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const uint16_t, const T &);

    friend void contentious::foreach_splt_cvec<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const uint16_t,
                    const std::reference_wrapper<cont_vector<T>> &);

    template <typename U, int... Offs>
    friend void contentious::stencil_splt(
                    cont_vector<U> &, cont_vector<U> &,
                    const uint16_t);

    friend class splt_vector<T>;

private:
    struct dependency_tracker
    {
        // TODO: remove this evilness... maps need default constructors
        dependency_tracker()
          : ops{contentious::plus<T>},
            imaps{contentious::identity}, iconflicts(1)
        {   /* nothing to do here! */ }

        dependency_tracker(const std::function<int(int)> imap_in,
                           const contentious::op<T> op_in)
          : ops{op_in},
            imaps{imap_in}, iconflicts(1)
        {   /* nothing to do here! */ }

        std::array<tr_vector<T>, hwconc> _used;
        
        std::vector<contentious::op<T>> ops;
        std::vector<std::function<int(int)>> imaps;
        std::vector<std::set<int64_t>> iconflicts;

        std::vector<cont_vector<T> *> frozen;
    };

    tr_vector<T> _data;
    std::mutex dlck;

    // forward tracking : dep_ptr -> tracker
    std::unordered_map<const cont_vector<T> *, dependency_tracker> tracker;
    // resolving onto (backward) :  uid -> latch
    std::unordered_map<const cont_vector<T> *, std::unique_ptr<boost::latch>> rlatches;
    // splinter tracking : sid -> reattached?
    std::unordered_map<int32_t, bool> reattached;
    std::set<int64_t> rconflicts;
    std::mutex rlck;

    bool abandoned = false;
    bool resolved = false;

    template <typename... U>
    void exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                         const uint16_t, const U &...),
                  cont_vector<T> &dep, const U &... args);

public:
    cont_vector()
    {   /* nothing to do here */ }
    cont_vector(const cont_vector<T> &other)
      : _data(other._data.new_id())
    {   /* nothing to do here */ 
        //std::cout << "COPY CVEC" << std::endl;
    }
    cont_vector<T> &operator=(cont_vector<T> other)
    {
        _data = other._data.new_id();
        //std::cout << "assign CVEC" << std::endl;
        return *this;
    }

    //cont_vector(cont_vector<T> &&other) = delete;
    /*
      : _data(std::move(other._data))
    {   // locked (TODO: this is really a terrible idea)
        std::cout << "RPOBLESM" << std::endl;
    }*/

    /*
    cont_vector(const std::vector<T> &other)
      : _data(other),
    {   // nothing to do here
    }
    */

    ~cont_vector()
    {
        // finish this round of resolutions to avoid segfaulting on ourselves
        for (auto &rl : rlatches) {
            rl.second->wait();
        }
        /*
        // make sure splinters are reattached, to avoid segfaulting in the
        // cont_vector I depend on
        // TODO: BAD SPINLOCK! BAD!!!
        if (!unsplintered) {
            while (reattached.size() != hwconc) {
                std::this_thread::yield();
            }
        }
        bool flag = false;
        while (!flag) {
            std::this_thread::yield();
            flag = true;
            for (const auto &it : reattached) {
                std::lock_guard<std::mutex> lock(data_lock);
                flag &= it.second;
            }
        }
        */
        // should be okay now
    }

    inline size_t size() const  { return _data.size(); }

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

    void freeze(cont_vector<T> &dep,
                bool onto,
                std::function<int(int)> imap,
                contentious::op<T> op,
                uint16_t ndetached = hwconc);
    void freeze(cont_vector<T> &cont, cont_vector<T> &dep,
                std::function<int(int)> imap, contentious::op<T> op);
    splt_vector<T> detach(cont_vector &dep, uint16_t p);
    void refresh(cont_vector &dep, uint16_t p, size_t a, size_t b);

    void reattach(splt_vector<T> &splt, cont_vector<T> &dep,
                  uint16_t p, size_t a, size_t b);

    void resolve(cont_vector<T> &dep);
    void resolve_monotonic(cont_vector<T> &dep);
    bool abandon() {
        resolved = false;
        abandoned = true;
        return false;
    }

    cont_vector<T> reduce(const contentious::op<T> op);
    cont_vector<T> foreach(const contentious::op<T> op, const T &val);
    cont_vector<T> foreach(const contentious::op<T> op, cont_vector<T> &other);

    template <int... Offs>
    cont_vector<T> stencil(const std::vector<T> &coeffs,
                           const contentious::op<T> op1 = contentious::mult<T>,
                           const contentious::op<T> op2 = contentious::plus<T>);
    template <int... Offs>
    cont_vector<T> *stencil2(const std::vector<T> &coeffs,
                           const contentious::op<T> op1 = contentious::mult<T>,
                           const contentious::op<T> op2 = contentious::plus<T>);
    template <int... Offs>
    std::shared_ptr<cont_vector<T>> stencil3(const std::vector<T> &coeffs,
                           const contentious::op<T> op1 = contentious::mult<T>,
                           const contentious::op<T> op2 = contentious::plus<T>);

    friend std::ostream &operator<<(std::ostream &out,
                                    const cont_vector<T> &cont)
    {
        out << "cont_vector{" << std::endl;
        out << "  id: " << cont._data.get_id() << std::endl;
        out << "  data: " << cont._data << std::endl;
        out << std::endl << "}/cont_vector" << std::endl;
        return out;
    }

};

#include "cont_vector-impl.h"

#endif  // CONT_VECTOR_H

