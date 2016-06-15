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

template <typename T, template<typename> typename TDer>
using bp_vector_base_ptr = boost::intrusive_ptr<bp_vector_base<T, TDer>>;
template <typename T>
using bp_node_ptr = boost::intrusive_ptr<bp_node<T>>;

enum class bp_node_t : uint8_t
{
    branches,
    leaves,
    uninitialized
};

template <typename T>
class bp_node : public boost::intrusive_ref_counter<bp_node<T>>
{
    using bp_branches = std::array<bp_node_ptr<T>, BP_WIDTH>;
    using bp_leaves = std::array<T, BP_WIDTH>;

public:
    ~bp_node()
    {
        if (_u_type == bp_node_t::branches) {
            as_branches().~bp_branches();
        } else if (_u_type == bp_node_t::leaves) {
            as_leaves().~bp_leaves();
        }
    }

private:
    bp_node() = delete;

    bp_node(bp_node_t _u_type_in)
      : _u_type(_u_type_in)
    {
        if (_u_type == bp_node_t::branches) {
            new (&_u) bp_branches();
        } else if (_u_type == bp_node_t::leaves) {
            new (&_u) bp_leaves();
        }
    }

    bp_node(bp_node_t _u_type_in, int32_t id_in)
      : _u_type(_u_type_in), id(id_in)
    {
        if (_u_type == bp_node_t::branches) {
            new (&_u) bp_branches();
        } else if (_u_type == bp_node_t::leaves) {
            new (&_u) bp_leaves();
        }
    }

    bp_node(const bp_node<T> &other, int32_t id_in)
      : _u_type(other._u_type), id(id_in)
    {
        if (_u_type == bp_node_t::branches) {
            new (&_u) bp_branches(*reinterpret_cast<const bp_branches *>(
                                    &(other._u)));
        } else if (_u_type == bp_node_t::leaves) {
            new (&_u) bp_leaves(*reinterpret_cast<const bp_leaves *>(
                                    &(other._u)));
        }
    }

    bp_node<T> &operator=(const bp_node<T> &other) = delete;

    inline const bp_branches &as_branches() const
    {
        return *reinterpret_cast<const bp_branches *>(&_u);
    }
    inline bp_branches &as_branches()
    {
        return *reinterpret_cast<bp_branches *>(&_u);
    }
    inline const bp_leaves &as_leaves() const
    {
        return *reinterpret_cast<const bp_leaves *>(&_u);
    }
    inline bp_leaves &as_leaves()
    {
        return *reinterpret_cast<bp_leaves *>(&_u);
    }

    std::aligned_union_t<BP_WIDTH, bp_branches, bp_leaves> _u;
    //std::array<bp_node_ptr<T>, BP_WIDTH> branches;
    //std::array<T, BP_WIDTH> values;
    bp_node_t _u_type;
    int32_t id;

    template <typename U, template <typename> typename TDer>
    friend class bp_vector_base;
    template <typename U, template <typename> typename TDer>
    friend class bp_vector_iterator;
    template <typename U, template <typename> typename TDer>
    friend class bp_vector_const_iterator;

    friend class bp_vector<T>;
    friend class ps_vector<T>;
    friend class tr_vector<T>;

};


class bp_vector_glob
{
protected:
    static inline int32_t get_unique_id() { return unique_id++; }

private:
    static std::atomic<int32_t> unique_id;

};


template <typename T, template<typename> typename TDer>
class bp_vector_base : protected bp_vector_glob
{
public:
    /* element access */
    const T &at(size_t i) const;
    T &at(size_t i);
    const inline T &operator[](size_t i) const
    {
        const bp_node<T> *node = root.get();
        for (uint16_t s = shift; s > 0; s -= BP_BITS) {
            node = node->as_branches()[i >> s & BP_MASK].get();
        }
        return node->as_leaves()[i & BP_MASK];
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

    inline iterator begin() { return iterator(*this); }
    inline iterator end()	{ return iterator(*this) + sz; }
    inline const_iterator cbegin() const { return const_iterator(*this); }
    inline const_iterator cend() const   { return const_iterator(*this) + sz; }

    /* capacity */
    inline bool empty() const   { return sz == 0; }
    inline size_t size() const  { return sz; }
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

    /* not members of std::vector */
    inline int32_t get_id() const       { return id; }
    inline uint8_t get_depth() const    { return calc_depth(); }

    /* printers */
    friend std::ostream &operator<<(std::ostream &out, const TDer<T> &data)
    {
        std::string name = data.get_name();
        out << name << "{" << data.id << "}{";
        for (size_t i = 0; i < data.size(); ++i) {
            out << data.at(i) << " ";
        }
        out << "}/" << name;
        return out;
    }

protected:
    // protected because we cannot create instances of base type
    bp_vector_base()
      : sz(0), shift(0), root(new bp_node<T>(bp_node_t::leaves, 0)), id(0)
    {   /* nothing to do here */ }

    // constructor that takes arbitrary id, for making transients
    bp_vector_base(int32_t id_in)
      : sz(0), shift(0), root(new bp_node<T>(bp_node_t::leaves, id_in)), id(id_in)
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

    inline bool node_copy(const int32_t other_id) const
    {
        return static_cast<const TDer<T> *>(this)->node_copy_impl(other_id);
    }

    inline uint8_t calc_depth() const { return shift / BP_BITS + 1; }

    // helper for assign
    uint16_t contained_at_shift(size_t, size_t) const;

    // returns an iterator over branches of an internal node
    const std::array<bp_node_ptr<T>, BP_WIDTH> &get_branch(
                                                uint8_t depth, int64_t i) const;
    std::array<bp_node_ptr<T>, BP_WIDTH> &get_branch(
                                                uint8_t depth, int64_t i);

    size_t sz;
    uint16_t shift;
    bp_node_ptr<T> root;
    int32_t id;

    template <typename U, template <typename> typename TDerOther>
    friend class bp_vector_base;
    friend class bp_vector_iterator<T, TDer>;
    friend class bp_vector_const_iterator<T, TDer>;

};


template <typename T>
class bp_vector : public bp_vector_base<T, bp_vector>
{
public:
    bp_vector() = default;
    bp_vector(const bp_vector<T> &other) = default;

