#include <cstring>
#include <random>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

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

template <class F>
void join_threads(size_t K, F f) {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < K; ++i) {
        threads.emplace_back(f, i);
    }
    for (size_t i = 0; i < K; ++i) {
        threads[i].join();
    }
}

std::atomic<bool> flag_reorder;
void merge(size_t start, size_t mid, size_t end) {
    if (a[mid - 1] <= a[mid]) {
        return;
    }
    flag_reorder.store(true);
    memcpy(buf + start, a + start, (mid - start) * sizeof(data_t));
    size_t ptr1 = start, ptr2 = mid, ptr_dst = start;
    while (ptr1 < mid && ptr2 < end) {
        if (buf[ptr1] <= a[ptr2]) {
            a[ptr_dst++] = buf[ptr1++];
        } else {
            a[ptr_dst++] = a[ptr2++];
        }
    }
    if (ptr1 < mid) {
        memcpy(a + ptr_dst, buf + ptr1, (mid - ptr1) * sizeof(data_t));
    }
}

void sort() {
    constexpr size_t K = 16; // the number of blocks the array is divided into
    static_assert(N % K == 0);
    static_assert(K % 2 == 0);
    constexpr size_t B = N / K;
    join_threads(K, [](size_t i) { std::sort(a + B * i, a + B * i + B); });
    while (true) {
        flag_reorder.store(false);
        join_threads(K / 2, [](size_t i) { merge(i * B * 2, i * B * 2 + B, i * B * 2 + B * 2); });
        join_threads(K / 2 - 1, [](size_t i) { merge(i * B * 2 + B, i * B * 2 + B * 2, i * B * 2 + B * 3); });
        if (!flag_reorder.load()) {
            break;
        }
    }
}

int main() {
    gen_input();
    timeit(sort);
    output_hash();
}
