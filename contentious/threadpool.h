#ifndef CONT_THREADPOOL_H
#define CONT_THREADPOOL_H

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
      : spin(true)
    {
        for (int p = 0; p < HWCONC; ++p) {
            tasks.emplace_back(folly::ProducerConsumerQueue<closure>(128));
            resns.emplace_back(folly::ProducerConsumerQueue<closure>(128));
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
    }

    inline void submitr(const closure &resn, int p)
    {
        while (!resns[p].write(resn)) {
            continue;
        }
        sems[p].post();
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

    std::vector<folly::ProducerConsumerQueue<closure>> tasks;
    std::vector<folly::ProducerConsumerQueue<closure>> resns;

    std::atomic<bool> spin;

    std::array<folly::LifoSem, HWCONC> sems;

    std::mutex fin_m;
    std::condition_variable fin_cv;

};

}

#endif
