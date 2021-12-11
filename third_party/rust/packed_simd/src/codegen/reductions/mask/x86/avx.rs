//! Mask reductions implementation for `x86` and `x86_64` targets with `AVX`

/// `x86`/`x86_64` 256-bit `AVX` implementation
/// FIXME: it might be faster here to do two `_mm_movmask_epi8`
#[cfg(target_feature = "avx")]
macro_rules! x86_m8x32_avx_impl {
    ($id:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "avx")]
            unsafe fn all(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm256_testc_si256;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm256_testc_si256;
                _mm256_testc_si256(
                    crate::mem::transmute(self),
                    crate::mem::transmute($id::splat(true)),
                ) != 0
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "avx")]
            unsafe fn any(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm256_testz_si256;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm256_testz_si256;
                _mm256_testz_si256(
                    crate::mem::transmute(self),
                    crate::mem::transmute(self),
                ) == 0
            }
        }
    };
}

/// `x86`/`x86_64` 256-bit m32x8 `AVX` implementation
macro_rules! x86_m32x8_avx_impl {
    ($id:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "sse")]
            unsafe fn all(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm256_movemask_ps;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm256_movemask_ps;
                // _mm256_movemask_ps(a) creates a 8bit mask containing the
                // most significant bit of each lane of `a`. If all bits are
                // set, then all 8 lanes of the mask are true.
                _mm256_movemask_ps(crate::mem::transmute(self)) == 0b_1111_1111_i32
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "sse")]
            unsafe fn any(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm256_movemask_ps;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm256_movemask_ps;

                _mm256_movemask_ps(crate::mem::transmute(self)) != 0
            }
        }
    };
}

/// `x86`/`x86_64` 256-bit m64x4 `AVX` implementation
macro_rules! x86_m64x4_avx_impl {
    ($id:ident) => {
        impl All for $id {
            #[inline]
            #[target_feature(enable = "sse")]
            unsafe fn all(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm256_movemask_pd;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm256_movemask_pd;
                // _mm256_movemask_pd(a) creates a 4bit mask containing the
                // most significant bit of each lane of `a`. If all bits are
                // set, then all 4 lanes of the mask are true.
                _mm256_movemask_pd(crate::mem::transmute(self)) == 0b_1111_i32
            }
        }
        impl Any for $id {
            #[inline]
            #[target_feature(enable = "sse")]
            unsafe fn any(self) -> bool {
                #[cfg(target_arch = "x86")]
                use crate::arch::x86::_mm256_movemask_pd;
                #[cfg(target_arch = "x86_64")]
                use crate::arch::x86_64::_mm256_movemask_pd;

                _mm256_movemask_pd(crate::mem::transmute(self)) != 0
            }
        }
    };
}
