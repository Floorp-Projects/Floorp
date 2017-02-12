#![feature(test)]

extern crate memchr;
extern crate test;

use std::iter;

fn bench_data() -> Vec<u8> { iter::repeat(b'z').take(10000).collect() }

#[bench]
fn iterator_memchr(b: &mut test::Bencher) {
    let haystack = bench_data();
    let needle = b'a';
    b.iter(|| {
        assert!(haystack.iter().position(|&b| b == needle).is_none());
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn optimized_memchr(b: &mut test::Bencher) {
    let haystack = bench_data();
    let needle = b'a';
    b.iter(|| {
        assert!(memchr::memchr(needle, &haystack).is_none());
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn iterator_memrchr(b: &mut test::Bencher) {
    let haystack = bench_data();
    let needle = b'a';
    b.iter(|| {
        assert!(haystack.iter().rposition(|&b| b == needle).is_none());
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn optimized_memrchr(b: &mut test::Bencher) {
    let haystack = bench_data();
    let needle = b'a';
    b.iter(|| {
        assert!(memchr::memrchr(needle, &haystack).is_none());
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn iterator_memchr2(b: &mut test::Bencher) {
    let haystack = bench_data();
    let (needle1, needle2) = (b'a', b'b');
    b.iter(|| {
        assert!(haystack.iter().position(|&b| {
            b == needle1 || b == needle2
        }).is_none());
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn manual_memchr2(b: &mut test::Bencher) {
    fn find_singles(
        sparse: &[bool],
        text: &[u8],
    ) -> Option<(usize, usize)> {
        for (hi, &b) in text.iter().enumerate() {
            if sparse[b as usize] {
                return Some((hi, hi+1));
            }
        }
        None
    }

    let haystack = bench_data();
    let mut sparse = vec![false; 256];
    sparse[b'a' as usize] = true;
    sparse[b'b' as usize] = true;
    b.iter(|| {
        assert!(find_singles(&sparse, &haystack).is_none());
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn optimized_memchr2(b: &mut test::Bencher) {
    let haystack = bench_data();
    let (needle1, needle2) = (b'a', b'b');
    b.iter(|| {
        assert!(memchr::memchr2(needle1, needle2, &haystack).is_none());
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn iterator_memchr3(b: &mut test::Bencher) {
    let haystack = bench_data();
    let (needle1, needle2, needle3) = (b'a', b'b', b'c');
    b.iter(|| {
        assert!(haystack.iter().position(|&b| {
            b == needle1 || b == needle2 || b == needle3
        }).is_none());
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn optimized_memchr3(b: &mut test::Bencher) {
    let haystack = bench_data();
    let (needle1, needle2, needle3) = (b'a', b'b', b'c');
    b.iter(|| {
        assert!(memchr::memchr3(
            needle1, needle2, needle3, &haystack).is_none());
    });
    b.bytes = haystack.len() as u64;
}
