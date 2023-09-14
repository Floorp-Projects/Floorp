//! Implements vertical (lane-wise) floating-point `sin`.

macro_rules! impl_math_float_sin {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Sine.
            #[inline]
            pub fn sin(self) -> Self {
                use crate::codegen::math::float::sin::Sin;
                Sin::sin(self)
            }

            /// Sine of `self * PI`.
            #[inline]
            pub fn sin_pi(self) -> Self {
                use crate::codegen::math::float::sin_pi::SinPi;
                SinPi::sin_pi(self)
            }

            /// Sine and cosine of `self * PI`.
            #[inline]
            pub fn sin_cos_pi(self) -> (Self, Self) {
                use crate::codegen::math::float::sin_cos_pi::SinCosPi;
                SinCosPi::sin_cos_pi(self)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_sin>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn sin() {
                        use crate::$elem_ty::consts::PI;
                        let z = $id::splat(0 as $elem_ty);
                        let p = $id::splat(PI as $elem_ty);
                        let ph = $id::splat(PI as $elem_ty / 2.);
                        let o_r = $id::splat((PI as $elem_ty / 2.).sin());
                        let z_r = $id::splat((PI as $elem_ty).sin());

                        assert_eq!(z, z.sin());
                        assert_eq!(o_r, ph.sin());
                        assert_eq!(z_r, p.sin());
                    }
                }
            }
        }
    };
}
