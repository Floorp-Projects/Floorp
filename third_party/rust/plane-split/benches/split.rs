#![feature(test)]

extern crate euclid;
extern crate plane_split;
extern crate test;

use euclid::vec3;
use plane_split::{make_grid, BspSplitter, Splitter};
use std::sync::Arc;

#[bench]
fn bench_bsp(b: &mut test::Bencher) {
    let polys = Arc::new(make_grid(5));
    let mut splitter = BspSplitter::new();
    let view = vec3(0.0, 0.0, 1.0);
    b.iter(|| {
        let p = polys.clone();
        splitter.solve(&p, view);
    });
}
