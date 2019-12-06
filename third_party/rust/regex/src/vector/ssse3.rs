#![allow(dead_code)]

use std::arch::x86_64::*;
use std::fmt;
use std::mem;

/// A builder for SSSE3 empowered vectors.
///
/// This builder represents a receipt that the SSSE3 target feature is enabled
/// on the currently running CPU. Namely, the only way to get a value of this
/// type is if the SSSE3 feature is enabled.
///
/// This type can then be used to build vector types that use SSSE3 features
/// safely.
#[derive(Clone, Copy, Debug)]
pub struct SSSE3VectorBuilder(());

impl SSSE3VectorBuilder {
    /// Create a new SSSE3 vector builder.
    ///
    /// If the SSSE3 feature is not enabled for the current target, then
    /// return `None`.
    pub fn new() -> Option<SSSE3VectorBuilder> {
        if is_x86_feature_detected!("ssse3") {
            Some(SSSE3VectorBuilder(()))
        } else {
            None
        }
    }

    /// Create a new u8x16 SSSE3 vector where all of the bytes are set to
    /// the given value.
    #[inline]
    pub fn u8x16_splat(self, n: u8) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe { u8x16::splat(n) }
    }

    /// Load 16 bytes from the given slice, with bounds checks.
    #[inline]
    pub fn u8x16_load_unaligned(self, slice: &[u8]) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe { u8x16::load_unaligned(slice) }
    }

    /// Load 16 bytes from the given slice, without bounds checks.
    #[inline]
    pub unsafe fn u8x16_load_unchecked_unaligned(self, slice: &[u8]) -> u8x16 {
        // Safe because we know SSSE3 is enabled, but still unsafe
        // because we aren't doing bounds checks.
        u8x16::load_unchecked_unaligned(slice)
    }

    /// Load 16 bytes from the given slice, with bound and alignment checks.
    #[inline]
    pub fn u8x16_load(self, slice: &[u8]) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe { u8x16::load(slice) }
    }

    /// Load 16 bytes from the given slice, without bound or alignment checks.
    #[inline]
    pub unsafe fn u8x16_load_unchecked(self, slice: &[u8]) -> u8x16 {
        // Safe because we know SSSE3 is enabled, but still unsafe
        // because we aren't doing bounds checks.
        u8x16::load_unchecked(slice)
    }
}

/// A u8x16 is a 128-bit vector with 16 single-byte lanes.
///
/// It provides a safe API that uses only SSE2 or SSSE3 instructions.
/// The only way for callers to construct a value of this type is
/// through the SSSE3VectorBuilder type, and the only way to get a
/// SSSE3VectorBuilder is if the `ssse3` target feature is enabled.
///
/// Note that generally speaking, all uses of this type should get
/// inlined, otherwise you probably have a performance bug.
#[derive(Clone, Copy)]
#[allow(non_camel_case_types)]
#[repr(transparent)]
pub struct u8x16 {
    vector: __m128i
}

impl u8x16 {
    #[inline]
    unsafe fn splat(n: u8) -> u8x16 {
        u8x16 { vector: _mm_set1_epi8(n as i8) }
    }

    #[inline]
    unsafe fn load_unaligned(slice: &[u8]) -> u8x16 {
        assert!(slice.len() >= 16);
        u8x16::load_unchecked(slice)
    }

    #[inline]
    unsafe fn load_unchecked_unaligned(slice: &[u8]) -> u8x16 {
        let v = _mm_loadu_si128(slice.as_ptr() as *const u8 as *const __m128i);
        u8x16 { vector: v }
    }

    #[inline]
    unsafe fn load(slice: &[u8]) -> u8x16 {
        assert!(slice.len() >= 16);
        assert!(slice.as_ptr() as usize % 16 == 0);
        u8x16::load_unchecked(slice)
    }

    #[inline]
    unsafe fn load_unchecked(slice: &[u8]) -> u8x16 {
        let v = _mm_load_si128(slice.as_ptr() as *const u8 as *const __m128i);
        u8x16 { vector: v }
    }

    #[inline]
    pub fn shuffle(self, indices: u8x16) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe {
            u8x16 { vector: _mm_shuffle_epi8(self.vector, indices.vector) }
        }
    }

    #[inline]
    pub fn ne(self, other: u8x16) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe {
            let boolv = _mm_cmpeq_epi8(self.vector, other.vector);
            let ones = _mm_set1_epi8(0xFF as u8 as i8);
            u8x16 { vector: _mm_andnot_si128(boolv, ones) }
        }
    }

    #[inline]
    pub fn and(self, other: u8x16) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe {
            u8x16 { vector: _mm_and_si128(self.vector, other.vector) }
        }
    }

    #[inline]
    pub fn movemask(self) -> u32 {
        // Safe because we know SSSE3 is enabled.
        unsafe {
            _mm_movemask_epi8(self.vector) as u32
        }
    }

    #[inline]
    pub fn alignr_14(self, other: u8x16) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe {
            u8x16 { vector: _mm_alignr_epi8(self.vector, other.vector, 14) }
        }
    }

    #[inline]
    pub fn alignr_15(self, other: u8x16) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe {
            u8x16 { vector: _mm_alignr_epi8(self.vector, other.vector, 15) }
        }
    }

    #[inline]
    pub fn bit_shift_right_4(self) -> u8x16 {
        // Safe because we know SSSE3 is enabled.
        unsafe {
            u8x16 { vector: _mm_srli_epi16(self.vector, 4) }
        }
    }

    #[inline]
    pub fn bytes(self) -> [u8; 16] {
        // Safe because __m128i and [u8; 16] are layout compatible
        unsafe { mem::transmute(self) }
    }

    #[inline]
    pub fn replace_bytes(&mut self, value: [u8; 16]) {
        // Safe because __m128i and [u8; 16] are layout compatible
        self.vector = unsafe { mem::transmute(value) };
    }
}

impl fmt::Debug for u8x16 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.bytes().fmt(f)
    }
}
