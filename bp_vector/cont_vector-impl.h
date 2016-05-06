
/*
#include "cont_vector.h"

template <typename T>
void cont_vector<T>::get(const size_t i, const uint16_t ts, const const T &out) {
    out = PS_Trie<T>::get(i);
    if (ts > ts_w) {
        dependencies[i].push_back(&T);
    }
}

template <typename T>
void cont_vector<T>::set(const size_t i, const T &val) {
    PS_Trie<T>::set(i, val);
    for (dep : dependencies[i]) {
        resolve(dep, val);
    }
}
*/

template <typename T>
splt_vector<T> cont_vector<T>::detach()
{   // locked
    std::lock_guard<std::mutex> lock(this->data_lock);
    if (splinters.size() == 0) {
        _used = _data;
        _data = _data.new_id();
        dependents[0]->_data = _used.new_id();
        //std::cout << "_used in detach: " << _used << std::endl;
    }
    splt_vector<T> ret(*this);
    splinters.insert(ret._data.get_id());
    dependents[0]->resolved.emplace(
            std::make_pair(ret._data.get_id(), false));
    return ret;
}

/* TODO: add next parameter */
template <typename T>
void cont_vector<T>::reattach(splt_vector<T> &splinter, cont_vector<T> &dep)
{
    //--num_detached;
    // TODO: no check that other was actually detached from this
    // TODO: different behavior for other. don't want to loop over all
    // values for changes, there's too many and it ruins the complexity
    // TODO: generalize beyond addition, ya silly goose! don't forget
    // non-modification operations, like insert/remove/push_back/pop_back
    T diff;

    // if we haven't yet started building a more current vector, make one
    // grow out from ourselves. this is safe to modify, but this isn't,
    // because others may be depending on it for resolution
    /*
    auto dep = new cont_vector<T>(*this);
    {   // locked
        std::lock_guard<std::mutex> lock(this->data_lock);
        dependents.push_back(dep);
        std::cout << "making new dep: " << std::endl;
    }
    */

    const uint16_t sid = splinter._data.get_id();

    /*
    std::cout << "sid: " << sid << std::endl;
    for (auto it = splinters.begin(); it != splinters.end(); ++it) {
        std::cout << "splinters[i]: " << *it << std::endl;
    }
    */

    // TODO: better (lock-free) mechanism here
    if (splinters.find(sid) != splinters.end()) {
        for (size_t i = 0; i < dep.size(); ++i) { // locked
            std::lock_guard<std::mutex> lock(dep.data_lock);
            diff = op->inv(splinter._data[i], _used[i]);
            /*
            if (diff == 2) {
                std::cout << "sid " << sid
                << " resolving with diff " << diff << " at " << i
                << ", dep._data has size " << dep._data.size() << std::endl;
            }
            */

            dep._data = dep._data.set(i, op->f(dep[i], diff));
        }
        {   //locked
            std::lock_guard<std::mutex>(dep.data_lock);
            dep.resolved[sid] = true;
        }
        /*
        for (const auto &it : dep.resolved) {
            std::cout << "bool for " << it.first << ": " << it.second << std::endl;
        }
        */
    }

    //splinters.erase(sid);
    //std::cout << "counting down for " << sid << std::endl;
    resolve_latch.count_down();

}
    
// TODO: add next parameter, make it work for multiple deps (or at all)
template <typename T>
void cont_vector<T>::resolve(cont_vector<T> &dep)
{
    resolve_latch.wait();
    if (true) {
        cont_vector<T> *curr = this;
        auto next = &dep;
        //for (auto next : curr->dependents) {
        for (size_t i = 0; i < next->size(); ++i) {  // locked
            std::lock_guard<std::mutex> lock(next->data_lock);
            T diff = next->op->inv(curr->at(i),
                    curr->_used.at(i));
            if (diff > 0) {
                //std::cout << "resolving forward with diff: " << diff << std::endl;
            }
            next->_data = next->_data.set(
                    i, next->op->f(next->at(i), diff));
            curr->_used = curr->_used.set(i, curr->at(i));
        }
        //}
        /*
           cont_vector<T> *next;
           while (curr->dependents.size() > 0) {
           next = curr->dependents[0];
           for (size_t i = 0; i < next->size(); ++i) {  // locked
           std::lock_guard<std::mutex> lock(next->data_lock);
           T diff = next->op->inv(curr->at(i),
           curr->_used.at(i));
           if (diff > 0) {
           std::cout << "resolving forward with diff: " << diff << std::endl;
           }
           next->_data = next->_data.set(
           i, next->op->f(next->at(i), diff));
           curr->_used = curr->_used.set(i, curr->at(i));
           }
           curr = next;
           }
           */
    }
}


