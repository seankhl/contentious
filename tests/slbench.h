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

static std::string durstr(std::chrono::duration<int64_t, period> dur)
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

enum struct outfmt
{
    console,
    csv
};

template <typename T>
struct data
{
    T res;
    size_t its;
    std::chrono::duration<int64_t, period> avg;
    std::chrono::duration<int64_t, period> min;
    std::chrono::duration<int64_t, period> max;
    outfmt ofmt;

    friend std::ostream &operator<<(std::ostream &out, const data<T> &data)
    {
        fmt::MemoryWriter w;
        if (data.ofmt == outfmt::console) {
            w.write("{:>24d}{:>s}{:>s}{:>s}",
                    data.its,
                    durstr(data.avg),
                    durstr(data.min),
                    durstr(data.max));
        } else {
            w.write("its:{},avg:{},min:{},max:{}",
                    data.its,
                    std::chrono::duration<double, std::milli>(data.avg).count(),
                    std::chrono::duration<double, std::milli>(data.min).count(),
                    std::chrono::duration<double, std::milli>(data.max).count());
        }
        return out << w.c_str();
    }
};

template <int N, typename T, typename ...Args>
data<T> run_bench(T (*F)(Args ...), Args ...args)
{
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    std::array<std::chrono::duration<int64_t, period>, N> durs;
    T res;
    for (int i = 0; i < N; ++i) {
        start = std::chrono::steady_clock::now();
        res = F(args...);
        end = std::chrono::steady_clock::now();
        durs[i] = end - start;
    }
    auto minmax_it = std::minmax_element(durs.begin(), durs.end());
    auto sum = std::accumulate(durs.begin(), durs.end(),
                               std::chrono::duration<int64_t, period>{0});
    return data<T>{ res, N, sum/N, *minmax_it.first, *minmax_it.second,
                    outfmt::console };
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

}

#endif
