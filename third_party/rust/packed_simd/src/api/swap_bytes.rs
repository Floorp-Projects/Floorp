//! Horizontal swap bytes

macro_rules! impl_swap_bytes {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Reverses the byte order of the vector.
            #[inline]
            pub fn swap_bytes(self) -> Self {
                super::codegen::swap_bytes::SwapBytes::swap_bytes(self)
            }

            /// Converts self to little endian from the target's endianness.
            ///
            /// On little endian this is a no-op. On big endian the bytes are
            /// swapped.
            #[inline]
            pub fn to_le(self) -> Self {
                #[cfg(target_endian = "little")]
                {
                    self
                }
                #[cfg(not(target_endian = "little"))]
                {
                    self.swap_bytes()
                }
            }

            /// Converts self to big endian from the target's endianness.
            ///
            /// On big endian this is a no-op. On little endian the bytes are
            /// swapped.
            #[inline]
            pub fn to_be(self) -> Self {
                #[cfg(target_endian = "big")]
                {
                    self
                }
                #[cfg(not(target_endian = "big"))]
                {
                    self.swap_bytes()
                }
            }

            /// Converts a vector from little endian to the target's endianness.
            ///
            /// On little endian this is a no-op. On big endian the bytes are
            /// swapped.
            #[inline]
            pub fn from_le(x: Self) -> Self {
                #[cfg(target_endian = "little")]
                {
                    x
                }
                #[cfg(not(target_endian = "little"))]
                {
                    x.swap_bytes()
                }
            }

            /// Converts a vector from big endian to the target's endianness.
            ///
            /// On big endian this is a no-op. On little endian the bytes are
            /// swapped.
            #[inline]
            pub fn from_be(x: Self) -> Self {
                #[cfg(target_endian = "big")]
                {
                    x
                }
                #[cfg(not(target_endian = "big"))]
                {
                    x.swap_bytes()
                }
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                pub mod [<$id _swap_bytes>] {
                    use super::*;

                    const BYTES: [u8; 64] = [
                        0, 1, 2, 3, 4, 5, 6, 7,
                        8, 9, 10, 11, 12, 13, 14, 15,
                        16, 17, 18, 19, 20, 21, 22, 23,
                        24, 25, 26, 27, 28, 29, 30, 31,
                        32, 33, 34, 35, 36, 37, 38, 39,
                        40, 41, 42, 43, 44, 45, 46, 47,
                        48, 49, 50, 51, 52, 53, 54, 55,
                        56, 57, 58, 59, 60, 61, 62, 63,
                    ];

                    macro_rules! swap {
                        ($func: ident) => {{
                            // catch possible future >512 vectors
                            assert!(mem::size_of::<$id>() <= 64);

                            let mut actual = BYTES;
                            let elems: &mut [$elem_ty] = unsafe {
                                slice::from_raw_parts_mut(
                                    actual.as_mut_ptr() as *mut $elem_ty,
                                    $id::lanes(),
                                )
                            };

                            let vec = $id::from_slice_unaligned(elems);
                            $id::$func(vec).write_to_slice_unaligned(elems);

                            actual
                        }};
                    }

                    macro_rules! test_swap {
                        ($func: ident) => {{
                            let actual = swap!($func);
                            let expected =
                                BYTES.iter().rev()
                                .skip(64 - crate::mem::size_of::<$id>());
                            assert!(actual.iter().zip(expected)
                                    .all(|(x, y)| x == y));
                        }};
                    }

                    macro_rules! test_no_swap {
                        ($func: ident) => {{
                            let actual = swap!($func);
                            let expected = BYTES.iter()
                                .take(mem::size_of::<$id>());

                            assert!(actual.iter().zip(expected)
                                    .all(|(x, y)| x == y));
                        }};
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn swap_bytes() {
                        test_swap!(swap_bytes);
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn to_le() {
                        #[cfg(target_endian = "little")]
                        {
                            test_no_swap!(to_le);
                        }
                        #[cfg(not(target_endian = "little"))]
                        {
                            test_swap!(to_le);
                        }
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn to_be() {
                        #[cfg(target_endian = "big")]
                        {
                            test_no_swap!(to_be);
                        }
                        #[cfg(not(target_endian = "big"))]
                        {
                            test_swap!(to_be);
                        }
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn from_le() {
                        #[cfg(target_endian = "little")]
                        {
                            test_no_swap!(from_le);
                        }
                        #[cfg(not(target_endian = "little"))]
                        {
                            test_swap!(from_le);
                        }
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn from_be() {
                        #[cfg(target_endian = "big")]
                        {
                            test_no_swap!(from_be);
                        }
                        #[cfg(not(target_endian = "big"))]
                        {
                            test_swap!(from_be);
                        }
                    }
                }
            }
        }
    };
}
