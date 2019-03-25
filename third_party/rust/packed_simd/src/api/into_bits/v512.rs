//! `FromBits` and `IntoBits` implementations for portable 512-bit wide vectors
#![rustfmt::skip]

#[allow(unused)]  // wasm_bindgen_test
use crate::*;

impl_from_bits!(i8x64[test_v512]: u8x64, m8x64, i16x32, u16x32, m16x32, i32x16, u32x16, f32x16, m32x16, i64x8, u64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(u8x64[test_v512]: i8x64, m8x64, i16x32, u16x32, m16x32, i32x16, u32x16, f32x16, m32x16, i64x8, u64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(m8x64[test_v512]: m16x32, m32x16, m64x8, m128x4);

impl_from_bits!(i16x32[test_v512]: i8x64, u8x64, m8x64, u16x32, m16x32, i32x16, u32x16, f32x16, m32x16, i64x8, u64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(u16x32[test_v512]: i8x64, u8x64, m8x64, i16x32, m16x32, i32x16, u32x16, f32x16, m32x16, i64x8, u64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(m16x32[test_v512]: m32x16, m64x8, m128x4);

impl_from_bits!(i32x16[test_v512]: i8x64, u8x64, m8x64, i16x32, u16x32, m16x32, u32x16, f32x16, m32x16, i64x8, u64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(u32x16[test_v512]: i8x64, u8x64, m8x64, i16x32, u16x32, m16x32, i32x16, f32x16, m32x16, i64x8, u64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(f32x16[test_v512]: i8x64, u8x64, m8x64, i16x32, u16x32, m16x32, i32x16, u32x16, m32x16, i64x8, u64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(m32x16[test_v512]: m64x8, m128x4);

impl_from_bits!(i64x8[test_v512]: i8x64, u8x64, m8x64, i16x32, u16x32, m16x32, i32x16, u32x16, f32x16, m32x16, u64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(u64x8[test_v512]: i8x64, u8x64, m8x64, i16x32, u16x32, m16x32, i32x16, u32x16, f32x16, m32x16, i64x8, f64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(f64x8[test_v512]: i8x64, u8x64, m8x64, i16x32, u16x32, m16x32, i32x16, u32x16, f32x16, m32x16, i64x8, u64x8, m64x8, i128x4, u128x4, m128x4);
impl_from_bits!(m64x8[test_v512]: m128x4);

impl_from_bits!(i128x4[test_v512]: i8x64, u8x64, m8x64, i16x32, u16x32, m16x32, i32x16, u32x16, f32x16, m32x16, i64x8, u64x8, f64x8, m64x8, u128x4, m128x4);
impl_from_bits!(u128x4[test_v512]: i8x64, u8x64, m8x64, i16x32, u16x32, m16x32, i32x16, u32x16, f32x16, m32x16, i64x8, u64x8, f64x8, m64x8, i128x4, m128x4);
// note: m128x4 cannot be constructed from all the other masks bit patterns in here
