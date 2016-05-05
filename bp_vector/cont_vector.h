
#ifndef CONT_VECTOR_H
#define CONT_VECTOR_H

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

#include <boost/thread/latch.hpp>

#include "bp_vector.h"


template <typename T>
class splt_vector;
template <typename T>
class cont_vector;


namespace contentious
{
    template <typename T>
    void reduce_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                     size_t a, size_t b)
    {
        //chrono::time_point<chrono::system_clock> splt_start, splt_end;
        //splt_start = chrono::system_clock::now();

        splt_vector<T> splt = cont.detach();
        for (size_t i = a; i < b; ++i) {
            // TODO: right now, reduces happen at index 0, which probably isn't
            // exactly right
            splt.mut_comp(0, splt._data[i]);
        }

        //splt_end = chrono::system_clock::now();
        //chrono::duration<double> splt_dur = splt_end - splt_start;
        //cout << "splt took: " << splt_dur.count() << " seconds; " << endl;
        //std::cout << "one cont_inc done: " << splt_ret._data[0] << std::endl;

        cont.reattach(splt, dep);
    }

    template <typename T>
    void foreach_splt(cont_vector<T> &cont, cont_vector<T> &dep,
                      const size_t a, const size_t b,
                      const T val)
    {
        splt_vector<T> splt = cont.detach();
        /* TODO: iterators, or at least all leaves at a time
        auto it_chunk_begin = splt_ret.begin() + chunk_sz * (i);
        auto it_chunk_end = splt_ret.begin() + chunk_sz * (i+1);
        for (auto it = it_chunk_begin; it != it_chunk_end; ++it) {
            splt_ret.mut_comp(*it, );
        }
        */
        for (size_t i = a; i < b; ++i) {
            splt.mut_comp(i, val);
        }
        cont.reattach(splt, dep);
    }

    template <typename T>
    void foreach_splt_cvec(cont_vector<T> &cont, cont_vector<T> &dep,
                           const size_t a, const size_t b,
                           const cont_vector<T> &other)
    {
        splt_vector<T> splt = cont.detach();
        // TODO: see other foreach
        for (size_t i = a; i < b; ++i) {
            splt.mut_comp(i, other[i]);
        }
        cont.reattach(splt, dep);
    }

    template <typename T>
    void foreach_splt_off(cont_vector<T> &cont, cont_vector<T> &dep,
                          const size_t a, const size_t b,
                          const cont_vector<T> &other, const int off)
    {
        size_t start = 0;
        size_t end = cont.size();
        if (off < 0) { start -= off; }
        else if (off > 0) { assert(end >= (size_t)off); end -= off; }
        start = std::max(start, a);
        end = std::min(end, b);

        splt_vector<T> splt = cont.detach();
        // TODO: see other foreach
        for (size_t i = start; i < end; ++i) {
            splt.mut_comp(i, other[i + off]);
        }
        cont.reattach(splt, dep);
    }

}   // end namespace contentious


template <typename T>
class Operator
{
public:
    T identity;
    Operator(T identity_in)
      : identity(identity_in) {}
    virtual T f(const T &lhs, const T &rhs) const = 0;
    virtual T inv(const T &lhs, const T &rhs) const = 0;
    virtual ~Operator() {}
};

template <typename T>
class Plus : public Operator<T>
{
public:
    Plus()
      : Operator<T>(0) {}
    inline T f(const T &lhs, const T &rhs) const
    {
        return lhs + rhs;
    }
    inline T inv(const T &lhs, const T &rhs) const
    {
        return lhs - rhs;
    }
    virtual ~Plus() {}
};

template <typename T>
class Multiply : public Operator<T>
{
public:
    Multiply()
      : Operator<T>(1) {}
    inline T f(const T &lhs, const T &rhs) const
    {
        return lhs * rhs;
    }
    inline T inv(const T &lhs, const T &rhs) const
    {
        return lhs / rhs;
    }
    virtual ~Multiply() {}
};


template <typename T>
class splt_vector
{
private:

public:
    tr_vector<T> _data;
    const Operator<T> *op;

