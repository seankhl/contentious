template <typename T>
void cont_vector<T>::freeze(cont_vector<T> &dep,
                            contentious::imap_fp imap,
                            contentious::op<T> op,
                            bool onto)
{
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    // if we want our output to depend on input
    tracker.emplace(dkey, imap, op);
    // make a latch for reattaching splinters
    dep.latches.emplace(dkey, new boost::latch(hwconc));
    // make sure we have a valid _orig if not monotonic
    if (!contentious::is_monotonic(imap)) {
        // locked this->_data
        std::lock_guard<std::mutex> lock(dlck);
        this->_orig = this->_data;
        this->_data = this->_data.new_id();
        if (onto) {
            dep._data = this->_data.new_id();
        }
    }
}

template <typename T>
void cont_vector<T>::freeze(cont_vector<T> &cont, cont_vector<T> &dep,
                            contentious::imap_fp imap, contentious::op<T> op)
{
    // create _used, which has the old id of _data
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    auto ret = tracker.emplace(dkey, imap, op);
    if (!ret.second) {
        (ret.first->second).imaps.emplace_back(imap);
        (ret.first->second).icontended.emplace_back();
        (ret.first->second).ops.emplace_back(op);
    } else {
        cont.tracker.find(dkey)->second.frozen.emplace_back(this);
    }
    // make a latch for reattaching splinters
    dep.latches.emplace(dkey, new boost::latch(0));
}

