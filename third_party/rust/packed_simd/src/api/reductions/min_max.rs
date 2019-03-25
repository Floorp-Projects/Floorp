//! Implements portable horizontal vector min/max reductions.

macro_rules! impl_reduction_min_max {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident
     | $ielem_ty:ident | $test_tt:tt) => {
        impl $id {
            /// Largest vector element value.
            #[inline]
            pub fn max_element(self) -> $elem_ty {
                #[cfg(not(any(
                    target_arch = "aarch64",
                    target_arch = "arm",
                    target_arch = "powerpc64",
                    target_arch = "wasm32",
                )))]
                {
                    use crate::llvm::simd_reduce_max;
                    let v: $ielem_ty = unsafe { simd_reduce_max(self.0) };
                    v as $elem_ty
                }
                #[cfg(any(
                    target_arch = "aarch64",
                    target_arch = "arm",
                    target_arch = "powerpc64",
                    target_arch = "wasm32",
                ))]
                {
                    // FIXME: broken on AArch64
                    // https://github.com/rust-lang-nursery/packed_simd/issues/15
                    // FIXME: broken on WASM32
                    // https://github.com/rust-lang-nursery/packed_simd/issues/91
                    let mut x = self.extract(0);
                    for i in 1..$id::lanes() {
                        x = x.max(self.extract(i));
                    }
                    x
                }
            }

            /// Smallest vector element value.
            #[inline]
            pub fn min_element(self) -> $elem_ty {
                #[cfg(not(any(
                    target_arch = "aarch64",
                    target_arch = "arm",
                    all(target_arch = "x86", not(target_feature = "sse2")),
                    target_arch = "powerpc64",
                    target_arch = "wasm32",
                ),))]
                {
                    use crate::llvm::simd_reduce_min;
                    let v: $ielem_ty = unsafe { simd_reduce_min(self.0) };
                    v as $elem_ty
                }
                #[cfg(any(
                    target_arch = "aarch64",
                    target_arch = "arm",
                    all(target_arch = "x86", not(target_feature = "sse2")),
                    target_arch = "powerpc64",
                    target_arch = "wasm32",
                ))]
                {
                    // FIXME: broken on AArch64
                    // https://github.com/rust-lang-nursery/packed_simd/issues/15
                    // FIXME: broken on i586-unknown-linux-gnu
                    // https://github.com/rust-lang-nursery/packed_simd/issues/22
                    // FIXME: broken on WASM32
                    // https://github.com/rust-lang-nursery/packed_simd/issues/91
                    let mut x = self.extract(0);
                    for i in 1..$id::lanes() {
                        x = x.min(self.extract(i));
                    }
                    x
                }
            }
        }
        test_if! {$test_tt:
        paste::item! {
            pub mod [<$id _reduction_min_max>] {
                use super::*;
                #[cfg_attr(not(target_arch = "wasm32"), test)]
                #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                pub fn max_element() {
                    let v = $id::splat(0 as $elem_ty);
                    assert_eq!(v.max_element(), 0 as $elem_ty);
                    if $id::lanes() > 1 {
                        let v = v.replace(1, 1 as $elem_ty);
                        assert_eq!(v.max_element(), 1 as $elem_ty);
                    }
                    let v = v.replace(0, 2 as $elem_ty);
                    assert_eq!(v.max_element(), 2 as $elem_ty);
                }

                #[cfg_attr(not(target_arch = "wasm32"), test)]
                #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                pub fn min_element() {
                    let v = $id::splat(0 as $elem_ty);
                    assert_eq!(v.min_element(), 0 as $elem_ty);
                    if $id::lanes() > 1 {
                        let v = v.replace(1, 1 as $elem_ty);
                        assert_eq!(v.min_element(), 0 as $elem_ty);
                    }
                    let v = $id::splat(1 as $elem_ty);
                    let v = v.replace(0, 2 as $elem_ty);
                    if $id::lanes() > 1 {
                        assert_eq!(v.min_element(), 1 as $elem_ty);
                    } else {
                        assert_eq!(v.min_element(), 2 as $elem_ty);
                    }
                    if $id::lanes() > 1 {
                        let v = $id::splat(2 as $elem_ty);
                        let v = v.replace(1, 1 as $elem_ty);
                        assert_eq!(v.min_element(), 1 as $elem_ty);
                    }
                }
            }
        }
        }
    };
}

