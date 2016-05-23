
template <typename T>
void cont_vector<T>::freeze(cont_vector<T> &dep,
                            bool nocopy, uint16_t splinters,
                            std::function<int(int)> imap)
{   // locked
    std::lock_guard<std::mutex> lock(data_lock);
    {
        std::lock_guard<std::mutex> lock(dep.data_lock);
        //dependents.push_back(&dep);

        // we're going to splinter
        dep.unsplintered = false;
        // if we want our output to depend on input
        if (!nocopy) { dep._data = _data.new_id(); }
        // create _used, which has the old id of _data
        const auto dep_ptr = &dep;
        tracker.emplace(std::piecewise_construct,
                    std::forward_as_tuple(dep_ptr),
                    std::forward_as_tuple(dependency_tracker(_data, imap, op))
        );
        // modifications to data cannot affect _used
        _data = _data.new_id();
        dep.resolve_latch.emplace(std::piecewise_construct,
                    std::forward_as_tuple(tracker[dep_ptr]._used.get_id()),
                    std::forward_as_tuple(new boost::latch(splinters))
        );
        //std::cout << "this used: " << tracker[dep_ptr]._used << std::endl;
    }
}

template <typename T>
splt_vector<T> cont_vector<T>::detach(cont_vector &dep)
{
    {   // locked *this
        std::lock_guard<std::mutex> lock(data_lock);
        const auto dep_ptr = &dep;
        if (tracker.count(dep_ptr) > 0) {
            splt_vector<T> splt(tracker[dep_ptr]._used, tracker[dep_ptr].op);
            {   // locked dep
                std::lock_guard<std::mutex> lock2(dep.data_lock);
                dep.reattached.emplace(
                                std::make_pair(splt._data.get_id(),
                                false)
                );
            }
            return splt;
        } else {
            std::cout << "DETACH ERROR: NO DEP IN TRACKER: dep_ptr is "
                      << dep_ptr << std::endl;
            return splt_vector<T>(tracker[dep_ptr]._used, tracker[dep_ptr].op);
        }

    }
}

