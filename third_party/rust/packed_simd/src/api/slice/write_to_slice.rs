//! Implements methods to write a vector type to a slice.

macro_rules! impl_slice_write_to_slice {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Writes the values of the vector to the `slice`.
            ///
            /// # Panics
            ///
            /// If `slice.len() < Self::lanes()` or `&slice[0]` is not
            /// aligned to an `align_of::<Self>()` boundary.
            #[inline]
            pub fn write_to_slice_aligned(self, slice: &mut [$elem_ty]) {
                unsafe {
                    assert!(slice.len() >= $elem_count);
                    let target_ptr = slice.as_mut_ptr();
                    assert_eq!(target_ptr.align_offset(crate::mem::align_of::<Self>()), 0);
                    self.write_to_slice_aligned_unchecked(slice);
                }
            }

            /// Writes the values of the vector to the `slice`.
            ///
            /// # Panics
            ///
            /// If `slice.len() < Self::lanes()`.
            #[inline]
            pub fn write_to_slice_unaligned(self, slice: &mut [$elem_ty]) {
                unsafe {
                    assert!(slice.len() >= $elem_count);
                    self.write_to_slice_unaligned_unchecked(slice);
                }
            }

            /// Writes the values of the vector to the `slice`.
            ///
            /// # Safety
            ///
            /// If `slice.len() < Self::lanes()` or `&slice[0]` is not
            /// aligned to an `align_of::<Self>()` boundary, the behavior is
            /// undefined.
            #[inline]
            pub unsafe fn write_to_slice_aligned_unchecked(self, slice: &mut [$elem_ty]) {
                debug_assert!(slice.len() >= $elem_count);
                let target_ptr = slice.as_mut_ptr();
                debug_assert_eq!(target_ptr.align_offset(crate::mem::align_of::<Self>()), 0);

                #[allow(clippy::cast_ptr_alignment)]
                #[allow(clippy::cast_ptr_alignment)]
                #[allow(clippy::cast_ptr_alignment)]
                #[allow(clippy::cast_ptr_alignment)]
                *(target_ptr as *mut Self) = self;
            }

            /// Writes the values of the vector to the `slice`.
            ///
            /// # Safety
            ///
            /// If `slice.len() < Self::lanes()` the behavior is undefined.
            #[inline]
            pub unsafe fn write_to_slice_unaligned_unchecked(self, slice: &mut [$elem_ty]) {
                debug_assert!(slice.len() >= $elem_count);
                let target_ptr = slice.as_mut_ptr().cast();
                let self_ptr = &self as *const Self as *const u8;
                crate::ptr::copy_nonoverlapping(self_ptr, target_ptr, crate::mem::size_of::<Self>());
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                // Comparisons use integer casts within mantissa^1 range.
                #[allow(clippy::float_cmp)]
                pub mod [<$id _slice_write_to_slice>] {
                    use super::*;
                    use crate::iter::Iterator;

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn write_to_slice_unaligned() {
                        let mut unaligned = [0 as $elem_ty; $id::lanes() + 1];
                        let vec = $id::splat(42 as $elem_ty);
                        vec.write_to_slice_unaligned(&mut unaligned[1..]);
                        for (index, &b) in unaligned.iter().enumerate() {
                            if index == 0 {
                                assert_eq!(b, 0 as $elem_ty);
                            } else {
                                assert_eq!(b, 42 as $elem_ty);
                                assert_eq!(b, vec.extract(index - 1));
                            }
                        }
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn write_to_slice_unaligned_fail() {
                        let mut unaligned = [0 as $elem_ty; $id::lanes() + 1];
                        let vec = $id::splat(42 as $elem_ty);
                        vec.write_to_slice_unaligned(&mut unaligned[2..]);
                    }

                    union A {
                        data: [$elem_ty; 2 * $id::lanes()],
                        _vec: $id,
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn write_to_slice_aligned() {
                        let mut aligned = A {
                            data: [0 as $elem_ty; 2 * $id::lanes()],
                        };
                        let vec = $id::splat(42 as $elem_ty);
                        unsafe {
                            vec.write_to_slice_aligned(
                                &mut aligned.data[$id::lanes()..]
                            );
                            for (idx, &b) in aligned.data.iter().enumerate() {
                                if idx < $id::lanes() {
                                    assert_eq!(b, 0 as $elem_ty);
                                } else {
                                    assert_eq!(b, 42 as $elem_ty);
                                    assert_eq!(
                                        b, vec.extract(idx - $id::lanes())
                                    );
                                }
                            }
                        }
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn write_to_slice_aligned_fail_lanes() {
                        let mut aligned = A {
                            data: [0 as $elem_ty; 2 * $id::lanes()],
                        };
                        let vec = $id::splat(42 as $elem_ty);
                        unsafe {
                            vec.write_to_slice_aligned(
                                &mut aligned.data[2 * $id::lanes()..]
                            )
                        };
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn write_to_slice_aligned_fail_align() {
                        unsafe {
                            let mut aligned = A {
                                data: [0 as $elem_ty; 2 * $id::lanes()],
                            };

                            // get a pointer to the front of data
                            let ptr: *mut $elem_ty
                                = aligned.data.as_mut_ptr() as *mut $elem_ty;
                            // offset pointer by one element
                            let ptr = ptr.wrapping_add(1);

                            if ptr.align_offset(crate::mem::align_of::<$id>())
                                == 0 {
                                // the pointer is properly aligned, so
                                // write_to_slice_aligned won't fail here (e.g.
                                // this can happen for i128x1). So we panic to
                                // make the "should_fail" test pass:
                                panic!("ok");
                            }

                            // create a slice - this is safe, because the
                            // elements of the slice exist, are properly
                            // initialized, and properly aligned:
                            let s: &mut [$elem_ty]
                                = slice::from_raw_parts_mut(ptr, $id::lanes());
                            // this should always panic because the slice
                            // alignment does not match the alignment
                            // requirements for the vector type:
                            let vec = $id::splat(42 as $elem_ty);
                            vec.write_to_slice_aligned(s);
                        }
                    }
                }
            }
        }
    };
}
