//! Implements vertical (lane-wise) floating-point `ln`.

macro_rules! impl_math_float_ln {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Returns the natural logarithm of `self`.
            #[inline]
            pub fn ln(self) -> Self {
                use crate::codegen::math::float::ln::Ln;
                Ln::ln(self)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_ln>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn ln() {
                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        assert_eq!(z, o.ln());

                        let e = $id::splat(crate::f64::consts::E as $elem_ty);
                        let tol = $id::splat(2.4e-4 as $elem_ty);
                        assert!((o - e.ln()).abs().le(tol).all());
                    }
                }
            }
        }
    };
}
