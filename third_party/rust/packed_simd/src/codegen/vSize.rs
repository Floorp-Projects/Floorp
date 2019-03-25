//! Vector types with pointer-sized elements

use crate::codegen::pointer_sized_int::{isize_, usize_};
use crate::masks::*;

impl_simd_array!([isize; 2]: isizex2 | isize_, isize_);
impl_simd_array!([usize; 2]: usizex2 | usize_, usize_);
impl_simd_array!([msize; 2]: msizex2 | isize_, isize_);

impl_simd_array!([isize; 4]: isizex4 | isize_, isize_, isize_, isize_);
impl_simd_array!([usize; 4]: usizex4 | usize_, usize_, usize_, usize_);
impl_simd_array!([msize; 4]: msizex4 | isize_, isize_, isize_, isize_);

impl_simd_array!(
    [isize; 8]: isizex8 | isize_,
    isize_,
    isize_,
    isize_,
    isize_,
    isize_,
    isize_,
    isize_
);
impl_simd_array!(
    [usize; 8]: usizex8 | usize_,
    usize_,
    usize_,
    usize_,
    usize_,
    usize_,
    usize_,
    usize_
);
impl_simd_array!(
    [msize; 8]: msizex8 | isize_,
    isize_,
    isize_,
    isize_,
    isize_,
    isize_,
    isize_,
    isize_
);
