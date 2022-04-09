#include <cstring>
#include <random>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <atomic>
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

std::atomic<bool> flag_reorder;
class MergeTask : public Task {
    size_t start, mid, end;
public:
    MergeTask(size_t start, size_t mid, size_t end):
        start(start), mid(mid), end(end)
    {}
    void run(thread_num_t thread_id) override {
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
};

void sort() {
    constexpr thread_num_t K = 16; // the number of blocks the array is divided into
    static_assert(N % K == 0);
    static_assert(K % 2 == 0);
    constexpr size_t B = N / K;
    ThreadPool pool{std::make_unique<QueueScheduler>()};
    pool.create_worker_threads(K);
    TaskCollection sort_inside_block;
    for (thread_num_t i = 0; i < K; ++i) {
        data_t *addr = a + i * B;
        sort_inside_block.add_callable([addr](thread_num_t thread_id) { std::sort(addr, addr + B); });
    }
    pool.issue_tasks(std::move(sort_inside_block), true);
    while (true) {
        flag_reorder.store(false);
        TaskCollection tasks1, tasks2;
        for (thread_num_t i = 0; i < K; i += 2) {
            tasks1.add(std::make_shared<MergeTask>(i * B, i * B + B, i * B + 2 * B));
        }
        pool.issue_tasks(tasks1, true);
        for (thread_num_t i = 1; i + 1 < K; i += 2) {
            tasks2.add(std::make_shared<MergeTask>(i * B, i * B + B, i * B + 2 * B));
        }
        pool.issue_tasks(tasks2, true);
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
