#![feature(test)]
#![cfg_attr(feature = "cargo-clippy", allow(needless_range_loop))]

extern crate slice_deque;
extern crate test;

use std::collections::VecDeque;

const MAX_IDX: usize = 100_000;

#[bench]
fn index_contiguous_std_vecdeque(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_IDX);
    deq.resize(MAX_IDX, 3);
    b.iter(|| {
        for i in 0..MAX_IDX {
            test::black_box(&deq[i]);
        }
    });
}

#[bench]
fn index_contiguous_slice_deque(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_IDX);
    deq.resize(MAX_IDX, 3);
    b.iter(|| {
        for i in 0..MAX_IDX {
            test::black_box(&deq[i]);
        }
    });
}

#[bench]
fn index_chunked_std_vecdeque(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_IDX);
    deq.resize(MAX_IDX, 3);
    for _ in 0..MAX_IDX / 2 {
        deq.pop_front();
    }
    for _ in 0..MAX_IDX / 4 {
        deq.push_back(3);
    }
    b.iter(|| {
        for i in 0..MAX_IDX / 4 * 3 {
            test::black_box(&deq[i]);
        }
    });
}

#[bench]
fn index_chunked_slice_deque(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_IDX);
    deq.resize(MAX_IDX, 3);
    for _ in 0..MAX_IDX / 2 {
        deq.pop_front();
    }
    for _ in 0..MAX_IDX / 4 {
        deq.push_back(3);
    }
    b.iter(|| {
        for i in 0..MAX_IDX / 4 * 3 {
            test::black_box(&deq[i]);
        }
    });
}