macro_rules! test_reduction_float_min_max {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _reduction_min_max_nan>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn min_element_test() {
                        let n = crate::$elem_ty::NAN;

                        assert_eq!(n.min(-3.), -3.);
                        assert_eq!((-3. as $elem_ty).min(n), -3.);

                        let v0 = $id::splat(-3.);

                        let target_with_broken_last_lane_nan = !cfg!(any(
                            target_arch = "arm", target_arch = "aarch64",
                            all(target_arch = "x86",
                                not(target_feature = "sse2")
                            ),
                            target_arch = "powerpc64",
                            target_arch = "wasm32",
                        ));

                        // The vector is initialized to `-3.`s: [-3, -3, -3, -3]
                        for i in 0..$id::lanes() {
                            // We replace the i-th element of the vector with
                            // `NaN`: [-3, -3, -3, NaN]
                            let mut v = v0.replace(i, n);

                            // If the NaN is in the last place, the LLVM
                            // implementation of these methods is broken on some
                            // targets:
                            if i == $id::lanes() - 1 &&
                                target_with_broken_last_lane_nan {
                                // FIXME:
                                // https://github.com/rust-lang-nursery/packed_simd/issues/5
                                //
                                // If there is a NaN, the result should always
                                // the smallest element, but currently when the
                                // last element is NaN the current
                                // implementation incorrectly returns NaN.
                                //
                                // The targets mentioned above use different
                                // codegen that produces the correct result.
                                //
                                // These asserts detect if this behavior changes
                                    assert!(v.min_element().is_nan(),
                                            // FIXME: ^^^ should be -3.
                                            "[A]: nan at {} => {} | {:?}",
                                            i, v.min_element(), v);

                                // If we replace all the elements in the vector
                                // up-to the `i-th` lane with `NaN`s, the result
                                // is still always `-3.` unless all elements of
                                // the vector are `NaN`s:
                                //
                                // This is also broken:
                                for j in 0..i {
                                    v = v.replace(j, n);
                                    assert!(v.min_element().is_nan(),
                                            // FIXME: ^^^ should be -3.
                                            "[B]: nan at {} => {} | {:?}",
                                            i, v.min_element(), v);
                                }

                                // We are done here, since we were in the last
                                // lane which is the last iteration of the loop.
                                break
                            }

                            // We are not in the last lane, and there is only
                            // one `NaN` in the vector.

                            // If the vector has one lane, the result is `NaN`:
                            if $id::lanes() == 1 {
                                assert!(v.min_element().is_nan(),
                                        "[C]: all nans | v={:?} | min={} | \
                                         is_nan: {}",
                                        v, v.min_element(),
                                        v.min_element().is_nan()
                                );

                                // And we are done, since the vector only has
                                // one lane anyways.
                                break;
                            }

                            // The vector has more than one lane, since there is
                            // only one `NaN` in the vector, the result is
                            // always `-3`.
                            assert_eq!(v.min_element(), -3.,
                                       "[D]: nan at {} => {} | {:?}",
                                       i, v.min_element(), v);

                            // If we replace all the elements in the vector
                            // up-to the `i-th` lane with `NaN`s, the result is
                            // still always `-3.` unless all elements of the
                            // vector are `NaN`s:
                            for j in 0..i {
                                v = v.replace(j, n);

                                if i == $id::lanes() - 1 && j == i - 1 {
                                    // All elements of the vector are `NaN`s,
                                    // therefore the result is NaN as well.
                                    //
                                    // Note: the #lanes of the vector is > 1, so
                                    // "i - 1" does not overflow.
                                    assert!(v.min_element().is_nan(),
                                            "[E]: all nans | v={:?} | min={} | \
                                             is_nan: {}",
                                            v, v.min_element(),
                                            v.min_element().is_nan());
                                } else {
                                    // There are non-`NaN` elements in the
                                    // vector, therefore the result is `-3.`:
                                    assert_eq!(v.min_element(), -3.,
                                               "[F]: nan at {} => {} | {:?}",
                                               i, v.min_element(), v);
                                }
                            }
                        }

                        // If the vector contains all NaNs the result is NaN:
                        assert!($id::splat(n).min_element().is_nan(),
                                "all nans | v={:?} | min={} | is_nan: {}",
                                $id::splat(n), $id::splat(n).min_element(),
                                $id::splat(n).min_element().is_nan());
                    }
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn max_element_test() {
                        let n = crate::$elem_ty::NAN;

                        assert_eq!(n.max(-3.), -3.);
                        assert_eq!((-3. as $elem_ty).max(n), -3.);

                        let v0 = $id::splat(-3.);

                        let target_with_broken_last_lane_nan = !cfg!(any(
                            target_arch = "arm", target_arch = "aarch64",
                            target_arch = "powerpc64", target_arch = "wasm32",
                        ));

                        // The vector is initialized to `-3.`s: [-3, -3, -3, -3]
                        for i in 0..$id::lanes() {
                            // We replace the i-th element of the vector with
                            // `NaN`: [-3, -3, -3, NaN]
                            let mut v = v0.replace(i, n);

                            // If the NaN is in the last place, the LLVM
                            // implementation of these methods is broken on some
                            // targets:
                            if i == $id::lanes() - 1 &&
                              target_with_broken_last_lane_nan {
                                // FIXME:
                                // https://github.com/rust-lang-nursery/packed_simd/issues/5
                                //
                                // If there is a NaN, the result should
                                // always the largest element, but currently
                                // when the last element is NaN the current
                                // implementation incorrectly returns NaN.
                                //
                                // The targets mentioned above use different
                                // codegen that produces the correct result.
                                //
                                // These asserts detect if this behavior
                                // changes
                                assert!(v.max_element().is_nan(),
                                        // FIXME: ^^^ should be -3.
                                        "[A]: nan at {} => {} | {:?}",
                                        i, v.max_element(), v);

                                // If we replace all the elements in the vector
                                // up-to the `i-th` lane with `NaN`s, the result
                                // is still always `-3.` unless all elements of
                                // the vector are `NaN`s:
                                //
                                // This is also broken:
                                for j in 0..i {
                                    v = v.replace(j, n);
                                    assert!(v.max_element().is_nan(),
                                            // FIXME: ^^^ should be -3.
                                            "[B]: nan at {} => {} | {:?}",
                                            i, v.max_element(), v);
                                }

                                // We are done here, since we were in the last
                                // lane which is the last iteration of the loop.
                                break
                            }

                            // We are not in the last lane, and there is only
                            // one `NaN` in the vector.

                            // If the vector has one lane, the result is `NaN`:
                            if $id::lanes() == 1 {
                                assert!(v.max_element().is_nan(),
                                        "[C]: all nans | v={:?} | min={} | \
                                         is_nan: {}",
                                        v, v.max_element(),
                                        v.max_element().is_nan());

                                // And we are done, since the vector only has
                                // one lane anyways.
                                break;
                            }

                            // The vector has more than one lane, since there is
                            // only one `NaN` in the vector, the result is
                            // always `-3`.
                            assert_eq!(v.max_element(), -3.,
                                       "[D]: nan at {} => {} | {:?}",
                                       i, v.max_element(), v);

                            // If we replace all the elements in the vector
                            // up-to the `i-th` lane with `NaN`s, the result is
                            // still always `-3.` unless all elements of the
                            // vector are `NaN`s:
                            for j in 0..i {
                                v = v.replace(j, n);

                                if i == $id::lanes() - 1 && j == i - 1 {
                                    // All elements of the vector are `NaN`s,
                                    // therefore the result is NaN as well.
                                    //
                                    // Note: the #lanes of the vector is > 1, so
                                    // "i - 1" does not overflow.
                                    assert!(v.max_element().is_nan(),
                                            "[E]: all nans | v={:?} | max={} | \
                                             is_nan: {}",
                                            v, v.max_element(),
                                            v.max_element().is_nan());
                                } else {
                                    // There are non-`NaN` elements in the
                                    // vector, therefore the result is `-3.`:
                                    assert_eq!(v.max_element(), -3.,
                                               "[F]: nan at {} => {} | {:?}",
                                               i, v.max_element(), v);
                                }
                            }
                        }

                        // If the vector contains all NaNs the result is NaN:
                        assert!($id::splat(n).max_element().is_nan(),
                                "all nans | v={:?} | max={} | is_nan: {}",
                                $id::splat(n), $id::splat(n).max_element(),
                                $id::splat(n).max_element().is_nan());
                    }
                }
            }
        }
    }
}
