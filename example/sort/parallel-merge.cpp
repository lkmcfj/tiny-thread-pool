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
    constexpr thread_num_t K = 8; // the number of blocks the array is divided into
    static_assert(N % K == 0);
    ThreadPool pool{std::make_unique<QueueScheduler>()};
    pool.create_worker_threads(K);
    TaskCollection sort_inside_block;
    std::vector<size_t> partitions;
    for (thread_num_t i = 0; i < K; ++i) {
        data_t *block_addr = a + i * (N / K);
        partitions.push_back(i * (N / K));
        sort_inside_block.add_callable([block_addr](thread_num_t thread_id) { std::sort(block_addr, block_addr + N / K); });
    }
    pool.issue_tasks(std::move(sort_inside_block), true);
    while (partitions.size() > 1) {
        std::vector<size_t> merged_partitions;
        TaskCollection merge_tasks;
        for (size_t i = 0; i + 1 < partitions.size(); i += 2) {
            size_t end;
            if (i + 2 < partitions.size()) {
                end = partitions[i + 2];
            } else {
                end = N;
            }
            merge_tasks.add(std::make_shared<MergeTask>(partitions[i], partitions[i + 1], end));
            merged_partitions.push_back(partitions[i]);
        }
        if (partitions.size() % 2 == 1) {
            merged_partitions.push_back(partitions[partitions.size() - 1]);
        }
        partitions.swap(merged_partitions);
        pool.issue_tasks(std::move(merge_tasks), true);
    }
}

int main() {
    gen_input();
    timeit(sort);
    output_hash();
}
