//! Implements vertical (lane-wise) floating-point `abs`.

macro_rules! impl_math_float_abs {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Absolute value.
            #[inline]
            pub fn abs(self) -> Self {
                use crate::codegen::math::float::abs::Abs;
                Abs::abs(self)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_abs>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn abs() {
                        let o = $id::splat(1 as $elem_ty);
                        assert_eq!(o, o.abs());

                        let mo = $id::splat(-1 as $elem_ty);
                        assert_eq!(o, mo.abs());
                    }
                }
            }
        }
    };
}
