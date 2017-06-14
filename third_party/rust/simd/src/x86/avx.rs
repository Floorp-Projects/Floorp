use super::super::*;
use sixty_four::*;

use super::super::bitcast;

pub use v256::{
    f64x4, bool64fx4, u64x4, i64x4, bool64ix4,
    f32x8, bool32fx8, u32x8, i32x8, bool32ix8,
    u16x16, i16x16, bool16ix16,
    u8x32, i8x32, bool8ix32,
    LowHigh128
};

#[allow(dead_code)]
extern "platform-intrinsic" {
    fn x86_mm256_addsub_ps(x: f32x8, y: f32x8) -> f32x8;
    fn x86_mm256_addsub_pd(x: f64x4, y: f64x4) -> f64x4;
    fn x86_mm256_dp_ps(x: f32x8, y: f32x8, z: i32) -> f32x8;
    fn x86_mm256_hadd_ps(x: f32x8, y: f32x8) -> f32x8;
    fn x86_mm256_hadd_pd(x: f64x4, y: f64x4) -> f64x4;
    fn x86_mm256_hsub_ps(x: f32x8, y: f32x8) -> f32x8;
    fn x86_mm256_hsub_pd(x: f64x4, y: f64x4) -> f64x4;
    fn x86_mm256_max_ps(x: f32x8, y: f32x8) -> f32x8;
    fn x86_mm256_max_pd(x: f64x4, y: f64x4) -> f64x4;
    fn x86_mm256_min_ps(x: f32x8, y: f32x8) -> f32x8;
    fn x86_mm256_min_pd(x: f64x4, y: f64x4) -> f64x4;
    fn x86_mm256_movemask_ps(x: f32x8) -> i32;
    fn x86_mm256_movemask_pd(x: f64x4) -> i32;
    fn x86_mm_permutevar_ps(x: f32x4, y: i32x4) -> f32x4;
    fn x86_mm_permutevar_pd(x: f64x2, y: i64x2) -> f64x2;
    fn x86_mm256_permutevar_ps(x: f32x8, y: i32x8) -> f32x8;
    fn x86_mm256_permutevar_pd(x: f64x4, y: i64x4) -> f64x4;
    fn x86_mm256_rcp_ps(x: f32x8) -> f32x8;
    fn x86_mm256_rsqrt_ps(x: f32x8) -> f32x8;
    fn x86_mm256_sqrt_ps(x: f32x8) -> f32x8;
    fn x86_mm256_sqrt_pd(x: f64x4) -> f64x4;
    fn x86_mm_testc_ps(x: f32x4, y: f32x4) -> i32;
    fn x86_mm256_testc_ps(x: f32x8, y: f32x8) -> i32;
    fn x86_mm_testc_pd(x: f64x2, y: f64x2) -> i32;
    fn x86_mm256_testc_pd(x: f64x4, y: f64x4) -> i32;
    fn x86_mm256_testc_si256(x: u64x4, y: u64x4) -> i32;
    fn x86_mm_testnzc_ps(x: f32x4, y: f32x4) -> i32;
    fn x86_mm256_testnzc_ps(x: f32x8, y: f32x8) -> i32;
    fn x86_mm_testnzc_pd(x: f64x2, y: f64x2) -> i32;
    fn x86_mm256_testnzc_pd(x: f64x4, y: f64x4) -> i32;
    fn x86_mm256_testnzc_si256(x: u64x4, y: u64x4) -> i32;
    fn x86_mm_testz_ps(x: f32x4, y: f32x4) -> i32;
    fn x86_mm256_testz_ps(x: f32x8, y: f32x8) -> i32;
    fn x86_mm_testz_pd(x: f64x2, y: f64x2) -> i32;
    fn x86_mm256_testz_pd(x: f64x4, y: f64x4) -> i32;
    fn x86_mm256_testz_si256(x: u64x4, y: u64x4) -> i32;
}

#[doc(hidden)]
pub mod common {
    use super::*;
    use std::mem;

    macro_rules! bools {
        ($($ty: ty, $all: ident, $any: ident, $testc: ident, $testz: ident;)*) => {
            $(
                #[inline]
                pub fn $all(x: $ty) -> bool {
                    unsafe {
                        super::$testc(mem::transmute(x), mem::transmute(<$ty>::splat(true))) != 0
                    }
                }
                #[inline]
                pub fn $any(x: $ty) -> bool {
                    unsafe {
                        super::$testz(mem::transmute(x), mem::transmute(x)) == 0
                    }
                }
                )*
        }
    }

    bools! {
        bool32fx8, bool32fx8_all, bool32fx8_any, x86_mm256_testc_ps, x86_mm256_testz_ps;
        bool64fx4, bool64fx4_all, bool64fx4_any, x86_mm256_testc_pd, x86_mm256_testz_pd;
        bool8ix32, bool8ix32_all, bool8ix32_any, x86_mm256_testc_si256, x86_mm256_testz_si256;
        bool16ix16, bool16ix16_all, bool16ix16_any, x86_mm256_testc_si256, x86_mm256_testz_si256;
        bool32ix8, bool32ix8_all, bool32ix8_any, x86_mm256_testc_si256, x86_mm256_testz_si256;
        bool64ix4, bool64ix4_all, bool64ix4_any, x86_mm256_testc_si256, x86_mm256_testz_si256;
    }
}

// 128-bit vectors:

// 32 bit floats

pub trait AvxF32x4 {
    fn permutevar(self, other: i32x4) -> f32x4;
}
impl AvxF32x4 for f32x4 {
    fn permutevar(self, other: i32x4) -> f32x4 {
        unsafe { x86_mm_permutevar_ps(self, other) }
    }
}

