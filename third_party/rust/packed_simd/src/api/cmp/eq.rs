//! Implements `Eq` for vector types.

macro_rules! impl_cmp_eq {
    (
        [$elem_ty:ident; $elem_count:expr]:
        $id:ident | $test_tt:tt |
        ($true:expr, $false:expr)
    ) => {
        impl crate::cmp::Eq for $id {}
        impl crate::cmp::Eq for LexicographicallyOrdered<$id> {}

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _cmp_eq>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn eq() {
                        fn foo<E: crate::cmp::Eq>(_: E) {}
                        let a = $id::splat($false);
                        foo(a);
                    }
                }
            }
        }
    };
}
