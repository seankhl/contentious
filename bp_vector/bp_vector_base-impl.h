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
    if (ret.node_copy(ret.root->id)) {
        ret.root = new bp_node<T>(*(ret.root), id);
    }
    bp_node<T> *node = ret.root.get();
    for (uint16_t s = ret.shift; s > 0; s -= BP_BITS) {
        bp_node_ptr<T> &next = node->branches.template 
                               get<bp_branches<T>>()[i >> s & BP_MASK];
        if (ret.node_copy(next->id)) {
            next = new bp_node<T>(*next, id);
        }
        if (s == BP_BITS) {
            //std::cout << "checking for leaf " << &(node->branches) << std::endl;
            assert(next->branches.template has<bp_leaves<T>>());
        }
        node = next.get();
    }
    //std::cout << "i & BP_MASK: " << (i & BP_MASK) << std::endl;
    node->branches.template get<bp_leaves<T>>()[i & BP_MASK] = val;
    //std::cout << "set val: " << ret.operator[](i) << std::endl;
    return ret;
}

template <typename T, template<typename> typename TDer>
TDer<T> bp_vector_base<T, TDer>::push_back(const T &val) const
{
    //std::cout << "adding: " << val << " at size: " << this->sz << std::endl;
    // just a set; only 1/BP_WIDTH times do we even have to construct nodes
    if (this->sz % BP_WIDTH != 0) {
        TDer<T> ret(this->set(this->sz, val));
        ++(ret.sz);
        return ret;
    }

    TDer<T> ret(*this);

    // we're gonna have to construct new nodes, or rotate

    // subvector capacity at this depth
    size_t depth_cap = ret.capacity();
    // depth at which to insert new node
    int16_t depth_ins = -1;
    // figure out how deep we must travel to branch
    while (sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= BP_WIDTH;
    }

    if (ret.node_copy(ret.root->id)) {
        ret.root = new bp_node<T>(*(ret.root), id);
    }

    // must rotate trie, as it's totally full (new root, depth_ins is -1)
    if (depth_ins == -1 && this->sz != 0) {
        // update appropriate values
        ret.shift += BP_BITS;
        // rotate trie
        bp_node_ptr<T> temp = new bp_node<T>(bp_branches<T>(), id);
        ret.root.swap(temp);
        std::cout << "rotating trie of size:" << this->sz << std::endl;
        ret.root->branches.template get<bp_branches<T>>()[0] = std::move(temp);
    }

    // travel to branch of trie where new node will be constructed (if any)
    bp_node<T> *node = ret.root.get();
    uint16_t s = ret.shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = node->branches.template
                               get<bp_branches<T>>()[sz >> s & BP_MASK];
        assert(next);
        if (ret.node_copy(next->id)) {
            next = new bp_node<T>(*next, id);
        }
        node = next.get();
        s -= BP_BITS;
        --depth_ins;
    }
    // we're either at the top, the bottom or somewhere in-between...
    assert(s <= ret.shift && s >= 0);

    // keep going, but this time, construct new nodes as necessary
    while (s > BP_BITS) {
        bp_node_ptr<T> &next = node->branches.template
                               get<bp_branches<T>>()[sz >> s & BP_MASK];
        next = new bp_node<T>(bp_branches<T>(), id);
        node = next.get();
        s -= BP_BITS;
    }
    if (s > 0) {
        bp_node_ptr<T> &next = node->branches.template 
                               get<bp_branches<T>>()[sz >> s & BP_MASK];
        node->branches.template
        get<bp_branches<T>>()[sz >> s & BP_MASK] = new bp_node<T>(bp_leaves<T>(), id);
        node = next.get();
        s -= BP_BITS;
        //std::cout << "made a leaf" << std::endl;
    }

    // add value
    node->branches.template get<bp_leaves<T>>()[sz & BP_MASK] = val;
    //std::cout << "added: " << &(node->branches) << std::endl;
    ++ret.sz;
    return ret;
}

template <typename T, template<typename> typename TDer>
uint16_t bp_vector_base<T, TDer>::contained_at_shift(size_t a, size_t b) const
{
    assert(a < b);
    bp_node<T> *parent = this->root.get();
    bp_node<T> *node_a = this->root.get();
    bp_node<T> *node_b = this->root.get();
    uint16_t s;
    for (s = this->shift; s > 0; s -= BP_BITS) {
        if (node_a != node_b) {
            break;
        }
        parent = node_a;
        node_a = parent->branches.template
                 get<bp_branches<T>>()[a >> s & BP_MASK].get();
        node_b = parent->branches.template
                 get<bp_branches<T>>()[b >> s & BP_MASK].get();
    }
    return s;
}

template <typename T, template<typename> typename TDer>
const std::array<bp_node_ptr<T>, BP_WIDTH> &
bp_vector_base<T, TDer>::get_branch(uint8_t depth, int64_t i) const
{
    bp_node<T> *node = root.get();
    uint16_t s;
    for (s = this->shift; s > 0; s -= BP_BITS) {
        if (--depth == 0) {
            break;
        }
        auto &next = node->branches.template
                     get<bp_branches<T>>()[i >> s & BP_MASK];
        node = next.get();
    }
    assert(depth == 0);
    return node->branches.template get<bp_branches<T>>();
}