pub trait AvxF64x4 {
    fn sqrt(self) -> Self;
    fn addsub(self, other: Self) -> Self;
    fn hadd(self, other: Self) -> Self;
    fn hsub(self, other: Self) -> Self;
    fn max(self, other: Self) -> Self;
    fn min(self, other: Self) -> Self;
    fn move_mask(self) -> u32;
}

impl AvxF64x4 for f64x4 {
    #[inline]
    fn sqrt(self) -> Self {
        unsafe { x86_mm256_sqrt_pd(self) }
    }

    #[inline]
    fn addsub(self, other: Self) -> Self {
        unsafe { x86_mm256_addsub_pd(self, other) }
    }

    #[inline]
    fn hadd(self, other: Self) -> Self {
        unsafe { x86_mm256_hadd_pd(self, other) }
    }

    #[inline]
    fn hsub(self, other: Self) -> Self {
        unsafe { x86_mm256_hsub_pd(self, other) }
    }

    #[inline]
    fn max(self, other: Self) -> Self {
        unsafe { x86_mm256_max_pd(self, other) }
    }

    #[inline]
    fn min(self, other: Self) -> Self {
        unsafe { x86_mm256_min_pd(self, other) }
    }

    #[inline]
    fn move_mask(self) -> u32 {
        unsafe { x86_mm256_movemask_pd(self) as u32 }
    }
}

pub trait AvxBool64fx4 {
    fn move_mask(self) -> u32;
}
impl AvxBool64fx4 for bool64fx4 {
    #[inline]
    fn move_mask(self) -> u32 {
        unsafe { x86_mm256_movemask_pd(bitcast(self)) as u32 }
    }
}

pub trait AvxF32x8 {
    fn sqrt(self) -> Self;
    fn addsub(self, other: Self) -> Self;
    fn hadd(self, other: Self) -> Self;
    fn hsub(self, other: Self) -> Self;
    fn max(self, other: Self) -> Self;
    fn min(self, other: Self) -> Self;
    fn move_mask(self) -> u32;
    /// Compute an approximation to the reciprocal of the square root
    /// of `self`, that is, `f32x8::splat(1.0) / self.sqrt()`.
    ///
    /// The accuracy of this approximation is platform dependent.
    fn approx_rsqrt(self) -> Self;
    /// Compute an approximation to the reciprocal of `self`, that is,
    /// `f32x8::splat(1.0) / self`.
    ///
    /// The accuracy of this approximation is platform dependent.
    fn approx_reciprocal(self) -> Self;
}

impl AvxF32x8 for f32x8 {
    #[inline]
    fn sqrt(self) -> Self {
        unsafe { x86_mm256_sqrt_ps(self) }
    }

    #[inline]
    fn addsub(self, other: Self) -> Self {
        unsafe { x86_mm256_addsub_ps(self, other) }
    }

    #[inline]
    fn hadd(self, other: Self) -> Self {
        unsafe { x86_mm256_hadd_ps(self, other) }
    }

    #[inline]
    fn hsub(self, other: Self) -> Self {
        unsafe { x86_mm256_hsub_ps(self, other) }
    }

    #[inline]
    fn max(self, other: Self) -> Self {
        unsafe { x86_mm256_max_ps(self, other) }
    }

    #[inline]
    fn min(self, other: Self) -> Self {
        unsafe { x86_mm256_min_ps(self, other) }
    }

    #[inline]
    fn move_mask(self) -> u32 {
        unsafe { x86_mm256_movemask_ps(self) as u32 }
    }

    #[inline]
    fn approx_reciprocal(self) -> Self {
        unsafe { x86_mm256_rcp_ps(self) }
    }

    #[inline]
    fn approx_rsqrt(self) -> Self {
        unsafe { x86_mm256_rsqrt_ps(self) }
    }
}

pub trait AvxBool32fx8 {
    fn move_mask(self) -> u32;
}
impl AvxBool32fx8 for bool32fx8 {
    #[inline]
    fn move_mask(self) -> u32 {
        unsafe { x86_mm256_movemask_ps(bitcast(self)) as u32 }
    }
}

pub trait AvxBool32fx4 {}
impl AvxBool32fx4 for bool32fx4 {}

// 64 bit floats

pub trait AvxF64x2 {
    fn permutevar(self, other: i64x2) -> f64x2;
}
impl AvxF64x2 for f64x2 {
    fn permutevar(self, other: i64x2) -> f64x2 {
        unsafe { x86_mm_permutevar_pd(self, other) }
    }
}

pub trait AvxBool64fx2 {}
impl AvxBool64fx2 for bool64fx2 {}

// 64 bit integers

pub trait AvxU64x2 {}
impl AvxU64x2 for u64x2 {}
pub trait AvxI64x2 {}
impl AvxI64x2 for i64x2 {}

pub trait AvxBool64ix2 {}
impl AvxBool64ix2 for bool64ix2 {}

// 32 bit integers

pub trait AvxU32x4 {}
impl AvxU32x4 for u32x4 {}
pub trait AvxI32x4 {}
impl AvxI32x4 for i32x4 {}

pub trait AvxBool32ix4 {}
impl AvxBool32ix4 for bool32ix4 {}

// 16 bit integers

pub trait AvxU16x8 {}
impl AvxU16x8 for u16x8 {}
pub trait AvxI16x8 {}
impl AvxI16x8 for i16x8 {}

pub trait AvxBool16ix8 {}
impl AvxBool16ix8 for bool16ix8 {}

// 8 bit integers

pub trait AvxU8x16 {}
impl AvxU8x16 for u8x16 {}
pub trait AvxI8x16 {}
impl AvxI8x16 for i8x16 {}

pub trait AvxBool8ix16 {}
impl AvxBool8ix16 for bool8ix16 {}
