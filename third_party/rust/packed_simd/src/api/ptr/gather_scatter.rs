//! Implements masked gather and scatters for vectors of pointers

macro_rules! impl_ptr_read {
    ([$elem_ty:ty; $elem_count:expr]: $id:ident, $mask_ty:ident
     | $test_tt:tt) => {
        impl<T> $id<T>
        where
            [T; $elem_count]: sealed::SimdArray,
        {
            /// Reads selected vector elements from memory.
            ///
            /// Instantiates a new vector by reading the values from `self` for
            /// those lanes whose `mask` is `true`, and using the elements of
            /// `value` otherwise.
            ///
            /// No memory is accessed for those lanes of `self` whose `mask` is
            /// `false`.
            ///
            /// # Safety
            ///
            /// This method is unsafe because it dereferences raw pointers. The
            /// pointers must be aligned to `mem::align_of::<T>()`.
            #[inline]
            pub unsafe fn read<M>(
                self, mask: Simd<[M; $elem_count]>,
                value: Simd<[T; $elem_count]>,
            ) -> Simd<[T; $elem_count]>
            where
                M: sealed::Mask,
                [M; $elem_count]: sealed::SimdArray,
            {
                use crate::llvm::simd_gather;
                Simd(simd_gather(value.0, self.0, mask.0))
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                mod [<$id _read>] {
                    use super::*;
                    #[test]
                    fn read() {
                        let mut v = [0_i32; $elem_count];
                        for i in 0..$elem_count {
                            v[i] = i as i32;
                        }

                        let mut ptr = $id::<i32>::null();

                        for i in 0..$elem_count {
                            ptr = ptr.replace(i, unsafe {
                                crate::mem::transmute(&v[i] as *const i32)
                            });
                        }

                        // all mask elements are true:
                        let mask = $mask_ty::splat(true);
                        let def = Simd::<[i32; $elem_count]>::splat(42_i32);
                        let r: Simd<[i32; $elem_count]> = unsafe {
                            ptr.read(mask, def)
                        };
                        assert_eq!(
                            r,
                            Simd::<[i32; $elem_count]>::from_slice_unaligned(
                                &v
                            )
                        );

                        let mut mask = mask;
                        for i in 0..$elem_count {
                            if i % 2 != 0 {
                                mask = mask.replace(i, false);
                            }
                        }

                        // even mask elements are true, odd ones are false:
                        let r: Simd<[i32; $elem_count]> = unsafe {
                            ptr.read(mask, def)
                        };
                        let mut e = v;
                        for i in 0..$elem_count {
                            if i % 2 != 0 {
                                e[i] = 42;
                            }
                        }
                        assert_eq!(
                            r,
                            Simd::<[i32; $elem_count]>::from_slice_unaligned(
                                &e
                            )
                        );

                        // all mask elements are false:
                        let mask = $mask_ty::splat(false);
                        let def = Simd::<[i32; $elem_count]>::splat(42_i32);
                        let r: Simd<[i32; $elem_count]> = unsafe {
                            ptr.read(mask, def) }
                        ;
                        assert_eq!(r, def);
                    }
                }
            }
        }
    };
}

macro_rules! impl_ptr_write {
    ([$elem_ty:ty; $elem_count:expr]: $id:ident, $mask_ty:ident
     | $test_tt:tt) => {
        impl<T> $id<T>
        where
            [T; $elem_count]: sealed::SimdArray,
        {
            /// Writes selected vector elements to memory.
            ///
            /// Writes the lanes of `values` for which the mask is `true` to
            /// their corresponding memory addresses in `self`.
            ///
            /// No memory is accessed for those lanes of `self` whose `mask` is
            /// `false`.
            ///
            /// Overlapping memory addresses of `self` are written to in order
            /// from the lest-significant to the most-significant element.
            ///
            /// # Safety
            ///
            /// This method is unsafe because it dereferences raw pointers. The
            /// pointers must be aligned to `mem::align_of::<T>()`.
            #[inline]
            pub unsafe fn write<M>(
                self, mask: Simd<[M; $elem_count]>,
                value: Simd<[T; $elem_count]>,
            ) where
                M: sealed::Mask,
                [M; $elem_count]: sealed::SimdArray,
            {
                // FIXME:
                // https://github.com/rust-lang-nursery/packed_simd/issues/85
                #[cfg(not(target_arch = "mips"))]
                {
                    use crate::llvm::simd_scatter;
                    simd_scatter(value.0, self.0, mask.0)
                }
                #[cfg(target_arch = "mips")]
                {
                    let m_ptr =
                        &mask as *const Simd<[M; $elem_count]> as *const M;
                    for i in 0..$elem_count {
                        let m = ptr::read(m_ptr.add(i));
                        if m.test() {
                            let t_ptr = &self
                                as *const Simd<[*mut T; $elem_count]>
                                as *mut *mut T;
                            let v_ptr = &value as *const Simd<[T; $elem_count]>
                                as *const T;
                            ptr::write(
                                ptr::read(t_ptr.add(i)),
                                ptr::read(v_ptr.add(i)),
                            );
                        }
                    }
                }
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                mod [<$id _write>] {
                    use super::*;
                    #[test]
                    fn write() {
                        // fourty_two = [42, 42, 42, ...]
                        let fourty_two
                            = Simd::<[i32; $elem_count]>::splat(42_i32);

                        // This test will write to this array
                        let mut arr = [0_i32; $elem_count];
                        for i in 0..$elem_count {
                            arr[i] = i as i32;
                        }
                        // arr = [0, 1, 2, ...]

                        let mut ptr = $id::<i32>::null();
                        for i in 0..$elem_count {
                            ptr = ptr.replace(i, unsafe {
                                crate::mem::transmute(arr.as_ptr().add(i))
                            });
                        }
                        // ptr = [&arr[0], &arr[1], ...]

                        // write `fourty_two` to all elements of `v`
                        {
                            let backup = arr;
                            unsafe {
                                ptr.write($mask_ty::splat(true), fourty_two)
                            };
                            assert_eq!(arr, [42_i32; $elem_count]);
                            arr = backup;  // arr = [0, 1, 2, ...]
                        }

                        // write 42 to even elements of arr:
                        {
                            // set odd elements of the mask to false
                            let mut mask = $mask_ty::splat(true);
                            for i in 0..$elem_count {
                                if i % 2 != 0 {
                                    mask = mask.replace(i, false);
                                }
                            }
                            // mask = [true, false, true, false, ...]

                            // expected result r = [42, 1, 42, 3, 42, 5, ...]
                            let mut r = arr;
                            for i in 0..$elem_count {
                                if i % 2 == 0 {
                                    r[i] = 42;
                                }
                            }

                            let backup = arr;
                            unsafe { ptr.write(mask, fourty_two) };
                            assert_eq!(arr, r);
                            arr = backup;  // arr = [0, 1, 2, 3, ...]
                        }

                        // write 42 to no elements of arr
                        {
                            let backup = arr;
                            unsafe {
                                ptr.write($mask_ty::splat(false), fourty_two)
                            };
                            assert_eq!(arr, backup);
                        }
                    }
                }
            }
        }
    };
}
