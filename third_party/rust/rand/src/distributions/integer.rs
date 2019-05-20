// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The implementations of the `Standard` distribution for integer types.

use {Rng};
use distributions::{Distribution, Standard};
#[cfg(feature="simd_support")]
use packed_simd::*;
#[cfg(all(target_arch = "x86", feature="nightly"))]
use core::arch::x86::*;
#[cfg(all(target_arch = "x86_64", feature="nightly"))]
use core::arch::x86_64::*;

impl Distribution<u8> for Standard {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> u8 {
        rng.next_u32() as u8
    }
}

impl Distribution<u16> for Standard {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> u16 {
        rng.next_u32() as u16
    }
}

impl Distribution<u32> for Standard {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> u32 {
        rng.next_u32()
    }
}

impl Distribution<u64> for Standard {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> u64 {
        rng.next_u64()
    }
}

#[cfg(all(rustc_1_26, not(target_os = "emscripten")))]
impl Distribution<u128> for Standard {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> u128 {
        // Use LE; we explicitly generate one value before the next.
        let x = rng.next_u64() as u128;
        let y = rng.next_u64() as u128;
        (y << 64) | x
    }
}

impl Distribution<usize> for Standard {
    #[inline]
    #[cfg(any(target_pointer_width = "32", target_pointer_width = "16"))]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> usize {
        rng.next_u32() as usize
    }

    #[inline]
    #[cfg(target_pointer_width = "64")]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> usize {
        rng.next_u64() as usize
    }
}

macro_rules! impl_int_from_uint {
    ($ty:ty, $uty:ty) => {
        impl Distribution<$ty> for Standard {
            #[inline]
            fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> $ty {
                rng.gen::<$uty>() as $ty
            }
        }
    }
}

impl_int_from_uint! { i8, u8 }
impl_int_from_uint! { i16, u16 }
impl_int_from_uint! { i32, u32 }
impl_int_from_uint! { i64, u64 }
#[cfg(all(rustc_1_26, not(target_os = "emscripten")))] impl_int_from_uint! { i128, u128 }
impl_int_from_uint! { isize, usize }

#[cfg(feature="simd_support")]
macro_rules! simd_impl {
    ($(($intrinsic:ident, $vec:ty),)+) => {$(
        impl Distribution<$intrinsic> for Standard {
            #[inline]
            fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> $intrinsic {
                $intrinsic::from_bits(rng.gen::<$vec>())
            }
        }
    )+};

    ($bits:expr,) => {};
    ($bits:expr, $ty:ty, $($ty_more:ty,)*) => {
        simd_impl!($bits, $($ty_more,)*);

        impl Distribution<$ty> for Standard {
            #[inline]
            fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> $ty {
                let mut vec: $ty = Default::default();
                unsafe {
                    let ptr = &mut vec;
                    let b_ptr = &mut *(ptr as *mut $ty as *mut [u8; $bits/8]);
                    rng.fill_bytes(b_ptr);
                }
                vec.to_le()
            }
        }
    };
}

#[cfg(feature="simd_support")]
simd_impl!(16, u8x2, i8x2,);
#[cfg(feature="simd_support")]
simd_impl!(32, u8x4, i8x4, u16x2, i16x2,);
#[cfg(feature="simd_support")]
simd_impl!(64, u8x8, i8x8, u16x4, i16x4, u32x2, i32x2,);
#[cfg(feature="simd_support")]
simd_impl!(128, u8x16, i8x16, u16x8, i16x8, u32x4, i32x4, u64x2, i64x2,);
#[cfg(feature="simd_support")]
simd_impl!(256, u8x32, i8x32, u16x16, i16x16, u32x8, i32x8, u64x4, i64x4,);
#[cfg(feature="simd_support")]
simd_impl!(512, u8x64, i8x64, u16x32, i16x32, u32x16, i32x16, u64x8, i64x8,);
#[cfg(all(feature="simd_support", feature="nightly", any(target_arch="x86", target_arch="x86_64")))]
simd_impl!((__m64, u8x8), (__m128i, u8x16), (__m256i, u8x32),);

#[cfg(test)]
mod tests {
    use Rng;
    use distributions::{Standard};
    
    #[test]
    fn test_integers() {
        let mut rng = ::test::rng(806);
        
        rng.sample::<isize, _>(Standard);
        rng.sample::<i8, _>(Standard);
        rng.sample::<i16, _>(Standard);
        rng.sample::<i32, _>(Standard);
        rng.sample::<i64, _>(Standard);
        #[cfg(all(rustc_1_26, not(target_os = "emscripten")))]
        rng.sample::<i128, _>(Standard);
        
        rng.sample::<usize, _>(Standard);
        rng.sample::<u8, _>(Standard);
        rng.sample::<u16, _>(Standard);
        rng.sample::<u32, _>(Standard);
        rng.sample::<u64, _>(Standard);
        #[cfg(all(rustc_1_26, not(target_os = "emscripten")))]
        rng.sample::<u128, _>(Standard);
    }
}
