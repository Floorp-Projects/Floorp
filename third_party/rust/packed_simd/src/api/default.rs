//! Implements `Default` for vector types.

macro_rules! impl_default {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl Default for $id {
            #[inline]
            fn default() -> Self {
                Self::splat($elem_ty::default())
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _default>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn default() {
                        let a = $id::default();
                        for i in 0..$id::lanes() {
                            assert_eq!(a.extract(i), $elem_ty::default());
                        }
                    }
                }
            }
        }
    };
}
