//! Minimal API of mask vectors.

macro_rules! impl_minimal_mask {
    ([$elem_ty:ident; $elem_count:expr]: $id:ident | $ielem_ty:ident
    | $test_tt:tt | $($elem_name:ident),+ | $(#[$doc:meta])*) => {
        $(#[$doc])*
        pub type $id = Simd<[$elem_ty; $elem_count]>;

        impl sealed::Simd for $id {
            type Element = $elem_ty;
            const LANES: usize = $elem_count;
            type LanesType = [u32; $elem_count];
        }

        impl $id {
            /// Creates a new instance with each vector elements initialized
            /// with the provided values.
            #[inline]
            #[allow(clippy::too_many_arguments)]
            pub const fn new($($elem_name: bool),*) -> Self {
                Simd(codegen::$id($(Self::bool_to_internal($elem_name)),*))
            }

            /// Converts a boolean type into the type of the vector lanes.
            #[inline]
            #[allow(clippy::indexing_slicing)]
            const fn bool_to_internal(x: bool) -> $ielem_ty {
                [0 as $ielem_ty, !(0 as $ielem_ty)][x as usize]
            }

            /// Returns the number of vector lanes.
            #[inline]
            pub const fn lanes() -> usize {
                $elem_count
            }

            /// Constructs a new instance with each element initialized to
            /// `value`.
            #[inline]
            pub const fn splat(value: bool) -> Self {
                Simd(codegen::$id($({
                    #[allow(non_camel_case_types, dead_code)]
                    struct $elem_name;
                    Self::bool_to_internal(value)
                }),*))
            }

            /// Extracts the value at `index`.
            ///
            /// # Panics
            ///
            /// If `index >= Self::lanes()`.
            #[inline]
            pub fn extract(self, index: usize) -> bool {
                assert!(index < $elem_count);
                unsafe { self.extract_unchecked(index) }
            }

            /// Extracts the value at `index`.
            ///
            /// # Safety
            ///
            /// If `index >= Self::lanes()` the behavior is undefined.
            #[inline]
            pub unsafe fn extract_unchecked(self, index: usize) -> bool {
                use crate::llvm::simd_extract;
                let x: $ielem_ty = simd_extract(self.0, index as u32);
                x != 0
            }

            /// Returns a new vector where the value at `index` is replaced by
            /// `new_value`.
            ///
            /// # Panics
            ///
            /// If `index >= Self::lanes()`.
            #[inline]
            #[must_use = "replace does not modify the original value - \
                          it returns a new vector with the value at `index` \
                          replaced by `new_value`d"
            ]
            pub fn replace(self, index: usize, new_value: bool) -> Self {
                assert!(index < $elem_count);
                unsafe { self.replace_unchecked(index, new_value) }
            }

            /// Returns a new vector where the value at `index` is replaced by
            /// `new_value`.
            ///
            /// # Safety
            ///
            /// If `index >= Self::lanes()` the behavior is undefined.
            #[inline]
            #[must_use = "replace_unchecked does not modify the original value - \
                          it returns a new vector with the value at `index` \
                          replaced by `new_value`d"
            ]
            pub unsafe fn replace_unchecked(
                self,
                index: usize,
                new_value: bool,
            ) -> Self {
                use crate::llvm::simd_insert;
                Simd(simd_insert(self.0, index as u32,
                                 Self::bool_to_internal(new_value)))
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _minimal>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn minimal() {
                        // TODO: test new

                        // lanes:
                        assert_eq!($elem_count, $id::lanes());

                        // splat and extract / extract_unchecked:
                        let vec = $id::splat(true);
                        for i in 0..$id::lanes() {
                            assert_eq!(true, vec.extract(i));
                            assert_eq!(true,
                                       unsafe { vec.extract_unchecked(i) }
                            );
                        }

                        // replace / replace_unchecked
                        let new_vec = vec.replace(0, false);
                        for i in 0..$id::lanes() {
                            if i == 0 {
                                assert_eq!(false, new_vec.extract(i));
                            } else {
                                assert_eq!(true, new_vec.extract(i));
                            }
                        }
                        let new_vec = unsafe {
                            vec.replace_unchecked(0, false)
                        };
                        for i in 0..$id::lanes() {
                            if i == 0 {
                                assert_eq!(false, new_vec.extract(i));
                            } else {
                                assert_eq!(true, new_vec.extract(i));
                            }
                        }
                    }

                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn extract_panic_oob() {
                        let vec = $id::splat(false);
                        let _ = vec.extract($id::lanes());
                    }
                    // FIXME: wasm-bindgen-test does not support #[should_panic]
                    // #[cfg_attr(not(target_arch = "wasm32"), test)]
                    // #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    #[cfg(not(target_arch = "wasm32"))]
                    #[test]
                    #[should_panic]
                    fn replace_panic_oob() {
                        let vec = $id::splat(false);
                        let _ = vec.replace($id::lanes(), true);
                    }
                }
            }
        }
    }
}
