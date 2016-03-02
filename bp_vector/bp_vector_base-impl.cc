
#include "bp_vector.h"
#include "util.h"

#include <iostream>
#include <string>
#include <stdexcept>

#include <cmath>
#include <cassert>

std::atomic<int16_t> bp_vector_glob::unique_id{1};

template <typename T, template<typename> typename TDer>
using bp_vector_base_ptr = std::shared_ptr<bp_vector_base<T, TDer>>;
template <typename T>
using bp_node_ptr = std::shared_ptr<bp_node<T>>;


template <typename T, template<typename> typename TDer>
uint8_t bp_vector_base<T, TDer>::calc_depth() const
{
    return shift / BITPART_SZ + 1;
}

template <typename T, template<typename> typename TDer>
bool bp_vector_base<T, TDer>::empty() const
{
    return sz == 0;
}    

template <typename T, template<typename> typename TDer>
size_t bp_vector_base<T, TDer>::size() const
{
    return sz;
}    

template <typename T, template<typename> typename TDer>
uint8_t bp_vector_base<T, TDer>::get_depth() const
{
    return calc_depth();
}

template <typename T, template<typename> typename TDer>
size_t bp_vector_base<T, TDer>::capacity() const
{
    if (sz == 0) {
        return 0;
    }
    return pow(br_sz, calc_depth());
}

template <typename T, template<typename> typename TDer>
int16_t bp_vector_base<T, TDer>::get_id() const
{
    return id;
}

template <typename T, template<typename> typename TDer>
const T &bp_vector_base<T, TDer>::operator[](size_t i) const
{
    const bp_node<T> *node = root.get();
    for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
        //std::cout << "s: " << s << "; ind: " << (i >> s & br_mask) << std::endl;
        node = node->br[i >> s & br_mask].get();
    }
    //std::cout << "s: " << 0 << "; ind: " << (i & br_mask) << std::endl;
    return node->val[i & br_mask];
}

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
T &bp_vector_base<T, TDer>::operator[](size_t i)
{
    // boilerplate implementation in terms of const version
    return const_cast<T &>(
      implicit_cast<const bp_vector_base<T, TDer> *>(this)->operator[](i));
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
TDer<T> bp_vector_base<T, TDer>::set(const size_t i, const T &val)
{
    // copy trie
    TDer<T> ret;
    ret.sz = this->sz;
    ret.shift = this->shift;
    if (this->root == nullptr) {
        std::cout << "AAH" << this->sz << std::endl;
    }
    ret.root = this->root;
    ret.id = this->id;
    //ret = *this;
    // copy root node and get it in a variable
    if (node_copy(ret.root->id)) {
        ret.root = std::make_shared<bp_node<T>>(*root);
        ret.root->id = id;
    }
    bp_node<T> *node = ret.root.get();
    for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
        //std::cout << "s: " << s << "; ind: " << (i >> s & br_mask) << "; addr: " << node << std::endl;
        bp_node_ptr<T> &next = node->br[i >> s & br_mask];
        if (node_copy(next->id)) {
            next = std::make_shared<bp_node<T>>(*next);
            next->id = id;
        }
        node = next.get();
    }
    //std::cout << "s: " << 0 << "; ind: " << (i & br_mask) << "; addr: " << node << std::endl;
    node->val[i & br_mask] = val;
    return ret;
}

template <typename T, template<typename> typename TDer>
TDer<T> bp_vector_base<T, TDer>::push_back(const T &val)
{
    TDer<T> ret;
    ret.sz = this->sz;
    ret.shift = this->shift;
    ret.root = this->root;
    ret.id = this->id;
    //bp_vector_base<T> ret = *this;
    // just a set; only 1/br_sz times do we even have to construct nodes
    if (ret.sz % br_sz != 0) {
        return ret.set(ret.sz++, val);
    }
    
    // simple case for empty trie
    if (sz == 0) {
        ret.root = std::make_shared<bp_node<T>>();
        ret.root->id = id;
        ret.root->val[ret.sz++] = val;
        return ret;
    }
    
    // we're gonna have to construct new nodes, or rotate
    uint8_t depth = calc_depth();
    // subvector capacity at this depth
    size_t depth_cap = pow(br_sz, depth);
    //std::cout << "original depth cap: " << depth_cap << std::endl;
    // depth at which to insert new node
    int16_t depth_ins = -1;
    // figure out how deep we must travel to branch
    while (sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= br_sz;
        //std::cout << "sz: " << sz << "; depth_cap: " << unsigned(depth_cap)
        //          << std::endl;
    }
    
    //std::cout << "root ptr: " << ret.root.get() << std::endl;
    //auto temp = std::make_shared<bp_node<T>>();
    //std::copy(ret.root->br.begin(), ret.root->br.end(), temp->br.begin());
    //ret.root = temp;
    if (node_copy(ret.root->id)) {
        ret.root = std::make_shared<bp_node<T>>(*root);
        ret.root->id = id;
    }
    //std::cout << "root ptr: " << ret.root.get() << std::endl;

    // must rotate trie, as it's totally full (new root, depth_ins is -1)
    if (depth_ins == -1) {
        //std::cout << "rotating trie" << std::endl;
        // update appropriate values
        ret.shift += BITPART_SZ;
        ++depth;
        // rotate trie
        auto temp = std::make_shared<bp_node<T>>();
        temp->id = id;
        ret.root.swap(temp);
        ret.root->br[0] = std::move(temp);
    } 

    // travel to branch of trie where new node will be constructed (if any)
    bp_node<T> *node = ret.root.get();
    //std::cout << "ret root ptr: " << ret.root->br[0].get() << std::endl;
    int16_t s = ret.shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = node->br[sz >> s & br_mask];
        if (!next) {
            std::cout << "Constructing node where one should have already been" 
                      << std::endl;
            next = std::make_shared<bp_node<T>>();
            next->id = id;
        }
        //std::cout << "Copy-chain for pers-vec: depth_ins = " << depth_ins 
        //          << std::endl;
        if (node_copy(next->id)) {
            next = std::make_shared<bp_node<T>>(*next);
            next->id = id;
        }
        node = node->br[sz >> s & br_mask].get();
        s -= BITPART_SZ;
        --depth_ins;
    }
    // we're either at the top, the bottom or somewhere in-between...
    //std::cout << "s: " << s << std::endl;
    assert(s <= ret.shift && s >= 0);
    
    // keep going, but this time, construct new nodes as necessary
    while (s > 0) {
        //std::cout << "s: " << s << "; ind: " << (sz >> s & br_mask)
        //          << "; addr: " << node << std::endl;
        bp_node_ptr<T> &next = node->br[sz >> s & br_mask];
        next = std::make_shared<bp_node<T>>();
        next->id = id;
        node = next.get();
        s -= BITPART_SZ;
    }
    // add value
    //std::cout << "s: " << s << "; ind: " << (sz & br_mask)
    //          << "; addr: " << node << std::endl;
    node->val[sz & br_mask] = val;
    ++ret.sz;
    return ret;
}

    // get node
    // if node is full
    //   get split version of node and insert new val into them,
    //    returning the new root that points to both new ones
    // else
    //   get copy with new val inserted
    // construct the rest of the pointer structure for the trie
    // construct the new root bp_vector with updated sz
    // return the ptr
 
