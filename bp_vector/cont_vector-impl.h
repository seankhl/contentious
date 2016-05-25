
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
        if (onto) { dep._data = _data.new_id(); }
        // create _used, which has the old id of _data
        tracker.emplace(std::piecewise_construct,
                        std::forward_as_tuple(&dep),
                        std::forward_as_tuple(dependency_tracker(_data, imap, op))
        );
        // modifications to data cannot affect _used
        _data = _data.new_id();
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
                              size_t a, size_t b)
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

        if (false) {/*contentious::getAddress(dep_tracker.indexmap) ==
            contentious::getAddress(std::function<int(int)>(contentious::identity)) &&
            dep._data.size() >= br_sz * contentious::hwconc) {*/
            std::lock_guard<std::mutex> lock(dep.dlck);
            dep._data.mut_set(a, splt._data[a]);
            uint16_t br_a;
            uint16_t br_b;
            std::tie(br_a, br_b) = dep_tracker._used.contained_by(a, b-1);
            //std::cout << br_a << " " << br_b << std::endl;
            auto splt_br_it = splt._data.branch_iterator() + br_a;
            auto splt_br_end = splt._data.branch_iterator() + br_b;
            auto dep_br_it = dep._data.branch_iterator() + br_a;
            while (splt_br_it != splt_br_end) {
                boost::swap(*dep_br_it, *splt_br_it);
                ++splt_br_it;
                ++dep_br_it;
            }

        } else {

        for (size_t i = a; i < b; ++i) {    // locked dep
            const auto &dep_op = dep_tracker.op;
            diff = dep_op.inv(splt._data[i], dep_tracker._used[i]);
            if (diff != dep_op.identity) {
                {
                    std::lock_guard<std::mutex> lock(dep.dlck);
                    dep._data.mut_set(i, dep_op.f(dep[i], diff));
                }
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
        for (const auto &p : dep.reattached) {
            std::cout << p.first;
        }
        std::cout << std::endl;
    }

    // TODO: erase splinters?
    //splinters.erase(sid);
    if (dep.rlatches.count(uid) > 0) {
        dep.rlatches[uid]->count_down();
    } else {
        std::cout << "didn't find latch" << std::endl;
    }
}

// TODO: multithreaded
template <typename T>
void cont_vector<T>::resolve(cont_vector<T> &dep)
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();

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
    //}

    for (auto &f : frozen[&dep]) {
        f->resolve(dep);
    }

    end = std::chrono::system_clock::now();
    std::chrono::duration<double> dur = end - start;
    //std::cout << "resolving took: " << dur.count() << " seconds; " << std::endl;
}


/* this function performs thread function f on the passed cont_vector
 * between size_t (f's second arg) and size_t (f's third arg)
 * with extra args U... as necessary */
template <typename T>
template <typename... U>
void cont_vector<T>::exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                                     const size_t, const size_t, const U &...),
                              cont_vector<T> &dep, const U &... args)
{
    size_t chunk_sz = std::ceil( ((double)size())/hwconc );
    /*
    std::vector<std::thread> cont_threads;

    for (int i = 0; i < hwconc; ++i) {
        cont_threads.push_back(
                std::thread(f,
                    std::ref(*this), std::ref(dep),
                    chunk_sz * i, std::min(chunk_sz * (i+1), size()),
                    args...));
    }
    for (int i = 0; i < hwconc; ++i) {
        cont_threads[i].detach();
    }
    */
    for (int i = 0; i < hwconc; ++i) {
        auto task = std::bind(f, std::ref(*this), std::ref(dep),
                              chunk_sz * i, std::min(chunk_sz * (i+1), size()),
                              args...);
        contentious::tp.submit(task, i);
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
    step2.reserve(offs.size() + 1);
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

    // all the resolutions... *exhale*
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        resolve(step1.at(coeffs_unique[i]));
        //std::cout << "coeff vec: " << step1.at(coeffs_unique[i]) << std::endl;
    }
    for (size_t i = 0; i < offs.size(); ++i) {
        step2[i].resolve(step2[i+1]);
        //std::cout << "step2: " << step2[i+1] << std::endl;
    }
    //step1.at(coeffs[offs.size()-1]).resolve(step2[offs.size()]);

    return step2[step2.size()-1];
}

