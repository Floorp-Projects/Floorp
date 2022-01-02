//! Vertical floating-point `sqrt`
#![allow(unused)]

// FIXME 64-bit 1 elem vectors sqrte

use crate::llvm::simd_fsqrt;
use crate::*;

crate trait Sqrte {
    fn sqrte(self) -> Self;
}

gen_unary_impl_table!(Sqrte, sqrte);

cfg_if! {
    if #[cfg(all(target_arch = "x86_64", feature = "sleef-sys"))] {
        use sleef_sys::*;
        cfg_if! {
            if #[cfg(target_feature = "avx2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_sqrtf4_u35avx2128);
                impl_unary!(f32x16[h => f32x8]: Sleef_sqrtf8_u35avx2);
                impl_unary!(f64x8[h => f64x4]: Sleef_sqrtd4_u35avx2);

                impl_unary!(f32x4: Sleef_sqrtf4_u35avx2128);
                impl_unary!(f32x8: Sleef_sqrtf8_u35avx2);
                impl_unary!(f64x2: Sleef_sqrtd2_u35avx2128);
                impl_unary!(f64x4: Sleef_sqrtd4_u35avx2);
            } else if #[cfg(target_feature = "avx")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_sqrtf4_u35sse4);
                impl_unary!(f32x16[h => f32x8]: Sleef_sqrtf8_u35avx);
                impl_unary!(f64x8[h => f64x4]: Sleef_sqrtd4_u35avx);

                impl_unary!(f32x4: Sleef_sqrtf4_u35sse4);
                impl_unary!(f32x8: Sleef_sqrtf8_u35avx);
                impl_unary!(f64x2: Sleef_sqrtd2_u35sse4);
                impl_unary!(f64x4: Sleef_sqrtd4_u35avx);
            } else if #[cfg(target_feature = "sse4.2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_sqrtf4_u35sse4);
                impl_unary!(f32x16[q => f32x4]: Sleef_sqrtf4_u35sse4);
                impl_unary!(f64x8[q => f64x2]: Sleef_sqrtd2_u35sse4);

                impl_unary!(f32x4: Sleef_sqrtf4_u35sse4);
                impl_unary!(f32x8[h => f32x4]: Sleef_sqrtf4_u35sse4);
                impl_unary!(f64x2: Sleef_sqrtd2_u35sse4);
                impl_unary!(f64x4[h => f64x2]: Sleef_sqrtd2_u35sse4);
            } else {
                impl_unary!(f32x2[g]: simd_fsqrt);
                impl_unary!(f32x16[g]: simd_fsqrt);
                impl_unary!(f64x8[g]: simd_fsqrt);

                impl_unary!(f32x4[g]: simd_fsqrt);
                impl_unary!(f32x8[g]: simd_fsqrt);
                impl_unary!(f64x2[g]: simd_fsqrt);
                impl_unary!(f64x4[g]: simd_fsqrt);
            }
        }
    } else {
        impl_unary!(f32x2[g]: simd_fsqrt);
        impl_unary!(f32x4[g]: simd_fsqrt);
        impl_unary!(f32x8[g]: simd_fsqrt);
        impl_unary!(f32x16[g]: simd_fsqrt);

        impl_unary!(f64x2[g]: simd_fsqrt);
        impl_unary!(f64x4[g]: simd_fsqrt);
        impl_unary!(f64x8[g]: simd_fsqrt);
    }
}
