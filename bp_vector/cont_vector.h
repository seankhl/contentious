
#ifndef CONT_VECTOR_H
#define CONT_VECTOR_H

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

#include "bp_vector.h"

constexpr uint16_t NUM_THREADS = 4;


template <typename T>
class cont_vector;

template <typename T>
class cont_record
{
    friend class cont_vector<T>;
private:
    std::map<std::thread::id, tr_vector<T>> splinters;
    //std::map<size_t, std::set<std::thread::id>> changed;
    // a different approach/idea
    //const uint16_t MAX_SPLINTERS = 512;
    //array<bool, MAX_SPLINTERS> finalized;

public:
    cont_record() = default;

    inline void set(const size_t i, const T &val)
    {
        std::thread::id tid = std::this_thread::get_id();
        /*
        if (changed.find(i) == changed.end()) {
            changed[i] = std::set<std::thread::id>();
        }
        changed[i].insert(tid);
        */
        //std::cout << "in record set: " << val << std::endl;
        splinters[tid] = splinters[tid].set(i, val);
    }
};


template <typename T>
class Operator
{
public:
    virtual T f(const T &lhs, const T &rhs) const = 0;
    virtual T inv(const T &lhs, const T &rhs) const = 0;
    virtual ~Operator() {}
};

template <typename T>
class Plus : public Operator<T>
{
public:
    inline T f(const T &lhs, const T &rhs) const
    {
        return lhs + rhs;
    }
    inline  T inv(const T &lhs, const T &rhs) const
    {
        return lhs - rhs;
    }
    virtual ~Plus() {}
};


template <typename T>
class splt_vector 
{
private:
public:
    tr_vector<T> data;
    const Plus<T> *op;

    splt_vector(const cont_vector<T> &trunk)
      : data(trunk.origin.new_id()), op(trunk.op)
    {
        // nothing to do here
    }

    inline splt_vector<T> comp(const size_t i, const T &val)
    {
        data.mut_set(i, op->f(data.at(i), val));
        return *this;
    }

    inline void mut_comp(const size_t i, const T &val)
    {
        data.mut_set(i, data[i] + val);//op->f(data.at(i), val));
    }

};


template <typename T>
class cont_vector
{
    friend class splt_vector<T>;
private:
    // timestamps for reading and writing
    // if read <= write:
    //     no dependents
    // if read > write:
    //     register dependents
    //std::atomic<uint16_t> ts_r;
    //std::atomic<uint16_t> ts_w;

    //std::atomic<uint16_t> num_detached;
    tr_vector<T> origin;
    //cont_record<T> record;
    std::set<uint16_t> splinters;
    std::map<std::thread::id, bool> resolved;
    std::mutex origin_lock;
    std::mutex record_lock;
    std::mutex unresolved_lock;

    std::map<std::thread::id, bool> prescient;
    cont_vector<T> *next;
    
    const Plus<T> *op;

    //void tick_w() { ++tick_w; }
    
public:
    cont_vector() = delete;
    cont_vector(Plus<T> *_op) : next(nullptr), op(_op) {}

    cont_vector(const cont_vector<T> &other)
      : origin(other.origin.new_id()), resolved(other.resolved), op(other.op) 
    {
        // nothing to do here
    }
    
    T &at(size_t i)
    {
        return origin.at(i);
    }
    
    T &at_prescient(size_t i)
    {
        return next->at(i);
    }


    // internal set passthroughs
    inline void unprotected_set(const size_t i, const T &val)
    {
        origin.mut_set(i, val);
    }
    inline void unprotected_push_back(const T &val)
    {
        origin.mut_push_back(val);
    }
    

    // user can tick the read counter
    //void tick_r() { ++tick_r; }
    
    
    splt_vector<T> detach()
    {
        splt_vector<T> ret(*this);
        {
            std::lock_guard<std::mutex> lock(this->origin_lock);
            splinters.insert(ret.data.get_id());
        }
        return ret;
    }

    void push(splt_vector<T> &splinter)
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
        {
            std::lock_guard<std::mutex> lock(this->origin_lock);
            if (next == nullptr) {
                //std::cout << "making new next: " << std::endl;
                next = new cont_vector<T>(*this);
            }
        }
        
        const uint16_t sid = splinter.data.get_id();
        /*
        std::cout << "sid: " << sid << std::endl;
        for (auto it = splinters.begin(); it != splinters.end(); ++it) {
            std::cout << "splinters[i]: " << *it << std::endl;
        }
        */
        // TODO: better (lock-free) mechanism here
        if (splinters.find(sid) != splinters.end()) {
            for (size_t i = 0; i < splinter.data.size(); ++i) {
                std::lock_guard<std::mutex> lock(this->origin_lock);
                diff = op->inv(splinter.data.at(i), origin.at(i));
                //std::cout << "resolving with diff " << diff << " at " << i
                //    << ", next->origin has size " << next->origin.size() << std::endl;
                next->origin = next->origin.set(i, op->f(next->origin.at(i), diff));
            }
        }

        /*
        if (resolved.size() == NUM_THREADS) {
            this = next;
        }
        */

        //delete other.op;
    }

    inline void pull()
    {
    }

    inline void sync()
    {
        push(); 
        pull();
    }

    void print_unresolved_info()
    {
        /*
        for (auto i: unresolved) {
            std::cout << "index " << i.first << " changed by " << unresolved[i.first].size() 
                      << " parties" << std::endl;
            for (T j: i.second) {
                std::cout << j << " ";
            }
            std::cout << std::endl;
        }
        */
    }
    
    // TODO: do it concurrently
    void resolve()
    {
        /*
        std::thread::id tid = std::this_thread::get_id();
        for (auto i: unresolved) {
            size_t cutoff = i.second.size() / 2;
            T temp = i.second[tid];
            while (tid < cutoff) {
                temp += i.second[tid + cutoff];
                cutoff /= 2;
            }
            if (tid == 0) {
                (*this)[i.first] += temp;
            }
        }
        for (auto i: unresolved) {
            for (auto j : i.second) {
                (*this)[i.first] += j;
            }
        }
        */
    }
    
};


#endif  // CONT_VECTOR_H

