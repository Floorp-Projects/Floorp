//! Implements portable horizontal float vector arithmetic reductions.

macro_rules! impl_reduction_float_arithmetic {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Horizontal sum of the vector elements.
            ///
            /// The intrinsic performs a tree-reduction of the vector elements.
            /// That is, for an 8 element vector:
            ///
            /// > ((x0 + x1) + (x2 + x3)) + ((x4 + x5) + (x6 + x7))
            ///
            /// If one of the vector element is `NaN` the reduction returns
            /// `NaN`. The resulting `NaN` is not required to be equal to any
            /// of the `NaN`s in the vector.
            #[inline]
            pub fn sum(self) -> $elem_ty {
                #[cfg(not(target_arch = "aarch64"))]
                {
                    use crate::llvm::simd_reduce_add_ordered;
                    unsafe { simd_reduce_add_ordered(self.0, 0 as $elem_ty) }
                }
                #[cfg(target_arch = "aarch64")]
                {
                    // FIXME: broken on AArch64
                    // https://github.com/rust-lang-nursery/packed_simd/issues/15
                    let mut x = self.extract(0) as $elem_ty;
                    for i in 1..$id::lanes() {
                        x += self.extract(i) as $elem_ty;
                    }
                    x
                }
            }

            /// Horizontal product of the vector elements.
            ///
            /// The intrinsic performs a tree-reduction of the vector elements.
            /// That is, for an 8 element vector:
            ///
            /// > ((x0 * x1) * (x2 * x3)) * ((x4 * x5) * (x6 * x7))
            ///
            /// If one of the vector element is `NaN` the reduction returns
            /// `NaN`. The resulting `NaN` is not required to be equal to any
            /// of the `NaN`s in the vector.
            #[inline]
            pub fn product(self) -> $elem_ty {
                #[cfg(not(target_arch = "aarch64"))]
                {
                    use crate::llvm::simd_reduce_mul_ordered;
                    unsafe { simd_reduce_mul_ordered(self.0, 1 as $elem_ty) }
                }
                #[cfg(target_arch = "aarch64")]
                {
                    // FIXME: broken on AArch64
                    // https://github.com/rust-lang-nursery/packed_simd/issues/15
                    let mut x = self.extract(0) as $elem_ty;
                    for i in 1..$id::lanes() {
                        x *= self.extract(i) as $elem_ty;
                    }
                    x
                }
            }
        }

        impl crate::iter::Sum for $id {
            #[inline]
            fn sum<I: Iterator<Item = $id>>(iter: I) -> $id {
                iter.fold($id::splat(0.), crate::ops::Add::add)
            }
        }

        impl crate::iter::Product for $id {
            #[inline]
            fn product<I: Iterator<Item = $id>>(iter: I) -> $id {
                iter.fold($id::splat(1.), crate::ops::Mul::mul)
            }
        }

        impl<'a> crate::iter::Sum<&'a $id> for $id {
            #[inline]
            fn sum<I: Iterator<Item = &'a $id>>(iter: I) -> $id {
                iter.fold($id::splat(0.), |a, b| crate::ops::Add::add(a, *b))
            }
        }

        impl<'a> crate::iter::Product<&'a $id> for $id {
            #[inline]
            fn product<I: Iterator<Item = &'a $id>>(iter: I) -> $id {
                iter.fold($id::splat(1.), |a, b| crate::ops::Mul::mul(a, *b))
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                // Comparisons use integer casts within mantissa^1 range.
                #[allow(clippy::float_cmp)]
                pub mod [<$id _reduction_float_arith>] {
                    use super::*;
                    fn alternating(x: usize) -> $id {
                        let mut v = $id::splat(1 as $elem_ty);
                        for i in 0..$id::lanes() {
                            if i % x == 0 {
                                v = v.replace(i, 2 as $elem_ty);
                            }
                        }
                        v
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn sum() {
                        let v = $id::splat(0 as $elem_ty);
                        assert_eq!(v.sum(), 0 as $elem_ty);
                        let v = $id::splat(1 as $elem_ty);
                        assert_eq!(v.sum(), $id::lanes() as $elem_ty);
                        let v = alternating(2);
                        assert_eq!(
                            v.sum(),
                            ($id::lanes() / 2 + $id::lanes()) as $elem_ty
                        );
                    }
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn product() {
                        let v = $id::splat(0 as $elem_ty);
                        assert_eq!(v.product(), 0 as $elem_ty);
                        let v = $id::splat(1 as $elem_ty);
                        assert_eq!(v.product(), 1 as $elem_ty);
                        let f = match $id::lanes() {
                            64 => 16,
                            32 => 8,
                            16 => 4,
                            _ => 2,
                        };
                        let v = alternating(f);
                        assert_eq!(
                            v.product(),
                            (2_usize.pow(($id::lanes() / f) as u32)
                             as $elem_ty)
                        );
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[allow(unreachable_code)]
                    fn sum_nan() {
                        // FIXME: https://bugs.llvm.org/show_bug.cgi?id=36732
                        // https://github.com/rust-lang-nursery/packed_simd/issues/6
                        return;

                        let n0 = crate::$elem_ty::NAN;
                        let v0 = $id::splat(-3.0);
                        for i in 0..$id::lanes() {
                            let mut v = v0.replace(i, n0);
                            // If the vector contains a NaN the result is NaN:
                            assert!(
                                v.sum().is_nan(),
                                "nan at {} => {} | {:?}",
                                i,
                                v.sum(),
                                v
                            );
                            for j in 0..i {
                                v = v.replace(j, n0);
                                assert!(v.sum().is_nan());
                            }
                        }
                        let v = $id::splat(n0);
                        assert!(v.sum().is_nan(), "all nans | {:?}", v);
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[allow(unreachable_code)]
                    fn product_nan() {
                        // FIXME: https://bugs.llvm.org/show_bug.cgi?id=36732
                        // https://github.com/rust-lang-nursery/packed_simd/issues/6
                        return;

                        let n0 = crate::$elem_ty::NAN;
                        let v0 = $id::splat(-3.0);
                        for i in 0..$id::lanes() {
                            let mut v = v0.replace(i, n0);
                            // If the vector contains a NaN the result is NaN:
                            assert!(
                                v.product().is_nan(),
                                "nan at {} => {} | {:?}",
                                i,
                                v.product(),
                                v
                            );
                            for j in 0..i {
                                v = v.replace(j, n0);
                                assert!(v.product().is_nan());
                            }
                        }
                        let v = $id::splat(n0);
                        assert!(v.product().is_nan(), "all nans | {:?}", v);
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[allow(unused, dead_code)]
                    fn sum_roundoff() {
                        // Performs a tree-reduction
                        fn tree_reduce_sum(a: &[$elem_ty]) -> $elem_ty {
                            assert!(!a.is_empty());
                            if a.len() == 1 {
                                a[0]
                            } else if a.len() == 2 {
                                a[0] + a[1]
                            } else {
                                let mid = a.len() / 2;
                                let (left, right) = a.split_at(mid);
                                tree_reduce_sum(left) + tree_reduce_sum(right)
                            }
                        }

                        let mut start = crate::$elem_ty::EPSILON;
                        let mut scalar_reduction = 0. as $elem_ty;

                        let mut v = $id::splat(0. as $elem_ty);
                        for i in 0..$id::lanes() {
                            let c = if i % 2 == 0 { 1e3 } else { -1. };
                            start *= ::core::$elem_ty::consts::PI * c;
                            scalar_reduction += start;
                            v = v.replace(i, start);
                        }
                        let simd_reduction = v.sum();

                        let mut a = [0. as $elem_ty; $id::lanes()];
                        v.write_to_slice_unaligned(&mut a);
                        let tree_reduction = tree_reduce_sum(&a);

                        // tolerate 1 ULP difference:
                        let red_bits = simd_reduction.to_bits();
                        let tree_bits = tree_reduction.to_bits();
                        assert!(
                            if red_bits > tree_bits {
                                red_bits - tree_bits
                            } else {
                                tree_bits - red_bits
                            } < 2,
                            "vector: {:?} | simd_reduction: {:?} | \
tree_reduction: {} | scalar_reduction: {}",
                            v,
                            simd_reduction,
                            tree_reduction,
                            scalar_reduction
                        );
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[allow(unused, dead_code)]
                    fn product_roundoff() {
                        use ::core::convert::TryInto;
                        // Performs a tree-reduction
                        fn tree_reduce_product(a: &[$elem_ty]) -> $elem_ty {
                            assert!(!a.is_empty());
                            if a.len() == 1 {
                                a[0]
                            } else if a.len() == 2 {
                                a[0] * a[1]
                            } else {
                                let mid = a.len() / 2;
                                let (left, right) = a.split_at(mid);
                                tree_reduce_product(left)
                                    * tree_reduce_product(right)
                            }
                        }

                        let mut start = crate::$elem_ty::EPSILON;
                        let mut scalar_reduction = 1. as $elem_ty;

                        let mut v = $id::splat(0. as $elem_ty);
                        for i in 0..$id::lanes() {
                            let c = if i % 2 == 0 { 1e3 } else { -1. };
                            start *= ::core::$elem_ty::consts::PI * c;
                            scalar_reduction *= start;
                            v = v.replace(i, start);
                        }
                        let simd_reduction = v.product();

                        let mut a = [0. as $elem_ty; $id::lanes()];
                        v.write_to_slice_unaligned(&mut a);
                        let tree_reduction = tree_reduce_product(&a);

                        // FIXME: Too imprecise, even only for product(f32x8).
                        // Figure out how to narrow this down.
                        let ulp_limit = $id::lanes() / 2;
                        let red_bits = simd_reduction.to_bits();
                        let tree_bits = tree_reduction.to_bits();
                        assert!(
                            if red_bits > tree_bits {
                                red_bits - tree_bits
                            } else {
                                tree_bits - red_bits
                            } < ulp_limit.try_into().unwrap(),
                            "vector: {:?} | simd_reduction: {:?} | \
tree_reduction: {} | scalar_reduction: {}",
                            v,
                            simd_reduction,
                            tree_reduction,
                            scalar_reduction
                        );
                    }
                }
            }
        }
    };
}
