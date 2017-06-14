#![feature(cfg_target_feature)]
extern crate simd;
use simd::f32x4;
#[cfg(target_feature = "avx")]
use simd::x86::avx::f32x8;

#[inline(never)]
pub fn axpy(z: &mut [f32], a: f32, x: &[f32], y: &[f32]) {
    assert_eq!(x.len(), y.len());
    assert_eq!(x.len(), z.len());

    let len = std::cmp::min(std::cmp::min(x.len(), y.len()), z.len());

    let mut i = 0;
    while i < len & !3 {
        let x = f32x4::load(x, i);
        let y = f32x4::load(y, i);
        (f32x4::splat(a) * x + y).store(z, i);
        i += 4
    }
}

#[cfg(target_feature = "avx")]
#[inline(never)]
pub fn axpy8(z: &mut [f32], a: f32, x: &[f32], y: &[f32]) {
    assert_eq!(x.len(), y.len());
    assert_eq!(x.len(), z.len());

    let len = std::cmp::min(std::cmp::min(x.len(), y.len()), z.len());

    let mut i = 0;
    while i < len & !7 {
        let x = f32x8::load(x, i);
        let y = f32x8::load(y, i);
        (f32x8::splat(a) * x + y).store(z, i);
        i += 8
    }
}


#[cfg(not(target_feature = "avx"))]
pub fn axpy8(_: &mut [f32], _: f32, _: &[f32], _: &[f32]) {
    unimplemented!()
}


fn main() {
    let mut z = vec![0.; 4];
    axpy(&mut z, 2., &[1.0, 3.0, 5.0, 7.0], &[2.0, 4.0, 6.0, 8.0]);
    println!("{:?}", z);
    let mut z = vec![0.; 8];
    axpy(&mut z, 3., &[1.0, 3.0, 6.0, 7.0, 10.0, 6.0, 3.0, 2.0],
                       &[2.0, 4.0, 6.0, 8.0, 2.0, 4.0, 6.0, 8.0]);
    println!("{:?}", z);

    if cfg!(target_feature = "avx") {
        let mut z = vec![0.; 4];
        axpy8(&mut z, 2., &[1.0, 3.0, 5.0, 7.0], &[2.0, 4.0, 6.0, 8.0]);
        println!("{:?}", z);
        let mut z = vec![0.; 8];
        axpy8(&mut z, 3., &[1.0, 3.0, 6.0, 7.0, 10.0, 6.0, 3.0, 2.0],
                           &[2.0, 4.0, 6.0, 8.0, 2.0, 4.0, 6.0, 8.0]);
        println!("{:?}", z);
    }
}
