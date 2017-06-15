#![feature(test)]

extern crate euclid;
extern crate plane_split;
extern crate test;

use std::sync::Arc;
use euclid::vec3;
use plane_split::{BspSplitter, NaiveSplitter, Splitter, _make_grid};

#[bench]
fn bench_naive(b: &mut test::Bencher) {
    let polys = Arc::new(_make_grid(5));
    let mut splitter = NaiveSplitter::new();
    let view = vec3(0.0, 0.0, 1.0);
    b.iter(|| {
        let p = polys.clone();
        splitter.solve(&p, view);
    });
}

#[bench]
fn bench_bsp(b: &mut test::Bencher) {
    let polys = Arc::new(_make_grid(5));
    let mut splitter = BspSplitter::new();
    let view = vec3(0.0, 0.0, 1.0);
    b.iter(|| {
        let p = polys.clone();
        splitter.solve(&p, view);
    });
}
