# Examples on sorting

Here are some examples on how to use the thread pool to write parallel sorting algorithms. 

`parallel-merge.cpp`: It partitions the sequence to $K$ equal blocks, uses `std::sort` in each block, and merges in parallel.

`odd-even-sort.cpp`: It implements the [odd-even sort algorithm](https://en.wikipedia.org/wiki/Odd%E2%80%93even_sort).

`static-odd-even-sort.cpp`: It also implements the odd-even sort algorithm, but doesn't use the thread pool. It serves as a control to test the overhead of the thread pool.

`trivial-std-sort.cpp`: Just `std::sort`, no parallel. It's for verifying the correctness.

Basically you just need to look at the implementation of the function `sort` (and the things it uses, like `MergeTask` in `parallel-merge.cpp`) in each CPP file. The other parts are utilities for generating input, timing and verifying correctness of the result.