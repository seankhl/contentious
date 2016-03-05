
#include <assert.h>

// mutating versions

template <typename T, template<typename> typename TDer>
using bp_vector_base_ptr = boost::intrusive_ptr<bp_vector_base<T, TDer>>;
template <typename T>
using bp_node_ptr = boost::intrusive_ptr<bp_node<T>>;

template <typename T>
using branches = std::array<boost::intrusive_ptr<bp_node<T>>,br_sz>;
template <typename T>
using values = std::array<T, br_sz>;


template <typename T>
tr_vector<T> bp_vector<T>::make_transient()
{
    tr_vector<T> ret(*this);
    return ret;
}


template <typename T>
void bp_vector<T>::mut_set(const size_t i, const T &val)
{
    bp_node<T> *node = this->root.get();
    for (int16_t s = this->shift; s > 0; s -= BITPART_SZ) {
        node = boost::get<branches<T>>(node->br)[i >> s & br_mask].get();
    }
    boost::get<values<T>>(node->br)[i & br_mask] = val;
}

template <typename T>
void bp_vector<T>::mut_push_back(const T &val)
{
    if (this->sz % br_sz != 0) {
        mut_set(this->sz++, val);
        return;
    }
    
    if (this->sz == 0) {
        this->root = new bp_node<T>(values<T>());
        boost::get<values<T>>(this->root->br)[this->sz++] = val;
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
        boost::intrusive_ptr<bp_node<T>> temp = new bp_node<T>(branches<T>());
        this->root.swap(temp);
        boost::get<branches<T>>(this->root->br)[0] = std::move(temp);
    } 

    bp_node<T> *node = this->root.get();
    int16_t s = this->shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = 
            boost::get<branches<T>>(node->br)[this->sz >> s & br_mask];
        if (!next) {
            std::cout << "Constructing node where one should have already been"
                      << std::endl;
            next = new bp_node<T>(branches<T>());
        }
        node = next.get();
        s -= BITPART_SZ;
        --depth_ins;
    }
    assert(s <= this->shift && s >= 0);
    
    while (s > BITPART_SZ) {
        bp_node_ptr<T> &next = 
            boost::get<branches<T>>(node->br)[this->sz >> s & br_mask];
        next = new bp_node<T>(branches<T>());
        node = next.get();
        s -= BITPART_SZ;
    }
    if (s == BITPART_SZ) {
        bp_node_ptr<T> &next = 
            boost::get<branches<T>>(node->br)[this->sz >> s & br_mask];
        next = new bp_node<T>(values<T>());
        node = next.get();
        s -= BITPART_SZ;
    }
    
    // add value
    boost::get<values<T>>(node->br)[this->sz & br_mask] = val;
    ++this->sz;
}

