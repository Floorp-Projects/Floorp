//! Vertical (lane-wise) vector-vector bitwise operations.

macro_rules! impl_ops_vector_mask_bitwise {
    (
        [$elem_ty:ident; $elem_count:expr]:
        $id:ident | $test_tt:tt |
        ($true:expr, $false:expr)
    ) => {
        impl crate::ops::Not for $id {
            type Output = Self;
            #[inline]
            fn not(self) -> Self {
                Self::splat($true) ^ self
            }
        }
        impl crate::ops::BitXor for $id {
            type Output = Self;
            #[inline]
            fn bitxor(self, other: Self) -> Self {
                use crate::llvm::simd_xor;
                unsafe { Simd(simd_xor(self.0, other.0)) }
            }
        }
        impl crate::ops::BitAnd for $id {
            type Output = Self;
            #[inline]
            fn bitand(self, other: Self) -> Self {
                use crate::llvm::simd_and;
                unsafe { Simd(simd_and(self.0, other.0)) }
            }
        }
        impl crate::ops::BitOr for $id {
            type Output = Self;
            #[inline]
            fn bitor(self, other: Self) -> Self {
                use crate::llvm::simd_or;
                unsafe { Simd(simd_or(self.0, other.0)) }
            }
        }
        impl crate::ops::BitAndAssign for $id {
            #[inline]
            fn bitand_assign(&mut self, other: Self) {
                *self = *self & other;
            }
        }
        impl crate::ops::BitOrAssign for $id {
            #[inline]
            fn bitor_assign(&mut self, other: Self) {
                *self = *self | other;
            }
        }
        impl crate::ops::BitXorAssign for $id {
            #[inline]
            fn bitxor_assign(&mut self, other: Self) {
                *self = *self ^ other;
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _ops_vector_mask_bitwise>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn ops_vector_mask_bitwise() {
                        let t = $id::splat(true);
                        let f = $id::splat(false);
                        assert!(t != f);
                        assert!(!(t == f));

                        // Not:
                        assert_eq!(!t, f);
                        assert_eq!(t, !f);

                        // BitAnd:
                        assert_eq!(t & f, f);
                        assert_eq!(f & t, f);
                        assert_eq!(t & t, t);
                        assert_eq!(f & f, f);

                        // BitOr:
                        assert_eq!(t | f, t);
                        assert_eq!(f | t, t);
                        assert_eq!(t | t, t);
                        assert_eq!(f | f, f);

                        // BitXOR:
                        assert_eq!(t ^ f, t);
                        assert_eq!(f ^ t, t);
                        assert_eq!(t ^ t, f);
                        assert_eq!(f ^ f, f);

                        {
                            // AndAssign:
                            let mut v = f;
                            v &= t;
                            assert_eq!(v, f);
                        }
                        {
                            // OrAssign:
                            let mut v = f;
                            v |= t;
                            assert_eq!(v, t);
                        }
                        {
                            // XORAssign:
                            let mut v = f;
                            v ^= t;
                            assert_eq!(v, t);
                        }
                    }
                }
            }
        }
    };
}
