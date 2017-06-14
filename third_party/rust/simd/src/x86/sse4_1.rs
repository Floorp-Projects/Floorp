use super::super::*;
use x86::sse2::*;

#[allow(dead_code)]
extern "platform-intrinsic" {
    fn x86_mm_dp_ps(x: f32x4, y: f32x4, z: i32) -> f32x4;
    fn x86_mm_dp_pd(x: f64x2, y: f64x2, z: i32) -> f64x2;
    fn x86_mm_max_epi8(x: i8x16, y: i8x16) -> i8x16;
    fn x86_mm_max_epu16(x: u16x8, y: u16x8) -> u16x8;
    fn x86_mm_max_epi32(x: i32x4, y: i32x4) -> i32x4;
    fn x86_mm_max_epu32(x: u32x4, y: u32x4) -> u32x4;
    fn x86_mm_min_epi8(x: i8x16, y: i8x16) -> i8x16;
    fn x86_mm_min_epu16(x: u16x8, y: u16x8) -> u16x8;
    fn x86_mm_min_epi32(x: i32x4, y: i32x4) -> i32x4;
    fn x86_mm_min_epu32(x: u32x4, y: u32x4) -> u32x4;
    fn x86_mm_minpos_epu16(x: u16x8) -> u16x8;
    fn x86_mm_mpsadbw_epu8(x: u8x16, y: u8x16, z: i32) -> u16x8;
    fn x86_mm_mul_epi32(x: i32x4, y: i32x4) -> i64x2;
    fn x86_mm_packus_epi32(x: i32x4, y: i32x4) -> u16x8;
    fn x86_mm_testc_si128(x: u64x2, y: u64x2) -> i32;
    fn x86_mm_testnzc_si128(x: u64x2, y: u64x2) -> i32;
    fn x86_mm_testz_si128(x: u64x2, y: u64x2) -> i32;
}

// 32 bit floats

pub trait Sse41F32x4 {}
impl Sse41F32x4 for f32x4 {}

// 64 bit floats

pub trait Sse41F64x2 {}
impl Sse41F64x2 for f64x2 {}

// 64 bit integers

pub trait Sse41U64x2 {
    fn testc(self, other: Self) -> i32;
    fn testnzc(self, other: Self) -> i32;
    fn testz(self, other: Self) -> i32;
}
impl Sse41U64x2 for u64x2 {
    #[inline]
    fn testc(self, other: Self) -> i32 {
        unsafe { x86_mm_testc_si128(self, other) }
    }
    #[inline]
    fn testnzc(self, other: Self) -> i32 {
        unsafe { x86_mm_testnzc_si128(self, other) }
    }
    #[inline]
    fn testz(self, other: Self) -> i32 {
        unsafe { x86_mm_testz_si128(self, other) }
    }
}
pub trait Sse41I64x2 {}
impl Sse41I64x2 for i64x2 {}

pub trait Sse41Bool64ix2 {}
impl Sse41Bool64ix2 for bool64ix2 {}

// 32 bit integers

pub trait Sse41U32x4 {
    fn max(self, other: Self) -> Self;
    fn min(self, other: Self) -> Self;
}
impl Sse41U32x4 for u32x4 {
    #[inline]
    fn max(self, other: Self) -> Self {
        unsafe { x86_mm_max_epu32(self, other) }
    }
    #[inline]
    fn min(self, other: Self) -> Self {
        unsafe { x86_mm_min_epu32(self, other) }
    }
}
pub trait Sse41I32x4 {
    fn max(self, other: Self) -> Self;
    fn min(self, other: Self) -> Self;
    fn low_mul(self, other: Self) -> i64x2;
    fn packus(self, other: Self) -> u16x8;
}
impl Sse41I32x4 for i32x4 {
    #[inline]
    fn max(self, other: Self) -> Self {
        unsafe { x86_mm_max_epi32(self, other) }
    }
    #[inline]
    fn min(self, other: Self) -> Self {
        unsafe { x86_mm_min_epi32(self, other) }
    }

    #[inline]
    fn low_mul(self, other: Self) -> i64x2 {
        unsafe { x86_mm_mul_epi32(self, other) }
    }
    #[inline]
    fn packus(self, other: Self) -> u16x8 {
        unsafe { x86_mm_packus_epi32(self, other) }
    }
}

pub trait Sse41Bool32ix4 {}
impl Sse41Bool32ix4 for bool32ix4 {}

// 16 bit integers

pub trait Sse41U16x8 {
    fn max(self, other: Self) -> Self;
    fn min(self, other: Self) -> Self;
    fn minpos(self) -> Self;
}
impl Sse41U16x8 for u16x8 {
    #[inline]
    fn max(self, other: Self) -> Self {
        unsafe { x86_mm_max_epu16(self, other) }
    }
    #[inline]
    fn min(self, other: Self) -> Self {
        unsafe { x86_mm_min_epu16(self, other) }
    }

    #[inline]
    fn minpos(self) -> Self {
        unsafe { x86_mm_minpos_epu16(self) }
    }
}
pub trait Sse41I16x8 {}
impl Sse41I16x8 for i16x8 {}

pub trait Sse41Bool16ix8 {}
impl Sse41Bool16ix8 for bool16ix8 {}

// 8 bit integers

pub trait Sse41U8x16 {}
impl Sse41U8x16 for u8x16 {}
pub trait Sse41I8x16 {
    fn max(self, other: Self) -> Self;
    fn min(self, other: Self) -> Self;
}
impl Sse41I8x16 for i8x16 {
    #[inline]
    fn max(self, other: Self) -> Self {
        unsafe { x86_mm_max_epi8(self, other) }
    }
    #[inline]
    fn min(self, other: Self) -> Self {
        unsafe { x86_mm_min_epi8(self, other) }
    }
}

pub trait Sse41Bool8ix16 {}
impl Sse41Bool8ix16 for bool8ix16 {}
