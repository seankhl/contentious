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
        ret.root = new bp_node<T>(*root, id);
    }
    bp_node<T> *node = ret.root.get();
    for (uint16_t s = shift; s > 0; s -= BP_BITS) {
        bp_node_ptr<T> &next = node->branches[i >> s & BP_MASK];
        if (node_copy(next->id)) {
            next = new bp_node<T>(*next, id);
        }
        node = next.get();
    }
    node->values[i & BP_MASK] = val;
    return ret;
}

template <typename T, template<typename> typename TDer>
TDer<T> bp_vector_base<T, TDer>::push_back(const T &val) const
{
    // just a set; only 1/BP_WIDTH times do we even have to construct nodes
    if (this->sz % BP_WIDTH != 0) {
        TDer<T> ret(this->set(this->sz, val));
        ++(ret.sz);
        return ret;
    }

    TDer<T> ret(*this);

    // we're gonna have to construct new nodes, or rotate

    // subvector capacity at this depth
    size_t depth_cap = capacity();
    // depth at which to insert new node
    int16_t depth_ins = -1;
    // figure out how deep we must travel to branch
    while (sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= BP_WIDTH;
    }

    if (node_copy(ret.root->id)) {
        ret.root = new bp_node<T>(*root, id);
    }

    // must rotate trie, as it's totally full (new root, depth_ins is -1)
    if (depth_ins == -1) {
        // update appropriate values
        ret.shift += BP_BITS;
        // rotate trie
        boost::intrusive_ptr<bp_node<T>> temp = new bp_node<T>(id);
        ret.root.swap(temp);
        ret.root->branches[0] = std::move(temp);
    }

    // travel to branch of trie where new node will be constructed (if any)
    bp_node<T> *node = ret.root.get();
    uint16_t s = ret.shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = node->branches[sz >> s & BP_MASK];
        if (!next) {
            std::cout << "Constructing node where one should have already been"
                      << std::endl;
            next = new bp_node<T>(id);
        }
        if (node_copy(next->id)) {
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
        bp_node_ptr<T> &next = node->branches[sz >> s & BP_MASK];
        next = new bp_node<T>(id);
        node = next.get();
        s -= BP_BITS;
    }
    if (s > 0) {
        bp_node_ptr<T> &next = node->branches[sz >> s & BP_MASK];
        next = new bp_node<T>(id);
        node = next.get();
        s -= BP_BITS;
    }

    // add value
    node->values[sz & BP_MASK] = val;
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
        node_a = parent->branches[a >> s & BP_MASK].get();
        node_b = parent->branches[b >> s & BP_MASK].get();
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
        auto &next = node->branches[i >> s & BP_MASK];
        node = next.get();
    }
    assert(depth == 0);
    return node->branches;
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
        auto &next = node->branches[i >> s & BP_MASK];
        if (node_copy(next->id)) {
            next = new bp_node<T>(*next, id);
        }
        node = next.get();
    }
    assert(depth == 0);
    return node->branches;
}

// copy from other to *this from at to at+sz; vectors must have same size
template <typename T, template<typename> typename TDer>
void bp_vector_base<T, TDer>::assign(const TDer<T> &other, size_t a, size_t b)
{
    assert(a < b);
    uint16_t s = contained_at_shift(a, b);
    uint16_t d = s / BP_BITS + 1;
    int64_t interval = std::pow(BP_WIDTH, d);
    size_t ar = next_multiple(a, interval);
    size_t br = next_multiple(b, interval);
    if (b != br) { br -= interval; }
    //std::cout << a << " " << ar << " " << b << " " << br << std::endl;

    // if we're smaller than a branch, branch-copying won't work
    if (b - a < BP_WIDTH) {
        size_t split = std::min((size_t)next_multiple(a, BP_WIDTH), b);
        std::copy(other.cbegin() + a, other.cbegin() + split,
                  this->begin() + a);
        std::copy(other.cbegin() + split, other.cbegin() + b,
                  this->begin() + ar);
        return;
    }

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
    d = calc_depth() - d;
    uint16_t ar_off = ar >> (s + BP_BITS) & BP_MASK;
    ar_off = (ar_off == 0 && ar == this->sz) ? BP_WIDTH : ar_off;
    uint16_t br_off = br >> (s + BP_BITS) & BP_MASK;
    br_off = (br_off == 0 && br == this->sz) ? BP_WIDTH : br_off;
    std::copy(other.get_branch(d, ar).cbegin() + ar_off,
              other.get_branch(d, br).cbegin() + br_off,
              this->get_branch(d, ar).begin() + ar_off);

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
