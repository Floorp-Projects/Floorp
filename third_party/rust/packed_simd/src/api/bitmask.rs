//! Bitmask API

macro_rules! impl_bitmask {
    ($id:ident | $ibitmask_ty:ident | ($set:expr, $clear:expr)
     | $test_tt:tt) => {
        impl $id {
            /// Creates a bitmask with the MSB of each vector lane.
            ///
            /// If the vector has less than 8 lanes, the bits that do not
            /// correspond to any vector lanes are cleared.
            #[inline]
            pub fn bitmask(self) -> $ibitmask_ty {
                unsafe { codegen::llvm::simd_bitmask(self.0) }
            }
        }

        test_if! {
            $test_tt:
            paste::item! {
                #[cfg(not(
                    // FIXME: https://github.com/rust-lang-nursery/packed_simd/issues/210
                    target_endian = "big"
                ))]
                pub mod [<$id _bitmask>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)]
                    #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn bitmask() {
                        // clear all lanes
                        let vec = $id::splat($clear as _);
                        let bitmask: $ibitmask_ty = 0;
                        assert_eq!(vec.bitmask(), bitmask);

                        // set even lanes
                        let mut vec = $id::splat($clear as _);
                        for i in 0..$id::lanes() {
                            if i % 2 == 0 {
                                vec = vec.replace(i, $set as _);
                            }
                        }
                        // create bitmask with even lanes set:
                        let mut bitmask: $ibitmask_ty = 0;
                        for i in 0..$id::lanes() {
                            if i % 2 == 0 {
                                bitmask |= 1 << i;
                            }
                        }
                        assert_eq!(vec.bitmask(), bitmask);


                        // set odd lanes
                        let mut vec = $id::splat($clear as _);
                        for i in 0..$id::lanes() {
                            if i % 2 != 0 {
                                vec = vec.replace(i, $set as _);
                            }
                        }
                        // create bitmask with odd lanes set:
                        let mut bitmask: $ibitmask_ty = 0;
                        for i in 0..$id::lanes() {
                            if i % 2 != 0 {
                                bitmask |= 1 << i;
                            }
                        }
                        assert_eq!(vec.bitmask(), bitmask);

                        // set all lanes
                        let vec = $id::splat($set as _);
                        let mut bitmask: $ibitmask_ty = 0;
                        for i in 0..$id::lanes() {
                            bitmask |= 1 << i;
                        }
                        assert_eq!(vec.bitmask(), bitmask);
                    }
                }
            }
        }
    };
}
