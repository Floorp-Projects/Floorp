//! Approximation for floating-point `mul_add`
use crate::*;

// FIXME: 64-bit 1 element mul_adde

pub(crate) trait MulAddE {
    fn mul_adde(self, y: Self, z: Self) -> Self;
}

#[cfg(not(target_arch = "s390x"))]
#[allow(improper_ctypes)]
extern "C" {
    #[link_name = "llvm.fmuladd.v2f32"]
    fn fmuladd_v2f32(x: f32x2, y: f32x2, z: f32x2) -> f32x2;
    #[link_name = "llvm.fmuladd.v4f32"]
    fn fmuladd_v4f32(x: f32x4, y: f32x4, z: f32x4) -> f32x4;
    #[link_name = "llvm.fmuladd.v8f32"]
    fn fmuladd_v8f32(x: f32x8, y: f32x8, z: f32x8) -> f32x8;
    #[link_name = "llvm.fmuladd.v16f32"]
    fn fmuladd_v16f32(x: f32x16, y: f32x16, z: f32x16) -> f32x16;
    /* FIXME 64-bit single elem vectors
    #[link_name = "llvm.fmuladd.v1f64"]
    fn fmuladd_v1f64(x: f64x1, y: f64x1, z: f64x1) -> f64x1;
    */
    #[link_name = "llvm.fmuladd.v2f64"]
    fn fmuladd_v2f64(x: f64x2, y: f64x2, z: f64x2) -> f64x2;
    #[link_name = "llvm.fmuladd.v4f64"]
    fn fmuladd_v4f64(x: f64x4, y: f64x4, z: f64x4) -> f64x4;
    #[link_name = "llvm.fmuladd.v8f64"]
    fn fmuladd_v8f64(x: f64x8, y: f64x8, z: f64x8) -> f64x8;
}

macro_rules! impl_mul_adde {
    ($id:ident : $fn:ident) => {
        impl MulAddE for $id {
            #[inline]
            fn mul_adde(self, y: Self, z: Self) -> Self {
                #[cfg(not(target_arch = "s390x"))]
                {
                    use crate::mem::transmute;
                    unsafe { transmute($fn(transmute(self), transmute(y), transmute(z))) }
                }
                #[cfg(target_arch = "s390x")]
                {
                    // FIXME: https://github.com/rust-lang-nursery/packed_simd/issues/14
                    self * y + z
                }
            }
        }
    };
}

impl_mul_adde!(f32x2: fmuladd_v2f32);
impl_mul_adde!(f32x4: fmuladd_v4f32);
impl_mul_adde!(f32x8: fmuladd_v8f32);
impl_mul_adde!(f32x16: fmuladd_v16f32);
// impl_mul_adde!(f64x1: fma_v1f64); // FIXME 64-bit fmagle elem vectors
impl_mul_adde!(f64x2: fmuladd_v2f64);
impl_mul_adde!(f64x4: fmuladd_v4f64);
impl_mul_adde!(f64x8: fmuladd_v8f64);
