
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
    virtual T f(const T &lhs, const T &rhs) const
    {
        return lhs + rhs;
    }
    virtual T inv(const T &lhs, const T &rhs) const
    {
        return lhs - rhs;
    }
    virtual ~Plus() {}
};

/*
template <typename T>
class Splinter_Vec
{

public:
    tr_vector<T> data;
    const cont_vector<T> *origin;
    std::set<size_t> modified;
    
    Splinter_Vec(const cont_vector<T> *_origin, 
                 const Operator<T> *_op)
      : data(*_origin), op(_op), origin(_origin) {}
    
    Splinter_Vec(const tr_vector<T> &&_data, 
                 const Operator<T> *_op, 
                 const cont_vector<T> *_origin,
                 const std::set<size_t> &_modified)
      : data(std::move(_data)), op(_op), origin(_origin), modified(_modified) {}
    
    Splinter_Vec<T> set(const size_t i, const T &val)
    {
        modified.insert(i);
        return Splinter_Vec<T>(data.set(i, val), op, origin, modified);
    }

};
*/

template <typename T>
class cont_vector
{
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
    cont_record<T> record;
    std::map<std::thread::id, bool> resolved;
    std::mutex origin_lock;
    std::mutex record_lock;
    std::mutex unresolved_lock;

    std::map<std::thread::id, bool> prescient;
    cont_vector<T> *next;
    
    const Operator<T> *op;

    //void tick_w() { ++tick_w; }
    
public:
    cont_vector() = delete;
    cont_vector(Operator<T> *_op) : next(nullptr), op(_op) {}

    cont_vector(const cont_vector<T> &other)
      : origin(other.origin.new_id()), record(other.record), 
        resolved(other.resolved), op(other.op) 
    {
        // nothing to do here
    }
    
    T &at(size_t i)
    {
        return origin.at(i);
    }
    
    T &at_splinter(size_t i)
    {
        return record.splinters[std::this_thread::get_id()].at(i);
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
    void validate()
    {
        std::thread::id tid = std::this_thread::get_id();
        if (record.splinters.find(tid) == record.splinters.end()) {
            //std::cout << "make_trans in comp" << std::endl;
            record.splinters[tid] = origin.new_id();
            //std::cout << "splinter has id: " << record.splinters[tid].get_id() << std::endl;
        }
    }

    void comp(size_t i, const T &val)
    {
        /*
        if ((origin.get_ind_id(i)).compare_exchange_strong(-1, tid) ||
            origin.get_ind_id(i) == tid) {
            // nobody owns the node or we own the node
            unprotected_set(i, op->f(origin.at(i), val));
        } else {
        */
            // someone else onws the node
            // TODO: proper encapuslation here
            std::thread::id tid = std::this_thread::get_id();
            record.set(i, op->f(record.splinters[tid].at(i), val));
        /*}*/
    }

    void push()
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
        
        std::thread::id tid = std::this_thread::get_id();
        for (size_t i = 0; i < origin.size(); ++i) {
            // TODO: better (lock-free) mechanism here
            //const size_t &i = kv.first;
            //for (auto &j : kv.second) {
            //    if (j == std::this_thread::get_id()) {
            if (record.splinters[tid].at(i) != origin.at(i)) {
                std::lock_guard<std::mutex> lock(this->origin_lock);
                diff = 
                    op->inv(record.splinters[tid].at(i), origin.at(i));
                //std::cout << "resolving with diff " << diff << " at " << i
                //    << ", next->origin has size " << next->origin.size() << std::endl;
                next->origin = next->origin.set(i, op->f(next->origin.at(i), diff));
            //    }
            }
        }

        resolved[tid] = true;
        if (resolved.size() == NUM_THREADS) {
            //this = next;
        }

        //delete other.op;
    }

    inline void pull()
    {
        std::thread::id tid = std::this_thread::get_id();
        prescient[tid] = true;
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

