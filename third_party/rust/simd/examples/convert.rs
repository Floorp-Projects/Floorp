extern crate simd;
use simd::f32x4;

#[inline(never)]
pub fn convert_scalar(x: &mut [i32], y: &[f32]) {
    assert_eq!(x.len(), y.len());

    let mut i = 0;
    while i < x.len() & !3 {
        x[i] = y[i] as i32;
        i += 1;
    }
}

#[inline(never)]
pub fn convert(x: &mut [i32], y: &[f32]) {
    assert_eq!(x.len(), y.len());

    let mut i = 0;
    while i < x.len() & !3 {
        let v = f32x4::load(y, i);
        v.to_i32().store(x, i);
        i += 4
    }
}

fn main() {
    let x = &mut [0; 12];
    let y = [1.0; 12];
    convert(x, &y);
    convert_scalar(x, &y);
    println!("{:?}", x);
    let x = &mut [0; 16];
    let y = [1.0; 16];
    convert(x, &y);
    convert_scalar(x, &y);
    println!("{:?}", x);
}
