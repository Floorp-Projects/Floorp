#![feature(test)]
#![cfg_attr(feature = "cargo-clippy", allow(option_unwrap_used))]

extern crate slice_deque;
extern crate test;

use std::collections::VecDeque;

const MAX_NO_ITERS: usize = 2_000_000_000;

#[bench]
fn pop_front_std_vecdeque(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(MAX_NO_ITERS, 3);
    b.iter(|| {
        test::black_box(deq.pop_front().unwrap());
    });
}

#[bench]
fn pop_front_slice_deque(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(MAX_NO_ITERS, 3);
    b.iter(|| {
        test::black_box(deq.pop_front().unwrap());
    });
}

#[bench]
fn truncate_front_1_slice_deque(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(MAX_NO_ITERS, 3);
    b.iter(|| {
        let new_len = deq.len() - 1;
        test::black_box(deq.truncate_front(new_len));
    });
}

#[bench]
fn truncate_front_100_slice_deque(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(MAX_NO_ITERS, 3);
    b.iter(|| {
        let new_len = deq.len() - 100;
        test::black_box(deq.truncate_front(new_len));
    });
}
