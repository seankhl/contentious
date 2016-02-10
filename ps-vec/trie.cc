#include "trie.h"

#include <iostream>
#include <math.h>
#include <assert.h>

template <typename T>
using PS_Trie_ptr = std::shared_ptr<PS_Trie<T>>;
template <typename T>
using PS_Node_ptr = std::shared_ptr<PS_Node<T>>;


template <typename T>
uint8_t PS_Trie<T>::calc_depth() const
{
    return shift / BITPART_SZ + 1;
}

template <typename T>
bool PS_Trie<T>::empty() const
{
    return sz == 0;
}    

template <typename T>
size_t PS_Trie<T>::size() const
{
    return sz;
}    

template <typename T>
uint8_t PS_Trie<T>::get_depth() const
{
    return calc_depth();
}

template <typename T>
size_t PS_Trie<T>::capacity() const
{
    return pow(br_sz, calc_depth());
}    

template <typename T>
T PS_Trie<T>::get(const size_t i) const
{
    const PS_Node<T> *node = root.get();
    for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
        //std::cout << "s: " << s << "; ind: " << (i >> s & br_mask) << std::endl;
        node = node->br[i >> s & br_mask].get();
    }
    //std::cout << "s: " << 0 << "; ind: " << (i & br_mask) << std::endl;
    const std::array<double,br_sz> &test = node->val;
    return node->val[i & br_mask];
}

// undefined behavior if i >= sz
template <typename T>
void PS_Trie<T>::set(const size_t i, const T val)
{
    PS_Node<T> *node = root.get();
    for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
        //std::cout << "s: " << s << "; ind: " << (i >> s & br_mask) << "; addr: " << node << std::endl;
        node = node->br[i >> s & br_mask].get();
    }
    //std::cout << "s: " << 0 << "; ind: " << (i & br_mask) << "; addr: " << node << std::endl;
    PS_Node<T> seg = *node;
    node->val[i & br_mask] = val;
}

// undefined behavior if i >= sz
template <typename T>
PS_Trie<T> PS_Trie<T>::pers_set(const size_t i, const T val)
{
    // copy trie
    PS_Trie<T> ret = *this;
    // copy root node and get it in a variable
    ret.root = std::make_shared<PS_Node<T>>(*root);
    //ret.root.reset(root.get());
    PS_Node<T> *node = ret.root.get();
    for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
        //std::cout << "s: " << s << "; ind: " << (i >> s & br_mask) << "; addr: " << node << std::endl;
        node->br[i >> s & br_mask] = std::make_shared<PS_Node<T>>(*(node->br[i >> s & br_mask]));
        //node->br[i >> s & br_mask].reset(node->br[i >> s & br_mask].get());
        node = node->br[i >> s & br_mask].get();
    }
    //std::cout << "s: " << 0 << "; ind: " << (i & br_mask) << "; addr: " << node << std::endl;
    PS_Node<T> seg = *node;
    node->val[i & br_mask] = val;
    return ret;
}

