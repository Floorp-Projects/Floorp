#![feature(test)]
#![cfg_attr(feature = "cargo-clippy", allow(option_unwrap_used))]

extern crate slice_deque;
extern crate test;

use std::collections::VecDeque;

const MAX_NO_ITERS: usize = 1_000_000_000;

#[bench]
fn pop_back_std_vecdeque(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(MAX_NO_ITERS, 3);
    b.iter(|| {
        test::black_box(deq.pop_back().unwrap());
    });
}

#[bench]
fn pop_back_slice_deque(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(MAX_NO_ITERS, 3);
    b.iter(|| {
        test::black_box(deq.pop_back().unwrap());
    });
}
