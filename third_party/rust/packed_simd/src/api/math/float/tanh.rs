//! Implements vertical (lane-wise) floating-point `tanh`.

macro_rules! impl_math_float_tanh {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Tanh.
            #[inline]
            pub fn tanh(self) -> Self {
                use crate::codegen::math::float::tanh::Tanh;
                Tanh::tanh(self)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_tanh>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn tanh() {
                        let z = $id::splat(0 as $elem_ty);

                        assert_eq!(z, z.tanh());
                    }
                }
            }
        }
    };
}
