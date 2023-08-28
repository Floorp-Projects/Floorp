//! Vertical (lane-wise) vector comparisons returning vector masks.

macro_rules! impl_cmp_vertical {
    (
        [$elem_ty:ident; $elem_count:expr]:
        $id:ident,
        $mask_ty:ident,
        $is_mask:expr,($true:expr, $false:expr) | $test_tt:tt
    ) => {
        impl $id {
            /// Lane-wise equality comparison.
            #[inline]
            pub fn eq(self, other: Self) -> $mask_ty {
                use crate::llvm::simd_eq;
                Simd(unsafe { simd_eq(self.0, other.0) })
            }

            /// Lane-wise inequality comparison.
            #[inline]
            pub fn ne(self, other: Self) -> $mask_ty {
                use crate::llvm::simd_ne;
                Simd(unsafe { simd_ne(self.0, other.0) })
            }

            /// Lane-wise less-than comparison.
            #[inline]
            pub fn lt(self, other: Self) -> $mask_ty {
                use crate::llvm::{simd_gt, simd_lt};
                if $is_mask {
                    Simd(unsafe { simd_gt(self.0, other.0) })
                } else {
                    Simd(unsafe { simd_lt(self.0, other.0) })
                }
            }

            /// Lane-wise less-than-or-equals comparison.
            #[inline]
            pub fn le(self, other: Self) -> $mask_ty {
                use crate::llvm::{simd_ge, simd_le};
                if $is_mask {
                    Simd(unsafe { simd_ge(self.0, other.0) })
                } else {
                    Simd(unsafe { simd_le(self.0, other.0) })
                }
            }

            /// Lane-wise greater-than comparison.
            #[inline]
            pub fn gt(self, other: Self) -> $mask_ty {
                use crate::llvm::{simd_gt, simd_lt};
                if $is_mask {
                    Simd(unsafe { simd_lt(self.0, other.0) })
                } else {
                    Simd(unsafe { simd_gt(self.0, other.0) })
                }
            }

            /// Lane-wise greater-than-or-equals comparison.
            #[inline]
            pub fn ge(self, other: Self) -> $mask_ty {
                use crate::llvm::{simd_ge, simd_le};
                if $is_mask {
                    Simd(unsafe { simd_le(self.0, other.0) })
                } else {
                    Simd(unsafe { simd_ge(self.0, other.0) })
                }
            }
        }
        test_if!{
            $test_tt:
            paste::item! {
                pub mod [<$id _cmp_vertical>] {
                    use super::*;
                    #[cfg_attr(not(target_arch = "wasm32"), test)] #[cfg_attr(target_arch = "wasm32", wasm_bindgen_test)]
                    fn cmp() {
                        let a = $id::splat($false);
                        let b = $id::splat($true);

                        let r = a.lt(b);
                        let e = $mask_ty::splat(true);
                        assert!(r == e);
                        let r = a.le(b);
                        assert!(r == e);

                        let e = $mask_ty::splat(false);
                        let r = a.gt(b);
                        assert!(r == e);
                        let r = a.ge(b);
                        assert!(r == e);
                        let r = a.eq(b);
                        assert!(r == e);

                        let mut a = a;
                        let mut b = b;
                        let mut e = e;
                        for i in 0..$id::lanes() {
                            if i % 2 == 0 {
                                a = a.replace(i, $false);
                                b = b.replace(i, $true);
                                e = e.replace(i, true);
                            } else {
                                a = a.replace(i, $true);
                                b = b.replace(i, $false);
                                e = e.replace(i, false);
                            }
                        }
                        let r = a.lt(b);
                        assert!(r == e);
                    }
                }
            }
        }
    };
}