template <typename T>
splt_vector<T> cont_vector<T>::detach(cont_vector &dep, uint16_t p)
{
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    auto dep_tracker_it = tracker.find(dkey);
    if (dep_tracker_it != tracker.end()) {
        auto &dep_tracker = dep_tracker_it->second;
        // must use _orig if not monotonic
        if (!contentious::is_monotonic(dep_tracker.imaps[0])) {
            dep_tracker._used[p] = _orig;
        } else {
            // locked this->_data
            std::lock_guard<std::mutex> lock(dlck);
            dep_tracker._used[p] = this->_data;
            this->_data = this->_data.new_id();
        }
        splt_vector<T> splt(dep_tracker._used[p], dep_tracker.ops);
        dep.splinters.emplace(splt._data.get_id(), false);
        return splt;
    } else {
        std::lock_guard<std::mutex> lock(contentious::plck);
        std::cout << "DETACH ERROR: NO DEP IN "
                  << this << " TRACKER (fr): &dep is "
                  << &dep
                  << ", candidates are: ";
        for (const auto &it : tracker) {
            std::cout << it.first << " ";
        }
        std::cout << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

template <typename T>
void cont_vector<T>::refresh(cont_vector &dep, uint16_t p, size_t a, size_t b)
{
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    auto dep_tracker_it = tracker.find(dkey);
    if (dep_tracker_it != tracker.end()) {
        auto &dep_tracker = dep_tracker_it->second;
        // locked this->data
        std::lock_guard<std::mutex> lock(dlck);
        dep_tracker._used[p].assign(this->_data, a, b);
    }
}

template <typename T>
void cont_vector<T>::reattach(splt_vector<T> &splt, cont_vector<T> &dep,
                              uint16_t p, size_t a, size_t b)
{
    // TODO: generalize to non-modification operations, like
    // insert/remove/push_back/pop_back

    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    auto &dep_tracker = tracker.find(dkey)->second;
    T diff;
    const int32_t skey = splt._data.get_id();
    auto splinter = dep.splinters.find(skey);
    if (splinter != dep.splinters.end()) {
        const auto &dep_op = dep_tracker.ops[0];
        if (contentious::is_monotonic(dep_tracker.imaps[0])) {
            std::lock_guard<std::mutex> lock(dep.dlck);
            /*{
                std::lock_guard<std::mutex> lock2(contentious::plck);
                std::cout << "copying " << splt._data.get_id() << " -> " << dep._data.get_id()
                          << " at " << a << ": ";
                auto end = splt._data.cbegin() + b;
                for (auto it = splt._data.cbegin() + a; it != end; ++it) {
                    std::cout << *it << " ";
                }
                std::cout << std::endl;
            }*/
            dep._data.assign(splt._data, a, b);
        } else {
            std::lock_guard<std::mutex> lock(dep.dlck);
            for (size_t i = a; i < b; ++i) {    // locked dep
                diff = dep_op.inv(splt._data[i], dep_tracker._used[p][i]);
                if (diff != dep_op.identity) {
                    dep._data.mut_set(i, dep_op.f(dep[i], diff));
                    /*std::cout << "sid " << sid
                              << " joins with " << dep._data.get_id()
                              << " with diff " << diff
                              << " to produce " << dep[i]
                              << std::endl;*/
                }
            }
        }
        splinter->second = true;
    } else {
        std::lock_guard<std::mutex> lock(contentious::plck);
        std::cout << "TROUBLE with " << skey
                  << "!!! splinters.size(): " << dep.splinters.size()
                  << std::endl;
        std::cout << "the splinters are: ";
        for (const auto &s : dep.splinters) {
            std::cout << s.first;
        }
        std::cout << std::endl;
    }

    contentious::closure resolution;
    if (p == 0) {
        if (contentious::is_monotonic(dep_tracker.imaps[0])) {
            resolution = std::bind(&cont_vector::resolve_monotonic,
                                   std::ref(*this), std::ref(dep));
        } else {
            resolution = std::bind(&cont_vector::resolve,
                                   std::ref(*this), std::ref(dep));
        }
        contentious::tp.submitr(resolution, p);
    }

    auto latch = dep.latches.find(dkey);
    if (latch != dep.latches.end()) {
        latch->second->count_down();
    } else {
        std::cout << "didn't find latch" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

// TODO: multithreaded and sparse
template <typename T>
void cont_vector<T>::resolve(cont_vector<T> &dep)
{
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    auto &dep_tracker = tracker.find(dkey)->second;

    auto latch = dep.latches.find(dkey);
    if (latch != dep.latches.end()) {
        latch->second->wait();
    } else {
        std::cout << "didn't find latch" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    auto f = dep_tracker.frozen.begin();
    while (dep_tracker.frozen.size() > 0) {
        auto ff = *f;
        f = dep_tracker.frozen.erase(f);
        ff->resolve(dep);
    }

    const auto &dep_op = dep_tracker.ops[0];
    {
        std::lock_guard<std::mutex> lock(dep.dlck);
        T &target = *(dep._data.begin() + dep_tracker.imaps[0](0));
        size_t c = this->_orig.next_diff_ids(this->_data, 0);
        while (c != this->size()) {
            auto curr = this->_data.cbegin() + c;
            auto trck = this->_orig.cbegin() + c;
            T diff = dep_op.inv(*curr, *trck);
            if (diff != dep_op.identity) {
                target = dep_op.f(target, diff);
            }
            c = this->_orig.next_diff_ids(this->_data, ++c);
        }
    }
    resolved = true;
    if (abandoned) {
        delete this;
    }
}

/* used:
 * this
 *  tracker[&dep]
 *  _data, size()
 *  frozen
 * dep
 *  latches[uid]
 *  _data, size()
 */
// TODO: multithreaded and sparse
template <typename T>
void cont_vector<T>::resolve_monotonic(cont_vector<T> &dep)
{
    auto dkey = reinterpret_cast<uintptr_t>(&dep);
    auto &dep_tracker = tracker.find(dkey)->second;

    auto latch = dep.latches.find(dkey);
    if (latch != dep.latches.end()) {
        latch->second->wait();
    } else {
        std::cout << "didn't find latch" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    auto f = dep_tracker.frozen.begin();
    while (dep_tracker.frozen.size() > 0) {
        auto ff = *f;
        f = dep_tracker.frozen.erase(f);
        ff->resolve_monotonic(dep);
    }

    size_t a, b;
    int64_t adom, aran, bdom, bran;
    for (size_t i = 0; i < dep_tracker.imaps.size(); ++i) {
        const auto &dep_op = dep_tracker.ops[i];
        auto &imap = dep_tracker.imaps[i];
        auto &icontended = dep_tracker.icontended[i];
        for (int p = 0; p < hwconc; ++p) {
            std::tie(a, b) = contentious::partition(p, this->size());
            //std::cout << "size of imaps: " << dep_tracker.imaps.size() << std::endl;
            std::tie(adom, aran) = contentious::safe_mapping(imap, a, 0, this->size());
            std::tie(bdom, bran) = contentious::safe_mapping(imap, b, 0, this->size());
            for (int64_t c = adom; c < aran; c += 1) {
                icontended.insert(c);
            }
            for (int64_t c = bran; c < bdom; c += 1) {
                icontended.insert(c);
            }
        }
        /*{
            std::lock_guard<std::mutex> lock(contentious::plck);
            std::cout << "data: " << this->_data << std::endl;
            std::cout << "used[0]: " << dep_tracker._used[0] << std::endl;
            std::cout << "used[1]: " << dep_tracker._used[1] << std::endl;
            std::cout << "used[2]: " << dep_tracker._used[2] << std::endl;
            std::cout << "used[3]: " << dep_tracker._used[3] << std::endl;
            std::cout << "dep (before): " << dep._data << std::endl;
            std::cout << "icontended for "
                      << this->_data.get_id() << " -> " << dep._data.get_id()
                      << " with o " << o << ": ";
            for (auto it = icontended.begin(); it != icontended.end(); ++it) {
                std::cout << *it << " ";
            }
            std::cout << std::endl;
        }*/
        {   // locked dep
            std::lock_guard<std::mutex> lock(dep.dlck);

            uint16_t p = hwconc;
            for (auto &c : icontended) {
                p = hwconc;
                int64_t cmap = imap(c);
                if (cmap < 0 || cmap > (int64_t)dep._data.size()) {
                    continue;
                }
                for (uint16_t pi = 0; pi < hwconc; ++pi) {
                    std::tie(a, b) = contentious::partition(pi, this->size());
                    if (cmap >= (int64_t)a && cmap < (int64_t)b) {
                        p = pi;
                    }
                }
                assert(p != hwconc);
                auto it = dep._data.begin() + cmap;
                auto curr = this->_data.cbegin() + c;
                auto trck = dep_tracker._used[p].cbegin() + c;
                T diff = dep_op.inv(*curr, *trck);
                if (diff != dep_op.identity) {
                    *it = dep_op.f(*it, diff);
                    /*{
                        std::lock_guard<std::mutex> lock2(contentious::plck);
                        std::cout << this->_data.get_id() << "->" << dep._data.get_id()
                                  << " would have resolved " << c
                                  << " onto " << cmap
                                  << " with curr, trck " << *curr << ", " << *trck
                                  << " with diff " << diff << " and p " << p
                                  << " to produce " << *it << std::endl;
                    }*/
                    // since this changed, we may need to resolve it even if it's
                    // not one of the potentially-conflicted values
                    dep.contended.insert(cmap);
                }
            }
            for (auto &c : contended) {
                int64_t cmap = imap(c);
                // not valid range
                if (cmap < 0 || cmap > (int64_t)dep._data.size()) {
                    continue;
                }
                // already resolved
                if (icontended.count(c) > 0) { continue; }
                for (uint16_t pi = 0; pi < hwconc; ++pi) {
                    std::tie(a, b) = contentious::partition(pi, this->size());
                    if (cmap >= (int64_t)a && cmap < (int64_t)b) {
                        p = pi;
                    }
                }
                auto it = dep._data.begin() + cmap;
                auto curr = this->_data.cbegin() + c;
                auto trck = dep_tracker._used[p].cbegin() + c;
                T diff = dep_op.inv(*curr, *trck);
                if (diff != dep_op.identity) {
                    /*{
                        std::lock_guard<std::mutex> lock2(contentious::plck);
                        std::cout << this->_data.get_id() << "->" << dep._data.get_id()
                                  << " would have resolved " << c
                                  << " onto " << cmap
                                  << " with curr, trck " << *curr << ", " << *trck
                                  << " with diff " << diff
                                  << " to produce " << *it << std::endl;
                    }*/
                    *it = dep_op.f(*it, diff);
                    // since this changed, we may need to resolve it even if it's
                    // not one of the potentially-conflicted values
                    dep.contended.insert(cmap);
                }
            }
        }
    }
    resolved = true;
    if (abandoned) {
        delete this;
    }
    /*{
        std::lock_guard<std::mutex> lock(contentious::plck);
        std::cout << "dep (after): " << dep._data << std::endl;
    }*/
}


/* this function performs thread function f on the passed cont_vector
 * based on a partition for processor p of hwconc
 * with extra args U... as necessary */
template <typename T>
template <typename... U>
void cont_vector<T>::exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                                     const uint16_t, const U &...),
                              cont_vector<T> &dep, const U &... args)
{
    for (uint16_t p = 0; p < hwconc; ++p) {
        auto task = std::bind(f, std::ref(*this), std::ref(dep),
                              p, args...);
        contentious::tp.submit(task, p);
    }
}


template <typename T>
cont_vector<T> cont_vector<T>::reduce(const contentious::op<T> op)
{
    // our reduce dep is just one value
    auto dep = cont_vector<T>();
    dep.unprotected_push_back(op.identity);

    freeze(dep, contentious::alltoone<0>, op);
    // no template parameters
    exec_par<>(contentious::reduce_splt<T>, dep);

    return dep;
}

template <typename T>
std::shared_ptr<cont_vector<T>> cont_vector<T>::foreach(
                                        const contentious::op<T> op,
                                        const T &val)
{
    auto dep = std::make_shared<cont_vector<T>>(*this);

    // template parameter is the arg to the foreach op
    freeze(*dep, contentious::identity, op);
    exec_par<T>(contentious::foreach_splt<T>, *dep, val);

    return dep;
}

// TODO: right now, this->()size must be equal to other.size()
template <typename T>
std::shared_ptr<cont_vector<T>> cont_vector<T>::foreach(
                                        const contentious::op<T> op,
                                        cont_vector<T> &other)
{
    auto dep = std::make_shared<cont_vector<T>>(*this);

    // gratuitous use of reference_wrapper
    freeze(*dep, contentious::identity, op);
    other.freeze(*this, *dep, contentious::identity, op);
    exec_par<std::reference_wrapper<cont_vector<T>>>(
            contentious::foreach_splt_cvec<T>, *dep, std::ref(other));

    return dep;
}

template <typename T>
template <int... Offs>
cont_vector<T> cont_vector<T>::stencil(const std::vector<T> &coeffs,
                                       const contentious::op<T> op1,
                                       const contentious::op<T> op2)
{
    using cvec_ref = std::reference_wrapper<cont_vector<T>>;

    constexpr size_t offs_sz = sizeof...(Offs);
    std::array<std::function<int(int)>, offs_sz> offs{contentious::offset<Offs>...};

    /*
     * part 1
     */
    // get unique coefficients
    auto coeffs_unique = coeffs;
    auto it = std::unique(coeffs_unique.begin(), coeffs_unique.end());
    coeffs_unique.resize(std::distance(coeffs_unique.begin(), it));

    // perform coefficient multiplications on original vector
    // TODO: limit range of multiplications to only those necessary
    std::map<T, cont_vector<T>> step1;
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        step1.emplace(std::piecewise_construct,
                      std::forward_as_tuple(coeffs_unique[i]),
                      std::forward_as_tuple(*this)
        );
        freeze(step1.at(coeffs_unique[i]), contentious::identity, op1);
        exec_par<T>(contentious::foreach_splt<T>,
                    step1.at(coeffs_unique[i]), coeffs_unique[i]);
    }
    /*
     * part 2
     */
    // sum up the different parts of the stencil
    std::vector<cont_vector<T>> step2;
    step2.reserve(offs_sz*2 + 1);
    step2.emplace_back(*this);
    for (size_t i = 0; i < offs_sz; ++i) {
        step2.emplace_back(step2[i]);
        step2[i].freeze(step2[i+1], contentious::identity, op2);
        step1.at(coeffs[i]).freeze(step2[i], step2[i+1], offs[i], op2);
        step2[i].template exec_par<cvec_ref>(contentious::foreach_splt_cvec<T>,
                                             step2[i+1], std::ref(step1.at(coeffs[i])));
    }
    contentious::tp.finish();

    /*
    auto &thisthis = step2[step2.size()-1];
    std::map<T, cont_vector<T>> step11;
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        step11.emplace(std::piecewise_construct,
                       std::forward_as_tuple(coeffs_unique[i]),
                       std::forward_as_tuple(thisthis)
        );
        thisthis.freeze(step11.at(coeffs_unique[i]), true, contentious::identity, op1);
        thisthis.exec_par<T>(contentious::foreach_splt<T>,
                             step11.at(coeffs_unique[i]), coeffs_unique[i]);
    }
    for (size_t i = offs_sz; i < offs_sz*2; ++i) {
        step2.emplace_back(step2[i]);
        step2[i].freeze(step2[i+1], true, contentious::identity, op2);
        step11.at(coeffs[1]).freeze(step2[i], step2[i+1], offs[i-offs_sz]);
        step2[i].template exec_par<cvec_ref>(contentious::foreach_splt_cvec<T>,
                                             step2[i+1], std::ref(step11.at(coeffs[i-offs_sz])));
    }

    contentious::tp.finish();

    //std::cout << "input: " << step2[0] << std::endl;
    // all the resolutions... *exhale*
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        //resolve(step1.at(coeffs_unique[i]));
        //std::cout << "coeff vec: " << step1.at(coeffs_unique[i]) << std::endl;
    }
    for (size_t i = 0; i < offs_sz; ++i) {
        //step2[i].resolve(step2[i+1]);
        //std::cout << "step2: " << step2[i+1] << std::endl;
    }

    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        //thisthis.resolve(step11.at(coeffs_unique[i]));
        //std::cout << "coeff vec: " << step11.at(coeffs_unique[i]) << std::endl;
    }
    for (size_t i = offs_sz; i < offs_sz*2; ++i) {
        //step2[i].resolve(step2[i+1]);
        //std::cout << "step2: " << step2[i+1] << std::endl;
    }
    */
    return step2[step2.size()-1];
}

template <typename T>
template <int... Offs>
cont_vector<T> *cont_vector<T>::stencil2(const std::vector<T> &coeffs,
                                         const contentious::op<T> op1,
                                         const contentious::op<T> op2)
{
    constexpr size_t offs_sz = sizeof...(Offs);
    std::array<std::function<int(int)>, offs_sz> offs{contentious::offset<Offs>...};
    using cvec_ref = std::reference_wrapper<cont_vector<T>>;

    cont_vector<T> *coeff_vec_ptr;
    std::array<cont_vector<T> *, offs_sz> coeff_vecs;
    for (size_t i = 0; i < coeffs.size(); ++i) {
        coeff_vec_ptr = new cont_vector<T>(*this);
        freeze(*coeff_vec_ptr, contentious::identity, op1);

        exec_par<T>(contentious::foreach_splt<T>,
                    *coeff_vec_ptr, coeffs[i]);
        coeff_vecs[i] = coeff_vec_ptr;
    }

    cont_vector<T> *curr_vec_ptr = new cont_vector<T>(*this);
    cont_vector<T> *next_vec_ptr;
    for (size_t i = 0; i < offs_sz; ++i) {
        next_vec_ptr = new cont_vector<T>(*curr_vec_ptr);
        curr_vec_ptr->freeze(*next_vec_ptr, contentious::identity, op2);
        coeff_vecs[i]->freeze(*curr_vec_ptr, *next_vec_ptr, offs[i], op2);

        curr_vec_ptr->template exec_par<cvec_ref>(
                            contentious::foreach_splt_cvec<T>,
                            *next_vec_ptr, std::ref(*coeff_vecs[i]));
        //curr_vec_ptr->abandon();
        curr_vec_ptr = next_vec_ptr;
    }
    /*for (auto &coeff_vec : coeff_vecs) {
        coeff_vec.second->abandon();
    }*/
    //std::cout << *next_vec_ptr << std::endl;

    //contentious::tp.finish();
    return next_vec_ptr;
}

template <typename T>
template <int... Offs>
std::shared_ptr<cont_vector<T>> cont_vector<T>::stencil3(
                                const std::vector<T> &coeffs,
                                const contentious::op<T>,
                                const contentious::op<T> op2)
{
    constexpr size_t NS = sizeof...(Offs);
    std::array<contentious::imap_fp, NS>
                                offs{contentious::offset<Offs>...};

    auto dep = std::make_shared<cont_vector<T>>(*this);
    freeze(*dep, contentious::identity, op2);
    for (size_t i = 0; i < NS; ++i) {
        contentious::op<T> fullop = {
            0,
            boost::bind<double>(contentious::multplus_fp<double>,
                                _1, _2, coeffs[i]),
            contentious::minus_fp<double>
        };
        freeze(*this, *dep, offs[i], fullop);
    }

    for (uint16_t p = 0; p < hwconc; ++p) {
        auto task = std::bind(contentious::stencil_splt<T, NS>,
                              std::ref(*this), std::ref(*dep), p);
        contentious::tp.submit(task, p);
    }
    //curr_vec_ptr->abandon();
    /*for (auto &coeff_vec : coeff_vecs) {
        coeff_vec.second->abandon();
    }*/
    //contentious::tp.finish();
    return dep;
}
