# Pattern-defeating quicksort

[![Build Status](https://travis-ci.org/stjepang/pdqsort.svg?branch=master)](https://travis-ci.org/stjepang/pdqsort)
[![License](https://img.shields.io/badge/license-Apache--2.0%2FMIT-blue.svg)](https://github.com/stjepang/pdqsort)
[![Cargo](https://img.shields.io/crates/v/pdqsort.svg)](https://crates.io/crates/pdqsort)
[![Documentation](https://docs.rs/pdqsort/badge.svg)](https://docs.rs/pdqsort)

This sort is significantly faster than the standard sort in Rust. In particular, it sorts
random arrays of integers approximately 45% faster. The key drawback is that it is an unstable
sort (i.e. may reorder equal elements). However, in most cases stability doesn't matter anyway.

The algorithm is based on pdqsort by Orson Peters, published at: https://github.com/orlp/pdqsort

#### Properties

* Best-case running time is `O(n)`.
* Worst-case running time is `O(n log n)`.
* Unstable, i.e. may reorder equal elements.
* Does not allocate additional memory.
* Uses `#![no_std]`.

#### Examples

```rust
extern crate pdqsort;

let mut v = [-5i32, 4, 1, -3, 2];

pdqsort::sort(&mut v);
assert!(v == [-5, -3, 1, 2, 4]);

pdqsort::sort_by(&mut v, |a, b| b.cmp(a));
assert!(v == [4, 2, 1, -3, -5]);

pdqsort::sort_by_key(&mut v, |k| k.abs());
assert!(v == [1, 2, -3, 4, -5]);
```

#### A simple benchmark

Sorting 10 million random numbers of type `u64`:

| Sort              | Time       |
|-------------------|-----------:|
| pdqsort           | **370 ms** |
| slice::sort       |     668 ms |
| [quickersort][qs] |     777 ms |
| [dmsort][ds]      |     728 ms |
| [rdxsort][rs]     |     602 ms |

#### Extensive benchmarks

The following benchmarks are [used in Rust][bench] for testing the performance of slice::sort.

*Note: The numbers for dmsort are outdated.*

| Benchmark               | pdqsort       | slice::sort   | [quickersort][qs] | ~~[dmsort][ds]~~ | [rdxsort][rs] |
|-------------------------|--------------:|--------------:|------------------:|-------------:|--------------:|
| large_ascending         |         11 us |      **9 us** |             12 us |        22 us |        358 us |
| large_descending        |     **14 us** |     **14 us** |             46 us |       144 us |        347 us |
| large_random            |    **342 us** |        509 us |            534 us |       544 us |        399 us |
| large_random_expensive  |         63 ms |     **24 ms** |                -  |        31 ms |            -  |
| large_mostly_ascending  |         82 us |        239 us |            129 us |    **45 us** |        353 us |
| large_mostly_descending |     **93 us** |        267 us |            135 us |       282 us |        349 us |
| large_big_ascending     |        375 us |    **370 us** |            374 us |       412 us |     49,597 us |
| large_big_descending    |    **441 us** |    **441 us** |            571 us |       814 us |     49,675 us |
| large_big_random        |  **1,573 us** |      2,141 us |          1,971 us |     2,308 us |     43,578 us |
| medium_ascending        |        186 ns |        145 ns |        **148 ns** |       259 ns |      4,509 ns |
| medium_descending       |        214 ns |    **188 ns** |            490 ns |     1,344 ns |      4,947 ns |
| medium_random           |  **2,926 ns** |      3,297 ns |          3,252 ns |     3,722 ns |      6,466 ns |
| small_ascending         |         32 ns |         32 ns |         **25 ns** |        47 ns |      1,597 ns |
| small_descending        |         55 ns |         55 ns |         **51 ns** |       232 ns |      1,595 ns |
| small_random            |        347 ns |        346 ns |        **333 ns** |       484 ns |      3,841 ns |
| small_big_ascending     |         92 ns |         92 ns |         **89 ns** |       136 ns |     35,749 ns |
| small_big_descending    |    **246 ns** |    **246 ns** |            420 ns |       517 ns |     35,807 ns |
| small_big_random        |    **492 ns** |    **492 ns** |            582 ns |       671 ns |     46,393 ns |

[qs]: https://github.com/notriddle/quickersort
[ds]: https://github.com/emilk/drop-merge-sort
[rs]: https://github.com/crepererum/rdxsort-rs
[bench]: https://github.com/rust-lang/rust/blob/468227129d08b52c4cf90313b29fdad1b80e596b/src/libcollectionstest/slice.rs#L1406
