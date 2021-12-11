//! Implements `PartialOrd` for vector types.
//!
//! This implements a lexicographical order.

macro_rules! impl_cmp_partial_ord {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Returns a wrapper that implements `PartialOrd`.
            #[inline]
            pub fn partial_lex_ord(&self) -> LexicographicallyOrdered<$id> {
                LexicographicallyOrdered(*self)
            }
        }

        impl crate::cmp::PartialOrd<LexicographicallyOrdered<$id>>
            for LexicographicallyOrdered<$id>
        {
            #[inline]
            fn partial_cmp(
                &self, other: &Self,
            ) -> Option<crate::cmp::Ordering> {
                if PartialEq::eq(self, other) {
                    Some(crate::cmp::Ordering::Equal)
                } else if PartialOrd::lt(self, other) {
                    Some(crate::cmp::Ordering::Less)
                } else if PartialOrd::gt(self, other) {
                    Some(crate::cmp::Ordering::Greater)
                } else {
                    None
                }
            }
            #[inline]
            fn lt(&self, other: &Self) -> bool {
                let m_lt = self.0.lt(other.0);
                let m_eq = self.0.eq(other.0);
                for i in 0..$id::lanes() {
                    if m_eq.extract(i) {
                        continue;
                    }
                    return m_lt.extract(i);
                }
                false
            }
            #[inline]
            fn le(&self, other: &Self) -> bool {
                self.lt(other) | PartialEq::eq(self, other)
            }
            #[inline]
            fn ge(&self, other: &Self) -> bool {
                self.gt(other) | PartialEq::eq(self, other)
            }
            #[inline]
            fn gt(&self, other: &Self) -> bool {
                let m_gt = self.0.gt(other.0);
                let m_eq = self.0.eq(other.0);
                for i in 0..$id::lanes() {
                    if m_eq.extract(i) {
                        continue;
                    }
                    return m_gt.extract(i);
                }
                false
            }
        }
    };
}

macro_rules! test_cmp_partial_ord_int {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _cmp_PartialOrd>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn partial_lex_ord() {
                        use crate::testing::utils::{test_cmp};
                        // constant values
                        let a = $id::splat(0);
                        let b = $id::splat(1);

                        test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Less));
                        test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Greater));
                        test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Equal));
                        test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Equal));

                        // variable values: a = [0, 1, 2, 3]; b = [3, 2, 1, 0]
                        let mut a = $id::splat(0);
                        let mut b = $id::splat(0);
                        for i in 0..$id::lanes() {
                            a = a.replace(i, i as $elem_ty);
                            b = b.replace(i, ($id::lanes() - i) as $elem_ty);
                        }
                        test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Less));
                        test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Greater));
                        test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Equal));
                        test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Equal));

                        // variable values: a = [0, 1, 2, 3]; b = [0, 1, 2, 4]
                        let mut b = a;
                        b = b.replace(
                            $id::lanes() - 1,
                            a.extract($id::lanes() - 1) + 1 as $elem_ty
                        );
                        test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Less));
                        test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Greater));
                        test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Equal));
                        test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(crate::cmp::Ordering::Equal));

                        if $id::lanes() > 2 {
                            // variable values a = [0, 1, 0, 0]; b = [0, 1, 2, 3]
                            let b = a;
                            let mut a = $id::splat(0);
                            a = a.replace(1, 1 as $elem_ty);
                            test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                     Some(crate::cmp::Ordering::Less));
                            test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                     Some(crate::cmp::Ordering::Greater));
                            test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                     Some(crate::cmp::Ordering::Equal));
                            test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                     Some(crate::cmp::Ordering::Equal));

                            // variable values: a = [0, 1, 2, 3]; b = [0, 1, 3, 2]
                            let mut b = a;
                            b = b.replace(
                                2, a.extract($id::lanes() - 1) + 1 as $elem_ty
                            );
                            test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                     Some(crate::cmp::Ordering::Less));
                            test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                     Some(crate::cmp::Ordering::Greater));
                            test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                     Some(crate::cmp::Ordering::Equal));
                            test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                     Some(crate::cmp::Ordering::Equal));
                        }
                    }
                }
            }
        }
    };
}

macro_rules! test_cmp_partial_ord_mask {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _cmp_PartialOrd>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn partial_lex_ord() {
                        use crate::testing::utils::{test_cmp};
                        use crate::cmp::Ordering;

                        // constant values
                        let a = $id::splat(false);
                        let b = $id::splat(true);

                        test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(Ordering::Less));
                        test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(Ordering::Greater));
                        test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(Ordering::Equal));
                        test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(Ordering::Equal));

                        // variable values:
                        // a = [false, false, false, false];
                        // b = [false, false, false, true]
                        let a = $id::splat(false);
                        let mut b = $id::splat(false);
                        b = b.replace($id::lanes() - 1, true);
                        test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(Ordering::Less));
                        test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(Ordering::Greater));
                        test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(Ordering::Equal));
                        test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(Ordering::Equal));

                        // variable values:
                        // a = [true, true, true, false];
                        // b = [true, true, true, true]
                        let mut a = $id::splat(true);
                        let b = $id::splat(true);
                        a = a.replace($id::lanes() - 1, false);
                        test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(Ordering::Less));
                        test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(Ordering::Greater));
                        test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                 Some(Ordering::Equal));
                        test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                 Some(Ordering::Equal));

                        if $id::lanes() > 2 {
                            // variable values
                            // a = [false, true, false, false];
                            // b = [false, true, true, true]
                            let mut a = $id::splat(false);
                            let mut b = $id::splat(true);
                            a = a.replace(1, true);
                            b = b.replace(0, false);
                            test_cmp(a.partial_lex_ord(), b.partial_lex_ord(),
                                     Some(Ordering::Less));
                            test_cmp(b.partial_lex_ord(), a.partial_lex_ord(),
                                     Some(Ordering::Greater));
                            test_cmp(a.partial_lex_ord(), a.partial_lex_ord(),
                                     Some(Ordering::Equal));
                            test_cmp(b.partial_lex_ord(), b.partial_lex_ord(),
                                     Some(Ordering::Equal));
                        }
                    }
                }
            }
        }
    };
}
