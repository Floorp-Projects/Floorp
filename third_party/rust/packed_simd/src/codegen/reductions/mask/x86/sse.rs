//! Mask reductions implementation for `x86` and `x86_64` targets with `SSE`.
#![allow(unused)]

/// `x86`/`x86_64` 128-bit `m32x4` `SSE` implementation
macro_rules! x86_m32x4_sse_impl {
    ($id:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "sse")]
            unsafe fn all(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm_movemask_ps;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm_movemask_ps;
                // _mm_movemask_ps(a) creates a 4bit mask containing the
                // most significant bit of each lane of `a`. If all
                // bits are set, then all 4 lanes of the mask are
                // true.
                _mm_movemask_ps(crate::mem::transmute(self)) == 0b_1111_i32
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "sse")]
            unsafe fn any(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm_movemask_ps;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm_movemask_ps;

                _mm_movemask_ps(crate::mem::transmute(self)) != 0
            }
        }
    };
}