/* this function performs thread function f on the passed cont_vector
 * between size_t (f's second arg) and size_t (f's third arg)
 * with extra args U... as necessary */
template <typename T>
template <typename... U>
void cont_vector<T>::exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                                     const size_t, const size_t, U...),
                              cont_vector<T> &cont, cont_vector<T> &dep, 
                              U... args)
{
    int num_threads = std::thread::hardware_concurrency(); // * 16;
    size_t chunk_sz = (this->size()/num_threads) + 1;
    std::vector<std::thread> cont_threads;
    for (int i = 0; i < num_threads; ++i) {
        cont_threads.push_back(
                std::thread(f,
                    std::ref(cont), std::ref(dep),
                    chunk_sz * i, std::min(chunk_sz * (i+1), this->size()),
                    args...));
    }
    for (int i = 0; i < num_threads; ++i) {
        cont_threads[i].detach();
    }
}


template <typename T>
cont_vector<T> *cont_vector<T>::reduce(Operator<T> *op)
{
    // our reduce dep is just one value
    auto dep = new cont_vector<T>(op);
    dep->unprotected_push_back(op->identity);
    register_dependent(dep);
    // no template parameters
    this->op = op;
    exec_par<>(contentious::reduce_splt<T>, *this, *dep);
    return dep;
}

template <typename T>
cont_vector<T> *cont_vector<T>::foreach(const Operator<T> *op, const T val)
{
    auto dep = new cont_vector<T>(*this);
    register_dependent(dep);
    // template parameter is the arg to the foreach op
    // TODO: why cannot put const T?
    this->op = op;
    exec_par<T>(contentious::foreach_splt<T>, *this, *dep, val);
    //std::cout << *dep << std::endl;
    return dep;
}

// TODO: right now, this->()size must be equal to other.size()
template <typename T>
cont_vector<T> cont_vector<T>::foreach(const Operator<T> *op,
                                       const cont_vector<T> &other)
{
    auto dep = new cont_vector<T>(*this);
    register_dependent(dep);
    // template parameter is the arg to the foreach op
    this->op = op;
    exec_par<const cont_vector<T> &>(
            contentious::foreach_splt_cvec<T>, *this, *dep, other);

    return *dep;
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

    resolve_latch.reset((offs.size() + coeffs_unique.size()) * \
            std::thread::hardware_concurrency());

    // perform coefficient multiplications on original vector
    // TODO: limit range of multiplications to only those necessary
    std::map<T, cont_vector<T>> step1;
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        step1.emplace(std::make_pair(
                    coeffs_unique[i],
                    *(this->foreach(op1, coeffs_unique[i]))
                    ));
        // TODO: keep track of nexts so they can be resolved
        // (done inside foreach I believe...)
        //assert(resolve_latch.try_wait());
        //resolve_latch.wait();
        //resolve_latch.reset(std::thread::hardware_concurrency());
    }
    std::cout << "after foreaches (this): " << *this << std::endl;

    /*
     * part 2
     */
    // sum up the different parts of the stencil
    // TODO: move inside for loop to correct semantics?
    auto dep = new cont_vector<T>(*this);
    register_dependent(dep);
    this->op = op2;
    std::cout << "after foreaches (dep): " << *dep << std::endl;
    for (size_t i = 0; i < offs.size(); ++i) {
        //resolve_latch.reset(std::thread::hardware_concurrency());
        exec_par<const cont_vector<T> &, int>(
                contentious::foreach_splt_off<T>,
                *this, *dep, step1.at(coeffs[i]), offs[i]
                );
    }
    /*
    // TODO: parallelize this
    // TODO: emplace?
    std::vector<splt_vector<T>> splts;
    for (size_t i = 0; i < offs.size(); ++i) {
    splts.push_back(this->detach());
    splt_vector<T> &splt = splts[splts.size()-1];
    for (size_t j = start; j < end; ++j) {
    splt.mut_comp(j, step1.at(coeffs[i])[j+offs[i]]);
    }
    std::cout << "after sum " << i << ": " << *this << std::endl;
    }
    for (size_t i = 0; i < offs.size(); ++i) {
    this->reattach(splts[i], );
    }
    */
    //assert(resolve_latch.try_wait());
    //resolve_latch.wait();

    return *dep;
}

