#![feature(test)]
extern crate test;
extern crate rand_jitter;

use test::Bencher;
use rand_jitter::rand_core::RngCore;

#[bench]
fn bench_add_two(b: &mut Bencher) {
    let mut rng = rand_jitter::JitterRng::new().unwrap();
    let mut buf = [0u8; 1024];
    b.iter(|| {
        rng.fill_bytes(&mut buf[..]);
        test::black_box(&buf);
    });
    b.bytes = buf.len() as u64;
}

