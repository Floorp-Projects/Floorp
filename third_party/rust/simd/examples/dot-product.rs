#![feature(cfg_target_feature)]
extern crate simd;
use simd::f32x4;
#[cfg(target_feature = "avx")]
use simd::x86::avx::{f32x8, LowHigh128};

#[inline(never)]
pub fn dot(x: &[f32], y: &[f32]) -> f32 {
    assert_eq!(x.len(), y.len());

    let len = std::cmp::min(x.len(), y.len());

    let mut sum = f32x4::splat(0.0);
    let mut i = 0;
    while i < len & !3 {
        let x = f32x4::load(x, i);
        let y = f32x4::load(y, i);
        sum = sum + x * y;
        i += 4
    }
    sum.extract(0) + sum.extract(1) + sum.extract(2) + sum.extract(3)
}

#[cfg(target_feature = "avx")]
#[inline(never)]
pub fn dot8(x: &[f32], y: &[f32]) -> f32 {
    assert_eq!(x.len(), y.len());

    let len = std::cmp::min(x.len(), y.len());

    let mut sum = f32x8::splat(0.0);
    let mut i = 0;
    while i < len & !7 {
        let x = f32x8::load(x, i);
        let y = f32x8::load(y, i);
        sum = sum + x * y;
        i += 8
    }
    let sum = sum.low() + sum.high();
    sum.extract(0) + sum.extract(1) + sum.extract(2) + sum.extract(3)
}


#[cfg(not(target_feature = "avx"))]
pub fn dot8(_: &[f32], _: &[f32]) -> f32 {
    unimplemented!()
}


fn main() {
    println!("{}", dot(&[1.0, 3.0, 5.0, 7.0], &[2.0, 4.0, 6.0, 8.0]));
    println!("{}", dot(&[1.0, 3.0, 6.0, 7.0, 10.0, 6.0, 3.0, 2.0],
                       &[2.0, 4.0, 6.0, 8.0, 2.0, 4.0, 6.0, 8.0]));

    if cfg!(target_feature = "avx") {
        println!("{}", dot8(&[1.0, 3.0, 5.0, 7.0], &[2.0, 4.0, 6.0, 8.0]));
        println!("{}", dot8(&[1.0, 3.0, 6.0, 7.0, 10.0, 6.0, 3.0, 2.0],
                           &[2.0, 4.0, 6.0, 8.0, 2.0, 4.0, 6.0, 8.0]));
    }
}
