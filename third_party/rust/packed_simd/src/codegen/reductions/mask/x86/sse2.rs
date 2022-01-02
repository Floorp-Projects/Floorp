//! Mask reductions implementation for `x86` and `x86_64` targets with `SSE2`.
#![allow(unused)]

/// `x86`/`x86_64` 128-bit m64x2 `SSE2` implementation
macro_rules! x86_m64x2_sse2_impl {
    ($id:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "sse")]
            unsafe fn all(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm_movemask_pd;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm_movemask_pd;
                // _mm_movemask_pd(a) creates a 2bit mask containing the
                // most significant bit of each lane of `a`. If all
                // bits are set, then all 2 lanes of the mask are
                // true.
                _mm_movemask_pd(crate::mem::transmute(self))
                    == 0b_11_i32
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "sse")]
            unsafe fn any(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm_movemask_pd;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm_movemask_pd;

                _mm_movemask_pd(crate::mem::transmute(self)) != 0
            }
        }
    };
}

/// `x86`/`x86_64` 128-bit m8x16 `SSE2` implementation
macro_rules! x86_m8x16_sse2_impl {
    ($id:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "sse2")]
            unsafe fn all(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm_movemask_epi8;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm_movemask_epi8;
                // _mm_movemask_epi8(a) creates a 16bit mask containing the
                // most significant bit of each byte of `a`. If all
                // bits are set, then all 16 lanes of the mask are
                // true.
                _mm_movemask_epi8(crate::mem::transmute(self))
                    == i32::from(u16::max_value())
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "sse2")]
            unsafe fn any(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm_movemask_epi8;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm_movemask_epi8;

                _mm_movemask_epi8(crate::mem::transmute(self)) != 0
            }
        }
    };
}
