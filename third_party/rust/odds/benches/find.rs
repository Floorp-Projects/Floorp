
#![feature(test)]
extern crate test;

extern crate odds;
extern crate itertools;

use itertools::{cloned};

use std::mem::size_of_val;

use odds::slice::SliceFindSplit;

use test::Bencher;
//use test::black_box;

#[bench]
fn find_split(bench: &mut Bencher) {
    let mut b = vec![0; 64 * 1024];
    const OFF: usize = 32 * 1024;
    b[OFF] = 1;

    bench.iter(|| {
        b.find_split(&1)
    });
    bench.bytes = OFF as u64 * size_of_val(&b[0]) as u64;
}

#[bench]
fn find_split_short_i32(bench: &mut Bencher) {
    let mut b = vec![0; 64 ];
    const OFF: usize = 32 ;
    b[OFF] = 1;

    bench.iter(|| {
        b.find_split(&1)
    });
    bench.bytes = OFF as u64 * size_of_val(&b[0]) as u64;
}

#[bench]
fn find_split_short_u8(bench: &mut Bencher) {
    let mut b = vec![0u8; 128 ];
    const OFF: usize = 64;
    b[OFF] = 1;

    bench.iter(|| {
        b.find_split(&1)
    });
    bench.bytes = OFF as u64 * size_of_val(&b[0]) as u64;
}
const FIND_SKIP: &'static [usize] = &[3, 9, 4, 16, 3, 2, 1];

// The loop bench wants to test a find/scan loop where there are many
// short intervals
#[bench]
fn find_split_loop_u8(bench: &mut Bencher) {
    let mut b = vec![0u8; 512];

    for i in cloned(FIND_SKIP).cycle().scan(0, |st, x| { *st += x; Some(*st) }) {
        if i >= b.len() { break; }
        b[i] = 1;
    }

    bench.iter(|| {
        let mut nfind = 0;
        let mut slc = &b[..];
        loop {
            let (_, tail) = slc.find_split(&1);
            if tail.len() == 0 {
                break;
            }
            nfind += 1;
            slc = &tail[1..];
        }
        nfind
    });
    bench.bytes = size_of_val(&b[..]) as u64;
}

#[bench]
fn rfind_split(bench: &mut Bencher) {
    let mut b = vec![0; 64 * 1024];
    const OFF: usize = 32 * 1024;
    b[OFF] = 1;

    bench.iter(|| {
        b.rfind_split(&1)
    });
    bench.bytes = OFF as u64 * size_of_val(&b[0]) as u64;
}

#[bench]
fn rfind_split_loop_u8(bench: &mut Bencher) {
    let mut b = vec![0u8; 512];

    for i in cloned(FIND_SKIP).cycle().scan(0, |st, x| { *st += x; Some(*st) }) {
        if i >= b.len() { break; }
        b[i] = 1;
    }

    bench.iter(|| {
        let mut nfind = 0;
        let mut slc = &b[..];
        loop {
            let (head, _) = slc.rfind_split(&1);
            if head.len() == 0 {
                break;
            }
            nfind += 1;
            slc = &head[..head.len() - 1];
        }
        nfind
    });
    bench.bytes = size_of_val(&b[..]) as u64;
}
