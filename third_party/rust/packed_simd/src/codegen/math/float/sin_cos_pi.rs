//! Vertical floating-point `sin_cos`
#![allow(unused)]

// FIXME 64-bit 1 elem vectors sin_cos

use crate::*;

pub(crate) trait SinCosPi: Sized {
    type Output;
    fn sin_cos_pi(self) -> Self::Output;
}

macro_rules! impl_def {
    ($vid:ident, $PI:path) => {
        impl SinCosPi for $vid {
            type Output = (Self, Self);
            #[inline]
            fn sin_cos_pi(self) -> Self::Output {
                let v = self * Self::splat($PI);
                (v.sin(), v.cos())
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

macro_rules! impl_unary_t {
    ($vid:ident: $fun:ident) => {
        impl SinCosPi for $vid {
            type Output = (Self, Self);
            fn sin_cos_pi(self) -> Self::Output {
                unsafe {
                    use crate::mem::transmute;
                    transmute($fun(transmute(self)))
                }
            }
        }
    };
    ($vid:ident[t => $vid_t:ident]: $fun:ident) => {
        impl SinCosPi for $vid {
            type Output = (Self, Self);
            fn sin_cos_pi(self) -> Self::Output {
                unsafe {
                    use crate::mem::{transmute, uninitialized};

                    union U {
                        vec: [$vid; 2],
                        twice: $vid_t,
                    }

                    let twice = U { vec: [self, uninitialized()] }.twice;
                    let twice = transmute($fun(transmute(twice)));

                    union R {
                        twice: ($vid_t, $vid_t),
                        vecs: ([$vid; 2], [$vid; 2]),
                    }
                    let r = R { twice }.vecs;
                    (*r.0.get_unchecked(0), *r.0.get_unchecked(1))
                }
            }
        }
    };
    ($vid:ident[h => $vid_h:ident]: $fun:ident) => {
        impl SinCosPi for $vid {
            type Output = (Self, Self);
            fn sin_cos_pi(self) -> Self::Output {
                unsafe {
                    use crate::mem::transmute;

                    union U {
                        vec: $vid,
                        halves: [$vid_h; 2],
                    }

                    let halves = U { vec: self }.halves;

                    let res_0: ($vid_h, $vid_h) = transmute($fun(transmute(*halves.get_unchecked(0))));
                    let res_1: ($vid_h, $vid_h) = transmute($fun(transmute(*halves.get_unchecked(1))));

                    union R {
                        result: ($vid, $vid),
                        halves: ([$vid_h; 2], [$vid_h; 2]),
                    }
                    R { halves: ([res_0.0, res_1.0], [res_0.1, res_1.1]) }.result
                }
            }
        }
    };
    ($vid:ident[q => $vid_q:ident]: $fun:ident) => {
        impl SinCosPi for $vid {
            type Output = (Self, Self);
            fn sin_cos_pi(self) -> Self::Output {
                unsafe {
                    use crate::mem::transmute;

                    union U {
                        vec: $vid,
                        quarters: [$vid_q; 4],
                    }

                    let quarters = U { vec: self }.quarters;

                    let res_0: ($vid_q, $vid_q) = transmute($fun(transmute(*quarters.get_unchecked(0))));
                    let res_1: ($vid_q, $vid_q) = transmute($fun(transmute(*quarters.get_unchecked(1))));
                    let res_2: ($vid_q, $vid_q) = transmute($fun(transmute(*quarters.get_unchecked(2))));
                    let res_3: ($vid_q, $vid_q) = transmute($fun(transmute(*quarters.get_unchecked(3))));

                    union R {
                        result: ($vid, $vid),
                        quarters: ([$vid_q; 4], [$vid_q; 4]),
                    }
                    R {
                        quarters: (
                            [res_0.0, res_1.0, res_2.0, res_3.0],
                            [res_0.1, res_1.1, res_2.1, res_3.1],
                        ),
                    }
                    .result
                }
            }
        }
    };
}

cfg_if! {
    if #[cfg(all(target_arch = "x86_64", feature = "sleef-sys"))] {
        use sleef_sys::*;
        cfg_if! {
            if #[cfg(target_feature = "avx2")] {
                impl_unary_t!(f32x2[t => f32x4]: Sleef_sincospif4_u05avx2128);
                impl_unary_t!(f32x16[h => f32x8]: Sleef_sincospif8_u05avx2);
                impl_unary_t!(f64x8[h => f64x4]: Sleef_sincospid4_u05avx2);

                impl_unary_t!(f32x4: Sleef_sincospif4_u05avx2128);
                impl_unary_t!(f32x8: Sleef_sincospif8_u05avx2);
                impl_unary_t!(f64x2: Sleef_sincospid2_u05avx2128);
                impl_unary_t!(f64x4: Sleef_sincospid4_u05avx2);
            } else if #[cfg(target_feature = "avx")] {
                impl_unary_t!(f32x2[t => f32x4]: Sleef_sincospif4_u05sse4);
                impl_unary_t!(f32x16[h => f32x8]: Sleef_sincospif8_u05avx);
                impl_unary_t!(f64x8[h => f64x4]: Sleef_sincospid4_u05avx);

                impl_unary_t!(f32x4: Sleef_sincospif4_u05sse4);
                impl_unary_t!(f32x8: Sleef_sincospif8_u05avx);
                impl_unary_t!(f64x2: Sleef_sincospid2_u05sse4);
                impl_unary_t!(f64x4: Sleef_sincospid4_u05avx);
            } else if #[cfg(target_feature = "sse4.2")] {
                impl_unary_t!(f32x2[t => f32x4]: Sleef_sincospif4_u05sse4);
                impl_unary_t!(f32x16[q => f32x4]: Sleef_sincospif4_u05sse4);
                impl_unary_t!(f64x8[q => f64x2]: Sleef_sincospid2_u05sse4);

                impl_unary_t!(f32x4: Sleef_sincospif4_u05sse4);
                impl_unary_t!(f32x8[h => f32x4]: Sleef_sincospif4_u05sse4);
                impl_unary_t!(f64x2: Sleef_sincospid2_u05sse4);
                impl_unary_t!(f64x4[h => f64x2]: Sleef_sincospid2_u05sse4);
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
