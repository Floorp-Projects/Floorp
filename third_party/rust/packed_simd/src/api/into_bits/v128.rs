//! `FromBits` and `IntoBits` implementations for portable 128-bit wide vectors
#![rustfmt::skip]

#[allow(unused)]  // wasm_bindgen_test
use crate::*;

impl_from_bits!(i8x16[test_v128]: u8x16, m8x16, i16x8, u16x8, m16x8, i32x4, u32x4, f32x4, m32x4, i64x2, u64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(u8x16[test_v128]: i8x16, m8x16, i16x8, u16x8, m16x8, i32x4, u32x4, f32x4, m32x4, i64x2, u64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(m8x16[test_v128]: m16x8, m32x4, m64x2, m128x1);

impl_from_bits!(i16x8[test_v128]: i8x16, u8x16, m8x16, u16x8, m16x8, i32x4, u32x4, f32x4, m32x4, i64x2, u64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(u16x8[test_v128]: i8x16, u8x16, m8x16, i16x8, m16x8, i32x4, u32x4, f32x4, m32x4, i64x2, u64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(m16x8[test_v128]: m32x4, m64x2, m128x1);

impl_from_bits!(i32x4[test_v128]: i8x16, u8x16, m8x16, i16x8, u16x8, m16x8, u32x4, f32x4, m32x4, i64x2, u64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(u32x4[test_v128]: i8x16, u8x16, m8x16, i16x8, u16x8, m16x8, i32x4, f32x4, m32x4, i64x2, u64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(f32x4[test_v128]: i8x16, u8x16, m8x16, i16x8, u16x8, m16x8, i32x4, u32x4, m32x4, i64x2, u64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(m32x4[test_v128]: m64x2, m128x1);

impl_from_bits!(i64x2[test_v128]: i8x16, u8x16, m8x16, i16x8, u16x8, m16x8, i32x4, u32x4, f32x4, m32x4, u64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(u64x2[test_v128]: i8x16, u8x16, m8x16, i16x8, u16x8, m16x8, i32x4, u32x4, f32x4, m32x4, i64x2, f64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(f64x2[test_v128]: i8x16, u8x16, m8x16, i16x8, u16x8, m16x8, i32x4, u32x4, f32x4, m32x4, i64x2, u64x2, m64x2, i128x1, u128x1, m128x1);
impl_from_bits!(m64x2[test_v128]: m128x1);

impl_from_bits!(i128x1[test_v128]: i8x16, u8x16, m8x16, i16x8, u16x8, m16x8, i32x4, u32x4, f32x4, m32x4, i64x2, u64x2, f64x2, m64x2, u128x1, m128x1);
impl_from_bits!(u128x1[test_v128]: i8x16, u8x16, m8x16, i16x8, u16x8, m16x8, i32x4, u32x4, f32x4, m32x4, i64x2, u64x2, f64x2, m64x2, i128x1, m128x1);
// note: m128x1 cannot be constructed from all the other masks bit patterns in here

