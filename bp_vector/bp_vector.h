#ifndef BP_VECTOR_H
#define BP_VECTOR_H

#include "util.h"
#include "bp_vector_constants.h"
#include "bp_vector_iterator.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#include <stdexcept>
#include <iostream>
#include <utility>
#include <array>
#include <string>
#include <atomic>

#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

// TODO: namespace


template <typename T, template<typename> typename TDer>
class bp_vector_base;
template <typename T>
class bp_vector;
template <typename T>
class ps_vector;
template <typename T>
class tr_vector;

template <typename T, template<typename> typename Tder>
class bp_vector_iterator;
template <typename T, template<typename> typename Tder>
class bp_vector_const_iterator;

template <typename T>
class bp_node : public boost::intrusive_ref_counter<bp_node<T>>
{
    template <typename U, template <typename> typename TDer>
    friend class bp_vector_base;
    template <typename U, template <typename> typename TDer>
    friend class bp_vector_iterator;
    template <typename U, template <typename> typename TDer>
    friend class bp_vector_const_iterator;

    friend class bp_vector<T>;
    friend class ps_vector<T>;
    friend class tr_vector<T>;

private:
    std::array<boost::intrusive_ptr<bp_node<T>>, BP_WIDTH> branches;
    std::array<T, BP_WIDTH> values;
    int32_t id;

    bp_node() = default;
    bp_node(int32_t id_in)
      : id(id_in)
    {   /* nothing to do here*/ }
    bp_node(const bp_node<T> &other, int32_t id_in)
      : branches(other.branches), values(other.values), id(id_in)
    {   /* nothing to do here*/ }
};


class bp_vector_glob
{
private:
    static std::atomic<int32_t> unique_id;

protected:
    static inline int32_t get_unique_id() { return unique_id++; }

};


template <typename T, template<typename> typename TDer>
class bp_vector_base : protected bp_vector_glob
{
    template <typename U, template <typename> typename TDerOther>
    friend class bp_vector_base;
    friend class bp_vector_iterator<T, TDer>;
    friend class bp_vector_const_iterator<T, TDer>;

protected:
    size_t sz;
    uint16_t shift;
    boost::intrusive_ptr<bp_node<T>> root;
    int32_t id;

    // protected because we cannot create instances of base type
    // TODO: remove...
    bp_vector_base()
      : sz(0), shift(0), root(new bp_node<T>(0)), id(0)
    {   /* nothing to do here */ }

    // copy constructor is protected because it would allow us to create
    // transient vecs from persistent vecs without assigning a unique id
    template <template <typename> typename TDerOther>
    bp_vector_base(const bp_vector_base<T, TDerOther> &other)
      : sz(other.sz), shift(other.shift), root(other.root), id(other.id)
    {   /* nothing to do here */ }

    /*
    template <typename T>
    bp_vector_base(const std::vector<T> &other)
    {
        for (int i = 0; i < other.size(); ++i) {
    */

    // constructor that takes arbitrary id, for making transients
    bp_vector_base(int32_t id_in)
      : sz(0), shift(0), root(new bp_node<T>(id_in)), id(id_in)
    {   /* nothing to do here */ }

    inline bool node_copy(const int32_t other_id) const
    {
        return static_cast<const TDer<T> *>(this)->node_copy_impl(other_id);
    }

    inline uint8_t calc_depth() const { return shift / BP_BITS + 1; }

    // helper for assign
    uint16_t contained_at_shift(size_t, size_t) const;

    // returns an iterator over branches of an internal node
    const std::array<boost::intrusive_ptr<bp_node<T>>, BP_WIDTH> &
    get_branch(uint8_t depth, int64_t i) const;
    std::array<boost::intrusive_ptr<bp_node<T>>, BP_WIDTH> &
    get_branch(uint8_t depth, int64_t i);

public:

    inline int32_t get_id() const       { return id; }
    inline uint8_t get_depth() const    { return calc_depth(); }

    /* element access */
    const T &at(size_t i) const;
    T &at(size_t i);
    const T &operator[](size_t i) const
    {
        const bp_node<T> *node = root.get();
        for (uint16_t s = shift; s > 0; s -= BP_BITS) {
            node = node->branches[i >> s & BP_MASK].get();
        }
        return node->values[i & BP_MASK];
    }
    inline T &operator[](size_t i)
    {
        // boilerplate implementation in terms of const version
        return const_cast<T &>(
          implicit_cast<const bp_vector_base<T, TDer> *>(this)->operator[](i));
    }
    // fast copy from a to b
    void assign(const TDer<T> &other, size_t a, size_t b);

