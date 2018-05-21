#![allow(dead_code)]

use std::arch::x86_64::*;
use std::fmt;

#[derive(Clone, Copy, Debug)]
pub struct AVX2VectorBuilder(());

impl AVX2VectorBuilder {
    pub fn new() -> Option<AVX2VectorBuilder> {
        if is_x86_feature_detected!("avx2") {
            Some(AVX2VectorBuilder(()))
        } else {
            None
        }
    }

    /// Create a new u8x32 AVX2 vector where all of the bytes are set to
    /// the given value.
    #[inline]
    pub fn u8x32_splat(self, n: u8) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe { u8x32::splat(n) }
    }

    /// Load 32 bytes from the given slice, with bounds checks.
    #[inline]
    pub fn u8x32_load_unaligned(self, slice: &[u8]) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe { u8x32::load_unaligned(slice) }
    }

    /// Load 32 bytes from the given slice, without bounds checks.
    #[inline]
    pub unsafe fn u8x32_load_unchecked_unaligned(self, slice: &[u8]) -> u8x32 {
        // Safe because we know AVX2 is enabled, but still unsafe
        // because we aren't doing bounds checks.
        u8x32::load_unchecked_unaligned(slice)
    }

    /// Load 32 bytes from the given slice, with bound and alignment checks.
    #[inline]
    pub fn u8x32_load(self, slice: &[u8]) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe { u8x32::load(slice) }
    }

    /// Load 32 bytes from the given slice, without bound or alignment checks.
    #[inline]
    pub unsafe fn u8x32_load_unchecked(self, slice: &[u8]) -> u8x32 {
        // Safe because we know AVX2 is enabled, but still unsafe
        // because we aren't doing bounds checks.
        u8x32::load_unchecked(slice)
    }
}

#[derive(Clone, Copy)]
#[allow(non_camel_case_types)]
pub union u8x32 {
    vector: __m256i,
    bytes: [u8; 32],
}

impl u8x32 {
    #[inline]
    unsafe fn splat(n: u8) -> u8x32 {
        u8x32 { vector: _mm256_set1_epi8(n as i8) }
    }

    #[inline]
    unsafe fn load_unaligned(slice: &[u8]) -> u8x32 {
        assert!(slice.len() >= 32);
        u8x32::load_unchecked_unaligned(slice)
    }

    #[inline]
    unsafe fn load_unchecked_unaligned(slice: &[u8]) -> u8x32 {
        let p = slice.as_ptr() as *const u8 as *const __m256i;
        u8x32 { vector: _mm256_loadu_si256(p) }
    }

    #[inline]
    unsafe fn load(slice: &[u8]) -> u8x32 {
        assert!(slice.len() >= 32);
        assert!(slice.as_ptr() as usize % 32 == 0);
        u8x32::load_unchecked(slice)
    }

    #[inline]
    unsafe fn load_unchecked(slice: &[u8]) -> u8x32 {
        let p = slice.as_ptr() as *const u8 as *const __m256i;
        u8x32 { vector: _mm256_load_si256(p) }
    }

    #[inline]
    pub fn extract(self, i: usize) -> u8 {
        // Safe because `bytes` is always accessible.
        unsafe { self.bytes[i] }
    }

    #[inline]
    pub fn replace(&mut self, i: usize, byte: u8) {
        // Safe because `bytes` is always accessible.
        unsafe { self.bytes[i] = byte; }
    }

    #[inline]
    pub fn shuffle(self, indices: u8x32) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe {
            u8x32 { vector: _mm256_shuffle_epi8(self.vector, indices.vector) }
        }
    }

    #[inline]
    pub fn ne(self, other: u8x32) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe {
            let boolv = _mm256_cmpeq_epi8(self.vector, other.vector);
            let ones = _mm256_set1_epi8(0xFF as u8 as i8);
            u8x32 { vector: _mm256_andnot_si256(boolv, ones) }
        }
    }

    #[inline]
    pub fn and(self, other: u8x32) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe {
            u8x32 { vector: _mm256_and_si256(self.vector, other.vector) }
        }
    }

    #[inline]
    pub fn movemask(self) -> u32 {
        // Safe because we know AVX2 is enabled.
        unsafe {
            _mm256_movemask_epi8(self.vector) as u32
        }
    }

    #[inline]
    pub fn alignr_14(self, other: u8x32) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe {
            // Credit goes to jneem for figuring this out:
            // https://github.com/jneem/teddy/blob/9ab5e899ad6ef6911aecd3cf1033f1abe6e1f66c/src/x86/teddy_simd.rs#L145-L184
            //
            // TL;DR avx2's PALIGNR instruction is actually just two 128-bit
            // PALIGNR instructions, which is not what we want, so we need to
            // do some extra shuffling.
            let v = _mm256_permute2x128_si256(other.vector, self.vector, 0x21);
            let v = _mm256_alignr_epi8(self.vector, v, 14);
            u8x32 { vector: v }
        }
    }

    #[inline]
    pub fn alignr_15(self, other: u8x32) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe {
            // Credit goes to jneem for figuring this out:
            // https://github.com/jneem/teddy/blob/9ab5e899ad6ef6911aecd3cf1033f1abe6e1f66c/src/x86/teddy_simd.rs#L145-L184
            //
            // TL;DR avx2's PALIGNR instruction is actually just two 128-bit
            // PALIGNR instructions, which is not what we want, so we need to
            // do some extra shuffling.
            let v = _mm256_permute2x128_si256(other.vector, self.vector, 0x21);
            let v = _mm256_alignr_epi8(self.vector, v, 15);
            u8x32 { vector: v }
        }
    }

    #[inline]
    pub fn bit_shift_right_4(self) -> u8x32 {
        // Safe because we know AVX2 is enabled.
        unsafe {
            u8x32 { vector: _mm256_srli_epi16(self.vector, 4) }
        }
    }
}

impl fmt::Debug for u8x32 {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // Safe because `bytes` is always accessible.
        unsafe { self.bytes.fmt(f) }
    }
}
