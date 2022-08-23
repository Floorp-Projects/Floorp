//! Vertical floating-point `cos`
#![allow(unused)]

// FIXME 64-bit 1 elem vector cos

use crate::*;

crate trait Cos {
    fn cos(self) -> Self;
}

#[allow(improper_ctypes)]
extern "C" {
    #[link_name = "llvm.cos.v2f32"]
    fn cos_v2f32(x: f32x2) -> f32x2;
    #[link_name = "llvm.cos.v4f32"]
    fn cos_v4f32(x: f32x4) -> f32x4;
    #[link_name = "llvm.cos.v8f32"]
    fn cos_v8f32(x: f32x8) -> f32x8;
    #[link_name = "llvm.cos.v16f32"]
    fn cos_v16f32(x: f32x16) -> f32x16;
    /* FIXME 64-bit cosgle elem vectors
    #[link_name = "llvm.cos.v1f64"]
    fn cos_v1f64(x: f64x1) -> f64x1;
     */
    #[link_name = "llvm.cos.v2f64"]
    fn cos_v2f64(x: f64x2) -> f64x2;
    #[link_name = "llvm.cos.v4f64"]
    fn cos_v4f64(x: f64x4) -> f64x4;
    #[link_name = "llvm.cos.v8f64"]
    fn cos_v8f64(x: f64x8) -> f64x8;

    #[link_name = "llvm.cos.f32"]
    fn cos_f32(x: f32) -> f32;
    #[link_name = "llvm.cos.f64"]
    fn cos_f64(x: f64) -> f64;
}

gen_unary_impl_table!(Cos, cos);

cfg_if! {
    if #[cfg(target_arch = "s390x")] {
        // FIXME: https://github.com/rust-lang-nursery/packed_simd/issues/14
        impl_unary!(f32x2[f32; 2]: cos_f32);
        impl_unary!(f32x4[f32; 4]: cos_f32);
        impl_unary!(f32x8[f32; 8]: cos_f32);
        impl_unary!(f32x16[f32; 16]: cos_f32);

        impl_unary!(f64x2[f64; 2]: cos_f64);
        impl_unary!(f64x4[f64; 4]: cos_f64);
        impl_unary!(f64x8[f64; 8]: cos_f64);
    } else if #[cfg(all(target_arch = "x86_64", feature = "sleef-sys"))] {
        use sleef_sys::*;
        cfg_if! {
            if #[cfg(target_feature = "avx2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_cosf4_u10avx2128);
                impl_unary!(f32x16[h => f32x8]: Sleef_cosf8_u10avx2);
                impl_unary!(f64x8[h => f64x4]: Sleef_cosd4_u10avx2);

                impl_unary!(f32x4: Sleef_cosf4_u10avx2128);
                impl_unary!(f32x8: Sleef_cosf8_u10avx2);
                impl_unary!(f64x2: Sleef_cosd2_u10avx2128);
                impl_unary!(f64x4: Sleef_cosd4_u10avx2);
            } else if #[cfg(target_feature = "avx")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_cosf4_u10sse4);
                impl_unary!(f32x16[h => f32x8]: Sleef_cosf8_u10avx);
                impl_unary!(f64x8[h => f64x4]: Sleef_cosd4_u10avx);

                impl_unary!(f32x4: Sleef_cosf4_u10sse4);
                impl_unary!(f32x8: Sleef_cosf8_u10avx);
                impl_unary!(f64x2: Sleef_cosd2_u10sse4);
                impl_unary!(f64x4: Sleef_cosd4_u10avx);
            } else if #[cfg(target_feature = "sse4.2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_cosf4_u10sse4);
                impl_unary!(f32x16[q => f32x4]: Sleef_cosf4_u10sse4);
                impl_unary!(f64x8[q => f64x2]: Sleef_cosd2_u10sse4);

                impl_unary!(f32x4: Sleef_cosf4_u10sse4);
                impl_unary!(f32x8[h => f32x4]: Sleef_cosf4_u10sse4);
                impl_unary!(f64x2: Sleef_cosd2_u10sse4);
                impl_unary!(f64x4[h => f64x2]: Sleef_cosd2_u10sse4);
            } else {
                impl_unary!(f32x2[f32; 2]: cos_f32);
                impl_unary!(f32x16: cos_v16f32);
                impl_unary!(f64x8: cos_v8f64);

                impl_unary!(f32x4: cos_v4f32);
                impl_unary!(f32x8: cos_v8f32);
                impl_unary!(f64x2: cos_v2f64);
                impl_unary!(f64x4: cos_v4f64);
            }
        }
    } else {
        impl_unary!(f32x2[f32; 2]: cos_f32);
        impl_unary!(f32x4: cos_v4f32);
        impl_unary!(f32x8: cos_v8f32);
        impl_unary!(f32x16: cos_v16f32);

        impl_unary!(f64x2: cos_v2f64);
        impl_unary!(f64x4: cos_v4f64);
        impl_unary!(f64x8: cos_v8f64);
    }
}
