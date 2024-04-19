//! `FromBits` and `IntoBits` implementations for portable 256-bit wide vectors
#[rustfmt::skip]

#[allow(unused)]  // wasm_bindgen_test
use crate::*;

impl_from_bits!(
    i8x32[test_v256]: u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(
    u8x32[test_v256]: i8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(m8x32[test_v256]: m16x16, m32x8, m64x4, m128x2);

impl_from_bits!(
    i16x16[test_v256]: i8x32,
    u8x32,
    m8x32,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(
    u16x16[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(m16x16[test_v256]: m32x8, m64x4, m128x2);

impl_from_bits!(
    i32x8[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(
    u32x8[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(
    f32x8[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(m32x8[test_v256]: m64x4, m128x2);

impl_from_bits!(
    i64x4[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(
    u64x4[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    f64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(
    f64x4[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    m64x4,
    i128x2,
    u128x2,
    m128x2
);
impl_from_bits!(m64x4[test_v256]: m128x2);

impl_from_bits!(
    i128x2[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    u128x2,
    m128x2
);
impl_from_bits!(
    u128x2[test_v256]: i8x32,
    u8x32,
    m8x32,
    i16x16,
    u16x16,
    m16x16,
    i32x8,
    u32x8,
    f32x8,
    m32x8,
    i64x4,
    u64x4,
    f64x4,
    m64x4,
    i128x2,
    m128x2
);
// note: m128x2 cannot be constructed from all the other masks bit patterns in
// here
