use sixty_four::*;
use super::super::*;

extern "platform-intrinsic" {
    fn x86_mm_addsub_ps(x: f32x4, y: f32x4) -> f32x4;
    fn x86_mm_addsub_pd(x: f64x2, y: f64x2) -> f64x2;
    fn x86_mm_hadd_ps(x: f32x4, y: f32x4) -> f32x4;
    fn x86_mm_hadd_pd(x: f64x2, y: f64x2) -> f64x2;
    fn x86_mm_hsub_ps(x: f32x4, y: f32x4) -> f32x4;
    fn x86_mm_hsub_pd(x: f64x2, y: f64x2) -> f64x2;
}

pub trait Sse3F32x4 {
    fn addsub(self, other: Self) -> Self;
    fn hadd(self, other: Self) -> Self;
    fn hsub(self, other: Self) -> Self;
}

impl Sse3F32x4 for f32x4 {
    #[inline]
    fn addsub(self, other: Self) -> Self {
        unsafe { x86_mm_addsub_ps(self, other) }
    }

    #[inline]
    fn hadd(self, other: Self) -> Self {
        unsafe { x86_mm_hadd_ps(self, other) }
    }

    #[inline]
    fn hsub(self, other: Self) -> Self {
        unsafe { x86_mm_hsub_ps(self, other) }
    }
}

pub trait Sse3F64x2 {
    fn addsub(self, other: Self) -> Self;
    fn hadd(self, other: Self) -> Self;
    fn hsub(self, other: Self) -> Self;
}

impl Sse3F64x2 for f64x2 {
    #[inline]
    fn addsub(self, other: Self) -> Self {
        unsafe { x86_mm_addsub_pd(self, other) }
    }

    #[inline]
    fn hadd(self, other: Self) -> Self {
        unsafe { x86_mm_hadd_pd(self, other) }
    }

    #[inline]
    fn hsub(self, other: Self) -> Self {
        unsafe { x86_mm_hsub_pd(self, other) }
    }
}
