#ifndef CONT_SLBENCH_H
#define CONT_SLBENCH_H

#include <utility>
#include <iostream>
#include <sstream>
#include <chrono>
#include <functional>
#include <algorithm>
#include <numeric>
#include <vector>
#include <map>

#include "fmt/format.h"

namespace slbench {

static std::string durstr(std::chrono::duration<double> dur)
{
    using namespace std::chrono;
    if (dur.count() < 0.1) {
        return fmt::format("{:>12.3f}ms",
                           static_cast<double>(duration_cast<microseconds>(dur).count())/1000);
    } else {
        return fmt::format("{:>12.3f}s ",
                           dur.count());
    }
}

template <typename T>
struct data
{
    T res;
    size_t its;
    std::chrono::duration<double> avg;
    std::chrono::duration<double> min;
    std::chrono::duration<double> max;

    friend std::ostream &operator<<(std::ostream &out, const data<T> &data)
    {
        fmt::MemoryWriter w;
        w.write("{:>24d}{:>16s}{:>14s}{:>14s}",
                data.its, durstr(data.avg), durstr(data.min), durstr(data.max));
        return out << w.c_str();
    }
};

template <int N, typename T, typename ...Args>
data<T> run_bench(T (*F)(Args ...), Args ...args)
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
    std::array<std::chrono::duration<double>, N> durs;
    T res;
    for (int i = 0; i < N; ++i) {
        start = std::chrono::system_clock::now();
        res = F(args...);
        end = std::chrono::system_clock::now();
        durs[i] = end - start;
    }
    auto minmax_it = std::minmax_element(durs.begin(), durs.end());
    auto sum = std::accumulate(durs.begin(), durs.end(),
                               std::chrono::duration<double>{0});
    return data<T>{ res, N, sum/N, *minmax_it.first, *minmax_it.second };
}

template <int N, typename T, typename ...Args1, typename ...Args2>
auto make_bench(T (*F)(Args2...), Args1 &&...args)
-> decltype(std::bind(run_bench<N, T , Args2...>, F, args...))
{
    return std::bind(run_bench<N, T, Args2...>, F, std::forward<Args1>(args)...);
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
    for (auto &test : suite) {
        output.emplace(test.first, test.second());
    }
    return output;
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
