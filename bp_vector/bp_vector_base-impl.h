
#include <iostream>
#include <string>
#include <stdexcept>

#include <cmath>
#include <cassert>


template <typename T, template<typename> typename TDer>
using bp_vector_base_ptr = boost::intrusive_ptr<bp_vector_base<T, TDer>>;
template <typename T>
using bp_node_ptr = boost::intrusive_ptr<bp_node<T>>;


template <typename T, template<typename> typename TDer>
const T &bp_vector_base<T, TDer>::at(size_t i) const
{
    if (i >= sz) {  // presumably, throw an exception... 
        throw std::out_of_range("trie has size " + std::to_string(sz) +
                                ", given index " + std::to_string(i));
    }
    return this->operator[](i);
}

template <typename T, template<typename> typename TDer>
T &bp_vector_base<T, TDer>::at(size_t i)
{
    // boilerplate implementation in terms of const version
    return const_cast<T &>(
      implicit_cast<const bp_vector_base<T, TDer> *>(this)->at(i));
}

// undefined behavior if i >= sz
template <typename T, template<typename> typename TDer>
TDer<T> bp_vector_base<T, TDer>::set(const size_t i, const T &val) const
{
    // copy trie
    TDer<T> ret(*this);
    
    // copy root node and get it in a variable
    if (node_copy(ret.root->id)) {
        ret.root = new bp_node<T>(*root);
        ret.root->id = id;
    }
    bp_node<T> *node = ret.root.get();
    for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
        bp_node_ptr<T> &next = node->branches[i >> s & br_mask];
        if (node_copy(next->id)) {
            next = new bp_node<T>(*next);
            next->id = id;
        }
        node = next.get();
    }
    node->values[i & br_mask] = val;
    return ret;
}

template <typename T, template<typename> typename TDer>
TDer<T> bp_vector_base<T, TDer>::push_back(const T &val) const
{
    // just a set; only 1/br_sz times do we even have to construct nodes
    if (this->sz % br_sz != 0) {
        TDer<T> ret(this->set(this->sz, val));
        ++(ret.sz);
        return ret;
    }
    
    TDer<T> ret(*this);
    
    // simple case for empty trie
    if (sz == 0) {
        ret.root = new bp_node<T>();
        ret.root->id = id;
        ret.root->values[ret.sz++] = val;
        return ret;
    }
    
    // we're gonna have to construct new nodes, or rotate
    
    // subvector capacity at this depth
    size_t depth_cap = capacity();
    // depth at which to insert new node
    int16_t depth_ins = -1;
    // figure out how deep we must travel to branch
    while (sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= br_sz;
    }
    
    if (node_copy(ret.root->id)) {
        ret.root = new bp_node<T>(*root);
        ret.root->id = id;
    }

    // must rotate trie, as it's totally full (new root, depth_ins is -1)
    if (depth_ins == -1) {
        // update appropriate values
        ret.shift += BITPART_SZ;
        // rotate trie
        boost::intrusive_ptr<bp_node<T>> temp = new bp_node<T>();
        temp->id = id;
        ret.root.swap(temp);
        ret.root->branches[0] = std::move(temp);
    } 

    // travel to branch of trie where new node will be constructed (if any)
    bp_node<T> *node = ret.root.get();
    int16_t s = ret.shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = node->branches[sz >> s & br_mask];
        if (!next) {
            std::cout << "Constructing node where one should have already been" 
                      << std::endl;
            next = new bp_node<T>();
            next->id = id;
        }
        if (node_copy(next->id)) {
            next = new bp_node<T>(*next);
            next->id = id;
        }
        node = next.get();
        s -= BITPART_SZ;
        --depth_ins;
    }
    // we're either at the top, the bottom or somewhere in-between...
    assert(s <= ret.shift && s >= 0);
    
    // keep going, but this time, construct new nodes as necessary
    while (s > BITPART_SZ) {
        bp_node_ptr<T> &next = node->branches[sz >> s & br_mask];
        next = new bp_node<T>();
        next->id = id;
        node = next.get();
        s -= BITPART_SZ;
    }
    if (s > 0) {
        bp_node_ptr<T> &next = node->branches[sz >> s & br_mask];
        next = new bp_node<T>();
        next->id = id;
        node = next.get();
        s -= BITPART_SZ;
    }

    // add value
    node->values[sz & br_mask] = val;
    ++ret.sz;
    return ret;
}

