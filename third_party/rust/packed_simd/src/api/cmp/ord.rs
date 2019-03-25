//! Implements `Ord` for vector types.

macro_rules! impl_cmp_ord {
    (
        [$elem_ty:ident; $elem_count:expr]:
        $id:ident | $test_tt:tt |
        ($true:expr, $false:expr)
    ) => {
        impl $id {
            /// Returns a wrapper that implements `Ord`.
            #[inline]
            pub fn lex_ord(&self) -> LexicographicallyOrdered<$id> {
                LexicographicallyOrdered(*self)
            }
        }

        impl crate::cmp::Ord for LexicographicallyOrdered<$id> {
            #[inline]
            fn cmp(&self, other: &Self) -> crate::cmp::Ordering {
                match self.partial_cmp(other) {
                    Some(x) => x,
                    None => unsafe { crate::hint::unreachable_unchecked() },
                }
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _cmp_ord>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn eq() {
                        fn foo<E: crate::cmp::Ord>(_: E) {}
                        let a = $id::splat($false);
                        foo(a.partial_lex_ord());
                        foo(a.lex_ord());
                    }
                }
            }
        }
    };
}
