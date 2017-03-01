#![feature(test)]

extern crate pdqsort;
extern crate rand;
extern crate test;

use rand::{Rng, thread_rng};
use std::mem;
use test::Bencher;

fn gen_ascending(len: usize) -> Vec<u64> {
    (0..len as u64).collect()
}

fn gen_descending(len: usize) -> Vec<u64> {
    (0..len as u64).rev().collect()
}

fn gen_random(len: usize) -> Vec<u64> {
    let mut rng = thread_rng();
    rng.gen_iter::<u64>().take(len).collect()
}

fn gen_mostly_ascending(len: usize) -> Vec<u64> {
    let mut rng = thread_rng();
    let mut v = gen_ascending(len);
    for _ in (0usize..).take_while(|x| x * x <= len) {
        let x = rng.gen::<usize>() % len;
        let y = rng.gen::<usize>() % len;
        v.swap(x, y);
    }
    v
}

fn gen_mostly_descending(len: usize) -> Vec<u64> {
    let mut rng = thread_rng();
    let mut v = gen_descending(len);
    for _ in (0usize..).take_while(|x| x * x <= len) {
        let x = rng.gen::<usize>() % len;
        let y = rng.gen::<usize>() % len;
        v.swap(x, y);
    }
    v
}

fn gen_big_random(len: usize) -> Vec<[u64; 16]> {
    let mut rng = thread_rng();
    rng.gen_iter().map(|x| [x; 16]).take(len).collect()
}

fn gen_big_ascending(len: usize) -> Vec<[u64; 16]> {
    (0..len as u64).map(|x| [x; 16]).take(len).collect()
}

fn gen_big_descending(len: usize) -> Vec<[u64; 16]> {
    (0..len as u64).rev().map(|x| [x; 16]).take(len).collect()
}

macro_rules! sort_bench {
    ($name:ident, $gen:expr, $len:expr) => {
        #[bench]
        fn $name(b: &mut Bencher) {
            b.iter(|| pdqsort::sort(&mut $gen($len)));
            b.bytes = $len * mem::size_of_val(&$gen(1)[0]) as u64;
        }
    }
}

sort_bench!(sort_small_random, gen_random, 10);
sort_bench!(sort_small_ascending, gen_ascending, 10);
sort_bench!(sort_small_descending, gen_descending, 10);

sort_bench!(sort_small_big_random, gen_big_random, 10);
sort_bench!(sort_small_big_ascending, gen_big_ascending, 10);
sort_bench!(sort_small_big_descending, gen_big_descending, 10);

sort_bench!(sort_medium_random, gen_random, 100);
sort_bench!(sort_medium_ascending, gen_ascending, 100);
sort_bench!(sort_medium_descending, gen_descending, 100);

sort_bench!(sort_large_random, gen_random, 10000);
sort_bench!(sort_large_ascending, gen_ascending, 10000);
sort_bench!(sort_large_descending, gen_descending, 10000);
sort_bench!(sort_large_mostly_ascending, gen_mostly_ascending, 10000);
sort_bench!(sort_large_mostly_descending, gen_mostly_descending, 10000);

sort_bench!(sort_large_big_random, gen_big_random, 10000);
sort_bench!(sort_large_big_ascending, gen_big_ascending, 10000);
sort_bench!(sort_large_big_descending, gen_big_descending, 10000);

#[bench]
fn sort_large_random_expensive(b: &mut Bencher) {
    let len = 10000;
    b.iter(|| {
        let mut v = gen_random(len);
        let mut count = 0;
        pdqsort::sort_by(&mut v, |a: &u64, b: &u64| {
            count += 1;
            if count % 1_000_000_000 == 0 {
                panic!("should not happen");
            }
            (*a as f64).cos().partial_cmp(&(*b as f64).cos()).unwrap()
        });
        test::black_box(count);
    });
    b.bytes = len as u64 * mem::size_of::<u64>() as u64;
}
