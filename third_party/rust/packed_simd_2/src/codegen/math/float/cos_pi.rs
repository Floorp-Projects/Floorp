//! Vertical floating-point `cos`
#![allow(unused)]

// FIXME 64-bit 1 elem vectors cos_pi

use crate::*;

crate trait CosPi {
    fn cos_pi(self) -> Self;
}

gen_unary_impl_table!(CosPi, cos_pi);

macro_rules! impl_def {
    ($vid:ident, $PI:path) => {
        impl CosPi for $vid {
            #[inline]
            fn cos_pi(self) -> Self {
                (self * Self::splat($PI)).cos()
            }
        }
    };
}
macro_rules! impl_def32 {
    ($vid:ident) => {
        impl_def!($vid, crate::f32::consts::PI);
    };
}
macro_rules! impl_def64 {
    ($vid:ident) => {
        impl_def!($vid, crate::f64::consts::PI);
    };
}

cfg_if! {
    if #[cfg(all(target_arch = "x86_64", feature = "sleef-sys"))] {
        use sleef_sys::*;
        cfg_if! {
            if #[cfg(target_feature = "avx2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_cospif4_u05avx2128);
                impl_unary!(f32x16[h => f32x8]: Sleef_cospif8_u05avx2);
                impl_unary!(f64x8[h => f64x4]: Sleef_cospid4_u05avx2);

                impl_unary!(f32x4: Sleef_cospif4_u05avx2128);
                impl_unary!(f32x8: Sleef_cospif8_u05avx2);
                impl_unary!(f64x2: Sleef_cospid2_u05avx2128);
                impl_unary!(f64x4: Sleef_cospid4_u05avx2);
            } else if #[cfg(target_feature = "avx")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_cospif4_u05sse4);
                impl_unary!(f32x16[h => f32x8]: Sleef_cospif8_u05avx);
                impl_unary!(f64x8[h => f64x4]: Sleef_cospid4_u05avx);

                impl_unary!(f32x4: Sleef_cospif4_u05sse4);
                impl_unary!(f32x8: Sleef_cospif8_u05avx);
                impl_unary!(f64x2: Sleef_cospid2_u05sse4);
                impl_unary!(f64x4: Sleef_cospid4_u05avx);
            } else if #[cfg(target_feature = "sse4.2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_cospif4_u05sse4);
                impl_unary!(f32x16[q => f32x4]: Sleef_cospif4_u05sse4);
                impl_unary!(f64x8[q => f64x2]: Sleef_cospid2_u05sse4);

                impl_unary!(f32x4: Sleef_cospif4_u05sse4);
                impl_unary!(f32x8[h => f32x4]: Sleef_cospif4_u05sse4);
                impl_unary!(f64x2: Sleef_cospid2_u05sse4);
                impl_unary!(f64x4[h => f64x2]: Sleef_cospid2_u05sse4);
            } else {
                impl_def32!(f32x2);
                impl_def32!(f32x4);
                impl_def32!(f32x8);
                impl_def32!(f32x16);

                impl_def64!(f64x2);
                impl_def64!(f64x4);
                impl_def64!(f64x8);
            }
        }
    } else {
        impl_def32!(f32x2);
        impl_def32!(f32x4);
        impl_def32!(f32x8);
        impl_def32!(f32x16);

        impl_def64!(f64x2);
        impl_def64!(f64x4);
        impl_def64!(f64x8);
    }
}
