
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
        /*
        const uint16_t sid = splt._data.get_id();
        std::cout << std::endl << std::endl << sid << std::endl << std::endl;
        if (sid == 25) {
            std::this_thread::yield();
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(1s);
            std::this_thread::yield();
        }
        */
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

    friend class cont_vector<T>;

private:
    tr_vector<T> _data;
    const Operator<T> *op;

public:
    splt_vector() = delete;

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

    friend class splt_vector<T>;

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

    template <typename... U>
    void exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                         const size_t, const size_t, U...),
                  cont_vector<T> &cont, cont_vector<T> &dep, U... args);

public:
    cont_vector() = delete;
    cont_vector(Operator<T> *_op)
      : resolve_latch(4), op(_op)
    {   /* nothing to do here */ }

    cont_vector(const cont_vector<T> &other)
      : _data(other._data.new_id()), resolve_latch(4), op(other.op)
    {   /* nothing to do here */ }
    cont_vector(const std::vector<T> &other)
      : _data(other), resolve_latch(4), op(nullptr)
    {   /* nothing to do here */ }

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
                    //std::cout << "bool for " << it.first
                    //          << ": " << it.second << std::endl;
                    flag &= it.second;
                }
            }
        }
    }

    inline size_t size() const {  return _data.size(); }

    // internal passthroughs
    inline const T &operator[](size_t i) const {    return _data[i]; }
    inline T &operator[](size_t i) {                return _data[i]; }

    inline const T &at(size_t i) const {    return _data.at(i); }
    inline T &at(size_t i) {                return _data.at(i); }

    // TODO: probably need to remove this junk
    inline const T &at_prescient(size_t i) const {  return dependents[0]->at(i); }
    inline T &at_prescient(size_t i) {              return dependents[0]->at(i); }

    inline void unprotected_set(const size_t i, const T &val)
    {
        _data.mut_set(i, val);
    }
    inline void unprotected_push_back(const T &val)
    {
        _data.mut_push_back(val);
    }

    inline void reset_latch(const size_t count)
    {
        resolve_latch.reset(count);
    }
    inline void register_dependent(cont_vector<T> *dep)
    {
        {   // locked
            std::lock_guard<std::mutex> lock(data_lock);
            dependents.push_back(dep);
            //std::cout << "registered dependent: " << dep << std::endl;
        }
    }

    splt_vector<T> detach();
    void reattach(splt_vector<T> &splinter, cont_vector<T> &dep);
    void resolve(cont_vector<T> &dep);

    cont_vector<T> *reduce(Operator<T> *op);
    cont_vector<T> *foreach(const Operator<T> *op, const T val);
    cont_vector<T> foreach(const Operator<T> *op, const cont_vector<T> &other);
    cont_vector<T> stencil(const std::vector<int> &offs,
                           const std::vector<T> &coeffs,
                           const Operator<T> *op1 = new Multiply<T>,
                           const Operator<T> *op2 = new Plus<T>());

    /* obsolete
    inline cont_vector<T> pull()
    {
        resolve_latch.wait();
        return *next;
        delete other.op;
    }

    inline void sync()
    {
        reattach();
        pull();
    }
    */

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

#include "cont_vector-impl.h"

#endif  // CONT_VECTOR_H