    /* iterators */
    typedef bp_vector_iterator<T, TDer> iterator;
    typedef bp_vector_const_iterator<T, TDer> const_iterator;

    iterator begin()	{ return iterator(*this); }
    iterator end()		{ return iterator(*this) + sz; }
    const_iterator cbegin() const	{ return const_iterator(*this); }
    const_iterator cend() const     { return const_iterator(*this) + sz; }

    /* capacity */
    inline bool empty() const           { return sz == 0; }
    inline size_t size() const          { return sz; }
    // max_size
    /*
    TODO: implement
    inline void reserve(size_t new_cap)
    {
        size_t cap = capacity();
        if (new_cap > cap) {
            for (; i < new_cap; ++i) {

            }
        }
    }
    */
    inline size_t capacity() const
    {
        return std::pow(BP_WIDTH, calc_depth());
    }
    // shrink_to_fit

    /* modifiers */
    // clear
    // insert
    // emplace
    // erase
    TDer<T> push_back(const T &val) const;
    // emplace_back
    // pop_back
    // resize
    // swap
    TDer<T> set(const size_t i, const T &val) const;

    /* printers */
    inline const std::string get_name() const { return "bp_vector_base"; }
    friend std::ostream &operator<<(std::ostream &out, const TDer<T> &data)
    {
        std::string name = data.get_name();
        out << name << "{ ";
        for (size_t i = 0; i < data.size(); ++i) {
            out << data.at(i) << " ";
        }
        out << "}/" << name;
        return out;
    }
};


template <typename T>
class bp_vector : public bp_vector_base<T, bp_vector>
{
private:

public:
    bp_vector() = default;
    bp_vector(const bp_vector<T> &other) = default;

    inline bool node_copy_impl(const int32_t) const { return false; }

    void mut_set(const size_t i, const T &val);
    void mut_push_back(const T &val);
    void insert(const size_t i, const T &val);
    T remove(const size_t i);

    tr_vector<T> make_transient() const;

    //bp_vector<T> set(const size_t i, const T &val);
    //bp_vector<T> push_back(const T &val);
    //bp_vector<T> pers_insert(const size_t i, const T val);

    inline const std::string get_name() const { return "bp_vector"; }

};


template <typename T>
class ps_vector : public bp_vector_base<T, ps_vector>
{
private:

public:
    ps_vector() = default;
    ps_vector(const ps_vector<T> &other) = default;

    ps_vector(const bp_vector_base<T, ::ps_vector> &other)
      : bp_vector_base<T, ::ps_vector>(other)
    {   /* nothing to do here */ }

    ps_vector(const tr_vector<T> &other);

    inline bool node_copy_impl(const int32_t) const { return true; }

    tr_vector<T> make_transient() const;

    //ps_vector<T> set(const size_t i, const T &val);
    //ps_vector<T> push_back(const T &val);
    //ps_vector<T> pers_insert(const size_t i, const T val);

    inline const std::string get_name() const { return "ps_vector"; }

};


template <typename T>
class tr_vector : public bp_vector_base<T, tr_vector>
{
private:

public:
    tr_vector()
      : bp_vector_base<T, ::tr_vector>(this->get_unique_id())
    {   /* nothing to do here */ }
    tr_vector(const tr_vector<T> &other) = default;

    tr_vector(const bp_vector_base<T, ::tr_vector> &other)
      : bp_vector_base<T, ::tr_vector>(other)
    {   /* nothing to do here */ }

    tr_vector(const bp_vector<T> &other);
    tr_vector(const ps_vector<T> &other);

    inline bool node_copy_impl(const int32_t other_id) const
    {
        return this->id != other_id;
    }

    void mut_set(const size_t i, const T &val);
    void mut_push_back(const T &val);

    ps_vector<T> make_persistent() const;
    tr_vector<T> new_id() const;

    //tr_vector<T> set(const size_t i, const T &val);
    //tr_vector<T> push_back(const T &val);
    //tr_vector<T> pers_insert(const size_t i, const T val);

    inline const std::string get_name() const { return "tr_vector"; }

};

#include "bp_vector_base-impl.h"
#include "bp_vector-impl.h"
#include "tr_vector-impl.h"
#include "pt_vector-impl.h"

#endif  // BP_VECTOR_H

