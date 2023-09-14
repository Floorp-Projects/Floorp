//! Implements portable horizontal bitwise vector reductions.
#![allow(unused)]

macro_rules! impl_reduction_bitwise {
    (
        [$elem_ty:ident; $elem_count:expr]:
        $id:ident | $ielem_ty:ident | $test_tt:tt |
        ($convert:expr) |
        ($true:expr, $false:expr)
    ) => {
        impl $id {
            /// Lane-wise bitwise `and` of the vector elements.
            ///
            /// Note: if the vector has one lane, the first element of the
            /// vector is returned.
            #[inline]
            pub fn and(self) -> $elem_ty {
                #[cfg(not(target_arch = "aarch64"))]
                {
                    use crate::llvm::simd_reduce_and;
                    let r: $ielem_ty = unsafe { simd_reduce_and(self.0) };
                    $convert(r)
                }
                #[cfg(target_arch = "aarch64")]
                {
                    // FIXME: broken on aarch64
                    // https://github.com/rust-lang-nursery/packed_simd/issues/15
                    let mut x = self.extract(0) as $elem_ty;
                    for i in 1..$id::lanes() {
                        x &= self.extract(i) as $elem_ty;
                    }
                    x
                }
            }

            /// Lane-wise bitwise `or` of the vector elements.
            ///
            /// Note: if the vector has one lane, the first element of the
            /// vector is returned.
            #[inline]
            pub fn or(self) -> $elem_ty {
                #[cfg(not(target_arch = "aarch64"))]
                {
                    use crate::llvm::simd_reduce_or;
                    let r: $ielem_ty = unsafe { simd_reduce_or(self.0) };
                    $convert(r)
                }
                #[cfg(target_arch = "aarch64")]
                {
                    // FIXME: broken on aarch64
                    // https://github.com/rust-lang-nursery/packed_simd/issues/15
                    let mut x = self.extract(0) as $elem_ty;
                    for i in 1..$id::lanes() {
                        x |= self.extract(i) as $elem_ty;
                    }
                    x
                }
            }

            /// Lane-wise bitwise `xor` of the vector elements.
            ///
            /// Note: if the vector has one lane, the first element of the
            /// vector is returned.
            #[inline]
            pub fn xor(self) -> $elem_ty {
                #[cfg(not(target_arch = "aarch64"))]
                {
                    use crate::llvm::simd_reduce_xor;
                    let r: $ielem_ty = unsafe { simd_reduce_xor(self.0) };
                    $convert(r)
                }
                #[cfg(target_arch = "aarch64")]
                {
                    // FIXME: broken on aarch64
                    // https://github.com/rust-lang-nursery/packed_simd/issues/15
                    let mut x = self.extract(0) as $elem_ty;
                    for i in 1..$id::lanes() {
                        x ^= self.extract(i) as $elem_ty;
                    }
                    x
                }
            }
        }

        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _reduction_bitwise>] {
                    use super::*;

                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn and() {
                        let v = $id::splat($false);
                        assert_eq!(v.and(), $false);
                        let v = $id::splat($true);
                        assert_eq!(v.and(), $true);
                        let v = $id::splat($false);
                        let v = v.replace(0, $true);
                        if $id::lanes() > 1 {
                            assert_eq!(v.and(), $false);
                        } else {
                            assert_eq!(v.and(), $true);
                        }
                        let v = $id::splat($true);
                        let v = v.replace(0, $false);
                        assert_eq!(v.and(), $false);

                    }
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn or() {
                        let v = $id::splat($false);
                        assert_eq!(v.or(), $false);
                        let v = $id::splat($true);
                        assert_eq!(v.or(), $true);
                        let v = $id::splat($false);
                        let v = v.replace(0, $true);
                        assert_eq!(v.or(), $true);
                        let v = $id::splat($true);
                        let v = v.replace(0, $false);
                        if $id::lanes() > 1 {
                            assert_eq!(v.or(), $true);
                        } else {
                            assert_eq!(v.or(), $false);
                        }
                    }
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn xor() {
                        let v = $id::splat($false);
                        assert_eq!(v.xor(), $false);
                        let v = $id::splat($true);
                        if $id::lanes() > 1 {
                            assert_eq!(v.xor(), $false);
                        } else {
                            assert_eq!(v.xor(), $true);
                        }
                        let v = $id::splat($false);
                        let v = v.replace(0, $true);
                        assert_eq!(v.xor(), $true);
                        let v = $id::splat($true);
                        let v = v.replace(0, $false);
                        if $id::lanes() > 1 {
                            assert_eq!(v.xor(), $true);
                        } else {
                            assert_eq!(v.xor(), $false);
                        }
                    }
                }
            }
        }
    };
}