template <typename T, template<typename> typename TDer>
std::array<bp_node_ptr<T>, BP_WIDTH> &
bp_vector_base<T, TDer>::get_branch(uint8_t depth, int64_t i)
{
    if (node_copy(root->id)) {
        root = new bp_node<T>(*root, id);
    }
    bp_node<T> *node = root.get();
    uint16_t s;
    for (s = this->shift; s > 0; s -= BP_BITS) {
        if (--depth == 0) {
            break;
        }
        auto &next = node->branches.template
                     get<bp_branches<T>>()[i >> s & BP_MASK];
        if (node_copy(next->id)) {
            next = new bp_node<T>(*next, id);
        }
        node = next.get();
    }
    assert(depth == 0);
    return node->branches.template get<bp_branches<T>>();
}

// copy from other to *this from at to at+sz; vectors must have same size
template <typename T, template<typename> typename TDer>
void bp_vector_base<T, TDer>::assign(const TDer<T> &other, size_t a, size_t b)
{
    assert(a < b);
    //size_t copy_count = 0;
    // if we're smaller than a branch, branch-copying won't work
    if (b - a < BP_WIDTH) {
        size_t split = std::min((size_t)next_multiple(a, BP_WIDTH), b);
        std::copy(other.cbegin() + a, other.cbegin() + split,
                  this->begin() + a);
        std::copy(other.cbegin() + split, other.cbegin() + b,
                  this->begin() + split);
        return;
    }

    int64_t interval = BP_WIDTH;
    size_t ar = next_multiple(a, interval);
    size_t br = next_multiple(b, interval);
    if (b != br) { br -= interval; }
    //std::cout << a << " " << ar << " " << br << " " << b << std::endl;

    // 1. copy individual vals for partial leaf we originate in, if n b
    if (ar - a > 0) {
        auto it_step1 = other.cbegin() + a;
        auto end1 = this->begin() + ar;
        for (auto it = this->begin() + a; it != end1; ++it, ++it_step1) {
            *it = *it_step1;
        }
        //copy_count += (ar - a);
    }

    size_t ai, bi;
    uint16_t ai_off, ar_off, bi_off, br_off, s, d;
    uint16_t last_s = contained_at_shift(a, b) + BP_BITS;
    uint16_t last_d = last_s / BP_BITS;
    int64_t last_interval = std::pow(BP_WIDTH, last_d);

    // 2. travel upwards, copying branches at shallowest depth possible
    // this is where we'll be when we do the shallowest branch copy
    s = 0;
    while (interval != last_interval) {
        // get next set of branches to copy at decreased depth
        interval *= BP_WIDTH;
        ai = ar;
        ar = next_multiple(ai, interval);
        s += BP_BITS;
        d = s / BP_BITS;
        if (ai == ar) { continue; }

        d = calc_depth() - d;
        ai_off = ai >> s & BP_MASK;
        assert(ai_off != 0);
        ar_off = ar >> s & BP_MASK;
        assert(ar_off == 0);
        ar_off = BP_WIDTH;
        /*std::cout << "(" << ai << ", " << ai_off
                  << "; " << ar << ", " << ar_off << ") "
                  << interval << " / " << last_interval << std::endl;*/
        std::copy(other.get_branch(d, ai).cbegin() + ai_off,
                  other.get_branch(d, ar-1).cbegin() + ar_off,
                  this->get_branch(d, ai).begin() + ai_off);
        //copy_count += (ar_off - ai_off);
    }

    // 3. copy shallow branches until we're in the final val's branch
    last_d = calc_depth() - last_d;
    ar = next_multiple(a, last_interval);
    br = next_multiple(b, last_interval);
    if (br != b) { br -= interval; }
    if (ar != br) {
        ar_off = ar >> last_s & BP_MASK;
        ar_off = (ar_off == 0 && ar == this->sz) ? BP_WIDTH : ar_off;
        br_off = br >> last_s & BP_MASK;
        br_off = (br_off == 0 && br == this->sz) ? BP_WIDTH : br_off;
        //std::cout << "PHASE 3: " << ar << " " << br << std::endl;
        std::copy(other.get_branch(last_d, ar).cbegin() + ar_off,
                  other.get_branch(last_d, br).cbegin() + br_off,
                  this->get_branch(last_d, ar).begin() + ar_off);
        //copy_count += (br_off - ar_off);
    }

    // 4. travel downwards, copying branches at [...]
    s = last_s;
    while (interval != BP_WIDTH) {
        // get next set of branches to copy at decreased depth
        interval /= BP_WIDTH;
        bi = br;
        br = next_multiple(b, interval);
        s -= BP_BITS;
        d = s / BP_BITS;

        if (b != br) { br -= interval; }
        if (bi == br) { continue; }
        d = calc_depth() - d;
        bi_off = bi >> s & BP_MASK;
        br_off = br >> s & BP_MASK;
        br_off = (br_off == 0 && bi != br) ? BP_WIDTH : br_off;
        /*std::cout << "(" << bi << ", " << bi_off
                  << "; " << br << ", " << br_off << ") "
                  << interval << " / " << last_interval << std::endl;*/
        assert(bi_off == 0);
        std::copy(other.get_branch(d, bi).cbegin() + bi_off,
                  other.get_branch(d, br-1).cbegin() + br_off,
                  this->get_branch(d, bi).begin() + bi_off);
        //copy_count += (br_off - bi_off);
    }

    // 5. copy individual vals for partial leaf we terminate in, if n e
    if (b - br > 0) {
        auto it_step3 = other.cbegin() + br;
        auto end3 = this->begin() + b;
        for (auto it = this->begin() + br; it != end3; ++it, ++it_step3) {
            *it = *it_step3;
        }
        //copy_count += (b - br);
    }
    /*std::cout << "copy count in assign (depth: " << (uint16_t)calc_depth()
              << ", BP_WIDTH: " << BP_WIDTH
              << ", [a,b): [" << a << "," << b << ")): "
              << copy_count << std::endl;*/
}
