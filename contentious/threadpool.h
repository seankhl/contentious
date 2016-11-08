#ifndef CONT_THREADPOOL_H
#define CONT_THREADPOOL_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "folly/ProducerConsumerQueue.h"
#include "folly/LifoSem.h"

#include "contentious_constants.h"

namespace contentious {

using closure = std::function<void (void)>;

class threadpool
{
public:
    threadpool()
      : resns(QUEUE_SZ), spin(true)
    {
        for (int p = 0; p < HWCONC; ++p) {
            tasks.emplace_back(folly::ProducerConsumerQueue<closure>(QUEUE_SZ));
            threads[p] = std::thread(&threadpool::worker, this, p);
        }
    }

    ~threadpool()
    {
        for (int p = 0; p < HWCONC; ++p) {
            threads[p].join();
        }
    }

    inline void submit(const closure &task, int p)
    {
        while (!tasks[p].write(task)) {
            continue;
        }
        sems[p].post();
        //std::cout << "submitted from " << p << std::endl;
    }

    inline void submitr(const closure &resn)
    {
        while (!resns.write(resn)) {
            continue;
        }
        for (int p = 0; p < HWCONC; ++p) {
            sems[p].post();
        }
    }

    void finish();

    inline void stop()
    {
        spin = false;
        for (int p = 0; p < HWCONC; ++p) {
            sems[p].post();
        }
    }

private:
    void worker(int p);

private:
    std::array<std::thread, HWCONC> threads;

    static constexpr uint16_t QUEUE_SZ = 512;
    std::vector<folly::ProducerConsumerQueue<closure>> tasks;
    folly::ProducerConsumerQueue<closure> resns;

    std::atomic<bool> spin;
    std::array<std::atomic<uint64_t>, contentious::HWCONC> comp_tasks{};
    std::atomic<uint64_t> comp_resns{0};

    std::array<folly::LifoSem, HWCONC> sems;

    std::mutex fin_m;
    std::condition_variable fin_cv;
    std::mutex rlck;

};

}

#endif
