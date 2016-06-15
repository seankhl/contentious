template <typename T>
void bp_vector<T>::mut_set(const size_t i, const T &val)
{
    bp_node<T> *node = this->root.get();
    for (uint16_t s = this->shift; s > 0; s -= BP_BITS) {
        node = node->branches.template
               get<bp_branches<T>>()[i >> s & BP_MASK].get();
    }
    node->branches.template get<bp_leaves<T>>()[i & BP_MASK] = val;
}

template <typename T>
void bp_vector<T>::mut_push_back(const T &val)
{
    if (this->sz % BP_WIDTH != 0) {
        mut_set(this->sz++, val);
        return;
    }

    size_t depth_cap = this->capacity();
    int16_t depth_ins = -1;
    while (this->sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= BP_WIDTH;
    }

    if (depth_ins == -1) {
        this->shift += BP_BITS;
        bp_node_ptr<T> temp = new bp_node<T>(bp_branches<T>());
        this->root.swap(temp);
        this->root->branches.template get<bp_branches<T>>()[0] = std::move(temp);
    }

    bp_node<T> *node = this->root.get();
    uint16_t s = this->shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = node->branches.template
                               get<bp_branches<T>>()[this->sz >> s & BP_MASK];
        assert(next);
        node = next.get();
        s -= BP_BITS;
        --depth_ins;
    }
    assert(s <= this->shift && s >= 0);

    while (s > BP_BITS) {
        bp_node_ptr<T> &next = node->branches.template
                               get<bp_branches<T>>()[this->sz >> s & BP_MASK];
        next = new bp_node<T>(bp_branches<T>());
        node = next.get();
        s -= BP_BITS;
    }
    if (s == BP_BITS) {
        bp_node_ptr<T> &next = node->branches.template
                               get<bp_branches<T>>()[this->sz >> s & BP_MASK];
        next = new bp_node<T>(bp_leaves<T>());
        node = next.get();
        s -= BP_BITS;
    }

    // add value
    node->branches.template get<bp_leaves<T>>()[this->sz & BP_MASK] = val;
    ++this->sz;
}
