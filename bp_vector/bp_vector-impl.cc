
#include "bp_vector.h"
#include <assert.h>

// mutating versions

template <typename T>
void bp_vector<T>::mut_set(const size_t i, const T &val)
{
    bp_node<T> *node = this->root.get();
    for (int16_t s = this->shift; s > 0; s -= BITPART_SZ) {
        node = node->br[i >> s & br_mask].get();
    }
    node->val[i & br_mask] = val;
}

template <typename T>
void bp_vector<T>::mut_push_back(const T &val)
{
    if (this->sz % br_sz != 0) {
        mut_set(this->sz++, val);
        return;
    }
    
    if (this->sz == 0) {
        this->root = std::make_shared<bp_node<T>>();
        this->root->val[this->sz++] = val;
        return;
    }

    size_t depth_cap = this->capacity();
    int16_t depth_ins = -1;
    while (this->sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= br_sz;
    }

    if (depth_ins == -1) {
        this->shift += BITPART_SZ;
        auto temp = std::make_shared<bp_node<T>>();
        this->root.swap(temp);
        this->root->br[0] = std::move(temp);
    } 

    bp_node<T> *node = this->root.get();
    int16_t s = this->shift;
    while (depth_ins > 0) {
        if (!node->br[this->sz >> s & br_mask]) {
            std::cout << "Constructing node where one should have already been" 
                      << std::endl;
            node->br[this->sz >> s & br_mask] = 
                std::make_shared<bp_node<T>>();
        }
        node = node->br[this->sz >> s & br_mask].get();
        s -= BITPART_SZ;
        --depth_ins;
    }
    assert(s <= this->shift && s >= 0);
    
    while (s > 0) {
        node->br[this->sz >> s & br_mask] = 
            std::make_shared<bp_node<T>>();
        node = node->br[this->sz >> s & br_mask].get();
        s -= BITPART_SZ;
    }
    // add value
    node->val[this->sz & br_mask] = val;
    ++this->sz;
}

