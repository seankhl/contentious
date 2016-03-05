
#ifndef CONT_VECTOR_H
#define CONT_VECTOR_H

#include <vector>
#include <set>
#include <unordered_map>
#include <atomic>
#include <mutex>

#include "bp_vector.h"

template <typename T>
class cont_vector;

template <typename T>
class Operator
{
public:
    virtual T f(const T &lhs, const T &rhs) const = 0;
    virtual T inv(const T &lhs, const T &rhs) const = 0;
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
};

template <typename T>
class Splinter_Vec
{

public:
    tr_vector<T> data;
    const Operator<T> *op;
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

    Splinter_Vec<T> comp(size_t i, const T &val)
    {
        return set(i, op->f(data.at(i), val));
    }

};


template <typename T>
class cont_vector : public bp_vector<T>
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

    // each index can have a vector of dependents
    std::mutex unresolved_lock;
    std::unordered_map<int, std::vector<T>> unresolved;
    //void tick_w() { ++tick_w; }

public:
    // user can tick the read counter
    //void tick_r() { ++tick_r; }

    //void set(const size_t i, const T &val);
    
    Splinter_Vec<T> detach(const Operator<T> *op)
    { 
        //++num_detached;
        return Splinter_Vec<T>(this, op); 
    }

    /*
    void mut_set(const size_t i, const T &val)
    {
        *this = bp_vector<T>::set(i, val);
    }
    void mut_push_back(const T &val)
    {
        *this = bp_vector<T>::push_back(val);
    }
    */

    void reattach(Splinter_Vec<T> &other)
    {
        //--num_detached;
        // TODO: no check that other was actually detached from this
        // TODO: different behavior for other. don't want to loop over all
        // values for changes, there's too many and it ruins the complexity
        // TODO: generalize beyond addition, ya silly goose! don't forget
        // non-modification operations, like insert/remove/push_back/pop_back
        T diff;
        for (size_t i : other.modified) {
            diff = other.op->inv(other.data.at(i), bp_vector<T>::at(i));
            if (diff != 0) {
                std::lock_guard<std::mutex> lock(unresolved_lock);
                //std::cout << "index i: " << i << " marked as modified" << std::endl;
                unresolved[i].push_back(diff);
            }
        }
        //other.reset();
    }

    void print_unresolved_info()
    {
        for (auto i: unresolved) {
            std::cout << "index " << i.first << " changed by " << unresolved[i.first].size() 
                      << " parties" << std::endl;
            for (T j: i.second) {
                std::cout << j << " ";
            }
            std::cout << std::endl;
        }
    }
    
    // TODO: do it concurrently
    void resolve()
    {
        /*
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
        */
        for (auto i: unresolved) {
            for (auto j : i.second) {
                (*this)[i.first] += j;
            }
        }
    }
    
};


#endif  // CONT_VECTOR_H

