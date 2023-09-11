//! Vertical (lane-wise) vector-vector arithmetic operations.

macro_rules! impl_ops_vector_arithmetic {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl crate::ops::Add for $id {
            type Output = Self;
            #[inline]
            fn add(self, other: Self) -> Self {
                use crate::llvm::simd_add;
                unsafe { Simd(simd_add(self.0, other.0)) }
            }
        }

        impl crate::ops::Sub for $id {
            type Output = Self;
            #[inline]
            fn sub(self, other: Self) -> Self {
                use crate::llvm::simd_sub;
                unsafe { Simd(simd_sub(self.0, other.0)) }
            }
        }

        impl crate::ops::Mul for $id {
            type Output = Self;
            #[inline]
            fn mul(self, other: Self) -> Self {
                use crate::llvm::simd_mul;
                unsafe { Simd(simd_mul(self.0, other.0)) }
            }
        }

        impl crate::ops::Div for $id {
            type Output = Self;
            #[inline]
            fn div(self, other: Self) -> Self {
                use crate::llvm::simd_div;
                unsafe { Simd(simd_div(self.0, other.0)) }
            }
        }

        impl crate::ops::Rem for $id {
            type Output = Self;
            #[inline]
            fn rem(self, other: Self) -> Self {
                use crate::llvm::simd_rem;
                unsafe { Simd(simd_rem(self.0, other.0)) }
            }
        }

        impl crate::ops::AddAssign for $id {
            #[inline]
            fn add_assign(&mut self, other: Self) {
                *self = *self + other;
            }
        }

        impl crate::ops::SubAssign for $id {
            #[inline]
            fn sub_assign(&mut self, other: Self) {
                *self = *self - other;
            }
        }

        impl crate::ops::MulAssign for $id {
            #[inline]
            fn mul_assign(&mut self, other: Self) {
                *self = *self * other;
            }
        }

        impl crate::ops::DivAssign for $id {
            #[inline]
            fn div_assign(&mut self, other: Self) {
                *self = *self / other;
            }
        }

        impl crate::ops::RemAssign for $id {
            #[inline]
            fn rem_assign(&mut self, other: Self) {
                *self = *self % other;
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
               pub mod [<$id _ops_vector_arith>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn ops_vector_arithmetic() {
                        let z = $id::splat(0 as $elem_ty);
                        let o = $id::splat(1 as $elem_ty);
                        let t = $id::splat(2 as $elem_ty);
                        let f = $id::splat(4 as $elem_ty);

                        // add
                        assert_eq!(z + z, z);
                        assert_eq!(o + z, o);
                        assert_eq!(t + z, t);
                        assert_eq!(t + t, f);
                        // sub
                        assert_eq!(z - z, z);
                        assert_eq!(o - z, o);
                        assert_eq!(t - z, t);
                        assert_eq!(f - t, t);
                        assert_eq!(f - o - o, t);
                        // mul
                        assert_eq!(z * z, z);
                        assert_eq!(z * o, z);
                        assert_eq!(z * t, z);
                        assert_eq!(o * t, t);
                        assert_eq!(t * t, f);
                        // div
                        assert_eq!(z / o, z);
                        assert_eq!(t / o, t);
                        assert_eq!(f / o, f);
                        assert_eq!(t / t, o);
                        assert_eq!(f / t, t);
                        // rem
                        assert_eq!(o % o, z);
                        assert_eq!(f % t, z);

                        {
                            let mut v = z;
                            assert_eq!(v, z);
                            v += o; // add_assign
                            assert_eq!(v, o);
                            v -= o; // sub_assign
                            assert_eq!(v, z);
                            v = t;
                            v *= o; // mul_assign
                            assert_eq!(v, t);
                            v *= t;
                            assert_eq!(v, f);
                            v /= o; // div_assign
                            assert_eq!(v, f);
                            v /= t;
                            assert_eq!(v, t);
                            v %= t; // rem_assign
                            assert_eq!(v, z);
                        }
                    }
                }
            }
        }
    };
}
