#ifndef CONT_SLBENCH_H
#define CONT_SLBENCH_H

#include <utility>
#include <iostream>
#include <sstream>
#include <fstream>
#include <ios>
#include <chrono>
#include <functional>
#include <algorithm>
#include <numeric>
#include <vector>
#include <map>

#include "fmt/format.h"

namespace slbench {

using period = std::chrono::steady_clock::period;
using duration = std::chrono::duration<int64_t, period>;

static std::string durstr(duration dur)
{
    using namespace std::chrono;
    constexpr intmax_t kilo = 1000;
    constexpr intmax_t mega = 1000000;
    constexpr intmax_t giga = 1000000000;
    {
        using namespace fmt::literals;
        static_assert(period::num == 1 && period::den % 1000 == 0,
                      "Steady clock is too coarse or has weird ratio");
    }
    if (dur.count() >= period::den) {
        return fmt::format("{:>14.3f}s ",
                           static_cast<double>(dur.count())/(period::den/1));
    } else if (period::den >= kilo && dur.count() >= period::den/kilo) {
        return fmt::format("{:>14.3f}ms",
                           static_cast<double>(dur.count())/(period::den/kilo));
    } else if (period::den >= mega && dur.count() >= period::den/mega) {
        return fmt::format(" {:>13.3f}\xC2\xB5s",
                           static_cast<double>(dur.count())/(period::den/mega));
    } else if (period::den >= giga && dur.count() >= period::den/giga) {
        return fmt::format("{:>14.3f}ns",
                           static_cast<double>(dur.count())/(period::den/giga));
    } else {
        return fmt::format("{:>14.3f}s ",
                           static_cast<double>(dur.count())/(period::den/1));
    }
}

using stat = std::function<duration (std::vector<duration>)>;

namespace stats {

inline duration sum(std::vector<duration> durs)
{
    return std::accumulate(durs.begin(), durs.end(), duration{0});
}

inline duration mean(std::vector<duration> durs)
{
    return std::accumulate(durs.begin(), durs.end(), duration{0})/durs.size();
}

inline duration min(std::vector<duration> durs)
{
    return *std::min_element(durs.begin(), durs.end());
}

inline duration max(std::vector<duration> durs)
{
    return *std::max_element(durs.begin(), durs.end());
}

/*
double variance(std::vector<duration> durs)
{
    duration avg{0};
    duration del{0};
    duration tmp{0};
    double M2{0};
    size_t N = durs.size();
    for (int i = 0; i < N; ++i) {
        del = durs[i] - avg;
        avg += del/N;
        tmp = durs[i] - avg;
        M2 += del.count() * tmp.count();
    }
    return M2/(N-1);
}
*/

}

enum struct outfmt
{
    console,
    csv
};

template <typename T>
struct data
{
    T res;
    std::vector<duration> durs;
    size_t its;
    std::vector<stat> stats;
    outfmt ofmt;

    friend std::ostream &operator<<(std::ostream &out, const data<T> &data)
    {
        fmt::MemoryWriter w;
        if (data.ofmt == outfmt::console) {
            w.write("{:>24d}{:>s}{:>s}{:>s}",
                    data.its,
                    durstr(data.stats[0](data.durs)),
                    durstr(data.stats[1](data.durs)),
                    durstr(data.stats[2](data.durs)));
        } else {
            w.write("its:{},avg:{},min:{},max:{}",
                    data.its,
                    std::chrono::duration<double, std::milli>(data.stats[0](data.durs)).count(),
                    std::chrono::duration<double, std::milli>(data.stats[1](data.durs)).count(),
                    std::chrono::duration<double, std::milli>(data.stats[2](data.durs)).count());
        }
        return out << w.c_str();
    }
};

struct stopwatch
{
    std::vector<duration> durs;

    void add(const duration &dur)
    {
        durs.push_back(dur);
    }
    template <typename U>
    void add(const U &start, const U &end)
    {
        durs.emplace_back(end - start);
    }
};

inline data<double> compute_data(const stopwatch &sw)
{
    std::vector<stat> statsv{};
    statsv.push_back(stats::sum);
    statsv.push_back(stats::mean);
    statsv.push_back(stats::min);
    statsv.push_back(stats::max);
    return data<double>{ 0, sw.durs, sw.durs.size(), statsv, outfmt::console };
}

template <int N, typename T, typename ...Args>
data<T> run_bench(T (*F)(Args ...), Args ...args)
{
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    std::vector<duration> durs(N);
    T res{};
    for (int i = 0; i < N; ++i) {
        start = std::chrono::steady_clock::now();
        res = F(args...);
        end = std::chrono::steady_clock::now();
        durs[i] = end - start;
    }
    std::vector<stat> statsv{};
    statsv.push_back(stats::mean);
    statsv.push_back(stats::min);
    statsv.push_back(stats::max);
    return data<T>{ res, durs, N, statsv, outfmt::console };
}

template <int N, typename T, typename ...Args1, typename ...Args2>
auto make_bench(T (*F)(Args2...), Args1 &&...args)
-> decltype(std::bind(run_bench<N, T , Args2...>, F, args...))
{
    return std::bind(run_bench<N, T, Args2...>,
                     F, std::forward<Args1>(args)...);
}

template <typename T>
using suite =
    std::map< std::string,
              std::function<slbench::data<T> (void)>
            >;

template <typename T>
using output =
    std::map< std::string,
              slbench::data<T>
            >;

template <typename T>
output<T> run_suite(suite<T> &suite)
{
    output<T> output;
    for (const auto &test : suite) {
        output.emplace(test.first, test.second());
    }
    return output;
}

template <typename T>
void log_output(const std::string &logname, output<T> &output) {
    fmt::MemoryWriter w;
    for (auto &o : output) {
        auto oldfmt = o.second.ofmt;
        std::stringstream temp;
        o.second.ofmt = outfmt::csv;
        temp << o.second;
        w.write("name:{},{}\n", o.first, temp.str());
        o.second.ofmt = oldfmt;
    }
    std::ofstream log;
    log.open(logname, std::ios_base::out | std::ios_base::app);
    log.write(w.c_str(), w.size());
    log.close();
}

template <typename T>
std::ostream &operator<<(std::ostream &out, const output<T> &output)
{
    fmt::MemoryWriter w;
    w.write("{:<24s}{:>24s}{:>14s}{:>14s}{:>14s}",
            "name", "iterations", "avg", "min", "max");
    for (const auto &o : output) {
        std::stringstream temp;
        temp << o.second;
        w.write("\n{:<24s}{:^s}", o.first, temp.str());
    }
    out << w.c_str();
    return out;
}

}   // namespace slbench

#endif