    inline bool node_copy_impl(const int32_t) const { return false; }

    void mut_set(const size_t i, const T &val);
    void mut_push_back(const T &val);
    void insert(const size_t i, const T &val);
    T remove(const size_t i);

    tr_vector<T> make_transient() const
    {
        tr_vector<T> ret(*this);
        return ret;
    }

    //bp_vector<T> set(const size_t i, const T &val);
    //bp_vector<T> push_back(const T &val);
    //bp_vector<T> pers_insert(const size_t i, const T val);

    inline const std::string get_name() const { return "bp_vector"; }

};


template <typename T>
class ps_vector : public bp_vector_base<T, ps_vector>
{
public:
    ps_vector() = default;
    ps_vector(const ps_vector<T> &other) = default;

    ps_vector(const tr_vector<T> &other)
      : bp_vector_base<T, ::ps_vector>(
              static_cast<const bp_vector_base<T, tr_vector> &>(other))
    {
        /* I admit, this code is truly disturbing */
        this->id = 0;
    }

    ps_vector(const bp_vector_base<T, ::ps_vector> &other)
      : bp_vector_base<T, ::ps_vector>(other)
    {   /* nothing to do here */ }

    inline bool node_copy_impl(const int32_t) const { return true; }

    tr_vector<T> make_transient() const
    {
        tr_vector<T> ret(*this);
        return ret;
    }

    //ps_vector<T> set(const size_t i, const T &val);
    //ps_vector<T> push_back(const T &val);
    //ps_vector<T> pers_insert(const size_t i, const T val);

    inline const std::string get_name() const { return "ps_vector"; }

};


template <typename T>
class tr_vector : public bp_vector_base<T, tr_vector>
{
public:
    tr_vector()
      : bp_vector_base<T, ::tr_vector>(this->get_unique_id())
    {   /* nothing to do here */ }

    tr_vector(const tr_vector<T> &other) = default;

    tr_vector(const bp_vector_base<T, ::tr_vector> &other)
      : bp_vector_base<T, ::tr_vector>(other)
    {   /* nothing to do here */ }

    tr_vector(const bp_vector<T> &other)
      : bp_vector_base<T, ::tr_vector>(
              static_cast<const bp_vector_base<T, bp_vector> &>(other))
    {
        /* I admit, this code is truly disturbing */
        this->id = this->get_unique_id();
    }
    tr_vector(const ps_vector<T> &other)
      : bp_vector_base<T, ::tr_vector>(
              static_cast<const bp_vector_base<T, ps_vector> &>(other))
    {
        /* I admit, this code is truly disturbing */
        this->id = this->get_unique_id();
    }

    inline bool node_copy_impl(const int32_t other_id) const
    {
        return this->id != other_id;
    }

    void mut_set(const size_t i, const T &val);
    void mut_push_back(const T &val);

    ps_vector<T> make_persistent() const
    {
        return *this;
    }
    tr_vector<T> new_id() const
    {
        tr_vector<T> ret(*this);
        ret.id = this->get_unique_id();
        return ret;
    }

    //tr_vector<T> set(const size_t i, const T &val);
    //tr_vector<T> push_back(const T &val);
    //tr_vector<T> pers_insert(const size_t i, const T val);

    inline const std::string get_name() const { return "tr_vector"; }

};

#include "bp_vector_base-impl.h"
#include "bp_vector-impl.h"
#include "tr_vector-impl.h"

#endif  // BP_VECTOR_H
