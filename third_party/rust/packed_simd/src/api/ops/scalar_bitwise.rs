//! Vertical (lane-wise) vector-scalar / scalar-vector bitwise operations.

macro_rules! impl_ops_scalar_bitwise {
    (
        [$elem_ty:ident; $elem_count:expr]:
        $id:ident | $test_tt:tt |
        ($true:expr, $false:expr)
    ) => {
        impl crate::ops::BitXor<$elem_ty> for $id {
            type Output = Self;
            #[inline]
            fn bitxor(self, other: $elem_ty) -> Self {
                self ^ $id::splat(other)
            }
        }
        impl crate::ops::BitXor<$id> for $elem_ty {
            type Output = $id;
            #[inline]
            fn bitxor(self, other: $id) -> $id {
                $id::splat(self) ^ other
            }
        }

        impl crate::ops::BitAnd<$elem_ty> for $id {
            type Output = Self;
            #[inline]
            fn bitand(self, other: $elem_ty) -> Self {
                self & $id::splat(other)
            }
        }
        impl crate::ops::BitAnd<$id> for $elem_ty {
            type Output = $id;
            #[inline]
            fn bitand(self, other: $id) -> $id {
                $id::splat(self) & other
            }
        }

        impl crate::ops::BitOr<$elem_ty> for $id {
            type Output = Self;
            #[inline]
            fn bitor(self, other: $elem_ty) -> Self {
                self | $id::splat(other)
            }
        }
        impl crate::ops::BitOr<$id> for $elem_ty {
            type Output = $id;
            #[inline]
            fn bitor(self, other: $id) -> $id {
                $id::splat(self) | other
            }
        }

        impl crate::ops::BitAndAssign<$elem_ty> for $id {
            #[inline]
            fn bitand_assign(&mut self, other: $elem_ty) {
                *self = *self & other;
            }
        }
        impl crate::ops::BitOrAssign<$elem_ty> for $id {
            #[inline]
            fn bitor_assign(&mut self, other: $elem_ty) {
                *self = *self | other;
            }
        }
        impl crate::ops::BitXorAssign<$elem_ty> for $id {
            #[inline]
            fn bitxor_assign(&mut self, other: $elem_ty) {
                *self = *self ^ other;
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _ops_scalar_bitwise>] {
                    use super::*;

                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn ops_scalar_bitwise() {
                        let zi = 0 as $elem_ty;
                        let oi = 1 as $elem_ty;
                        let ti = 2 as $elem_ty;
                        let z = $id::splat(zi);
                        let o = $id::splat(oi);
                        let t = $id::splat(ti);

                        // BitAnd:
                        assert_eq!(oi & o, o);
                        assert_eq!(o & oi, o);
                        assert_eq!(oi & z, z);
                        assert_eq!(o & zi, z);
                        assert_eq!(zi & o, z);
                        assert_eq!(z & oi, z);
                        assert_eq!(zi & z, z);
                        assert_eq!(z & zi, z);

                        assert_eq!(ti & t, t);
                        assert_eq!(t & ti, t);
                        assert_eq!(ti & o, z);
                        assert_eq!(t & oi, z);
                        assert_eq!(oi & t, z);
                        assert_eq!(o & ti, z);

                        // BitOr:
                        assert_eq!(oi | o, o);
                        assert_eq!(o | oi, o);
                        assert_eq!(oi | z, o);
                        assert_eq!(o | zi, o);
                        assert_eq!(zi | o, o);
                        assert_eq!(z | oi, o);
                        assert_eq!(zi | z, z);
                        assert_eq!(z | zi, z);

                        assert_eq!(ti | t, t);
                        assert_eq!(t | ti, t);
                        assert_eq!(zi | t, t);
                        assert_eq!(z | ti, t);
                        assert_eq!(ti | z, t);
                        assert_eq!(t | zi, t);

                        // BitXOR:
                        assert_eq!(oi ^ o, z);
                        assert_eq!(o ^ oi, z);
                        assert_eq!(zi ^ z, z);
                        assert_eq!(z ^ zi, z);
                        assert_eq!(zi ^ o, o);
                        assert_eq!(z ^ oi, o);
                        assert_eq!(oi ^ z, o);
                        assert_eq!(o ^ zi, o);

                        assert_eq!(ti ^ t, z);
                        assert_eq!(t ^ ti, z);
                        assert_eq!(ti ^ z, t);
                        assert_eq!(t ^ zi, t);
                        assert_eq!(zi ^ t, t);
                        assert_eq!(z ^ ti, t);

                        {
                            // AndAssign:
                            let mut v = o;
                            v &= ti;
                            assert_eq!(v, z);
                        }
                        {
                            // OrAssign:
                            let mut v = z;
                            v |= oi;
                            assert_eq!(v, o);
                        }
                        {
                            // XORAssign:
                            let mut v = z;
                            v ^= oi;
                            assert_eq!(v, o);
                        }
                    }
                }
            }
        }
    };
}
