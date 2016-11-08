#include "contentious.h"

namespace contentious {

void threadpool::finish()
{
    //std::cout << "finishing" << std::endl;
    std::unique_lock<std::mutex> lk(fin_m);
    fin_cv.wait(lk, [this] {
        bool ret = true;
        for (uint16_t p = 0; p < HWCONC; ++p) {
            ret &= (tasks[p].isEmpty() && resns.isEmpty());
        }
        return ret;
    });
    //std::cout << "finished!" << std::endl;
}

void threadpool::worker(int p)
{
    closure *task;
    closure *resn;
    for (;;) {
        // wait until we have something to do
        sems[p].wait();
        // threadpool is done
        if (!spin) {
            assert(tasks[p].isEmpty());
            assert(resns.isEmpty());
            break;
        }
        if (rlck.try_lock()) {
            if (resns.isEmpty()) {
                rlck.unlock();
            } else {
            //std::cout << "thread " << p << " took resolution" << std::endl;
            // must resolve
            resolver = p;
            resn = resns.frontPtr();
            assert(resn);
#ifdef CTTS_TIMING
            slbench::steady_timepoint splt_time;
            slbench::cpu_timepoint splt_start, splt_end;
            splt_time = std::chrono::steady_clock::now();
            splt_start = slbench::cpu_clock_now();
#endif
            (*resn)();
#ifdef CTTS_TIMING
            splt_end = slbench::cpu_clock_now();
            rslv_durs[p].add(splt_start, splt_end);
            rslv_series[p].add(splt_time, contentious::conflicted[p].back());
#endif
            resns.popFront();
            comp_resns++;
            rlck.unlock();
            for (int pi = 0; pi < HWCONC; ++pi) {
                sems[pi].post();
            }
            //std::cout << "thread " << p << " did resolution " << comp_resns << std::endl;
            continue;
            }
        }
        if (!tasks[p].isEmpty()) {
            // normal parallel processing
            task = tasks[p].frontPtr();
            assert(task);
#ifdef CTTS_TIMING
            slbench::cpu_timepoint splt_start, splt_end;
            splt_start = slbench::cpu_clock_now();
#endif
            (*task)();
#ifdef CTTS_TIMING
            splt_end = slbench::cpu_clock_now();
            splt_durs[p].add(splt_start, splt_end);
#endif
            tasks[p].popFront();
            comp_tasks[p]++;
            //std::cout << "thread " << p << " done " << comp_tasks[p] << " tasks" << std::endl;
        }
        fin_cv.notify_one();
    }
}

}
