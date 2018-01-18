extern crate simd;

use simd::*;

#[allow(unused_variables)]
fn main() {
    let x = i32x4::splat(1_i32);
    let y = -x;
    let z = !x;
}
