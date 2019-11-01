#![feature(test)]
#![cfg_attr(feature = "cargo-clippy", allow(option_unwrap_used))]

extern crate slice_deque;
extern crate test;

use std::collections::VecDeque;

const MAX_NO_ITERS: usize = 1_000_000_000;

#[bench]
fn insert_front_vdeq(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        test::black_box(deq.insert(0, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_front_sdeq(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        test::black_box(deq.insert(0, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_back_vdeq(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        let i = deq.len() - 1;
        test::black_box(deq.insert(i, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_back_sdeq(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        let i = deq.len() - 1;
        test::black_box(deq.insert(i, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_mid_vdeq(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        let i = (deq.len() - 1) / 2;
        test::black_box(deq.insert(i, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_mid_sdeq(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        let i = (deq.len() - 1) / 2;
        test::black_box(deq.insert(i, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_quarter_vdeq(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        let i = (deq.len() - 1) / 4;
        test::black_box(deq.insert(i, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_quarter_sdeq(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        let i = (deq.len() - 1) / 4;
        test::black_box(deq.insert(i, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_three_quarter_vdeq(b: &mut test::Bencher) {
    let mut deq = VecDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        let i = (deq.len() - 1) / 4 * 3;
        test::black_box(deq.insert(i, 3));
        test::black_box(&mut deq);
    });
}

#[bench]
fn insert_three_quarter_sdeq(b: &mut test::Bencher) {
    let mut deq = slice_deque::SliceDeque::<u8>::with_capacity(MAX_NO_ITERS);
    deq.resize(10, 3);
    b.iter(|| {
        let i = (deq.len() - 1) / 4 * 3;
        test::black_box(deq.insert(i, 3));
        test::black_box(&mut deq);
    });
}
