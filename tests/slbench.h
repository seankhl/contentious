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
using system_timepoint = std::chrono::time_point<std::chrono::system_clock>;
using steady_timepoint = std::chrono::time_point<std::chrono::steady_clock>;
using cpu_timepoint = std::clock_t;

inline cpu_timepoint cpu_clock_now()
{
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (ts.tv_sec * 1000000000 + ts.tv_nsec)*(period::den/1000000000);
}

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
    if (durs.size() == 0) { return duration{0}; }
    return std::accumulate(durs.begin(), durs.end(), duration{0})/durs.size();
}

inline duration min(std::vector<duration> durs)
{
    if (durs.size() == 0) { return duration{0}; }
    return *std::min_element(durs.begin(), durs.end());
}

inline duration max(std::vector<duration> durs)
{
    if (durs.size() == 0) { return duration{0}; }
    return *std::max_element(durs.begin(), durs.end());
}

inline duration variance(std::vector<duration> durs)
{
    if (durs.size() < 2) { return duration{0}; }
    duration avg = mean(durs);
    duration delsq{0};
    constexpr intmax_t kilo = 1000;
    size_t N = durs.size();
    for (size_t i = 0; i < N; ++i) {
        delsq += (durs[i] - avg) * (durs[i] - avg).count()/(period::den/kilo);
        //std::cout << std::chrono::duration<double, std::milli>(durs[i]).count() << " ";
    }
    std::cout << std::endl;
    //M2 += del * tmp.count()/(period::den/kilo);
    return delsq/(N-1);
}

}

enum struct outfmt
{
    console,
    csv
};

template <typename T = double>
struct data
{
    T res;
    std::vector<duration> durs;
    size_t its;
    std::vector<stat> stats;
    std::vector<std::string> statsn;
    outfmt ofmt;

    friend std::ostream &operator<<(std::ostream &out, const data<T> &data)
    {
        fmt::MemoryWriter w;
        if (data.ofmt == outfmt::console) {
            w.write("{:>24d}", data.its);
            for (const auto &stat: data.stats) {
                w.write("{:>24s}", durstr(stat(data.durs)));
            }
        } else {
            w.write("its:{}", data.its);
            int i = 0;
            for (const auto &stat: data.stats) {
                w.write(",{}:{}",
                        data.statsn[i++],
                        std::chrono::duration<double, std::milli>(
                            stat(data.durs)).count()
                );
            }
        }
        return out << w.c_str();
    }
};

template <typename T = steady_timepoint>
struct stopwatch
{
    T strt;
    std::vector<duration> vals;
    std::vector<uint64_t> data;

    inline void start(const T &start_in)
    {
        strt = start_in;
    }
    inline void add(const T &split)
    {
        vals.emplace_back(split - strt);
    }
    inline void add(const T &split, const uint64_t d)
    {
        vals.emplace_back(split - strt);
        data.emplace_back(d);
    }
    template <typename U>
    void add(const U &start, const U &end)
    {
        vals.emplace_back(end - start);
    }
    void log(const std::string &logname)
    {
        fmt::MemoryWriter w;
        w << "splits: ";
        for (const auto &split: vals) {
            w << ",";
            w << std::chrono::duration<double, std::milli>(split).count();
        }
        w << "\ndata: ";
        for (const  auto &d: data) {
            w << ",";
            w << d;
        }
        w << "\n";

        std::ofstream log;
        log.open(logname, std::ios_base::out | std::ios_base::app);
        log.write(w.c_str(), w.size());
        log.close();
    }
    
    friend std::ostream &operator<<(std::ostream &out, const stopwatch<T> &sw)
    {
        fmt::MemoryWriter w;
        w << "splits: ";
        for (const auto &split: sw.vals) {
            w << ",";
            w << std::chrono::duration<double, std::milli>(split).count();
        }
        w << "\ndata: ";
        for (const auto &d: sw.data) {
            w << ",";
            w << d;
        }
        w << "\n"; 
        return out << w.c_str();
    }
};

template <typename T>
inline data<double> compute_data(const stopwatch<T> &sw)
{
    std::vector<stat> statsv{};
    statsv.push_back(stats::mean);
    statsv.push_back(stats::min);
    statsv.push_back(stats::max);
    statsv.push_back(stats::variance);
    std::vector<std::string> statsn{ "avg", "min", "max", "var" };
    return data<double>{ 0, sw.vals, sw.vals.size(), 
                         statsv, statsn,
                         outfmt::console };
}

template <int N, typename T, typename ...Args>
data<T> run_bench(T (*F)(Args ...), Args ...args)
{
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    std::vector<duration> durs(N);
    T res{};
    std::cout << "[" << std::flush;
    for (int i = -1; i < N; ++i) {
        start = std::chrono::steady_clock::now();
        res = F(args...);
        end = std::chrono::steady_clock::now();
        if (i > -1) {
            durs[i] = end - start;
        }
        std::cout << "." << std::flush;
    }
    std::cout << "]" << std::endl;
    std::vector<stat> statsv{};
    statsv.push_back(stats::mean);
    statsv.push_back(stats::min);
    statsv.push_back(stats::max);
    statsv.push_back(stats::variance);
    std::vector<std::string> statsn{ "avg", "min", "max", "var" };
    return data<T>{ res, durs, N, statsv, statsn, outfmt::console };
}

template <int N, typename T, typename ...Args1, typename ...Args2>
auto make_bench(T (*F)(Args2...), Args1 &&...args)
-> decltype(std::bind(run_bench<N, T , Args2...>, F, args...))
{
    return std::bind(run_bench<N, T, Args2...>,
                     F, std::forward<Args1>(args)...);
}

template <typename T = double>
using suite =
    std::map< std::string,
              std::function<slbench::data<T> (void)>
            >;

template <typename T = double>
using output =
    std::map< std::string,
              slbench::data<T>
            >;

template <typename T = double>
output<T> run_suite(suite<T> &suite)
{
    output<T> output;
    for (const auto &test : suite) {
        std::cout << test.first << ": " << std::flush;
        output.emplace(test.first, test.second());
    }
    return output;
}

template <typename T = double>
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

template <typename T = double>
std::ostream &operator<<(std::ostream &out, const output<T> &output)
{
    fmt::MemoryWriter w;
    w.write("{:<24s}{:>24s}{:>24s}{:>24s}{:>24s}{:>24s}",
            "name", "iterations", "avg", "min", "max", "var");
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
