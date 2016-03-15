#ifndef BP_VECTOR_H
#define BP_VECTOR_H

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <atomic>
#include <array>
#include <cmath>
#include <iostream>

#include <boost/variant.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include "util.h"

// TODO: namespace


constexpr uint8_t BITPART_SZ = 6;
// TODO: make these all caps
constexpr size_t br_sz = 1 << BITPART_SZ;
constexpr uint8_t br_mask = br_sz - 1;

template <typename T, template<typename> typename TDer>
class bp_vector_base;
template <typename T>
class bp_vector;
template <typename T>
class ps_vector;
template <typename T>
class tr_vector;

template <typename T>
class bp_node : public boost::intrusive_ref_counter<bp_node<T>>
{
	template <typename U, template <typename> typename TDer>
    friend class bp_vector_base;
    friend class bp_vector<T>;
	friend class ps_vector<T>;
	friend class tr_vector<T>;

private:
    std::array<boost::intrusive_ptr<bp_node<T>>, br_sz> branches;
    std::array<T, br_sz> values;
    int32_t id;

};


class bp_vector_glob
{
protected:
    static std::atomic<int16_t> unique_id;

};


template <typename T, template<typename> typename TDer>
class bp_vector_base : bp_vector_glob
{
    template <typename U, template <typename> typename TDerOther>
    friend class bp_vector_base;

protected:
    size_t sz;
    uint8_t shift;
    boost::intrusive_ptr<bp_node<T>> root;
    int16_t id;
   
    // protected because we cannot create instances of base type
    bp_vector_base()
      : sz(0), shift(0), root(nullptr), id(0)
    {   /* nothing to do here */ }
    
    // copy constructor is protected because it would allow us to create
    // transient vecs from persistent vecs without assigning a unique id
    template <template <typename> typename TDerOther>
    bp_vector_base(const bp_vector_base<T, TDerOther> &other)
      : sz(other.sz), shift(other.shift), root(other.root), id(other.id)
    {   /* nothing to do here */ }
    
    // constructor that takes arbitrary id, for making transients
    bp_vector_base(int16_t id_in) 
      : sz(0), shift(0), root(nullptr), id(id_in) 
    {   /* nothing to do here */ }
    
    inline bool node_copy(const int16_t other_id) const 
    { 
        return static_cast<const TDer<T> *>(this)->node_copy_impl(other_id);
    }

    inline uint8_t calc_depth() const { return shift / BITPART_SZ + 1; }

public:
    static inline int16_t get_unique_id() { return unique_id++; }
    
    // size-related getters
    inline bool empty() const           { return sz == 0; }
    inline size_t size() const          { return sz; }
    inline uint8_t get_depth() const    { return calc_depth(); }
    inline size_t capacity() const
    { 
        if (sz == 0) { return 0; }
        return pow(br_sz, calc_depth());
    }
    inline int16_t get_id() const       { return id; }

    // value-related getters
    const T &operator[](size_t i) const
    {
        const bp_node<T> *node = root.get();
        for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
            node = node->branches[i >> s & br_mask].get();
        }
        return node->values[i & br_mask];
    }
    inline T &operator[](size_t i)
    {
        // boilerplate implementation in terms of const version
        return const_cast<T &>(
          implicit_cast<const bp_vector_base<T, TDer> *>(this)->operator[](i));
    }
    
    const T &at(size_t i) const;
    T &at(size_t i);
    
    TDer<T> set(const size_t i, const T &val) const;
    TDer<T> push_back(const T &val) const;

};


template <typename T>
class bp_vector : public bp_vector_base<T, bp_vector>
{
private:

public:
    bp_vector() = default;
    bp_vector(const bp_vector<T> &other) = default;
    
    inline bool node_copy_impl(const int16_t) const { return false; }
    
    void mut_set(const size_t i, const T &val);
    void mut_push_back(const T &val);
    void insert(const size_t i, const T &val);
    T remove(const size_t i);
    
    tr_vector<T> make_transient() const;

    //bp_vector<T> set(const size_t i, const T &val);
    //bp_vector<T> push_back(const T &val);
    //bp_vector<T> pers_insert(const size_t i, const T val);

    // TODO: put this in base class and use get_name or something
    friend std::ostream &operator<<(std::ostream &out, const bp_vector &data)
	{
        out << "bp_vector[ ";
        for (size_t i = 0; i < data.size(); ++i) {
            out << data.at(i) << " ";
        }
        out << "]/bp_vector";
        return out;
    }

};


template <typename T>
class ps_vector : public bp_vector_base<T, ps_vector>
{
private:

public:
    ps_vector() = default; 
    ps_vector(const ps_vector<T> &other) = default;
    
    ps_vector(const bp_vector_base<T, ps_vector> &other) 
      : bp_vector_base<T, ps_vector>(other) 
    {   /* nothing to do here */ }

    ps_vector(const tr_vector<T> &other);

    inline bool node_copy_impl(const int16_t) const { return true; }
    
    tr_vector<T> make_transient() const;

    //ps_vector<T> set(const size_t i, const T &val);
    //ps_vector<T> push_back(const T &val);
    //ps_vector<T> pers_insert(const size_t i, const T val);

    friend std::ostream &operator<<(std::ostream &out, const ps_vector &data)
	{
        out << "ps_vector[ ";
        for (size_t i = 0; i < data.size(); ++i) {
            out << data.at(i) << " ";
        }
        out << "]/ps_vector";
        return out;
    }

};


template <typename T>
class tr_vector : public bp_vector_base<T, tr_vector>
{
private:

public:
    tr_vector() = default;
    tr_vector(const tr_vector<T> &other) = default;

    tr_vector(const bp_vector_base<T, tr_vector> &other) 
      : bp_vector_base<T, tr_vector>(other)
    {   /* nothing to do here */ }

    tr_vector(const bp_vector<T> &other);
    tr_vector(const ps_vector<T> &other);

    inline bool node_copy_impl(const int16_t other_id) const
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

    friend std::ostream &operator<<(std::ostream &out, const tr_vector &data)
	{
        out << "tr_vector[ ";
        for (size_t i = 0; i < data.size(); ++i) {
            out << data.at(i) << " ";
        }
        out << "]/tr_vector";
        return out;
    }

};

#include "bp_vector_base-impl.h"
#include "bp_vector-impl.h"
#include "tr_vector-impl.h"
#include "pt_vector-impl.h"

#endif  // BP_VECTOR_H

