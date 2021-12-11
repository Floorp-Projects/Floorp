//! `FromCast` and `IntoCast` implementations for portable 32-bit wide vectors
#[rustfmt::skip]

use crate::*;

impl_from_cast!(
    i8x4[test_v32]: u8x4, m8x4, i16x4, u16x4, m16x4, i32x4, u32x4, f32x4, m32x4,
    i64x4, u64x4, f64x4, m64x4, i128x4, u128x4, m128x4, isizex4, usizex4, msizex4
);
impl_from_cast!(
    u8x4[test_v32]: i8x4, m8x4, i16x4, u16x4, m16x4, i32x4, u32x4, f32x4, m32x4,
    i64x4, u64x4, f64x4, m64x4, i128x4, u128x4, m128x4, isizex4, usizex4, msizex4
);
impl_from_cast_mask!(
    m8x4[test_v32]: i8x4, u8x4, i16x4, u16x4, m16x4, i32x4, u32x4, f32x4, m32x4,
    i64x4, u64x4, f64x4, m64x4, i128x4, u128x4, m128x4, isizex4, usizex4, msizex4
);

impl_from_cast!(
    i16x2[test_v32]: i8x2, u8x2, m8x2, u16x2, m16x2, i32x2, u32x2, f32x2, m32x2,
    i64x2, u64x2, f64x2, m64x2, i128x2, u128x2, m128x2, isizex2, usizex2, msizex2
);
impl_from_cast!(
    u16x2[test_v32]: i8x2, u8x2, m8x2, i16x2, m16x2, i32x2, u32x2, f32x2, m32x2,
    i64x2, u64x2, f64x2, m64x2, i128x2, u128x2, m128x2, isizex2, usizex2, msizex2
);
impl_from_cast_mask!(
    m16x2[test_v32]: i8x2, u8x2, m8x2, i16x2, u16x2, i32x2, u32x2, f32x2, m32x2,
    i64x2, u64x2, f64x2, m64x2, i128x2, u128x2, m128x2, isizex2, usizex2, msizex2
);
