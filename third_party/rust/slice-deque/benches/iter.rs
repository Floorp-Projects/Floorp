#![feature(test)]

extern crate slice_deque;
extern crate test;

use std::collections::VecDeque;

const MAX_IDX: usize = 100_000;

#[bench]
fn iter_contiguous_std_vecdeque(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_IDX);
    deq.resize(MAX_IDX, 3);
    b.iter(|| {
        deq.iter().for_each(|v| {
            test::black_box(v);
        });
    });
}

#[bench]
fn iter_contiguous_slice_deque(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_IDX);
    deq.resize(MAX_IDX, 3);
    b.iter(|| {
        deq.iter().for_each(|v| {
            test::black_box(v);
        });
    });
}

#[bench]
fn iter_chunked_std_vecdeque(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_IDX);
    deq.resize(MAX_IDX, 3);
    for _ in 0..MAX_IDX / 2 {
        deq.pop_front();
    }
    for _ in 0..MAX_IDX / 4 {
        deq.push_back(3);
    }
    b.iter(|| {
        deq.iter().for_each(|v| {
            test::black_box(v);
        });
    });
}

#[bench]
fn iter_chunked_slice_deque(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_IDX);
    deq.resize(MAX_IDX, 3);
    for _ in 0..MAX_IDX / 2 {
        deq.pop_front();
    }
    for _ in 0..MAX_IDX / 4 {
        deq.push_back(3);
    }
    b.iter(|| {
        deq.iter().for_each(|v| {
            test::black_box(v);
        });
    });
}
