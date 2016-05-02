
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

constexpr uint16_t NUM_THREADS = 4;


template <typename T>
class cont_vector;
template <typename T>
class splt_vector;


namespace contentious 
{
    template <typename T>
    void foreach_splt(cont_vector<T> &cont, size_t a, size_t b, T val)
    {
        //chrono::time_point<chrono::system_clock> splt_start, splt_end;
        //splt_start = chrono::system_clock::now();
        
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
        cont.join(splt);
    }
    
    template <typename T>
    void reduce_splt(cont_vector<T> &cont, size_t a, size_t b)
    {
        //chrono::time_point<chrono::system_clock> splt_start, splt_end;
        //splt_start = chrono::system_clock::now();
        
        splt_vector<T> splt = cont.detach();
        for (size_t i = a; i < b; ++i) {
            // TODO: right now, reduces happen at index 0, which probably isn't
            // exactly right
            splt.mut_comp(0, splt.data[i]);
        }
        
        //splt_end = chrono::system_clock::now();
        //chrono::duration<double> splt_dur = splt_end - splt_start;
        //cout << "splt took: " << splt_dur.count() << " seconds; " << endl;
        //std::cout << "one cont_inc done: " << splt_ret.data.at(0) << std::endl;
        
        cont.join(splt);
    }

}


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
    tr_vector<T> data;
    const Operator<T> *op;

    splt_vector(const cont_vector<T> &trunk)
      : data(trunk.origin.new_id()), op(trunk.op)
    {   /* nothing to do here */ }

    inline splt_vector<T> comp(const size_t i, const T &val)
    {
        data.mut_set(i, op->f(data.at(i), val));
        return *this;
    }

    inline void mut_comp(const size_t i, const T &val)
    {
        data.mut_set(i, op->f(data[i],val));//op->f(data.at(i), val));
    }

};


template <typename T>
class cont_vector
{
    friend class splt_vector<T>;
    friend void contentious::foreach_splt<T>(cont_vector<T> &, size_t, size_t, T);
    friend void contentious::reduce_splt<T>(cont_vector<T> &, size_t, size_t);
private:
    // TODO: timestamps for reading and writing?
    // if read <= write:
    //     no dependents
    // if read > write:
    //     register dependents
    //std::atomic<uint16_t> ts_r;
    //std::atomic<uint16_t> ts_w;

    tr_vector<T> origin;
    
    std::set<uint16_t> splinters;
    std::map<uint16_t, bool> resolved;
    
    std::mutex origin_lock;
    boost::latch resolve_latch;

    cont_vector<T> *next;
    std::map<std::thread::id, bool> prescient;

    
    const Operator<T> *op;
    
public:
    cont_vector() = delete;
    cont_vector(Operator<T> *_op) 
      : resolve_latch(4), next(nullptr), op(_op) {}

    cont_vector(const cont_vector<T> &other)
      : origin(other.origin.new_id()), resolved(other.resolved), 
        resolve_latch(4), next(nullptr), op(other.op) 
    {
        // nothing to do here
    }

    cont_vector(const std::vector<T> &other)
      : origin(other), resolve_latch(4), next(nullptr), op(nullptr) {}
    
    inline const T &operator[](size_t i) const {    return origin[i]; }
    inline T &operator[](size_t i) {                return origin[i]; }
    
    inline const T &at(size_t i) const {    return origin.at(i); }
    inline T &at(size_t i) {                return origin.at(i); }
    
    inline const T &at_prescient(size_t i) const {  return next->at(i); }
    inline T &at_prescient(size_t i) {              return next->at(i); }

    inline size_t size() const {  return origin.size(); }

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

    /*
    void cont_inc(cont_vector<double> &cont_ret,
            vector<double>::const_iterator a, vector<double>::const_iterator b)
    {
        //chrono::time_point<chrono::system_clock> splt_start, splt_end;
        splt_vector<double> splt_ret = cont_ret.detach();
        double temp(0);
        //splt_start = chrono::system_clock::now();
        for (auto it = a; it != b; ++it) {
            temp += *it;
        }
        splt_ret.mut_comp(0, temp);
        cont_ret.join(splt_ret);
    }
    split_vector<T> map(function f)
    {
        cont_vector<double> cont_ret(new Plus<double>());
        std::vector<thread> cont_threads;
        int num_threads = thread::hardware_concurrency();
        size_t chunk_sz = test_vec.size()/num_threads;
        for (int i = 0; i < num_threads; ++i) {
            cont_threads.push_back(
                    thread(cont_inc, 
                        std::ref(cont_ret), 
                        test_vec.begin() + chunk_sz * i,
                        test_vec.begin() + chunk_sz * (i+1)));
        }
        for (int i = 0; i < num_threads; ++i) {
            cont_threads[i].join();
        }
        return cont_ret.at_prescient(0);
    }
    */
    void join(splt_vector<T> &splinter)
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
            for (size_t i = 0; i < next->size(); ++i) {
                std::lock_guard<std::mutex> lock(this->origin_lock);
                diff = op->inv(splinter.data.at(i), origin.at(i));
                std::cout << "resolving with diff " << diff << " at " << i
                          << ", next->origin has size " << next->origin.size() << std::endl;
                next->origin = next->origin.set(i, op->f(next->origin.at(i), diff));
            }
        }
        resolve_latch.count_down();

    }

    inline cont_vector<T> pull()
    {
        resolve_latch.wait();
        return *next;
        //delete other.op;
    }

    inline void sync()
    {
        join(); 
        pull();
    }

    cont_vector<T> reduce(Operator<T> *op)
    {
		{	// locked
			std::lock_guard<std::mutex> lock(origin_lock);
            this->op = op;
			if (next == nullptr) {
				//std::cout << "making new next: " << std::endl;
				next = new cont_vector<T>(op);
                next->unprotected_push_back(op->identity);
			}
		}
        std::vector<std::thread> cont_threads;
        int num_threads = std::thread::hardware_concurrency(); // * 16;
        size_t chunk_sz = this->size()/num_threads + 1;
        for (int i = 0; i < num_threads; ++i) {
            cont_threads.push_back(
              std::thread(contentious::reduce_splt<T>, 
                     std::ref(*this), 
                     chunk_sz * i,
                     std::min(chunk_sz * (i+1), this->size())
              )
            );
        }
        for (int i = 0; i < num_threads; ++i) {
            cont_threads[i].join();
        }
        return *next;
    }

	cont_vector<T> foreach(Operator<T> *op, T val)
	{
		{	// locked
			std::lock_guard<std::mutex> lock(origin_lock);
            this->op = op;
			if (next == nullptr) {
				//std::cout << "making new next: " << std::endl;
				next = new cont_vector<T>(*this);
			}
		}
        std::vector<std::thread> cont_threads;
		int num_threads = std::thread::hardware_concurrency(); // * 16;
		size_t chunk_sz = (this->size()/num_threads) + 1;
		for (int i = 0; i < num_threads; ++i) {
			cont_threads.push_back(
			  std::thread(contentious::foreach_splt<T>,
                     std::ref(*this),
					 chunk_sz * i,
					 std::min(chunk_sz * (i+1), this->size()),
                     val
              )
            );
		}
		for (int i = 0; i < num_threads; ++i) {
			cont_threads[i].join();
		}
		return *next;
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
    
};


#endif  // CONT_VECTOR_H

