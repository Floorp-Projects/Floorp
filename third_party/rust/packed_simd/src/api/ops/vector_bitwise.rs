//! Vertical (lane-wise) vector-vector bitwise operations.

macro_rules! impl_ops_vector_bitwise {
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
                pub mod [<$id _ops_vector_bitwise>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn ops_vector_bitwise() {

                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let t = $id::splat(2 as $elem_ty);
                        let m = $id::splat(!z.extract(0));

                        // Not:
                        assert_eq!(!z, m);
                        assert_eq!(!m, z);

                        // BitAnd:
                        assert_eq!(o & o, o);
                        assert_eq!(o & z, z);
                        assert_eq!(z & o, z);
                        assert_eq!(z & z, z);

                        assert_eq!(t & t, t);
                        assert_eq!(t & o, z);
                        assert_eq!(o & t, z);

                        // BitOr:
                        assert_eq!(o | o, o);
                        assert_eq!(o | z, o);
                        assert_eq!(z | o, o);
                        assert_eq!(z | z, z);

                        assert_eq!(t | t, t);
                        assert_eq!(z | t, t);
                        assert_eq!(t | z, t);

                        // BitXOR:
                        assert_eq!(o ^ o, z);
                        assert_eq!(z ^ z, z);
                        assert_eq!(z ^ o, o);
                        assert_eq!(o ^ z, o);

                        assert_eq!(t ^ t, z);
                        assert_eq!(t ^ z, t);
                        assert_eq!(z ^ t, t);

                        {
                            // AndAssign:
                            let mut v = o;
                            v &= t;
                            assert_eq!(v, z);
                        }
                        {
                            // OrAssign:
                            let mut v = z;
                            v |= o;
                            assert_eq!(v, o);
                        }
                        {
                            // XORAssign:
                            let mut v = z;
                            v ^= o;
                            assert_eq!(v, o);
                        }
                    }
                }
            }
        }
    };
}
