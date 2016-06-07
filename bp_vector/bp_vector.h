#ifndef BP_VECTOR_H
#define BP_VECTOR_H

#include "util.h"

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


constexpr uint8_t BITPART_SZ = 6;
// TODO: make these all caps
constexpr uint16_t br_sz = 1 << BITPART_SZ;
constexpr uint16_t br_mask = br_sz - 1;


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

    inline uint8_t calc_depth() const { return shift / BITPART_SZ + 1; }

public:
    // size-related getters
    inline bool empty() const           { return sz == 0; }
    inline size_t size() const          { return sz; }
    inline size_t capacity() const
    {
        return std::pow(br_sz, calc_depth());
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

    using bp_branch_iterator =
        typename std::array<boost::intrusive_ptr<bp_node<T>>, br_sz>::iterator;

    constexpr inline int
    round_to(int i, int m) const
    {
        assert(m && ((m & (m-1)) == 0));
        return (i + m-1) & ~(m-1);
    }

    inline uint16_t 
    contained_at_shift(size_t a, size_t b) const
    {
        assert(a < b);
        bp_node<T> *parent = this->root.get();
        bp_node<T> *node_a = this->root.get();
        bp_node<T> *node_b = this->root.get();
        uint16_t s;
        for (s = this->shift; s > 0; s -= BITPART_SZ) {
            if (node_a != node_b) {
                break;
            }
            parent = node_a;
            node_a = parent->branches[a >> s & br_mask].get();
            node_b = parent->branches[b >> s & br_mask].get();
        }
        return s;
    }

    inline std::pair<uint16_t, uint16_t>
    contained_by(size_t a, size_t b) const
    {
        assert(a < b);
        bp_node<T> *parent = this->root.get();
        bp_node<T> *node_a = this->root.get();
        bp_node<T> *node_b = this->root.get();
        uint16_t s;
        for (s = this->shift; s > 0; s -= BITPART_SZ) {
            if (node_a != node_b) {
                break;
            }
            parent = node_a;
            node_a = parent->branches[a >> s & br_mask].get();
            node_b = parent->branches[b >> s & br_mask].get();
        }
        return std::make_pair(a >> shift & br_mask,
                              b >> shift & br_mask);
    }
    
    inline size_t 
    first_diff_id(TDer<T> other, size_t a) const
    {
        assert(sz == other.sz);
        bp_node<T> *node_t = this->root.get();
        bp_node<T> *node_o = other.root.get();
        if (node_t->id == node_o->id || a >= sz) {
            return sz;
        }
        uint16_t s;
        for (s = this->shift; s > 0; s -= BITPART_SZ) {
            if (node_t->id != node_o->id) {
                std::cout << "diff ids: " << node_t->id << " " << node_o->id << std::endl;
                node_t = node_t->branches[a >> s & br_mask].get();
                node_o = node_o->branches[a >> s & br_mask].get();
            } else { 
                break;
            }
        }
        if (s == 0) {
            std::cout << "diff all the way down" << std::endl;
            return a;
        }
        else {
            uint16_t d = s / BITPART_SZ + 1;
            int64_t interval = std::pow(br_sz, d);
            int64_t ar = round_to(a+1, interval);
            std::cout << "ar: " << ar << std::endl;
            return first_diff_id(other, ar);
        }
    }

    inline bp_branch_iterator branch_iterator(uint8_t depth, int64_t i)
    {
        if (node_copy(root->id)) {
            root = new bp_node<T>(*root, id);
        }
        bp_node<T> *node = root.get();
        uint16_t s;
        for (s = this->shift; s > 0; s -= BITPART_SZ) {
            if (--depth == 0) {
                break;
            }
            auto &next = node->branches[i >> s & br_mask];
            if (node_copy(next->id)) {
                next = new bp_node<T>(*next, id);
            }
            node = next.get();
        }
        assert(depth == 0);
        return node->branches.begin() + (i >> s & br_mask);
    }

    // copy from other to *this from at to at+sz; vectors must have same size
    void copy(TDer<T> other, int64_t a, int64_t b)
    {
        assert(a < b);
        // if we're smaller than a branch, branch-copying won't work
        if (b - a < br_sz) {
            auto ot = other.cbegin() + a;
            auto end = this->begin() + b;
            for (auto it = this->begin() + a; it != end; ++it, ++ot) {
                *it = *ot;
            }
            return;
        }

        uint16_t s = contained_at_shift(a, b);
        uint16_t d = s / BITPART_SZ + 1;
        int64_t interval = std::pow(br_sz, d);

        int64_t ar = round_to(a, interval);
        int64_t br = round_to(b, interval);
        if (b != br) {
            br -= interval;
        }
        
        /*std::cout << a << " " << ar << " " << b << " " << br
                  << " d: " << (uint16_t)calc_depth() << " " << d << std::endl;*/

        // 1. copy individual vals for partial leaf we originate in, if n b
        // 2. travel upwards, copying branches at shallowest depth possible
        if (ar - a > 0) {
            auto it_step1 = other.cbegin() + a;
            auto end1 = this->begin() + ar;
            for (auto it = this->begin() + a; it != end1; ++it, ++it_step1) {
                *it = *it_step1;
            }
        }
        
        // 3. copy shallow branches until we're in the final val's branch
        auto it_step2 = other.branch_iterator(calc_depth() - d, ar);
        auto end2 = branch_iterator(calc_depth() - d, br);
        for (auto it = branch_iterator(calc_depth() - d, ar);
             it != end2;
             ++it, ++it_step2) {
            //std::cout << "swapped " << *it << " and " << *it_step2 << std::endl;
            *it = new bp_node<T>(*(it_step2->get()), other.id);
            //std::cout << "new id: " << other.id << std::endl;
            //*it = *it_step2;
        }
        
        // 4. travel downwards, copying branches at [...]
        // 5. copy individual vals for partial leaf we terminate in, if n e
        if (b - br > 0) {
            auto it_step3 = other.cbegin() + br;
            auto end3 = this->begin() + b;
            for (auto it = this->begin() + br; it != end3; ++it, ++it_step3) {
                *it = *it_step3;
            }
        }
    }

    inline int32_t get_id() const       { return id; }
    inline uint8_t get_depth() const    { return calc_depth(); }

    // value-related getters
    const T &operator[](size_t i) const
    {
        const bp_node<T> *node = root.get();
        for (uint16_t s = shift; s > 0; s -= BITPART_SZ) {
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

    class iterator {
        using bp_node_ptr = boost::intrusive_ptr<bp_node<T>>;

    private:
        bp_node<T> *root;
        const uint16_t shift;
        size_t i;
        const size_t sz;
        const int32_t id;

        // leaf that we're at (array of node at end of path)
        // pos at the leaf that we're at
        typename std::array<T, br_sz>::iterator cur;
        typename std::array<T, br_sz>::iterator end;

    public:
        /*
        typedef typename A::difference_type difference_type;
        typedef typename A::value_type value_type;
        typedef typename A::reference const_reference;
        typedef typename A::pointer const_pointer;
        typedef std::random_access_iterator_tag iterator_category; //or another tag
        */

        iterator() = delete;

        iterator(bp_vector_base<T, TDer> &toit)
          : shift(toit.shift), i(0), sz(toit.sz), id(toit.id)
        {
            if (id != toit.root->id) {
                toit.root = new bp_node<T>(*toit.root, id);
            }
            root = toit.root.get();
            bp_node<T> *node = root;
            for (uint16_t s = shift; s > 0; s -= BITPART_SZ) {
                bp_node_ptr &next = node->branches[i >> s & br_mask];
                if (id != next->id) {
                    next = new bp_node<T>(*next, id);
                }
                node = node->branches[i >> s & br_mask].get();
            }
            cur = node->values.begin();
            end = node->values.begin() + (br_sz-1);
        }

        iterator(const iterator &other)
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
            if (cur != end) {
                ++cur;
                return *this;
            }
            // end of full trie
            i += br_sz;
            if (i == sz) {
                ++cur;
                return *this;
            }

            bp_node<T> *node = root;
            for (uint16_t s = shift; s > 0; s -= BITPART_SZ) {
                bp_node_ptr &next = node->branches[i >> s & br_mask];
                if (id != next->id) {
                    next = new bp_node<T>(*next, id);
                }
                node = node->branches[i >> s & br_mask].get();
            }
            cur = node->values.begin();
            end = node->values.begin() + (br_sz-1);
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
            if (ret.end - ret.cur >= (int64_t)n) {
                ret.cur += n;
                return ret;
            }

            int plusplus = 0;
            // update i and add n to it
            ret.i += br_sz - (ret.end - ret.cur + 1) + n;
            if (ret.i >= ret.sz) {
                ret.i = ret.sz - 1;
                plusplus = 1;
            }

            bp_node<T> *node = ret.root;
            for (uint16_t s = ret.shift; s > 0; s -= BITPART_SZ) {
                bp_node_ptr &next = node->branches[ret.i >> s & br_mask];
                if (ret.id != next->id) {
                    next = new bp_node<T>(*next, ret.id);
                }
                node = node->branches[ret.i >> s & br_mask].get();
            }
            ret.cur = node->values.begin() + (ret.i & br_mask) + plusplus;
            ret.end = node->values.begin() + (br_sz-1);
            ret.i -= ret.i & br_mask;
            return ret;
        }

        /*
        friend iterator operator+(size_t n, const iterator &it); //optional
        iterator& operator-=(size_type); //optional
        iterator operator-(size_type) const; //optional
        difference_type operator-(iterator) const; //optional
        */
        T &operator*() const
        {
            return *cur;
        }
        const T *operator->() const { return cur; }
        /*
        const_reference operator[](size_type) const; //optional
        */
    };

    class const_iterator {
    private:
        const bp_node<T> *root;
        const uint16_t shift;
        size_t i;
        const size_t sz;

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
          : root(toit.root.get()), shift(toit.shift), i(0), sz(toit.sz)
        {
            const bp_node<T> *node = root;
            for (uint16_t s = shift; s > 0; s -= BITPART_SZ) {
                node = node->branches[i >> s & br_mask].get();
            }
            cur = node->values.begin() + (i & br_mask);
            end = node->values.begin() + (br_sz-1);
        }

        const_iterator(const const_iterator &other)
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
            if (cur != end) {
                ++cur;
                return *this;
            }
            // end of full trie
            i += br_sz;
            if (i == sz) {
                ++cur;
                return *this;
            }

            const bp_node<T> *node = root;
            for (uint16_t s = shift; s > 0; s -= BITPART_SZ) {
                node = node->branches[i >> s & br_mask].get();
            }
            cur = node->values.begin() + (i & br_mask);
            end = node->values.begin() + (br_sz-1);
            i -= i & br_mask;
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

            int plusplus = 0;
            // update i and add n to it
            ret.i += br_sz - (ret.end - ret.cur + 1) + n;
            if (ret.i >= ret.sz) {
                ret.i = ret.sz - 1;
                plusplus = 1;
            }

            const bp_node<T> *node = ret.root;
            for (uint16_t s = ret.shift; s > 0; s -= BITPART_SZ) {
                node = node->branches[ret.i >> s & br_mask].get();
            }
            ret.cur = node->values.begin() + (ret.i & br_mask) + plusplus;
            ret.end = node->values.begin() + (br_sz-1);
            ret.i -= ret.i & br_mask;
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

    iterator begin()          { return iterator(*this); }
    iterator end()            { return iterator(*this) + sz; }
    const_iterator cbegin() const   { return const_iterator(*this); }
    const_iterator cend() const     { return const_iterator(*this) + sz; }

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

