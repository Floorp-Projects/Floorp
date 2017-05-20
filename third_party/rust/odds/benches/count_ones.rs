#![feature(test)]
extern crate test;

use std::mem::{size_of, size_of_val};
use test::Bencher;

extern crate odds;
use odds::slice::split_aligned_for;
use odds::slice::unalign::UnalignedIter;

fn count_ones(data: &[u8]) -> u32 {
    let mut total = 0;
    let (head, mid, tail) = split_aligned_for::<[u64; 2]>(data);
    total += head.iter().map(|x| x.count_ones()).sum();
    total += mid.iter().map(|x| x[0].count_ones() + x[1].count_ones()).sum();
    total += tail.iter().map(|x| x.count_ones()).sum();
    total
}

fn unalign_count_ones(data: &[u8]) -> u32 {
    let mut total = 0;
    let mut iter = UnalignedIter::<u64>::from_slice(data);
    total += (&mut iter).map(|x| x.count_ones()).sum();
    total += iter.tail().map(|x| x.count_ones()).sum();
    total
}

#[bench]
fn split_count_ones(b: &mut Bencher) {
    let v = vec![3u8; 127];
    b.iter(|| {
        count_ones(&v)
    });
    b.bytes = size_of_val(&v[..]) as u64;
}

#[bench]
fn bench_unalign_count_ones(b: &mut Bencher) {
    let v = vec![3u8; 127];
    b.iter(|| {
        unalign_count_ones(&v)
    });
    b.bytes = size_of_val(&v[..]) as u64;
}
