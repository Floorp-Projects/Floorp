//! Implements vertical (lane-wise) floating-point `powf`.

macro_rules! impl_math_float_powf {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Raises `self` number to the floating point power of `x`.
            #[inline]
            pub fn powf(self, x: Self) -> Self {
                use crate::codegen::math::float::powf::Powf;
                Powf::powf(self, x)
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _math_powf>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn powf() {
                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let t = $id::splat(2 as $elem_ty);
                        assert_eq!(o, o.powf(z));
                        assert_eq!(o, t.powf(z));
                        assert_eq!(o, o.powf(o));
                        assert_eq!(t, t.powf(o));

                        let f = $id::splat(4 as $elem_ty);
                        assert_eq!(f, t.powf(t));
                    }
                }
            }
        }
    };
}
