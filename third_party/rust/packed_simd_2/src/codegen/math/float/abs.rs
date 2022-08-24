//! Vertical floating-point `fabs`
#![allow(unused)]

// FIXME 64-bit 1 elem vectors fabs

use crate::*;

pub(crate) trait Abs {
    fn abs(self) -> Self;
}

#[allow(improper_ctypes)]
extern "C" {
    #[link_name = "llvm.fabs.v2f32"]
    fn fabs_v2f32(x: f32x2) -> f32x2;
    #[link_name = "llvm.fabs.v4f32"]
    fn fabs_v4f32(x: f32x4) -> f32x4;
    #[link_name = "llvm.fabs.v8f32"]
    fn fabs_v8f32(x: f32x8) -> f32x8;
    #[link_name = "llvm.fabs.v16f32"]
    fn fabs_v16f32(x: f32x16) -> f32x16;
    /* FIXME 64-bit fabsgle elem vectors
    #[link_name = "llvm.fabs.v1f64"]
    fn fabs_v1f64(x: f64x1) -> f64x1;
     */
    #[link_name = "llvm.fabs.v2f64"]
    fn fabs_v2f64(x: f64x2) -> f64x2;
    #[link_name = "llvm.fabs.v4f64"]
    fn fabs_v4f64(x: f64x4) -> f64x4;
    #[link_name = "llvm.fabs.v8f64"]
    fn fabs_v8f64(x: f64x8) -> f64x8;

    #[link_name = "llvm.fabs.f32"]
    fn fabs_f32(x: f32) -> f32;
    #[link_name = "llvm.fabs.f64"]
    fn fabs_f64(x: f64) -> f64;
}

gen_unary_impl_table!(Abs, abs);

cfg_if! {
    if #[cfg(target_arch = "s390x")] {
        // FIXME: https://github.com/rust-lang-nursery/packed_simd/issues/14
        impl_unary!(f32x2[f32; 2]: fabs_f32);
        impl_unary!(f32x4[f32; 4]: fabs_f32);
        impl_unary!(f32x8[f32; 8]: fabs_f32);
        impl_unary!(f32x16[f32; 16]: fabs_f32);

        impl_unary!(f64x2[f64; 2]: fabs_f64);
        impl_unary!(f64x4[f64; 4]: fabs_f64);
        impl_unary!(f64x8[f64; 8]: fabs_f64);
    } else if #[cfg(all(target_arch = "x86_64", feature = "sleef-sys"))] {
        use sleef_sys::*;
        cfg_if! {
            if #[cfg(target_feature = "avx2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_fabsf4_avx2128);
                impl_unary!(f32x16[h => f32x8]: Sleef_fabsf8_avx2);
                impl_unary!(f64x8[h => f64x4]: Sleef_fabsd4_avx2);

                impl_unary!(f32x4: Sleef_fabsf4_avx2128);
                impl_unary!(f32x8: Sleef_fabsf8_avx2);
                impl_unary!(f64x2: Sleef_fabsd2_avx2128);
                impl_unary!(f64x4: Sleef_fabsd4_avx2);
            } else if #[cfg(target_feature = "avx")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_fabsf4_sse4);
                impl_unary!(f32x16[h => f32x8]: Sleef_fabsf8_avx);
                impl_unary!(f64x8[h => f64x4]: Sleef_fabsd4_avx);

                impl_unary!(f32x4: Sleef_fabsf4_sse4);
                impl_unary!(f32x8: Sleef_fabsf8_avx);
                impl_unary!(f64x2: Sleef_fabsd2_sse4);
                impl_unary!(f64x4: Sleef_fabsd4_avx);
            } else if #[cfg(target_feature = "sse4.2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_fabsf4_sse4);
                impl_unary!(f32x16[q => f32x4]: Sleef_fabsf4_sse4);
                impl_unary!(f64x8[q => f64x2]: Sleef_fabsd2_sse4);

                impl_unary!(f32x4: Sleef_fabsf4_sse4);
                impl_unary!(f32x8[h => f32x4]: Sleef_fabsf4_sse4);
                impl_unary!(f64x2: Sleef_fabsd2_sse4);
                impl_unary!(f64x4[h => f64x2]: Sleef_fabsd2_sse4);
            } else {
                impl_unary!(f32x2[f32; 2]: fabs_f32);
                impl_unary!(f32x16: fabs_v16f32);
                impl_unary!(f64x8: fabs_v8f64);

                impl_unary!(f32x4: fabs_v4f32);
                impl_unary!(f32x8: fabs_v8f32);
                impl_unary!(f64x2: fabs_v2f64);
                impl_unary!(f64x4: fabs_v4f64);
            }
        }
    } else {
        impl_unary!(f32x2[f32; 2]: fabs_f32);
        impl_unary!(f32x4: fabs_v4f32);
        impl_unary!(f32x8: fabs_v8f32);
        impl_unary!(f32x16: fabs_v16f32);

        impl_unary!(f64x2: fabs_v2f64);
        impl_unary!(f64x4: fabs_v4f64);
        impl_unary!(f64x8: fabs_v8f64);
    }
}