template <typename T>
void cont_vector<T>::reattach(splt_vector<T> &splt, cont_vector<T> &dep,
                              size_t a, size_t b)
{
    //std::chrono::time_point<std::chrono::system_clock> exec_start, exec_end;
    //exec_start = std::chrono::system_clock::now();

    // TODO: no check that other was actually detached from this
    // TODO: different behavior for other. don't want to loop over all
    // values for changes, there's too many and it ruins the complexity
    // TODO: generalize beyond addition, ya silly goose! don't forget
    // non-modification operations, like insert/remove/push_back/pop_back

    // OLD but leaving here for future reference
    // if we haven't yet started building a more current vector, make one
    // grow out from ourselves. this is safe to modify, but this isn't,
    // because others may be depending on it for resolution

    const int32_t sid = splt._data.get_id();
    // TODO: better (lock-free) mechanism here
    bool found = false;
    {   // locked *this
        std::lock_guard<std::mutex> lock(data_lock);
        found = dep.reattached.count(sid) > 0;
    }


    const auto dep_ptr = &dep;
    auto &dep_tracker = tracker[dep_ptr];
    const int32_t uid = dep_tracker._used.get_id();
    T diff;
    if (found) {
        if (true/*dep_tracker.indexmap == contentious::identity*/) {
            std::lock_guard<std::mutex> lock(dep.data_lock);
            dep._data.mut_set(a, splt._data[a]);
            uint16_t br_a;
            uint16_t br_b;
            std::tie(br_a, br_b) = dep_tracker._used.contained_by(a, b);
            if (b == splt._data.size()) br_b = 64;
            std::cout << br_a << " " << br_b << std::endl;
            //std::cout << splt._data << std::endl;
            auto splt_s = splt._data.branch_iterator() + br_a;
            auto splt_e = splt._data.branch_iterator() + br_b;
            auto join_s = dep._data.branch_iterator() + br_a;
            while (splt_s != splt_e) {
                std::cout << "*";
                boost::swap(*splt_s, *join_s);
                ++splt_s;
                ++join_s;
            }
            std::cout << std::endl;
        } else {

        for (size_t i = a; i < b; ++i) {    // locked dep
            std::lock_guard<std::mutex> lock(dep.data_lock);
            const auto &dep_op = dep_tracker.op;
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
            std::lock_guard<std::mutex>(dep.data_lock);
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
    //std::cout << "reattach bef" << std::endl;
    if (dep.resolve_latch.count(uid) > 0) {
        dep.resolve_latch[uid]->count_down();
    } else {
        std::cout << "didn't find latch" << std::endl;
    }
    //std::cout << "reattach aft" << std::endl;

    //exec_end = std::chrono::system_clock::now();
    //std::chrono::duration<double> exec_dur = exec_end - exec_start;
    //std::cout << "exec took: " << exec_dur.count() << " seconds; " << std::endl;
}

// TODO: add next parameter, make it work for multiple deps (or at all)
template <typename T>
void cont_vector<T>::resolve(cont_vector<T> &dep)
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();

    const auto dep_ptr = &dep;
    const int32_t uid = tracker[dep_ptr]._used.get_id();
    dep.resolve_latch[uid]->wait();
    
    cont_vector<T> *curr = this;
    cont_vector<T> *next = &dep;
    //for (auto next : curr->dependents) {
    auto &dep_tracker = curr->tracker[dep_ptr];
    for (size_t i = 0; i < next->size(); ++i) {  // locked
        std::lock_guard<std::mutex> lock(next->data_lock);

        int index = dep_tracker.indexmap(i);
        if (index < 0 || index >= (int64_t)next->size()) { continue; }

        const auto &dep_op = dep_tracker.op;
        T diff = dep_op.inv((*curr)[i], dep_tracker._used[i]);
        if (diff != dep_op.identity) {
            next->_data.mut_set(index, dep_op.f((*next)[index], diff));
            /*
            std::cout << "sid " << _data.get_id() << "at " << i
                << " resolves onto " << dep._data.get_id()
                << " with stale value " << curr->tracker[dep_ptr]._used[i]
                << " and fresh value " << _data[i]
                << std::endl;
            std::cout << "_used for " << dep_ptr << " -> " << uid << ": "
                << curr->tracker[dep_ptr]._used << std::endl;
            std::cout << "next: "
                << next->_data << std::endl;
            */
            //curr->tracker[dep_ptr]._used.mut_set(i, (*curr)[i]);
        }
    }
    //}
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> dur = end - start;
    std::cout << "resolving took: " << dur.count() << " seconds; " << std::endl;
    
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
}


template <typename T>
cont_vector<T> cont_vector<T>::reduce(const contentious::op<T> op)
{
    // our reduce dep is just one value
    this->op = op;
    auto dep = cont_vector<T>(op);
    dep.unprotected_push_back(op.identity);

    // no template parameters
    freeze(dep, true);
    exec_par<>(contentious::reduce_splt<T>, dep);

    return dep;
}

template <typename T>
cont_vector<T> cont_vector<T>::foreach(const contentious::op<T> op, const T &val)
{
    this->op = op;
    auto dep = cont_vector<T>(*this);

    // template parameter is the arg to the foreach op
    freeze(dep);
    exec_par<T>(contentious::foreach_splt<T>, dep, val);

    return dep;
}

// TODO: right now, this->()size must be equal to other.size()
template <typename T>
cont_vector<T> cont_vector<T>::foreach(const contentious::op<T> op,
                                       cont_vector<T> &other)
{
    this->op = op;
    auto dep = cont_vector<T>(*this);

    // gratuitous use of reference_wrapper
    freeze(dep);
    other.freeze(dep, true, 0);
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
    this->op = op1;
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        step1.emplace(std::piecewise_construct,
                        std::forward_as_tuple(coeffs_unique[i]),
                        std::forward_as_tuple(*this)
        );
        freeze(step1.at(coeffs_unique[i]));
        exec_par<T>(contentious::foreach_splt<T>,
                        step1.at(coeffs_unique[i]),
                        coeffs_unique[i]);
    }
    /*
     * part 2
     */
    // sum up the different parts of the stencil
    // TODO: move inside for loop to correct semantics?
    this->op = op2;

    std::vector<cont_vector<T>> step2;
    step2.reserve(offs.size() + 1);
    step2.emplace_back(*this);
    for (size_t i = 0; i < offs.size(); ++i) {
        step2.emplace_back(step2[i]);
        if (i == 0) {
            step1.at(coeffs[i]).freeze(step2[i+1], true, 0, contentious::offset<-1>);
        } else {
            step1.at(coeffs[i]).freeze(step2[i+1], true, 0, contentious::offset<1>);
        }
        step2[i].freeze(step2[i+1]);
        step2[i].template exec_par< std::reference_wrapper<cont_vector<T>>>(
                contentious::foreach_splt_cvec<T>,
                step2[i+1], std::ref(step1.at(coeffs[i]))
        );
    }

    // all the resolutions... *exhale*
    this->op = op1;
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        resolve(step1.at(coeffs_unique[i]));
        //std::cout << "coeff vec: " << step1.at(coeffs_unique[i]) << std::endl;
    }
    this->op = op2;
    for (size_t i = 0; i < offs.size(); ++i) {
        step2[i].resolve(step2[i+1]);
        step1.at(coeffs[i]).resolve(step2[i+1]);
        //std::cout << "step2: " << step2[i+1] << std::endl;
    }
    //step1.at(coeffs[offs.size()-1]).resolve(step2[offs.size()]);

    return step2[step2.size()-1];
}

