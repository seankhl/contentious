
#include <utility>
#include <chrono>
/*
template<typename Sig, Sig &S>
struct wrapper;

template<typename Ret, typename ...Args, Ret(&P)(Args...)>
struct wrapper<Ret(Args...), P>
{
    static int apply(Args... args)
    {
        Ret result = P(args...);
        return result;
    }
};

double mymult(double a, double b) {
    return a * b;
}
template<, typename ...Args>
double call_with_args(double F(ArgsArgs... args) {
    return wrapper<double F(Args...), F>::apply(args...);
}
*/

template <typename T, typename ...Args>
std::pair<T, std::chrono::duration<double>>
timetest(T F(const Args &...), const Args &...args)
{
    std::chrono::time_point<std::chrono::system_clock> start, end;
    start = std::chrono::system_clock::now();
    T ret = 0;
    for (int i = 0; i < 1; ++i) {
        ret = F(args...);
    }
    end = std::chrono::system_clock::now();
    return std::make_pair(ret, end - start);
}

