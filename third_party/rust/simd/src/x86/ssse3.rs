use super::super::*;
use bitcast;

macro_rules! bitcast {
    ($func: ident($a: ident, $b: ident)) => {
        bitcast($func(bitcast($a), bitcast($b)))
    }
}

extern "platform-intrinsic" {
    fn x86_mm_abs_epi8(x: i8x16) -> i8x16;
    fn x86_mm_abs_epi16(x: i16x8) -> i16x8;
    fn x86_mm_abs_epi32(x: i32x4) -> i32x4;
    fn x86_mm_hadd_epi16(x: i16x8, y: i16x8) -> i16x8;
    fn x86_mm_hadd_epi32(x: i32x4, y: i32x4) -> i32x4;
    fn x86_mm_hadds_epi16(x: i16x8, y: i16x8) -> i16x8;
    fn x86_mm_hsub_epi16(x: i16x8, y: i16x8) -> i16x8;
    fn x86_mm_hsub_epi32(x: i32x4, y: i32x4) -> i32x4;
    fn x86_mm_hsubs_epi16(x: i16x8, y: i16x8) -> i16x8;
    fn x86_mm_maddubs_epi16(x: u8x16, y: i8x16) -> i16x8;
    fn x86_mm_mulhrs_epi16(x: i16x8, y: i16x8) -> i16x8;
    fn x86_mm_shuffle_epi8(x: i8x16, y: i8x16) -> i8x16;
    fn x86_mm_sign_epi8(x: i8x16, y: i8x16) -> i8x16;
    fn x86_mm_sign_epi16(x: i16x8, y: i16x8) -> i16x8;
    fn x86_mm_sign_epi32(x: i32x4, y: i32x4) -> i32x4;
}

// 32 bit integers

pub trait Ssse3I32x4 {
    fn abs(self) -> Self;
    fn hadd(self, other: Self) -> Self;
    fn hsub(self, other: Self) -> Self;
    fn sign(self, other: Self) -> Self;
}
impl Ssse3I32x4 for i32x4 {
    #[inline]
    fn abs(self) -> Self {
        unsafe { x86_mm_abs_epi32(self) }
    }

    #[inline]
    fn hadd(self, other: Self) -> Self {
        unsafe { x86_mm_hadd_epi32(self, other) }
    }
    #[inline]
    fn hsub(self, other: Self) -> Self {
        unsafe { x86_mm_hsub_epi32(self, other) }
    }

    #[inline]
    fn sign(self, other: Self) -> Self {
        unsafe { x86_mm_sign_epi32(self, other) }
    }
}

pub trait Ssse3U32x4 {
    fn hadd(self, other: Self) -> Self;
    fn hsub(self, other: Self) -> Self;
}
impl Ssse3U32x4 for u32x4 {
    #[inline]
    fn hadd(self, other: Self) -> Self {
        unsafe { bitcast!(x86_mm_hadd_epi32(self, other)) }
    }
    #[inline]
    fn hsub(self, other: Self) -> Self {
        unsafe { bitcast!(x86_mm_hsub_epi32(self, other)) }
    }
}

// 16 bit integers

pub trait Ssse3I16x8 {
    fn abs(self) -> Self;
    fn hadd(self, other: Self) -> Self;
    fn hadds(self, other: Self) -> Self;
    fn hsub(self, other: Self) -> Self;
    fn hsubs(self, other: Self) -> Self;
    fn sign(self, other: Self) -> Self;
    fn mulhrs(self, other: Self) -> Self;
}
impl Ssse3I16x8 for i16x8 {
    #[inline]
    fn abs(self) -> Self {
        unsafe { x86_mm_abs_epi16(self) }
    }

    #[inline]
    fn hadd(self, other: Self) -> Self {
        unsafe { x86_mm_hadd_epi16(self, other) }
    }
    #[inline]
    fn hadds(self, other: Self) -> Self {
        unsafe { x86_mm_hadds_epi16(self, other) }
    }
    #[inline]
    fn hsub(self, other: Self) -> Self {
        unsafe { x86_mm_hsub_epi16(self, other) }
    }
    #[inline]
    fn hsubs(self, other: Self) -> Self {
        unsafe { x86_mm_hsubs_epi16(self, other) }
    }

    #[inline]
    fn sign(self, other: Self) -> Self {
        unsafe { x86_mm_sign_epi16(self, other) }
    }

    #[inline]
    fn mulhrs(self, other: Self) -> Self {
        unsafe { x86_mm_mulhrs_epi16(self, other) }
    }
}

pub trait Ssse3U16x8 {
    fn hadd(self, other: Self) -> Self;
    fn hsub(self, other: Self) -> Self;
}
impl Ssse3U16x8 for u16x8 {
    #[inline]
    fn hadd(self, other: Self) -> Self {
        unsafe { bitcast!(x86_mm_hadd_epi16(self, other)) }
    }
    #[inline]
    fn hsub(self, other: Self) -> Self {
        unsafe { bitcast!(x86_mm_hsub_epi16(self, other)) }
    }
}


// 8 bit integers

pub trait Ssse3U8x16 {
    fn shuffle_bytes(self, indices: Self) -> Self;
    fn maddubs(self, other: i8x16) -> i16x8;
}

impl Ssse3U8x16 for u8x16 {
    #[inline]
    fn shuffle_bytes(self, indices: Self) -> Self {
        unsafe {bitcast!(x86_mm_shuffle_epi8(self, indices))}
    }

    fn maddubs(self, other: i8x16) -> i16x8 {
        unsafe { x86_mm_maddubs_epi16(self, other) }
    }
}

pub trait Ssse3I8x16 {
    fn abs(self) -> Self;
    fn shuffle_bytes(self, indices: Self) -> Self;
    fn sign(self, other: Self) -> Self;
}
impl Ssse3I8x16 for i8x16 {
    #[inline]
    fn abs(self) -> Self {
        unsafe {x86_mm_abs_epi8(self)}
    }
    #[inline]
    fn shuffle_bytes(self, indices: Self) -> Self {
        unsafe {
            x86_mm_shuffle_epi8(self, indices)
        }
    }

    #[inline]
    fn sign(self, other: Self) -> Self {
        unsafe { x86_mm_sign_epi8(self, other) }
    }
}
