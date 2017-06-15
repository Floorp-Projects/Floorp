#![feature(test)]

extern crate coco;
extern crate test;

use coco::epoch;
use test::Bencher;

#[bench]
fn pin_empty(b: &mut Bencher) {
    b.iter(|| epoch::pin(|_| ()))
}
