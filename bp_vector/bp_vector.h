#ifndef BP_VECTOR_H
#define BP_VECTOR_H

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <atomic>
#include <array>
#include <stack>
#include <vector>
#include <utility>
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
private:
    static std::atomic<int16_t> unique_id;

protected:
    static inline int16_t get_unique_id() { return unique_id++; }

};


template <typename T, template<typename> typename TDer>
class bp_vector_base : protected bp_vector_glob
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

    /*
    template <typename T>
    bp_vector_base(const std::vector<T> &other)
    {
        for (int i = 0; i < other.size(); ++i) {
    */


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
    // size-related getters
    inline bool empty() const           { return sz == 0; }
    inline size_t size() const          { return sz; }
    inline size_t capacity() const
    {
        if (sz == 0) { return 0; }
        return pow(br_sz, calc_depth());
    }
    inline void reserve(size_t new_cap)
    {
        size_t cap = capacity();
        /* TODO: implement
        if (new_cap > cap) {
            for (; i < new_cap; ++i) {

            }
        }
        */
    }

    inline int16_t get_id() const       { return id; }
    inline uint8_t get_depth() const    { return calc_depth(); }

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

    /**  iterator *************************************************************/

    class const_iterator {
    private:
        // depth of the tree we're iterating over
        int16_t depth;

        // path, from top to bottom, node pointers and index at that node
        //std::stack<std::pair<bp_node<T> *, size_t>> path;
        std::vector<std::pair<bp_node<T> *, size_t>> path;
        int16_t last;

        // leaf that we're at (array of node at end of path)
        // pos at the leaf that we're at
        typename std::array<T, br_sz>::const_iterator cur;
        typename std::array<T, br_sz>::const_iterator end;

    public:
        /*
        typedef typename A::difference_type difference_type;
        typedef typename A::value_type value_type;
        typedef typename A::reference const_reference;
        typedef typename A::pointer const_pointer;
        typedef std::random_access_iterator_tag iterator_category; //or another tag
        */

        const_iterator() = delete;

        const_iterator(const bp_vector_base<T, TDer> &toit)
        {
            depth = toit.calc_depth();
            path.reserve(depth);
            last = -1;
            if (toit.size() == 0) {
                --depth;
            } else {
                path.emplace_back(
                        std::make_pair(toit.root.get(), 0));
                ++last;
            }
            while (last+1 != depth) {
                path.emplace_back(
                        std::make_pair(path[last].first->branches[0].get(), 0));
                ++last;
            }
            path.resize(depth);
            if (toit.size() == 0) {
                cur = toit.root->values.end();
            } else {
                cur = path[last].first->values.begin();
                end = path[last].first->values.end();
            }
        }

        const_iterator(const const_iterator &other)
          : depth(other.depth), path(other.path), last(other.last),
            cur(other.cur), end(other.end)
        {   /* nothing to do here */ }

        //const_iterator(const const_iterator&);
        //const_iterator(const iterator&);
        //~const_iterator();

        //const_iterator &operator=(const const_iterator &other)
        bool operator==(const const_iterator &other) const
        {
            return cur == other.cur;
        }
        bool operator!=(const const_iterator &other) const
        {
            return cur != other.cur;
        }
        //bool operator<(const const_iterator&) const; //optional
        //bool operator>(const const_iterator&) const; //optional
        //bool operator<=(const const_iterator&) const; //optional
        //bool operator>=(const const_iterator&) const; //optional

        const_iterator &operator++()
        {
            // interior node iteration; != means fast overflow past end
            if (cur != end) {
                ++cur;
                return *this;
            }
            --last;
            auto pos = std::ref(path[last].second);
            // go up until we're not at the end of our node
            while (pos == br_sz - 1) {
                --last;
                if (last == -1) {
                    return *this;
                }
                pos = std::ref(path[last].second);
            }
            ++pos;
            assert(pos < br_sz);
            while (last+1 != depth &&
                   path[last].first->branches[pos] != nullptr) {
                path[last+1] = std::make_pair(
                             path[last].first->branches[pos].get(), 0);
                ++last;
                pos = std::ref(path[last].second);
            }
            cur = path[last].first->values.begin();
            end = path[last].first->values.end();
            return *this;
        }
        /*
        const_iterator operator++(int); //optional
        const_iterator& operator--(); //optional
        const_iterator operator--(int); //optional
        const_iterator& operator+=(size_type); //optional
        */

        const_iterator operator+(size_t n) const
        {
            // no need to do anything if size == 0 or if we're not incrementing
            if (depth == 0 || n == 0) {
                return *this;
            }

            auto ret = *this;
            // +ing within leaf
            if (end - cur > (int64_t)n) {
                ret.cur += n;
                return *this;
            }
            // p is the greatest jump between spots we'll make
            // 1 means jumps within node;
            // br_sz means jumps between leaves with the same parent;
            // br_sz ** 2 would mean shared grandparents
            std::vector<size_t> pos_chain;
            --ret.last;
            auto pos = std::ref(ret.path[ret.last].second);
            uint32_t p = br_sz;
            //std::cout << "ret.last before loop : " << ret.last << std::endl;
            while (pos + (n / p) >= br_sz) {
                pos_chain.push_back(pos);
                --ret.last;
                //std::cout << "ret.last in loop : " << ret.last << std::endl;
                if (ret.last == -1) {
                    ret.cur = ret.end;
                    return ret;
                }
                p *= br_sz;
                pos = std::ref(ret.path[ret.last].second);
            }
            size_t left = n;
            pos += left / p;
            size_t i = 0;
            while (ret.last+1 != ret.depth &&
                   ret.path[ret.last].first->branches[pos] != nullptr) {
                left = left % p;
                p /= br_sz;
                ret.path[ret.last+1] =
                        std::make_pair(
                             ret.path[ret.last].first->branches[pos].get(),
                             pos_chain[i++] + left / p);
                ++ret.last;
                pos = std::ref(ret.path[ret.last].second);
            }
            left = left % p;
            assert(left == 0);
            ret.cur = ret.path[ret.last].first->values.begin() + pos;
            ret.end = ret.path[ret.last].first->values.end();
            return ret;
        }

        /*
        friend const_iterator operator+(size_t n, const const_iterator &it); //optional
        const_iterator& operator-=(size_type); //optional
        const_iterator operator-(size_type) const; //optional
        difference_type operator-(const_iterator) const; //optional
        */
        const T &operator*() const  { return *cur; }
        const T *operator->() const { return cur; }
        /*
        const_reference operator[](size_type) const; //optional
        */
    };

    /**  end iterator *********************************************************/

    const_iterator begin() const    { return const_iterator(*this); }
    const_iterator cbegin() const   { return const_iterator(*this); }
    const_iterator end() const      { return const_iterator(*this) + sz; }
    const_iterator cend() const     { return const_iterator(*this) + sz; }

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

    inline bool node_copy_impl(const int16_t) const { return true; }

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

    inline const std::string get_name() const { return "tr_vector"; }

};

#include "bp_vector_base-impl.h"
#include "bp_vector-impl.h"
#include "tr_vector-impl.h"
#include "pt_vector-impl.h"

#endif  // BP_VECTOR_H

