
template <typename T>
void cont_vector<T>::freeze(cont_vector<T> &dep, bool nocopy)
{   // locked
    std::lock_guard<std::mutex> lock(data_lock);
    {
        std::lock_guard<std::mutex> lock(dep.data_lock);
        dependents.push_back(&dep);
        dep.unsplintered = false;
        _used = _data;
        _data = _data.new_id();
        if (!nocopy)
            dep._data = _used.new_id();
        resolve_latch.reset(hwconc);
    }
}

template <typename T>
splt_vector<T> cont_vector<T>::detach(cont_vector &dep)
{
    {   // locked *this
        std::lock_guard<std::mutex> lock(data_lock);
        splt_vector<T> splt(*this);
        splinters.insert(splt._data.get_id());
        {   // locked dep
            std::lock_guard<std::mutex> lock2(dep.data_lock);
            dep.resolved.emplace(
                    std::make_pair(splt._data.get_id(),
                                   false)
            );
        }
        return splt;
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

    const uint16_t sid = splt._data.get_id();

    // TODO: better (lock-free) mechanism here
    bool found = false;
    {   // locked *this
        std::lock_guard<std::mutex> lock(data_lock);
        found = splinters.find(sid) != splinters.end();
    }
    T diff;
    if (found) {
        for (size_t i = a; i < b; ++i) {    // locked dep
            std::lock_guard<std::mutex> lock(dep.data_lock);
            diff = op->inv(splt._data[i], _used[i]);
            if (diff != op->identity) {
                dep._data.mut_set(i, op->f(dep[i], diff));
                /*
                std::cout << "sid " << sid
                          << " joins with " << dep._data.get_id()
                          << " with diff " << diff
                          << " to produce " << dep[i]
                          << std::endl;
                */
            }
        }
        {   // locked dep
            std::lock_guard<std::mutex>(dep.data_lock);
            dep.resolved[sid] = true;
        }
    } else {
        std::cout << "TROUBLE with " << sid
                  << "!!! splinters.size(): " << splinters.size() << std::endl;
        std::cout << "the splinters are: ";
        for (const auto &it : splinters) {
            std::cout << it;
        }
        std::cout << std::endl;
    }

    // TODO: erase splinters?
    //splinters.erase(sid);
    resolve_latch.count_down();

    //exec_end = std::chrono::system_clock::now();
    //std::chrono::duration<double> exec_dur = exec_end - exec_start;
    //std::cout << "exec took: " << exec_dur.count() << " seconds; " << std::endl;
}

// TODO: add next parameter, make it work for multiple deps (or at all)
template <typename T>
void cont_vector<T>::resolve(cont_vector<T> &dep)
{
    resolve_latch.wait();

    cont_vector<T> *curr = this;
    cont_vector<T> *next = &dep;
    //for (auto next : curr->dependents) {
    for (size_t i = 0; i < next->size(); ++i) {  // locked
        std::lock_guard<std::mutex> lock(next->data_lock);
        T diff = next->op->inv((*curr)[i], curr->_used[i]);
        if (diff != op->identity) {
            next->_data.mut_set(i, next->op->f((*next)[i], diff));
            //curr->_used.mut_set(i, (*curr)[i]);
            /*
            std::cout << "sid " << _data.get_id()
                      << " resolves onto " << dep._data.get_id()
                      << " with diff " << diff
                      << " to produce " << (*next)[i]
                      << std::endl;
            */
        }
    }
    //}
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

    freeze(dep, true);
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
cont_vector<T> cont_vector<T>::reduce(const Operator<T> *op)
{
    // our reduce dep is just one value
    this->op = op;
    auto dep = cont_vector<T>(op);
    dep.unprotected_push_back(op->identity);

    // no template parameters
    exec_par<>(contentious::reduce_splt<T>, dep);

    return dep;
}

template <typename T>
cont_vector<T> cont_vector<T>::foreach(const Operator<T> *op, const T &val)
{
    this->op = op;
    auto dep = cont_vector<T>(*this);

    // template parameter is the arg to the foreach op
    exec_par<T>(contentious::foreach_splt<T>, dep, val);

    return dep;
}

// TODO: right now, this->()size must be equal to other.size()
template <typename T>
cont_vector<T> cont_vector<T>::foreach(const Operator<T> *op,
                                       const cont_vector<T> &other)
{
    this->op = op;
    auto dep = cont_vector<T>(*this);

    // gratuitous use of reference_wrapper
    exec_par< std::reference_wrapper<const cont_vector<T>> >(
            contentious::foreach_splt_cvec<T>, dep, std::cref(other));

    return dep;
}

template <typename T>
cont_vector<T> cont_vector<T>::stencil(const std::vector<int> &offs,
                                       const std::vector<T> &coeffs,
                                       const Operator<T> *op1,
                                       const Operator<T> *op2)
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
        // TODO: avoid copies here
        // we can't use foreach because the return value cannot be emplaced
        // directly into the map, which is kind of a problem
        auto copy = *this;
        step1.emplace(std::piecewise_construct,
                        std::forward_as_tuple(coeffs_unique[i]),
                        std::forward_as_tuple(*this)
        );
        copy.exec_par<T>(contentious::foreach_splt<T>,
                            step1.at(coeffs_unique[i]),
                            coeffs_unique[i]);
        // TODO: if *this could have multiple dependents, remove this
        copy.resolve(step1.at(coeffs_unique[i]));
    }

    /*
     * part 2
     */
    // sum up the different parts of the stencil
    // TODO: move inside for loop to correct semantics?
    this->op = op2;

    auto copy2a = *this;
    auto copy2b = *this;
    //step1a.register_dependent(&copy2b);
    //step1a.reset_latch(4);
    copy2a.exec_par< std::reference_wrapper<const cont_vector<T>>, int >(
                contentious::foreach_splt_off<T>,
                copy2b, std::cref(step1.at(coeffs[0])), offs[0]
    );

    auto dep = copy2b;
    //step1b.register_dependent(&dep);
    //step1b.reset_latch(4);
    copy2b.exec_par< std::reference_wrapper<const cont_vector<T>>, int >(
                contentious::foreach_splt_off<T>,
                dep, std::cref(step1.at(coeffs[1])), offs[1]
    );

    //step1a.resolve(copy2b);
    copy2a.resolve(copy2b);
    //step1b.resolve(dep);
    copy2b.resolve(dep);
    /*
    //std::cout << "after foreaches (dep): " << *dep << std::endl;
    for (size_t i = 0; i < offs.size(); ++i) {
        step1.at(coeffs[i]).register_dependent(&dep);
        //resolve_latch.reset(hwconc);
        std::cout << "butts" << std::endl;
        // TODO: fix suprious copy
        exec_par< std::reference_wrapper<const cont_vector<T>>, int >(
                    contentious::foreach_splt_off<T>,
                    dep, std::cref(step1.at(coeffs[i])), offs[i]
        );
        std::cout << "butts" << std::endl;
    }
    */

    return dep;
}

