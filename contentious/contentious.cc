#include "threadpool.h"
#include "contentious.h"

namespace contentious {

#ifdef CTTS_STATS
std::array<std::vector<uint64_t>, contentious::HWCONC> conflicted{};
uint16_t resolver{};
#endif
#ifdef CTTS_TIMING
std::array<slbench::stopwatch<slbench::cpu_timepoint>, contentious::HWCONC> splt_durs{};
std::array<slbench::stopwatch<slbench::cpu_timepoint>, contentious::HWCONC> rslv_durs{};
std::array<slbench::stopwatch<>, contentious::HWCONC> rslv_series{};
#endif
std::mutex plck;

threadpool tp;

std::pair<int64_t, int64_t> safe_mapping(const imap_fp &imap, size_t i,
                                         int64_t lo, int64_t hi)
{
    int64_t o;
    int64_t imapped = imap(i);
    o = i - imapped;
    int64_t idom = i + o;
    int64_t iran = i;
    if (idom < lo) {
        iran += (lo - idom);
        idom += (lo - idom);
    }
    if (idom >= hi) {
        iran -= (idom - hi);
        idom -= (idom - hi);
    }
    return std::make_pair(idom, iran);
}

}
