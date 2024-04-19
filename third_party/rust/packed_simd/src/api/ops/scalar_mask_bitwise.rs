//! Vertical (lane-wise) vector-vector bitwise operations.

macro_rules! impl_ops_scalar_mask_bitwise {
    (
        [$elem_ty:ident; $elem_count:expr]:
        $id:ident | $test_tt:tt |
        ($true:expr, $false:expr)
    ) => {
        impl crate::ops::BitXor<bool> for $id {
            type Output = Self;
            #[inline]
            fn bitxor(self, other: bool) -> Self {
                self ^ $id::splat(other)
            }
        }
        impl crate::ops::BitXor<$id> for bool {
            type Output = $id;
            #[inline]
            fn bitxor(self, other: $id) -> $id {
                $id::splat(self) ^ other
            }
        }

        impl crate::ops::BitAnd<bool> for $id {
            type Output = Self;
            #[inline]
            fn bitand(self, other: bool) -> Self {
                self & $id::splat(other)
            }
        }
        impl crate::ops::BitAnd<$id> for bool {
            type Output = $id;
            #[inline]
            fn bitand(self, other: $id) -> $id {
                $id::splat(self) & other
            }
        }

        impl crate::ops::BitOr<bool> for $id {
            type Output = Self;
            #[inline]
            fn bitor(self, other: bool) -> Self {
                self | $id::splat(other)
            }
        }
        impl crate::ops::BitOr<$id> for bool {
            type Output = $id;
            #[inline]
            fn bitor(self, other: $id) -> $id {
                $id::splat(self) | other
            }
        }

        impl crate::ops::BitAndAssign<bool> for $id {
            #[inline]
            fn bitand_assign(&mut self, other: bool) {
                *self = *self & other;
            }
        }
        impl crate::ops::BitOrAssign<bool> for $id {
            #[inline]
            fn bitor_assign(&mut self, other: bool) {
                *self = *self | other;
            }
        }
        impl crate::ops::BitXorAssign<bool> for $id {
            #[inline]
            fn bitxor_assign(&mut self, other: bool) {
                *self = *self ^ other;
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _ops_scalar_mask_bitwise>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn ops_scalar_mask_bitwise() {
                        let ti = true;
                        let fi = false;
                        let t = $id::splat(ti);
                        let f = $id::splat(fi);
                        assert!(t != f);
                        assert!(!(t == f));

                        // BitAnd:
                        assert_eq!(ti & f, f);
                        assert_eq!(t & fi, f);
                        assert_eq!(fi & t, f);
                        assert_eq!(f & ti, f);
                        assert_eq!(ti & t, t);
                        assert_eq!(t & ti, t);
                        assert_eq!(fi & f, f);
                        assert_eq!(f & fi, f);

                        // BitOr:
                        assert_eq!(ti | f, t);
                        assert_eq!(t | fi, t);
                        assert_eq!(fi | t, t);
                        assert_eq!(f | ti, t);
                        assert_eq!(ti | t, t);
                        assert_eq!(t | ti, t);
                        assert_eq!(fi | f, f);
                        assert_eq!(f | fi, f);

                        // BitXOR:
                        assert_eq!(ti ^ f, t);
                        assert_eq!(t ^ fi, t);
                        assert_eq!(fi ^ t, t);
                        assert_eq!(f ^ ti, t);
                        assert_eq!(ti ^ t, f);
                        assert_eq!(t ^ ti, f);
                        assert_eq!(fi ^ f, f);
                        assert_eq!(f ^ fi, f);

                        {
                            // AndAssign:
                            let mut v = f;
                            v &= ti;
                            assert_eq!(v, f);
                        }
                        {
                            // OrAssign:
                            let mut v = f;
                            v |= ti;
                            assert_eq!(v, t);
                        }
                        {
                            // XORAssign:
                            let mut v = f;
                            v ^= ti;
                            assert_eq!(v, t);
                        }
                    }
                }
            }
        }
    };
}
