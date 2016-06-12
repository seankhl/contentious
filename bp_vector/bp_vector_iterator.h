#ifndef BP_VECTOR_ITERATOR_H
#define BP_VECTOR_ITERATOR_H

#include "bp_vector_constants.h"
#include "bp_vector.h"

#include <boost/intrusive_ptr.hpp>

template <typename T, template<typename> typename TDer>
class bp_vector_base;
template <typename T>
class bp_node;

template <typename T, template<typename> typename TDer>
class bp_vector_iterator
{
    using iterator = bp_vector_iterator;
    using bp_node_ptr = boost::intrusive_ptr<bp_node<T>>;

private:
    bp_node<T> *root;
    const uint16_t shift;
    size_t i;
    const size_t sz;
    const int32_t id;

    // leaf that we're at (array of node at end of path)
    // pos at the leaf that we're at
    typename std::array<T, BP_WIDTH>::iterator cur;
    typename std::array<T, BP_WIDTH>::iterator end;

public:
    typedef ptrdiff_t difference_type;
    typedef T value_type;
    typedef T & reference;
    typedef T * pointer;
    typedef std::output_iterator_tag iterator_category;


    bp_vector_iterator() = delete;

    bp_vector_iterator(bp_vector_base<T, TDer> &toit)
    : shift(toit.shift), i(0), sz(toit.sz), id(toit.id)
    {
        if (id != toit.root->id) {
            toit.root = new bp_node<T>(*toit.root, id);
        }
        root = toit.root.get();
        bp_node<T> *node = root;
        for (uint16_t s = shift; s > 0; s -= BP_BITS) {
            bp_node_ptr &next = node->branches[i >> s & BP_MASK];
            if (id != next->id) {
                next = new bp_node<T>(*next, id);
            }
            node = next.get();
        }
        cur = node->values.begin();
        end = node->values.end();
    }

    bp_vector_iterator(const iterator &other)
    : root(other.root), shift(other.shift), i(other.i), sz(other.sz),
    id(other.id),
    cur(other.cur), end(other.end)
    {   /* nothing to do here */ }

    //iterator(const iterator&);
    //~iterator();

    //iterator &operator=(const iterator &other)

    inline bool operator==(const iterator &other) const
    {
        return cur == other.cur;
    }
    inline bool operator!=(const iterator &other) const
    {
        return cur != other.cur;
    }
    //bool operator<(const iterator&) const; //optional
    //bool operator>(const iterator&) const; //optional
    //bool operator<=(const iterator&) const; //optional
    //bool operator>=(const iterator&) const; //optional

    iterator &operator++()
    {
        // interior node iteration; != means fast overflow past end
        if (++cur != end) {
            return *this;
        }
        // end of full trie
        i += BP_WIDTH;
        if (i == sz) {
            return *this;
        }

        bp_node<T> *node = root;
        for (uint16_t s = shift; s > 0; s -= BP_BITS) {
            bp_node_ptr &next = node->branches[i >> s & BP_MASK];
            if (id != next->id) {
                next = new bp_node<T>(*next, id);
            }
            node = next.get();
        }
        cur = node->values.begin();
        end = node->values.end();
        return *this;
    }
    /*
       iterator operator++(int); //optional
       iterator& operator--(); //optional
       iterator operator--(int); //optional
       iterator& operator+=(size_type); //optional
       */

    iterator operator+(size_t n) const
    {
        // no need to do anything if size == 0 or if we're not incrementing
        if (sz == 0 || n == 0) {
            return *this;
        }
        auto ret = *this;
        // +ing within leaf
        if (ret.end - ret.cur > (int64_t)n) {
            ret.cur += n;
            return ret;
        }

        uint8_t plusplus = 0;
        // update i and add n to it
        ret.i += BP_WIDTH - (ret.end - ret.cur) + n;
        if (ret.i >= ret.sz) {
            ret.i = ret.sz - 1;
            plusplus = 1;
        }

        bp_node<T> *node = ret.root;
        for (uint16_t s = ret.shift; s > 0; s -= BP_BITS) {
            bp_node_ptr &next = node->branches[ret.i >> s & BP_MASK];
            if (ret.id != next->id) {
                next = new bp_node<T>(*next, ret.id);
            }
            node = next.get();
        }
        ret.cur = node->values.begin() + (ret.i & BP_MASK) + plusplus;
        ret.end = node->values.end();
        ret.i -= ret.i & BP_MASK;
        return ret;
    }

    /*
       friend iterator operator+(size_t n, const iterator &it); //optional
       iterator& operator-=(size_type); //optional
       iterator operator-(size_type) const; //optional
       difference_type operator-(iterator) const; //optional
       */
    T &operator*() const { return *cur; }
    const T *operator->() const { return cur; }
    /*
       const_reference operator[](size_type) const; //optional
       */
};

template <typename T, template<typename> typename TDer>
class bp_vector_const_iterator
{
    using const_iterator = bp_vector_const_iterator;

private:
    const bp_node<T> *root;
    const uint16_t shift;
    size_t i;
    const size_t sz;

    // leaf that we're at (array of node at end of path)
    // pos at the leaf that we're at
    typename std::array<T, BP_WIDTH>::const_iterator cur;
    typename std::array<T, BP_WIDTH>::const_iterator end;

public:
    typedef ptrdiff_t difference_type;
    typedef T value_type;
    typedef const T & reference;
    typedef const T * pointer;
    typedef std::input_iterator_tag iterator_category;

    bp_vector_const_iterator() = delete;

    bp_vector_const_iterator(const bp_vector_base<T, TDer> &toit)
    : root(toit.root.get()), shift(toit.shift), i(0), sz(toit.sz)
    {
        const bp_node<T> *node = root;
        for (uint16_t s = shift; s > 0; s -= BP_BITS) {
            node = node->branches[i >> s & BP_MASK].get();
        }
        cur = node->values.begin() + (i & BP_MASK);
        end = node->values.end();
    }

    bp_vector_const_iterator(const const_iterator &other)
    : root(other.root), shift(other.shift), i(other.i), sz(other.sz),
    cur(other.cur), end(other.end)
    {   /* nothing to do here */ }

    //const_iterator(const iterator&);
    //~const_iterator();

    //const_iterator &operator=(const const_iterator &other)

    inline bool operator==(const const_iterator &other) const
    {
        return cur == other.cur;
    }
    inline bool operator!=(const const_iterator &other) const
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
        if (++cur != end) {
            return *this;
        }
        // end of full trie
        i += BP_WIDTH;
        if (i == sz) {
            return *this;
        }

        const bp_node<T> *node = root;
        for (uint16_t s = shift; s > 0; s -= BP_BITS) {
            node = node->branches[i >> s & BP_MASK].get();
        }
        cur = node->values.begin();
        end = node->values.end();
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
        if (sz == 0 || n == 0) {
            return *this;
        }
        auto ret = *this;
        // +ing within leaf
        if (ret.end - ret.cur >= (int64_t)n) {
            ret.cur += n;
            return ret;
        }

        uint8_t plusplus = 0;
        // update i and add n to it
        ret.i += BP_WIDTH - (ret.end - ret.cur) + n;
        if (ret.i >= ret.sz) {
            ret.i = ret.sz - 1;
            plusplus = 1;
        }

        const bp_node<T> *node = ret.root;
        for (uint16_t s = ret.shift; s > 0; s -= BP_BITS) {
            node = node->branches[ret.i >> s & BP_MASK].get();
        }
        ret.cur = node->values.begin() + (ret.i & BP_MASK) + plusplus;
        ret.end = node->values.end();
        ret.i -= ret.i & BP_MASK;
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

#endif