    splt_vector()
      : op(nullptr)
    {   /* nothing to do here */ }

    splt_vector(const cont_vector<T> &trunk)
      : _data(trunk._used.new_id()), op(trunk.op)
    {   /* nothing to do here */ }

    inline splt_vector<T> comp(const size_t i, const T &val)
    {
        _data.mut_set(i, op->f(_data.at(i), val));
        return *this;
    }

    inline void mut_comp(const size_t i, const T &val)
    {
        _data.mut_set(i, op->f(_data[i], val));
    }

};


template <typename T>
class cont_vector
{
    friend class splt_vector<T>;

    friend void contentious::reduce_splt<T>(
            cont_vector<T> &, cont_vector<T> &, size_t, size_t);

    friend void contentious::foreach_splt<T>(
            cont_vector<T> &, cont_vector<T> &,
            const size_t, const size_t, const T);

    friend void contentious::foreach_splt_cvec<T>(
            cont_vector<T> &, cont_vector<T> &,
            const size_t, const size_t,
            const cont_vector<T> &);

    friend void contentious::foreach_splt_off<T>(
            cont_vector<T> &, cont_vector<T> &,
            const size_t, const size_t,
            const cont_vector<T> &, const int);

private:
    tr_vector<T> _data;
    tr_vector<T> _used;

    std::set<uint16_t> splinters;
    std::map<uint16_t, bool> resolved;

    std::mutex data_lock;
    boost::latch resolve_latch;

    std::vector<cont_vector<T> *> dependents;

    // TODO: op in cont? stencil? both? what's the deal
    //       requires determining if a single cont can have multiple ops
    //       (other than stencils, which as for now have exactly 2)
    const Operator<T> *op;

    // TODO: timestamps for reading and writing?
    // if read <= write:
    //     no dependents
    // if read > write:
    //     register dependents
    //std::atomic<uint16_t> ts_r;
    //std::atomic<uint16_t> ts_w;

    // user can tick the read counter (obsolete)
    //void tick_r() { ++tick_r; }

    // tids to lookahead if prescient (obsolete)
    //std::map<std::thread::id, bool> prescient;

public:
    cont_vector() = delete;
    cont_vector(Operator<T> *_op)
      : resolve_latch(4), op(_op)
    {
        // nothing to do here
    }
    cont_vector(const cont_vector<T> &other)
      : _data(other._data.new_id()), resolve_latch(4), op(other.op)
    {
        // nothing to do here
    }
    cont_vector(const std::vector<T> &other)
      : _data(other), resolve_latch(4), op(nullptr)
    {
        // nothing to do here
    }

    ~cont_vector()
    {
        resolve_latch.wait();
        if (dependents.size() > 0) {
            assert(dependents.size() == 1);
            bool flag = false;
            while (!flag) {
                std::lock_guard<std::mutex>(this->data_lock);
                flag = true;
                std::cout << "spinning... " << std::endl;
                for (const auto &it : dependents[0]->resolved) {
                    //std::cout << "bool for " << it.first << ": " << it.second << std::endl;
                    flag &= it.second;
                }
            }
        }
    }

    inline const T &operator[](size_t i) const {    return _data[i]; }
    inline T &operator[](size_t i) {                return _data[i]; }

    inline const T &at(size_t i) const {    return _data.at(i); }
    inline T &at(size_t i) {                return _data.at(i); }

    inline const T &at_prescient(size_t i) const {  return dependents[0]->at(i); }
    inline T &at_prescient(size_t i) {              return dependents[0]->at(i); }

    inline size_t size() const {  return _data.size(); }

    // internal set passthroughs
    inline void unprotected_set(const size_t i, const T &val)
    {
        _data.mut_set(i, val);
    }
    inline void unprotected_push_back(const T &val)
    {
        _data.mut_push_back(val);
    }


    inline void reset_latch(const size_t count) {   resolve_latch.reset(count); }

