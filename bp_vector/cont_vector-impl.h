
template <typename T>
void cont_vector<T>::freeze(cont_vector<T> &dep,
                            bool onto,
                            std::function<int(int)> imap,
                            contentious::op<T> op,
                            uint16_t ndetached)
{
    {   // locked _data (all 3 actions must happen atomically)
        std::lock_guard<std::mutex> lock(dlck);
        // if we want our output to depend on input
        if (onto) {
            std::lock_guard<std::mutex> lock(dep.dlck);
            dep._data = _data.new_id();
        }
        // create _used, which has the old id of _data
        tracker.emplace(std::piecewise_construct,
                        std::forward_as_tuple(&dep),
                        std::forward_as_tuple(dependency_tracker(_data, imap, op))
        );
        // modifications to data cannot affect _used
        _data = _data.new_id();
        frozen[&dep] = {};
    }
    // make a latch for reattaching splinters
    dep.rlatches.emplace(std::piecewise_construct,
                         std::forward_as_tuple(tracker[&dep]._used.get_id()),
                         std::forward_as_tuple(new boost::latch(ndetached))
    );
}

template <typename T>
void cont_vector<T>::freeze(cont_vector<T> &cont, cont_vector<T> &dep,
                            std::function<int(int)> imap)
{
    const auto &op = cont.tracker[&dep].op;
    {   // locked _data (all 3 actions must happen atomically)
        std::lock_guard<std::mutex> lock(dlck);
        // create _used, which has the old id of _data
        tracker.emplace(std::piecewise_construct,
                        std::forward_as_tuple(&dep),
                        std::forward_as_tuple(dependency_tracker(_data, imap, op))
        );
        // modifications to data cannot affect _used
        _data = _data.new_id();
        cont.frozen[&dep].push_back(this);
    }
    // make a latch for reattaching splinters
    dep.rlatches.emplace(std::piecewise_construct,
                         std::forward_as_tuple(tracker[&dep]._used.get_id()),
                         std::forward_as_tuple(new boost::latch(0))
    );
}


template <typename T>
splt_vector<T> cont_vector<T>::detach(cont_vector &dep)
{
    if (tracker.count(&dep) > 0) {
        splt_vector<T> splt(tracker[&dep]._used, tracker[&dep].op);
        {   // locked dep.reattached
            std::lock_guard<std::mutex> lock(dep.rlck);
            dep.reattached.emplace(std::make_pair(splt._data.get_id(),
                                                  false));
        }
        return splt;
    } else {
        std::cout << "DETACH ERROR: NO DEP IN TRACKER: &dep is "
                  << &dep << std::endl;
        return splt_vector<T>(tracker[&dep]._used, tracker[&dep].op);
    }
}

template <typename T>
void cont_vector<T>::reattach(splt_vector<T> &splt, cont_vector<T> &dep,
                              uint16_t p, size_t a, size_t b)
{
    // TODO: different behavior for other. don't want to loop over all
    // values for changes, there's too many and it ruins the complexity
    // TODO: generalize to non-modification operations, like
    // insert/remove/push_back/pop_back

    // OLD but leaving here for future reference
    // if we haven't yet started building a more current vector, make one
    // grow out from ourselves. this is safe to modify, but this isn't,
    // because others may be depending on it for resolution

    const int32_t sid = splt._data.get_id();
    // TODO: better (lock-free) mechanism here
    bool found = false;
    {   // locked dep.reattached
        std::lock_guard<std::mutex> lock(dep.rlck);
        found = dep.reattached.count(sid) > 0;
    }

    auto &dep_tracker = tracker[&dep];
    const int32_t uid = dep_tracker._used.get_id();
    T diff;
    if (found) {
        if (contentious::getAddress(dep_tracker.indexmap) ==
            contentious::getAddress(std::function<int(int)>(contentious::identity))) {
            std::lock_guard<std::mutex> lock(dep.dlck);
            dep._data.copy(splt._data, a, b);
        } else {
            const auto &dep_op = dep_tracker.op;
            std::lock_guard<std::mutex> lock(dep.dlck);
            for (size_t i = a; i < b; ++i) {    // locked dep
                diff = dep_op.inv(splt._data[i], dep_tracker._used[i]);
                if (diff != dep_op.identity) {
                    dep._data.mut_set(i, dep_op.f(dep[i], diff));
                    /*
                    std::cout << "sid " << sid
                              << " joins with " << dep._data.get_id()
                              << " with diff " << diff
                              << " to produce " << dep[i]
                              << std::endl;
                    */
                }
            }

        }

        {   // locked dep
            std::lock_guard<std::mutex>(dep.rlck);
            dep.reattached[sid] = true;
        }

    } else {
        std::cout << "TROUBLE with " << sid
                  << "!!! splinters.size(): " << dep.reattached.size()
                  << std::endl;
        std::cout << "the splinters are: ";
        for (const auto &r : dep.reattached) {
            std::cout << r.first;
        }
        std::cout << std::endl;
    }
    
    auto resolution = std::bind(&cont_vector::resolve2, std::ref(*this),
                                std::ref(dep), p);
    contentious::tp.submitr(resolution, p);

    // TODO: erase splinters?
    //splinters.erase(sid);
    if (dep.rlatches.count(uid) > 0) {
        dep.rlatches[uid]->count_down();
    } else {
        std::cout << "didn't find latch" << std::endl;
    }
}

