
#ifndef CONT_VECTOR_H
#define CONT_VECTOR_H

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

#include "folly/AtomicHashMap.h"

#include "bp_vector.h"
#include "contentious_constants.h"
#include "contentious.h"

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

    template <typename U, size_t NS>
    friend void contentious::stencil_splt(
                    cont_vector<U> &, cont_vector<U> &,
                    const uint16_t);

    friend class cont_vector<T>;

public://private:
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

    template <typename U, size_t NS>
    friend void contentious::stencil_splt(
                    cont_vector<U> &, cont_vector<U> &,
                    const uint16_t);

    friend class splt_vector<T>;

public:
    struct dependency_tracker
    {
        // TODO: remove this evilness... maps need default constructors
        dependency_tracker()
          : ops{contentious::plus<T>},
            imaps{contentious::identity}, icontended(1)
        {   /* nothing to do here! */ }

        dependency_tracker(const contentious::imap_fp imap_in,
                           const contentious::op<T> op_in)
          : ops{op_in},
            imaps{imap_in}, icontended(1)
        {   /* nothing to do here! */ }

        std::array<tr_vector<T>, contentious::HWCONC> _used;

        // all ops that were used from *this -> dep
        std::vector<contentious::op<T>> ops;
        // all imaps that were used from *this-> dep
        std::vector<contentious::imap_fp> imaps;
        // will hold contended indices for this imap
        std::vector<std::set<int64_t>> icontended;
        // helper dependents of dep
        std::vector<cont_vector<T> *> frozen;
    };

    tr_vector<T> _data;
    tr_vector<T> _orig;
    std::mutex dlck;

    // forward tracking : dep_ptr -> tracker
    folly::AtomicHashMap<uintptr_t, dependency_tracker> tracker;
    // resolving onto (backward) :  uid -> latch
    folly::AtomicHashMap<uintptr_t, std::unique_ptr<boost::latch>> latches;
    // splinter tracking : sid -> reattached?
    folly::AtomicHashMap<int32_t, bool> splinters;
    // indices we have to check during resolution from previous resolutions
    std::set<int64_t> contended;

    bool abandoned = false;
    bool resolved = false;

    template <typename... U>
    void exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                         const uint16_t, const U &...),
                  cont_vector<T> &dep, const U &... args);

public:
    cont_vector()
      : tracker(8), latches(8),
        splinters(contentious::HWCONC)
    {   /* nothing to do here */ }
    cont_vector(const cont_vector<T> &other)
      : _data(other._data.new_id()), tracker(8), latches(8),
        splinters(contentious::HWCONC)
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
        for (auto &l : latches) {
            l.second->wait();
        }
        /*
        // make sure splinters are reattached, to avoid segfaulting in the
        // cont_vector I depend on
        // TODO: BAD SPINLOCK! BAD!!!
        if (!unsplintered) {
            while (reattached.size() != contentious::HWCONC) {
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

    typedef bp_vector_const_iterator<T, tr_vector> const_iterator;

    inline const_iterator cbegin() const { return this->_data.cbegin(); }
    inline const_iterator cend() const   { return this->_data.cend(); }

    inline size_t size() const  { return _data.size(); }

    // internal passthroughs
    inline const T &operator[](size_t i) const  { return _data[i]; }
    //inline T &operator[](size_t i)              { return _data[i]; }

    inline const T &at(size_t i) const  { return _data.at(i); }
    //inline T &at(size_t i)              { return _data.at(i); }

    inline void unprotected_set(const size_t i, const T &val)
    {
        _data.mut_set(i, val);
    }
    inline void unprotected_push_back(const T &val)
    {
        _data.mut_push_back(val);
    }

    void freeze(cont_vector<T> &dep,
                contentious::imap_fp imap, contentious::op<T> op,
                bool onto = false);
    void freeze(cont_vector<T> &cont, cont_vector<T> &dep,
                contentious::imap_fp imap, contentious::op<T> op);
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
    std::shared_ptr<cont_vector<T>> foreach(const contentious::op<T> op,
                                            const T &val);
    std::shared_ptr<cont_vector<T>> foreach(const contentious::op<T> op,
                                            cont_vector<T> &other);

    template <int... Offs>
    cont_vector<T> stencil_oldest(const std::vector<T> &coeffs,
                           const contentious::op<T> op1 = contentious::mult<T>,
                           const contentious::op<T> op2 = contentious::plus<T>);
    template <int... Offs>
    cont_vector<T> *stencil_old(const std::vector<T> &coeffs,
                           const contentious::op<T> op1 = contentious::mult<T>,
                           const contentious::op<T> op2 = contentious::plus<T>);
    template <int... Offs>
    std::shared_ptr<cont_vector<T>> stencil(const std::vector<T> &coeffs,
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