    splt_vector<T> detach()
    {
        splt_vector<T> ret;
        {   // locked
            std::lock_guard<std::mutex> lock(this->data_lock);
            if (splinters.size() == 0) {
                _used = _data;
                _data = _data.new_id();
                dependents[0]->_data = _used.new_id();
                //std::cout << "_used in detach: " << _used << std::endl;

            }
            ret = *this;
            splinters.insert(ret._data.get_id());
            dependents[0]->resolved.emplace(std::make_pair(ret._data.get_id(), false));
        }
        return ret;
    }


    /* TODO: add next parameter */
    void reattach(splt_vector<T> &splinter, cont_vector<T> &dep)
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
                std::cout << "sid " << sid
                          << " resolving with diff " << diff << " at " << i
                          << ", dep._data has size " << dep._data.size() << std::endl;
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
    void resolve(cont_vector<T> &dep)
    {
        resolve_latch.wait();
        if (resolve_latch.try_wait()) {
            cont_vector<T> *curr = this;
            auto next = &dep;
            //for (auto next : curr->dependents) {
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

    /* obsolete
    inline cont_vector<T> pull()
    {
        resolve_latch.wait();
        return *next;
        //delete other.op;
    }

    inline void sync()
    {
        reattach();
        pull();
    }
    */

    void register_dependent(cont_vector<T> *dep)
    {
        {   // locked
            std::lock_guard<std::mutex> lock(data_lock);
            dependents.push_back(dep);
            //std::cout << "registered dependent: " << dep << std::endl;
        }
    }

    /* this function performs thread function f on the passed cont_vector
     * between size_t (f's second arg) and size_t (f's third arg)
     * with extra args U... as necessary */
    template <typename... U>
    void exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                         const size_t, const size_t, U...),
                  cont_vector<T> &cont, cont_vector<T> &dep, U... args)
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
            cont_threads[i].join();
        }
    }


    cont_vector<T> reduce(Operator<T> *op)
    {
        // our reduce dep is just one value
        auto dep = new cont_vector<T>(op);
        dep->unprotected_push_back(op->identity);
        register_dependent(dep);
        // no template parameters
        this->op = op;
        exec_par<>(contentious::reduce_splt<T>, *this, *dep);
        return *dep;
    }

    cont_vector<T> foreach(const Operator<T> *op, const T val)
    {
        auto dep = new cont_vector<T>(*this);
        register_dependent(dep);
        // template parameter is the arg to the foreach op
        // TODO: why cannot put const T?
        this->op = op;
        exec_par<T>(contentious::foreach_splt<T>, *this, *dep, val);

        return *dep;
    }

    // TODO: right now, this->()size must be equal to other.size()
    cont_vector<T> foreach(const Operator<T> *op, const cont_vector<T> &other)
    {
        auto dep = new cont_vector<T>(*this);
        register_dependent(dep);
        // template parameter is the arg to the foreach op
        this->op = op;
        exec_par<const cont_vector<T> &>(
                contentious::foreach_splt_cvec<T>, *this, *dep, other);

        return *dep;
    }


    cont_vector<T> stencil(const std::vector<int> &offs,
                           const std::vector<T> &coeffs,
                           const Operator<T> *op1 = new Multiply<T>,
                           const Operator<T> *op2 = new Plus<T>())
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
                            this->foreach(op1, coeffs_unique[i])
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


    void print_unresolved_info()
    {
        /*
        for (auto i: unresolved) {
            std::cout << "index " << i.first
                      << " changed by " << unresolved[i.first].size()
                      << " parties" << std::endl;
            for (T j: i.second) {
                std::cout << j << " ";
            }
            std::cout << std::endl;
        }
        */
    }

    friend std::ostream &operator<<(std::ostream &out,
                                    const cont_vector<T> &cont)
    {
        out << "cont_vector{" << std::endl;
        out << "  data: " << cont._data << std::endl;
        out << "  used: " << cont._used << std::endl;
        out << "  splt: ";
        for (const auto &i : cont.splinters) { out << i << " "; }
        out << std::endl << "}/cont_vector" << std::endl;
        return out;
    }

};


#endif  // CONT_VECTOR_H

