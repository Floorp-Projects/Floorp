//! Bit manipulations.

macro_rules! impl_bit_manip {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $test_tt:tt) => {
        impl $id {
            /// Returns the number of ones in the binary representation of
            /// the lanes of `self`.
            #[inline]
            pub fn count_ones(self) -> Self {
                super::codegen::bit_manip::BitManip::ctpop(self)
            }

            /// Returns the number of zeros in the binary representation of
            /// the lanes of `self`.
            #[inline]
            pub fn count_zeros(self) -> Self {
                super::codegen::bit_manip::BitManip::ctpop(!self)
            }

            /// Returns the number of leading zeros in the binary
            /// representation of the lanes of `self`.
            #[inline]
            pub fn leading_zeros(self) -> Self {
                super::codegen::bit_manip::BitManip::ctlz(self)
            }

            /// Returns the number of trailing zeros in the binary
            /// representation of the lanes of `self`.
            #[inline]
            pub fn trailing_zeros(self) -> Self {
                super::codegen::bit_manip::BitManip::cttz(self)
            }
        }

        test_if! {
            $test_tt:
            paste::item_with_macros! {
                #[allow(overflowing_literals)]
                pub mod [<$id _bit_manip>] {
                    use super::*;

                    const LANE_WIDTH: usize = mem::size_of::<$elem_ty>() * 8;

                    macro_rules! test_func {
                        ($x:expr, $func:ident) => {{
                            let mut actual = $x;
                            for i in 0..$id::lanes() {
                                actual = actual.replace(
                                    i,
                                    $x.extract(i).$func() as $elem_ty
                                );
                            }
                            let expected = $x.$func();
                            assert_eq!(actual, expected);
                        }};
                    }

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

                    fn load_bytes() -> $id {
                        let elems: &mut [$elem_ty] = unsafe {
                            slice::from_raw_parts_mut(
                                BYTES.as_mut_ptr() as *mut $elem_ty,
                                $id::lanes(),
                            )
                        };
                        $id::from_slice_unaligned(elems)
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn count_ones() {
                        test_func!($id::splat(0), count_ones);
                        test_func!($id::splat(!0), count_ones);
                        test_func!(load_bytes(), count_ones);
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn count_zeros() {
                        test_func!($id::splat(0), count_zeros);
                        test_func!($id::splat(!0), count_zeros);
                        test_func!(load_bytes(), count_zeros);
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn leading_zeros() {
                        test_func!($id::splat(0), leading_zeros);
                        test_func!($id::splat(1), leading_zeros);
                        // some implementations use `pshufb` which has unique
                        // behavior when the 8th bit is set.
                        test_func!($id::splat(0b1000_0010), leading_zeros);
                        test_func!($id::splat(!0), leading_zeros);
                        test_func!(
                            $id::splat(1 << (LANE_WIDTH - 1)),
                            leading_zeros
                        );
                        test_func!(load_bytes(), leading_zeros);
                    }

                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn trailing_zeros() {
                        test_func!($id::splat(0), trailing_zeros);
                        test_func!($id::splat(1), trailing_zeros);
                        test_func!($id::splat(0b1000_0010), trailing_zeros);
                        test_func!($id::splat(!0), trailing_zeros);
                        test_func!(
                            $id::splat(1 << (LANE_WIDTH - 1)),
                            trailing_zeros
                        );
                        test_func!(load_bytes(), trailing_zeros);
                    }
                }
            }
        }
    };
}