// TODO: multithreaded and sparse
template <typename T>
void cont_vector<T>::resolve(cont_vector<T> &dep)
{
    const int32_t uid = tracker[&dep]._used.get_id();
    dep.rlatches[uid]->wait();

    cont_vector<T> *curr = this;
    cont_vector<T> *next = &dep;
    auto &dep_tracker = curr->tracker[&dep];
    for (size_t i = 0; i < next->size(); ++i) {  // locked

        int index = dep_tracker.indexmap(i);
        if (index < 0 || index >= (int64_t)next->size()) { continue; }

        const auto &dep_op = dep_tracker.op;
        T diff = dep_op.inv((*curr)[i], dep_tracker._used[i]);
        if (diff != dep_op.identity) {
            next->_data.mut_set(index, dep_op.f((*next)[index], diff));
            /*
            std::cout << "sid " << _data.get_id() << " at " << i << "," << index
                << " resolves onto " << dep._data.get_id()
                << " with stale value " << curr->tracker[&dep]._used[i]
                << " and fresh value " << _data[i]
                << std::endl;
            std::cout << "_used for " << &dep << " -> " << uid << ": "
                << curr->tracker[&dep]._used << std::endl;
            std::cout << "next: "
                << next->_data << std::endl;
            */
            //curr->tracker[&dep]._used.mut_set(i, (*curr)[i]);
        }
    }

    for (auto &f : frozen[&dep]) {
        f->resolve(dep);
    }
}


/* used:
 * this
 *  tracker[&dep]
 *  _data, size()
 *  frozen
 * dep
 *  rlatches[uid]
 *  _data, size()
 */
