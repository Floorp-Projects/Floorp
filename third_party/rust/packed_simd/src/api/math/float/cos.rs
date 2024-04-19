//! Implements vertical (lane-wise) floating-point `cos`.

macro_rules! impl_math_float_cos {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Cosine.
            #[inline]
            pub fn cos(self) -> Self {
                use crate::codegen::math::float::cos::Cos;
                Cos::cos(self)
            }

            /// Cosine of `self * PI`.
            #[inline]
            pub fn cos_pi(self) -> Self {
                use crate::codegen::math::float::cos_pi::CosPi;
                CosPi::cos_pi(self)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_cos>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn cos() {
                        use crate::$elem_ty::consts::PI;
                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let p = $id::splat(PI as $elem_ty);
                        let ph = $id::splat(PI as $elem_ty / 2.);
                        let z_r = $id::splat((PI as $elem_ty / 2.).cos());
                        let o_r = $id::splat((PI as $elem_ty).cos());

                        assert_eq!(o, z.cos());
                        assert_eq!(z_r, ph.cos());
                        assert_eq!(o_r, p.cos());
                    }
                }
            }
        }
    };
}
