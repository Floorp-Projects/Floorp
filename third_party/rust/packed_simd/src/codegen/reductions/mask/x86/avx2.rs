//! Mask reductions implementation for `x86` and `x86_64` targets with `AVX2`.
#![allow(unused)]

/// x86/x86_64 256-bit m8x32 AVX2 implementation
macro_rules! x86_m8x32_avx2_impl {
    ($id:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "sse2")]
            unsafe fn all(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm256_movemask_epi8;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm256_movemask_epi8;
                // _mm256_movemask_epi8(a) creates a 32bit mask containing the
                // most significant bit of each byte of `a`. If all
                // bits are set, then all 32 lanes of the mask are
                // true.
                _mm256_movemask_epi8(crate::mem::transmute(self)) == -1_i32
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "sse2")]
            unsafe fn any(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm256_movemask_epi8;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm256_movemask_epi8;

                _mm256_movemask_epi8(crate::mem::transmute(self)) != 0
            }
        }
    };
}
