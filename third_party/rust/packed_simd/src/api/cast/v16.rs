//! `FromCast` and `IntoCast` implementations for portable 16-bit wide vectors
#![rustfmt::skip]

use crate::*;

impl_from_cast!(
    i8x2[test_v16]: u8x2, m8x2, i16x2, u16x2, m16x2, i32x2, u32x2, f32x2, m32x2,
    i64x2, u64x2, f64x2, m64x2, i128x2, u128x2, m128x2, isizex2, usizex2, msizex2
);
impl_from_cast!(
    u8x2[test_v16]: i8x2, m8x2, i16x2, u16x2, m16x2, i32x2, u32x2, f32x2, m32x2,
    i64x2, u64x2, f64x2, m64x2, i128x2, u128x2, m128x2, isizex2, usizex2, msizex2
);
impl_from_cast_mask!(
    m8x2[test_v16]: i8x2, u8x2, i16x2, u16x2, m16x2, i32x2, u32x2, f32x2, m32x2,
    i64x2, u64x2, f64x2, m64x2, i128x2, u128x2, m128x2, isizex2, usizex2, msizex2
);
