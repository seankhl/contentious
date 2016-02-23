
#ifndef PS_CONT_VEC
#define PS_CONT_VEC

#include <vector>
#include <set>
#include <unordered_map>
#include <atomic>
#include <mutex>

#include "trie.h"

template <typename T>
class Cont_Vec;

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
    PS_Trie<T> data;
    const Operator<T> *op;
    const Cont_Vec<T> *origin;
    std::set<size_t> modified;
    
    Splinter_Vec(const Cont_Vec<T> *_origin, 
                 const Operator<T> *_op)
      : data(*_origin), op(_op), origin(_origin) {}
    
    Splinter_Vec(const PS_Trie<T> &&_data, 
                 const Operator<T> *_op, 
                 const Cont_Vec<T> *_origin,
                 const std::set<size_t> &_modified)
      : data(std::move(_data)), op(_op), origin(_origin), modified(_modified) {}
    
    Splinter_Vec<T> pers_set(const size_t i, const T &val)
    {
        modified.insert(i);
        return Splinter_Vec<T>(data.pers_set(i, val), op, origin, modified);
    }

    Splinter_Vec<T> comp(size_t i, const T &val)
    {
        return pers_set(i, op->f(data.at(i), val));
    }
    

};


template <typename T>
class Cont_Vec : public PS_Trie<T>
{

private:
    // timestamps for reading and writing
    // if read <= write:
    //     no dependents
    // if read > write:
    //     register dependents
    std::atomic<uint16_t> ts_r;
    std::atomic<uint16_t> ts_w;

    std::atomic<uint16_t> num_detached;

    // each index can have a vector of dependents
    std::mutex map_lock;
    std::unordered_map<int, std::vector<T>> unresolved;
    void tick_w() { ++tick_w; }

public:
    // user can tick the read counter
    void tick_r() { ++tick_r; }

    void set(const size_t i, const T &val);
    
    Splinter_Vec<T> detach(const Operator<T> *op)
    { 
        ++num_detached;
        return Splinter_Vec<T>(this,op); 
    }

    void reattach(Splinter_Vec<T> &other)
    {
        --num_detached;
        // TODO: no check that other was actually detached from this
        // TODO: different behavior for other. don't want to loop over all
        // values for changes, there's too many and it ruins the complexity
        // TODO: generalize beyond addition, ya silly goose! don't forget
        // non-modification operations, like insert/remove/push_back/pop_back
        T diff;
        for (size_t i : other.modified) {
            diff = other.op->inv(other.data.at(i), PS_Trie<T>::at(i));
            if (diff != 0) {
                map_lock.lock();
                std::cout << "index i: " << i << " marked as modified" << std::endl;
                unresolved[i].push_back(diff);
                map_lock.unlock();
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
    }
    
};

#endif // PS_CONT_VEC

