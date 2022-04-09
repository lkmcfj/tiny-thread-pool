#include <cstring>
#include <random>
#include <algorithm>
#include <chrono>
#include <iostream>
#include "threadpool.hpp"

using data_t = unsigned int;
constexpr size_t N = 100000000;
data_t a[N], buf[N];

void gen_input() {
    std::minstd_rand engine{20220409};
    for (auto &cur : a) {
        cur = static_cast<data_t>(engine());
    }
}

void output_hash() {
    data_t ans = 0;
    for (size_t i = 0; i < N; ++i) {
        ans += (static_cast<data_t>(i) + 1) * a[i];
    }
    std::cout << "hash value of result array: " << ans << '\n';
}

template <class F>
void timeit(F tested_func) {
    auto start = std::chrono::steady_clock::now();
    tested_func();
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> interval = end - start;
    std::cout << "time: " << interval.count() << "s\n";
}

void sort() {
    std::sort(a, a + N);
}

int main() {
    gen_input();
    timeit(sort);
    output_hash();
}