template <typename T>
void PS_Trie<T>::push_back(const T val)
{
    // just a set; only 1/br_sz times do we even have to construct nodes
    if (sz % br_sz != 0) {
        set(sz++, val);
        return;
    }
    
    // simple case for empty tree
    if (sz == 0) {
        root = std::make_shared<PS_Node<T>>();
        root->val[sz++] = val;
        return;
    }

    // we're gonna have to construct new nodes, or rotate
    size_t depth_cap = capacity();
    //std::cout << "original depth cap: " << depth_cap << std::endl;
    // depth at which to insert new tree
    int16_t depth_ins = -1;
    // figure out how deep we must travel to branch
    while (sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= br_sz;
        //std::cout << "sz: " << sz << "; depth_cap: " << unsigned(depth_cap) << std::endl;
    }

    // must rotate tree, as it's totally full (new root)
    if (depth_ins == -1) {
        //std::cout << "rotating tree" << std::endl;
        // update appropriate values
        shift += BITPART_SZ;
        // rotate tree
        auto temp = std::make_shared<PS_Node<T>>();
        root.swap(temp);
        root->br[0] = std::move(temp);
    } 

    // travel to branch of tree where new node will be constructed (if any)
    PS_Node<T> *node = root.get();
    int16_t s = shift;
    while (depth_ins > 0) {
        if (!node->br[sz >> s & br_mask]) {
            std::cout << "Constructing node where one should have already been" << std::endl;
            node->br[sz >> s & br_mask] = std::make_shared<PS_Node<T>>();
        }
        node = node->br[sz >> s & br_mask].get();
        s -= BITPART_SZ;
        --depth_ins;
    }
    // we're either at the top, the bottom or somewhere in-between...
    //std::cout << "s: " << s << std::endl;
    assert(s <= shift && s >= 0);
    
    // keep going, but this time, construct new nodes as necessary
    while (s > 0) {
        //std::cout << "s: " << s << "; ind: " << (sz >> s & br_mask) << "; addr: " << node << std::endl;
        node->br[sz >> s & br_mask] = std::make_shared<PS_Node<T>>();
        node = node->br[sz >> s & br_mask].get();
        s -= BITPART_SZ;
    }
    // add value
    //std::cout << "s: " << s << "; ind: " << (sz & br_mask) << "; addr: " << node << std::endl;
    node->val[sz & br_mask] = val;
    ++sz;
}

template <typename T>
PS_Trie<T> PS_Trie<T>::pers_push_back(const T val)
{
    PS_Trie<T> ret = *this;
    // just a set; only 1/br_sz times do we even have to construct nodes
    if (ret.sz % br_sz != 0) {
        return ret.pers_set(ret.sz++, val);
    }
    
    // simple case for empty tree
    if (sz == 0) {
        ret.root = std::make_shared<PS_Node<T>>();
        ret.root->val[ret.sz++] = val;
        return ret;
    }
    /*
    // we're gonna have to construct new nodes, or rotate
    uint8_t depth = calc_depth();
    // subvector capacity at this depth
    size_t depth_cap = pow(br_sz, depth);
    //std::cout << "original depth cap: " << depth_cap << std::endl;
    // depth at which to insert new tree
    int16_t depth_ins = -1;
    // figure out how deep we must travel to branch
    while (sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= br_sz;
        //std::cout << "sz: " << sz << "; depth_cap: " << unsigned(depth_cap) << std::endl;
    }

    // must rotate tree, as it's totally full (new root, depth == -1)
    if (depth_ins == -1) {
        //std::cout << "rotating tree" << std::endl;
        // update appropriate values
        shift += BITPART_SZ;
        ++depth;
        // rotate tree
        auto temp = std::make_shared<PS_Node<T>>();
        root.swap(temp);
        root->br[0] = std::move(temp);
    } 

    // travel to branch of tree where new node will be constructed (if any)
    PS_Node<T> *node = root.get();
    int16_t s = shift;
    while (depth_ins > 0) {
        if (!node->br[sz >> s & br_mask]) {
            std::cout << "Constructing node where one should have already been" << std::endl;
            node->br[sz >> s & br_mask] = std::make_shared<PS_Node<T>>();
        }
        node = node->br[sz >> s & br_mask].get();
        s -= BITPART_SZ;
        --depth_ins;
    }
    // we're either at the top, the bottom or somewhere in-between...
    //std::cout << "s: " << s << std::endl;
    assert(s <= shift && s >= 0);
    
    // keep going, but this time, construct new nodes as necessary
    while (s > 0) {
        //std::cout << "s: " << s << "; ind: " << (sz >> s & br_mask) << "; addr: " << node << std::endl;
        node->br[sz >> s & br_mask] = std::make_shared<PS_Node<T>>();
        node = node->br[sz >> s & br_mask].get();
        s -= BITPART_SZ;
    }
    // add value
    //std::cout << "s: " << s << "; ind: " << (sz & br_mask) << "; addr: " << node << std::endl;
    node->val[sz & br_mask] = val;
    ++sz;
    */
}

    // get node
    // if node is full
    //   get split version of node and insert new val into them,
    //    returning the new root that points to both new ones
    // else
    //   get copy with new val inserted
    // construct the rest of the pointer structure for the tree
    // construct the new root PS_Trie with updated sz
    // return the ptr
    
