//! Tests the memory allocation performance
#![feature(test)]

extern crate slice_deque;
extern crate test;

use std::collections::VecDeque;

/// Returns the `capacity` of a VecDeque<u8> that uses
/// `factor` times allocation granularity memory.
fn alloc_granularity_cap(factor: usize) -> usize {
    let mut deq = VecDeque::<u8>::with_capacity(1);
    // The capacity allocated is the allocation granularity of the OS (e.g. 1
    // memory page on OSX/Linux):
    let cap = deq.capacity();
    let new_cap = factor * cap + 1;
    deq.resize(new_cap, 0);
    deq.capacity()
}

/// Allocates a VecDeque<u8> with capacity for `cap` elements, flushes it back
/// to memory, and drops it.
#[inline(always)]
fn alloc_and_flush(cap: usize) {
    let mut deq = VecDeque::<u8>::with_capacity(cap);
    deq.push_back(1);
    test::black_box(&mut deq.get_mut(0));
    test::black_box(&mut deq);
}

macro_rules! alloc {
    ($name:ident, $factor:expr) => {
        #[bench]
        fn $name(b: &mut test::Bencher) {
            let cap = alloc_granularity_cap($factor);
            b.iter(|| {
                alloc_and_flush(cap);
            });
        }
    };
}

alloc!(alloc_1, 1);
alloc!(alloc_2, 2);
alloc!(alloc_4, 4);
alloc!(alloc_8, 8);
alloc!(alloc_16, 16);
alloc!(alloc_32, 32);
alloc!(alloc_64, 64);
alloc!(alloc_128, 128);
alloc!(alloc_256, 256);
alloc!(alloc_512, 512);
alloc!(alloc_1024, 1024);
alloc!(alloc_2048, 2048);
alloc!(alloc_4096, 4096); // Linux 16 Mb
alloc!(alloc_16384, 16384); // Linux 64 Mb
alloc!(alloc_32768, 32768); // Linux 128 Mb
alloc!(alloc_65536, 65536); // Linux 256 Mb
alloc!(alloc_131072, 131072); // Linux 512 Mb
