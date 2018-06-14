#![feature(test)]

extern crate test;
extern crate time;

use test::Bencher;

#[bench]
fn bench_precise_time_ns(b: &mut Bencher) {
    b.iter(|| {
        time::precise_time_ns();
        time::precise_time_ns();
    });
}
