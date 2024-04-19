//! Vertical floating-point `tanh`
#![allow(unused)]

// FIXME 64-bit 1 elem vectors tanh

#[cfg(not(feature = "std"))]
use num_traits::Float;

use crate::*;

pub(crate) trait Tanh {
    fn tanh(self) -> Self;
}

macro_rules! define_tanh {
    ($name:ident, $basetype:ty, $simdtype:ty, $lanes:expr, $trait:path) => {
        fn $name(x: $simdtype) -> $simdtype {
            use core::intrinsics::transmute;
            let mut buf: [$basetype; $lanes] = unsafe { transmute(x) };
            for elem in &mut buf {
                *elem = <$basetype as $trait>::tanh(*elem);
            }
            unsafe { transmute(buf) }
        }
    };

    (f32 => $name:ident, $type:ty, $lanes:expr) => {
        define_tanh!($name, f32, $type, $lanes, Float);
    };

    (f64 => $name:ident, $type:ty, $lanes:expr) => {
        define_tanh!($name, f64, $type, $lanes, Float);
    };
}

// llvm does not seem to expose the hyperbolic versions of trigonometric
// functions; we thus call the classical rust versions on all of them (which
// stem from cmath).
define_tanh!(f32 => tanh_v2f32, f32x2, 2);
define_tanh!(f32 => tanh_v4f32, f32x4, 4);
define_tanh!(f32 => tanh_v8f32, f32x8, 8);
define_tanh!(f32 => tanh_v16f32, f32x16, 16);

define_tanh!(f64 => tanh_v2f64, f64x2, 2);
define_tanh!(f64 => tanh_v4f64, f64x4, 4);
define_tanh!(f64 => tanh_v8f64, f64x8, 8);

fn tanh_f32(x: f32) -> f32 {
    Float::tanh(x)
}

fn tanh_f64(x: f64) -> f64 {
    Float::tanh(x)
}

gen_unary_impl_table!(Tanh, tanh);

cfg_if! {
    if #[cfg(target_arch = "s390x")] {
        // FIXME: https://github.com/rust-lang-nursery/packed_simd/issues/14
        impl_unary!(f32x2[f32; 2]: tanh_f32);
        impl_unary!(f32x4[f32; 4]: tanh_f32);
        impl_unary!(f32x8[f32; 8]: tanh_f32);
        impl_unary!(f32x16[f32; 16]: tanh_f32);

        impl_unary!(f64x2[f64; 2]: tanh_f64);
        impl_unary!(f64x4[f64; 4]: tanh_f64);
        impl_unary!(f64x8[f64; 8]: tanh_f64);
    } else if #[cfg(all(target_arch = "x86_64", feature = "sleef-sys"))] {
        use sleef_sys::*;
        cfg_if! {
            if #[cfg(target_feature = "avx2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_tanhf4_u10avx2128);
                impl_unary!(f32x16[h => f32x8]: Sleef_tanhf8_u10avx2);
                impl_unary!(f64x8[h => f64x4]: Sleef_tanhd4_u10avx2);

                impl_unary!(f32x4: Sleef_tanhf4_u10avx2128);
                impl_unary!(f32x8: Sleef_tanhf8_u10avx2);
                impl_unary!(f64x2: Sleef_tanhd2_u10avx2128);
                impl_unary!(f64x4: Sleef_tanhd4_u10avx2);
            } else if #[cfg(target_feature = "avx")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_tanhf4_u10sse4);
                impl_unary!(f32x16[h => f32x8]: Sleef_tanhf8_u10avx);
                impl_unary!(f64x8[h => f64x4]: Sleef_tanhd4_u10avx);

                impl_unary!(f32x4: Sleef_tanhf4_u10sse4);
                impl_unary!(f32x8: Sleef_tanhf8_u10avx);
                impl_unary!(f64x2: Sleef_tanhd2_u10sse4);
                impl_unary!(f64x4: Sleef_tanhd4_u10avx);
            } else if #[cfg(target_feature = "sse4.2")] {
                impl_unary!(f32x2[t => f32x4]: Sleef_tanhf4_u10sse4);
                impl_unary!(f32x16[q => f32x4]: Sleef_tanhf4_u10sse4);
                impl_unary!(f64x8[q => f64x2]: Sleef_tanhd2_u10sse4);

                impl_unary!(f32x4: Sleef_tanhf4_u10sse4);
                impl_unary!(f32x8[h => f32x4]: Sleef_tanhf4_u10sse4);
                impl_unary!(f64x2: Sleef_tanhd2_u10sse4);
                impl_unary!(f64x4[h => f64x2]: Sleef_tanhd2_u10sse4);
            } else {
                impl_unary!(f32x2[f32; 2]: tanh_f32);
                impl_unary!(f32x16: tanh_v16f32);
                impl_unary!(f64x8: tanh_v8f64);

                impl_unary!(f32x4: tanh_v4f32);
                impl_unary!(f32x8: tanh_v8f32);
                impl_unary!(f64x2: tanh_v2f64);
                impl_unary!(f64x4: tanh_v4f64);
            }
        }
    } else {
        impl_unary!(f32x2[f32; 2]: tanh_f32);
        impl_unary!(f32x4: tanh_v4f32);
        impl_unary!(f32x8: tanh_v8f32);
        impl_unary!(f32x16: tanh_v16f32);

        impl_unary!(f64x2: tanh_v2f64);
        impl_unary!(f64x4: tanh_v4f64);
        impl_unary!(f64x8: tanh_v8f64);
    }
}