// TODO: multithreaded and sparse
template <typename T>
void cont_vector<T>::resolve2(cont_vector<T> &dep, const uint16_t p)
{
    const int32_t uid = this->tracker[&dep]._used.get_id();
    dep.rlatches[uid]->wait();

    size_t a, b;
    std::tie(a, b) = contentious::partition(p, this->size());

    const auto &dep_tracker = this->tracker[&dep];
    const auto &dep_op = dep_tracker.op;

    int amap = dep_tracker.indexmap(a); int aoff = 0;
    if (a == 0) {
        b = dep._data.size();
    }
    if (amap < 0) {
        aoff = -1 * amap;
        amap = 0;
    }
    int bmap = dep_tracker.indexmap(b);
    if (bmap > (int64_t)dep._data.size()) {
        bmap = (int64_t)dep._data.size();
    }
    if (a != 0) {
        a = 0; amap = 0; aoff = 0; b = 0; bmap = 0;
    } else {
    }

    {
        std::lock_guard<std::mutex> lock(dep.dlck);
        auto curr = this->_data.cbegin() + a + aoff;
        auto trck = dep_tracker._used.cbegin() + a + aoff;
        auto end = dep._data.begin() + bmap;
        int count = 0;
        const int32_t did = dep._data.get_id();
        for (auto it = dep._data.begin() + amap; it != end;) {
            T diff = dep_op.inv(*curr, *trck);
            if (diff != dep_op.identity) {
                *it = dep_op.f(*it, diff);
            }
            if (dep._data.get_id() != did) {
                std::cout << "locked[" << &(dep.dlck) << "]BAD DEREF AHEAD: "
                          << did << " -> " << dep._data.get_id() << std::endl;
                exit(1);
            }
            ++it;
            ++curr;
            ++trck;
            ++count;
        }
    }

    if (this->frozen.count(&dep) > 0) {
        for (auto &f : this->frozen[&dep]) {
            f->resolve2(dep, p);
        }
    }
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
    for (int p = 0; p < hwconc; ++p) {
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

    freeze(dep, false, contentious::alltoone<0>, op);
    // no template parameters
    exec_par<>(contentious::reduce_splt<T>, dep);

    return dep;
}

template <typename T>
cont_vector<T> cont_vector<T>::foreach(const contentious::op<T> op, const T &val)
{
    auto dep = cont_vector<T>(*this);

    // template parameter is the arg to the foreach op
    freeze(dep, true, contentious::identity, op);
    exec_par<T>(contentious::foreach_splt<T>, dep, val);

    return dep;
}

// TODO: right now, this->()size must be equal to other.size()
template <typename T>
cont_vector<T> cont_vector<T>::foreach(const contentious::op<T> op,
                                       cont_vector<T> &other)
{
    auto dep = cont_vector<T>(*this);

    // gratuitous use of reference_wrapper
    freeze(dep, true, contentious::identity, op);
    other.freeze(*this, dep, contentious::identity);
    exec_par< std::reference_wrapper<cont_vector<T>> >(
            contentious::foreach_splt_cvec<T>, dep, std::ref(other));

    return dep;
}

template <typename T>
cont_vector<T> cont_vector<T>::stencil(const std::vector<int> &offs,
                                       const std::vector<T> &coeffs,
                                       const contentious::op<T> op1,
                                       const contentious::op<T> op2)
{
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
        freeze(step1.at(coeffs_unique[i]), true, contentious::identity, op1);
        exec_par<T>(contentious::foreach_splt<T>,
                    step1.at(coeffs_unique[i]),
                    coeffs_unique[i]);
    }
    /*
     * part 2
     */
    // sum up the different parts of the stencil
    // TODO: move inside for loop to correct semantics?
    std::vector<cont_vector<T>> step2;
    step2.reserve(offs.size()*2 + 1);
    step2.emplace_back(*this);
    for (size_t i = 0; i < offs.size(); ++i) {
        step2.emplace_back(step2[i]);
        step2[i].freeze(step2[i+1], true, contentious::identity, op2);
        if (i == 0) {
            step1.at(coeffs[i]).freeze(step2[i], step2[i+1], contentious::offset<-1>);
        } else {
            step1.at(coeffs[i]).freeze(step2[i], step2[i+1], contentious::offset<+1>);
        }
        step2[i].template exec_par<std::reference_wrapper<cont_vector<T>>>(
                contentious::foreach_splt_cvec<T>,
                step2[i+1], std::ref(step1.at(coeffs[i]))
        );
    }

    auto &thisthis = step2[step2.size()-1];
    std::map<T, cont_vector<T>> step11;
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        step11.emplace(std::piecewise_construct,
                       std::forward_as_tuple(coeffs_unique[i]),
                       std::forward_as_tuple(thisthis)
        );
        thisthis.freeze(step11.at(coeffs_unique[i]), true, contentious::identity, op1);
        thisthis.exec_par<T>(contentious::foreach_splt<T>,
                             step11.at(coeffs_unique[i]),
                             coeffs_unique[i]);
    }
    for (size_t i = offs.size(); i < offs.size()*2; ++i) {
        step2.emplace_back(step2[i]);
        step2[i].freeze(step2[i+1], true, contentious::identity, op2);
        if (i == offs.size()) {
            step11.at(coeffs[0]).freeze(step2[i], step2[i+1], contentious::offset<-1>);
        } else {
            step11.at(coeffs[1]).freeze(step2[i], step2[i+1], contentious::offset<+1>);
        }
        step2[i].template exec_par<std::reference_wrapper<cont_vector<T>>>(
                contentious::foreach_splt_cvec<T>,
                step2[i+1], std::ref(step11.at(coeffs[i-offs.size()]))
        );
    }

    contentious::tp.finish();

    //std::cout << "input: " << step2[0] << std::endl;
    // all the resolutions... *exhale*
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        //resolve(step1.at(coeffs_unique[i]));
        //std::cout << "coeff vec: " << step1.at(coeffs_unique[i]) << std::endl;
    }
    for (size_t i = 0; i < offs.size(); ++i) {
        //step2[i].resolve(step2[i+1]);
        //std::cout << "step2: " << step2[i+1] << std::endl;
    }

    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        //thisthis.resolve(step11.at(coeffs_unique[i]));
        //std::cout << "coeff vec: " << step11.at(coeffs_unique[i]) << std::endl;
    }
    for (size_t i = offs.size(); i < offs.size()*2; ++i) {
        //step2[i].resolve(step2[i+1]);
        //std::cout << "step2: " << step2[i+1] << std::endl;
    }
    return step2[step2.size()-1];
}

