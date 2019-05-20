// Copyright 2018 Developers of the Rand project.
// Copyright 2013-2017 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Utilities for random number generation
//!
//! Rand provides utilities to generate random numbers, to convert them to
//! useful types and distributions, and some randomness-related algorithms.
//!
//! # Quick Start
//!
//! To get you started quickly, the easiest and highest-level way to get
//! a random value is to use [`random()`]; alternatively you can use
//! [`thread_rng()`]. The [`Rng`] trait provides a useful API on all RNGs, while
//! the [`distributions`] and [`seq`] modules provide further
//! functionality on top of RNGs.
//!
//! ```
//! use rand::prelude::*;
//!
//! if rand::random() { // generates a boolean
//!     // Try printing a random unicode code point (probably a bad idea)!
//!     println!("char: {}", rand::random::<char>());
//! }
//!
//! let mut rng = rand::thread_rng();
//! let y: f64 = rng.gen(); // generates a float between 0 and 1
//!
//! let mut nums: Vec<i32> = (1..100).collect();
//! nums.shuffle(&mut rng);
//! ```
//!
//! # The Book
//!
//! For the user guide and futher documentation, please read
//! [The Rust Rand Book](https://rust-random.github.io/book).


#![doc(html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk.png",
       html_favicon_url = "https://www.rust-lang.org/favicon.ico",
       html_root_url = "https://rust-random.github.io/rand/")]

#![deny(missing_docs)]
#![deny(missing_debug_implementations)]
#![doc(test(attr(allow(unused_variables), deny(warnings))))]

#![cfg_attr(not(feature="std"), no_std)]
#![cfg_attr(all(feature="alloc", not(feature="std")), feature(alloc))]
#![cfg_attr(all(feature="simd_support", feature="nightly"), feature(stdsimd))]

#[cfg(feature = "std")] extern crate core;
#[cfg(all(feature = "alloc", not(feature="std")))] #[macro_use] extern crate alloc;

#[cfg(feature="simd_support")] extern crate packed_simd;

extern crate rand_jitter;
#[cfg(feature = "rand_os")]
extern crate rand_os;

extern crate rand_core;
extern crate rand_isaac;    // only for deprecations
extern crate rand_chacha;    // only for deprecations
extern crate rand_hc;
extern crate rand_pcg;
extern crate rand_xorshift;

#[cfg(feature = "log")] #[macro_use] extern crate log;
#[allow(unused)]
#[cfg(not(feature = "log"))] macro_rules! trace { ($($x:tt)*) => () }
#[allow(unused)]
#[cfg(not(feature = "log"))] macro_rules! debug { ($($x:tt)*) => () }
#[allow(unused)]
#[cfg(not(feature = "log"))] macro_rules! info { ($($x:tt)*) => () }
#[allow(unused)]
#[cfg(not(feature = "log"))] macro_rules! warn { ($($x:tt)*) => () }
#[allow(unused)]
#[cfg(not(feature = "log"))] macro_rules! error { ($($x:tt)*) => () }


// Re-exports from rand_core
pub use rand_core::{RngCore, CryptoRng, SeedableRng};
pub use rand_core::{ErrorKind, Error};

// Public exports
#[cfg(feature="std")] pub use rngs::thread::thread_rng;

// Public modules
pub mod distributions;
pub mod prelude;
#[deprecated(since="0.6.0")]
pub mod prng;
pub mod rngs;
pub mod seq;

////////////////////////////////////////////////////////////////////////////////
// Compatibility re-exports. Documentation is hidden; will be removed eventually.

#[doc(hidden)] mod deprecated;

#[allow(deprecated)]
#[doc(hidden)] pub use deprecated::ReseedingRng;

#[allow(deprecated)]
#[cfg(feature="std")] #[doc(hidden)] pub use deprecated::EntropyRng;

#[allow(deprecated)]
#[cfg(feature="rand_os")]
#[doc(hidden)]
pub use deprecated::OsRng;

#[allow(deprecated)]
#[doc(hidden)] pub use deprecated::{ChaChaRng, IsaacRng, Isaac64Rng, XorShiftRng};
#[allow(deprecated)]
#[doc(hidden)] pub use deprecated::StdRng;


#[allow(deprecated)]
#[doc(hidden)]
pub mod jitter {
    pub use deprecated::JitterRng;
    pub use rngs::TimerError;
}
#[allow(deprecated)]
#[cfg(feature="rand_os")]
#[doc(hidden)]
pub mod os {
    pub use deprecated::OsRng;
}
#[allow(deprecated)]
#[doc(hidden)]
pub mod chacha {
    pub use deprecated::ChaChaRng;
}
#[allow(deprecated)]
#[doc(hidden)]
pub mod isaac {
    pub use deprecated::{IsaacRng, Isaac64Rng};
}
#[allow(deprecated)]
#[cfg(feature="std")]
#[doc(hidden)]
pub mod read {
    pub use deprecated::ReadRng;
}

#[allow(deprecated)]
#[cfg(feature="std")] #[doc(hidden)] pub use deprecated::ThreadRng;

////////////////////////////////////////////////////////////////////////////////


use core::{mem, slice};
use distributions::{Distribution, Standard};
use distributions::uniform::{SampleUniform, UniformSampler, SampleBorrow};

/// An automatically-implemented extension trait on [`RngCore`] providing high-level
/// generic methods for sampling values and other convenience methods.
///
/// This is the primary trait to use when generating random values.
///
/// # Generic usage
///
/// The basic pattern is `fn foo<R: Rng + ?Sized>(rng: &mut R)`. Some
/// things are worth noting here:
///
/// - Since `Rng: RngCore` and every `RngCore` implements `Rng`, it makes no
///   difference whether we use `R: Rng` or `R: RngCore`.
/// - The `+ ?Sized` un-bounding allows functions to be called directly on
///   type-erased references; i.e. `foo(r)` where `r: &mut RngCore`. Without
///   this it would be necessary to write `foo(&mut r)`.
///
/// An alternative pattern is possible: `fn foo<R: Rng>(rng: R)`. This has some
/// trade-offs. It allows the argument to be consumed directly without a `&mut`
/// (which is how `from_rng(thread_rng())` works); also it still works directly
/// on references (including type-erased references). Unfortunately within the
/// function `foo` it is not known whether `rng` is a reference type or not,
/// hence many uses of `rng` require an extra reference, either explicitly
/// (`distr.sample(&mut rng)`) or implicitly (`rng.gen()`); one may hope the
/// optimiser can remove redundant references later.
///
/// Example:
///
/// ```
/// # use rand::thread_rng;
/// use rand::Rng;
///
/// fn foo<R: Rng + ?Sized>(rng: &mut R) -> f32 {
///     rng.gen()
/// }
///
/// # let v = foo(&mut thread_rng());
/// ```
pub trait Rng: RngCore {
    /// Return a random value supporting the [`Standard`] distribution.
    ///
    /// [`Standard`]: distributions::Standard
    ///
    /// # Example
    ///
    /// ```
    /// use rand::{thread_rng, Rng};
    ///
    /// let mut rng = thread_rng();
    /// let x: u32 = rng.gen();
    /// println!("{}", x);
    /// println!("{:?}", rng.gen::<(f64, bool)>());
    /// ```
    #[inline]
    fn gen<T>(&mut self) -> T where Standard: Distribution<T> {
        Standard.sample(self)
    }

    /// Generate a random value in the range [`low`, `high`), i.e. inclusive of
    /// `low` and exclusive of `high`.
    ///
    /// This function is optimised for the case that only a single sample is
    /// made from the given range. See also the [`Uniform`] distribution
    /// type which may be faster if sampling from the same range repeatedly.
    ///
    /// # Panics
    ///
    /// Panics if `low >= high`.
    ///
    /// # Example
    ///
    /// ```
    /// use rand::{thread_rng, Rng};
    ///
    /// let mut rng = thread_rng();
    /// let n: u32 = rng.gen_range(0, 10);
    /// println!("{}", n);
    /// let m: f64 = rng.gen_range(-40.0f64, 1.3e5f64);
    /// println!("{}", m);
    /// ```
    ///
    /// [`Uniform`]: distributions::uniform::Uniform
    fn gen_range<T: SampleUniform, B1, B2>(&mut self, low: B1, high: B2) -> T
        where B1: SampleBorrow<T> + Sized,
              B2: SampleBorrow<T> + Sized {
        T::Sampler::sample_single(low, high, self)
    }

    /// Sample a new value, using the given distribution.
    ///
    /// ### Example
    ///
    /// ```
    /// use rand::{thread_rng, Rng};
    /// use rand::distributions::Uniform;
    ///
    /// let mut rng = thread_rng();
    /// let x = rng.sample(Uniform::new(10u32, 15));
    /// // Type annotation requires two types, the type and distribution; the
    /// // distribution can be inferred.
    /// let y = rng.sample::<u16, _>(Uniform::new(10, 15));
    /// ```
    fn sample<T, D: Distribution<T>>(&mut self, distr: D) -> T {
        distr.sample(self)
    }

    /// Create an iterator that generates values using the given distribution.
    ///
    /// # Example
    ///
    /// ```
    /// use rand::{thread_rng, Rng};
    /// use rand::distributions::{Alphanumeric, Uniform, Standard};
    ///
    /// let mut rng = thread_rng();
    ///
    /// // Vec of 16 x f32:
    /// let v: Vec<f32> = thread_rng().sample_iter(&Standard).take(16).collect();
    ///
    /// // String:
    /// let s: String = rng.sample_iter(&Alphanumeric).take(7).collect();
    ///
    /// // Combined values
    /// println!("{:?}", thread_rng().sample_iter(&Standard).take(5)
    ///                              .collect::<Vec<(f64, bool)>>());
    ///
    /// // Dice-rolling:
    /// let die_range = Uniform::new_inclusive(1, 6);
    /// let mut roll_die = rng.sample_iter(&die_range);
    /// while roll_die.next().unwrap() != 6 {
    ///     println!("Not a 6; rolling again!");
    /// }
    /// ```
    fn sample_iter<'a, T, D: Distribution<T>>(&'a mut self, distr: &'a D)
        -> distributions::DistIter<'a, D, Self, T> where Self: Sized
    {
        distr.sample_iter(self)
    }

    /// Fill `dest` entirely with random bytes (uniform value distribution),
    /// where `dest` is any type supporting [`AsByteSliceMut`], namely slices
    /// and arrays over primitive integer types (`i8`, `i16`, `u32`, etc.).
    ///
    /// On big-endian platforms this performs byte-swapping to ensure
    /// portability of results from reproducible generators.
    ///
    /// This uses [`fill_bytes`] internally which may handle some RNG errors
    /// implicitly (e.g. waiting if the OS generator is not ready), but panics
    /// on other errors. See also [`try_fill`] which returns errors.
    ///
    /// # Example
    ///
    /// ```
    /// use rand::{thread_rng, Rng};
    ///
    /// let mut arr = [0i8; 20];
    /// thread_rng().fill(&mut arr[..]);
    /// ```
    ///
    /// [`fill_bytes`]: RngCore::fill_bytes
    /// [`try_fill`]: Rng::try_fill
    fn fill<T: AsByteSliceMut + ?Sized>(&mut self, dest: &mut T) {
        self.fill_bytes(dest.as_byte_slice_mut());
        dest.to_le();
    }

    /// Fill `dest` entirely with random bytes (uniform value distribution),
    /// where `dest` is any type supporting [`AsByteSliceMut`], namely slices
    /// and arrays over primitive integer types (`i8`, `i16`, `u32`, etc.).
    ///
    /// On big-endian platforms this performs byte-swapping to ensure
    /// portability of results from reproducible generators.
    ///
    /// This uses [`try_fill_bytes`] internally and forwards all RNG errors. In
    /// some cases errors may be resolvable; see [`ErrorKind`] and
    /// documentation for the RNG in use. If you do not plan to handle these
    /// errors you may prefer to use [`fill`].
    ///
    /// # Example
    ///
    /// ```
    /// # use rand::Error;
    /// use rand::{thread_rng, Rng};
    ///
    /// # fn try_inner() -> Result<(), Error> {
    /// let mut arr = [0u64; 4];
    /// thread_rng().try_fill(&mut arr[..])?;
    /// # Ok(())
    /// # }
    ///
    /// # try_inner().unwrap()
    /// ```
    ///
    /// [`try_fill_bytes`]: RngCore::try_fill_bytes
    /// [`fill`]: Rng::fill
    fn try_fill<T: AsByteSliceMut + ?Sized>(&mut self, dest: &mut T) -> Result<(), Error> {
        self.try_fill_bytes(dest.as_byte_slice_mut())?;
        dest.to_le();
        Ok(())
    }

    /// Return a bool with a probability `p` of being true.
    ///
    /// See also the [`Bernoulli`] distribution, which may be faster if
    /// sampling from the same probability repeatedly.
    ///
    /// # Example
    ///
    /// ```
    /// use rand::{thread_rng, Rng};
    ///
    /// let mut rng = thread_rng();
    /// println!("{}", rng.gen_bool(1.0 / 3.0));
    /// ```
    ///
    /// # Panics
    ///
    /// If `p < 0` or `p > 1`.
    ///
    /// [`Bernoulli`]: distributions::bernoulli::Bernoulli
    #[inline]
    fn gen_bool(&mut self, p: f64) -> bool {
        let d = distributions::Bernoulli::new(p);
        self.sample(d)
    }

    /// Return a bool with a probability of `numerator/denominator` of being
    /// true. I.e. `gen_ratio(2, 3)` has chance of 2 in 3, or about 67%, of
    /// returning true. If `numerator == denominator`, then the returned value
    /// is guaranteed to be `true`. If `numerator == 0`, then the returned
    /// value is guaranteed to be `false`.
    ///
    /// See also the [`Bernoulli`] distribution, which may be faster if
    /// sampling from the same `numerator` and `denominator` repeatedly.
    ///
    /// # Panics
    ///
    /// If `denominator == 0` or `numerator > denominator`.
    ///
    /// # Example
    ///
    /// ```
    /// use rand::{thread_rng, Rng};
    ///
    /// let mut rng = thread_rng();
    /// println!("{}", rng.gen_ratio(2, 3));
    /// ```
    ///
    /// [`Bernoulli`]: distributions::bernoulli::Bernoulli
    #[inline]
    fn gen_ratio(&mut self, numerator: u32, denominator: u32) -> bool {
        let d = distributions::Bernoulli::from_ratio(numerator, denominator);
        self.sample(d)
    }

    /// Return a random element from `values`.
    ///
    /// Deprecated: use [`seq::SliceRandom::choose`] instead.
    #[deprecated(since="0.6.0", note="use SliceRandom::choose instead")]
    fn choose<'a, T>(&mut self, values: &'a [T]) -> Option<&'a T> {
        use seq::SliceRandom;
        values.choose(self)
    }

    /// Return a mutable pointer to a random element from `values`.
    ///
    /// Deprecated: use [`seq::SliceRandom::choose_mut`] instead.
    #[deprecated(since="0.6.0", note="use SliceRandom::choose_mut instead")]
    fn choose_mut<'a, T>(&mut self, values: &'a mut [T]) -> Option<&'a mut T> {
        use seq::SliceRandom;
        values.choose_mut(self)
    }

    /// Shuffle a mutable slice in place.
    ///
    /// Deprecated: use [`seq::SliceRandom::shuffle`] instead.
    #[deprecated(since="0.6.0", note="use SliceRandom::shuffle instead")]
    fn shuffle<T>(&mut self, values: &mut [T]) {
        use seq::SliceRandom;
        values.shuffle(self)
    }
}

impl<R: RngCore + ?Sized> Rng for R {}

/// Trait for casting types to byte slices
///
/// This is used by the [`Rng::fill`] and [`Rng::try_fill`] methods.
pub trait AsByteSliceMut {
    /// Return a mutable reference to self as a byte slice
    fn as_byte_slice_mut(&mut self) -> &mut [u8];

    /// Call `to_le` on each element (i.e. byte-swap on Big Endian platforms).
    fn to_le(&mut self);
}

impl AsByteSliceMut for [u8] {
    fn as_byte_slice_mut(&mut self) -> &mut [u8] {
        self
    }

    fn to_le(&mut self) {}
}

macro_rules! impl_as_byte_slice {
    ($t:ty) => {
        impl AsByteSliceMut for [$t] {
            fn as_byte_slice_mut(&mut self) -> &mut [u8] {
                if self.len() == 0 {
                    unsafe {
                        // must not use null pointer
                        slice::from_raw_parts_mut(0x1 as *mut u8, 0)
                    }
                } else {
                    unsafe {
                        slice::from_raw_parts_mut(&mut self[0]
                            as *mut $t
                            as *mut u8,
                            self.len() * mem::size_of::<$t>()
                        )
                    }
                }
            }

            fn to_le(&mut self) {
                for x in self {
                    *x = x.to_le();
                }
            }
        }
    }
}

impl_as_byte_slice!(u16);
impl_as_byte_slice!(u32);
impl_as_byte_slice!(u64);
#[cfg(all(rustc_1_26, not(target_os = "emscripten")))] impl_as_byte_slice!(u128);
impl_as_byte_slice!(usize);
impl_as_byte_slice!(i8);
impl_as_byte_slice!(i16);
impl_as_byte_slice!(i32);
impl_as_byte_slice!(i64);
#[cfg(all(rustc_1_26, not(target_os = "emscripten")))] impl_as_byte_slice!(i128);
impl_as_byte_slice!(isize);

macro_rules! impl_as_byte_slice_arrays {
    ($n:expr,) => {};
    ($n:expr, $N:ident, $($NN:ident,)*) => {
        impl_as_byte_slice_arrays!($n - 1, $($NN,)*);

        impl<T> AsByteSliceMut for [T; $n] where [T]: AsByteSliceMut {
            fn as_byte_slice_mut(&mut self) -> &mut [u8] {
                self[..].as_byte_slice_mut()
            }

            fn to_le(&mut self) {
                self[..].to_le()
            }
        }
    };
    (!div $n:expr,) => {};
    (!div $n:expr, $N:ident, $($NN:ident,)*) => {
        impl_as_byte_slice_arrays!(!div $n / 2, $($NN,)*);

        impl<T> AsByteSliceMut for [T; $n] where [T]: AsByteSliceMut {
            fn as_byte_slice_mut(&mut self) -> &mut [u8] {
                self[..].as_byte_slice_mut()
            }

            fn to_le(&mut self) {
                self[..].to_le()
            }
        }
    };
}
impl_as_byte_slice_arrays!(32, N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,N,);
impl_as_byte_slice_arrays!(!div 4096, N,N,N,N,N,N,N,);


/// A convenience extension to [`SeedableRng`] allowing construction from fresh
/// entropy. This trait is automatically implemented for any PRNG implementing
/// [`SeedableRng`] and is not intended to be implemented by users.
///
/// This is equivalent to using `SeedableRng::from_rng(EntropyRng::new())` then
/// unwrapping the result.
///
/// Since this is convenient and secure, it is the recommended way to create
/// PRNGs, though two alternatives may be considered:
///
/// *   Deterministic creation using [`SeedableRng::from_seed`] with a fixed seed
/// *   Seeding from `thread_rng`: `SeedableRng::from_rng(thread_rng())?`;
///     this will usually be faster and should also be secure, but requires
///     trusting one extra component.
///
/// ## Example
///
/// ```
/// use rand::{Rng, FromEntropy};
/// use rand::rngs::StdRng;
///
/// let mut rng = StdRng::from_entropy();
/// println!("Random die roll: {}", rng.gen_range(1, 7));
/// ```
///
/// [`EntropyRng`]: rngs::EntropyRng
#[cfg(feature="std")]
pub trait FromEntropy: SeedableRng {
    /// Creates a new instance, automatically seeded with fresh entropy.
    ///
    /// Normally this will use `OsRng`, but if that fails `JitterRng` will be
    /// used instead. Both should be suitable for cryptography. It is possible
    /// that both entropy sources will fail though unlikely; failures would
    /// almost certainly be platform limitations or build issues, i.e. most
    /// applications targetting PC/mobile platforms should not need to worry
    /// about this failing.
    ///
    /// # Panics
    ///
    /// If all entropy sources fail this will panic. If you need to handle
    /// errors, use the following code, equivalent aside from error handling:
    ///
    /// ```
    /// # use rand::Error;
    /// use rand::prelude::*;
    /// use rand::rngs::EntropyRng;
    ///
    /// # fn try_inner() -> Result<(), Error> {
    /// // This uses StdRng, but is valid for any R: SeedableRng
    /// let mut rng = StdRng::from_rng(EntropyRng::new())?;
    ///
    /// println!("random number: {}", rng.gen_range(1, 10));
    /// # Ok(())
    /// # }
    ///
    /// # try_inner().unwrap()
    /// ```
    fn from_entropy() -> Self;
}

#[cfg(feature="std")]
impl<R: SeedableRng> FromEntropy for R {
    fn from_entropy() -> R {
        R::from_rng(rngs::EntropyRng::new()).unwrap_or_else(|err|
            panic!("FromEntropy::from_entropy() failed: {}", err))
    }
}


/// Generates a random value using the thread-local random number generator.
///
/// This is simply a shortcut for `thread_rng().gen()`. See [`thread_rng`] for
/// documentation of the entropy source and [`Standard`] for documentation of
/// distributions and type-specific generation.
///
/// # Examples
///
/// ```
/// let x = rand::random::<u8>();
/// println!("{}", x);
///
/// let y = rand::random::<f64>();
/// println!("{}", y);
///
/// if rand::random() { // generates a boolean
///     println!("Better lucky than good!");
/// }
/// ```
///
/// If you're calling `random()` in a loop, caching the generator as in the
/// following example can increase performance.
///
/// ```
/// use rand::Rng;
///
/// let mut v = vec![1, 2, 3];
///
/// for x in v.iter_mut() {
///     *x = rand::random()
/// }
///
/// // can be made faster by caching thread_rng
///
/// let mut rng = rand::thread_rng();
///
/// for x in v.iter_mut() {
///     *x = rng.gen();
/// }
/// ```
///
/// [`Standard`]: distributions::Standard
#[cfg(feature="std")]
#[inline]
pub fn random<T>() -> T where Standard: Distribution<T> {
    thread_rng().gen()
}

#[cfg(test)]
mod test {
    use rngs::mock::StepRng;
    use rngs::StdRng;
    use super::*;
    #[cfg(all(not(feature="std"), feature="alloc"))] use alloc::boxed::Box;

    pub struct TestRng<R> { inner: R }

    impl<R: RngCore> RngCore for TestRng<R> {
        fn next_u32(&mut self) -> u32 {
            self.inner.next_u32()
        }
        fn next_u64(&mut self) -> u64 {
            self.inner.next_u64()
        }
        fn fill_bytes(&mut self, dest: &mut [u8]) {
            self.inner.fill_bytes(dest)
        }
        fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
            self.inner.try_fill_bytes(dest)
        }
    }

    pub fn rng(seed: u64) -> TestRng<StdRng> {
        TestRng { inner: StdRng::seed_from_u64(seed) }
    }

    #[test]
    fn test_fill_bytes_default() {
        let mut r = StepRng::new(0x11_22_33_44_55_66_77_88, 0);

        // check every remainder mod 8, both in small and big vectors.
        let lengths = [0, 1, 2, 3, 4, 5, 6, 7,
                       80, 81, 82, 83, 84, 85, 86, 87];
        for &n in lengths.iter() {
            let mut buffer = [0u8; 87];
            let v = &mut buffer[0..n];
            r.fill_bytes(v);

            // use this to get nicer error messages.
            for (i, &byte) in v.iter().enumerate() {
                if byte == 0 {
                    panic!("byte {} of {} is zero", i, n)
                }
            }
        }
    }

    #[test]
    fn test_fill() {
        let x = 9041086907909331047;    // a random u64
        let mut rng = StepRng::new(x, 0);

        // Convert to byte sequence and back to u64; byte-swap twice if BE.
        let mut array = [0u64; 2];
        rng.fill(&mut array[..]);
        assert_eq!(array, [x, x]);
        assert_eq!(rng.next_u64(), x);

        // Convert to bytes then u32 in LE order
        let mut array = [0u32; 2];
        rng.fill(&mut array[..]);
        assert_eq!(array, [x as u32, (x >> 32) as u32]);
        assert_eq!(rng.next_u32(), x as u32);
    }

    #[test]
    fn test_fill_empty() {
        let mut array = [0u32; 0];
        let mut rng = StepRng::new(0, 1);
        rng.fill(&mut array);
        rng.fill(&mut array[..]);
    }

    #[test]
    fn test_gen_range() {
        let mut r = rng(101);
        for _ in 0..1000 {
            let a = r.gen_range(-4711, 17);
            assert!(a >= -4711 && a < 17);
            let a = r.gen_range(-3i8, 42);
            assert!(a >= -3i8 && a < 42i8);
            let a = r.gen_range(&10u16, 99);
            assert!(a >= 10u16 && a < 99u16);
            let a = r.gen_range(-100i32, &2000);
            assert!(a >= -100i32 && a < 2000i32);
            let a = r.gen_range(&12u32, &24u32);
            assert!(a >= 12u32 && a < 24u32);

            assert_eq!(r.gen_range(0u32, 1), 0u32);
            assert_eq!(r.gen_range(-12i64, -11), -12i64);
            assert_eq!(r.gen_range(3_000_000, 3_000_001), 3_000_000);
        }
    }

    #[test]
    #[should_panic]
    fn test_gen_range_panic_int() {
        let mut r = rng(102);
        r.gen_range(5, -2);
    }

    #[test]
    #[should_panic]
    fn test_gen_range_panic_usize() {
        let mut r = rng(103);
        r.gen_range(5, 2);
    }

    #[test]
    fn test_gen_bool() {
        let mut r = rng(105);
        for _ in 0..5 {
            assert_eq!(r.gen_bool(0.0), false);
            assert_eq!(r.gen_bool(1.0), true);
        }
    }

    #[test]
    fn test_rng_trait_object() {
        use distributions::{Distribution, Standard};
        let mut rng = rng(109);
        let mut r = &mut rng as &mut RngCore;
        r.next_u32();
        r.gen::<i32>();
        assert_eq!(r.gen_range(0, 1), 0);
        let _c: u8 = Standard.sample(&mut r);
    }

    #[test]
    #[cfg(feature="alloc")]
    fn test_rng_boxed_trait() {
        use distributions::{Distribution, Standard};
        let rng = rng(110);
        let mut r = Box::new(rng) as Box<RngCore>;
        r.next_u32();
        r.gen::<i32>();
        assert_eq!(r.gen_range(0, 1), 0);
        let _c: u8 = Standard.sample(&mut r);
    }

    #[test]
    #[cfg(feature="std")]
    fn test_random() {
        // not sure how to test this aside from just getting some values
        let _n : usize = random();
        let _f : f32 = random();
        let _o : Option<Option<i8>> = random();
        let _many : ((),
                     (usize,
                      isize,
                      Option<(u32, (bool,))>),
                     (u8, i8, u16, i16, u32, i32, u64, i64),
                     (f32, (f64, (f64,)))) = random();
    }

    #[test]
    fn test_gen_ratio_average() {
        const NUM: u32 = 3;
        const DENOM: u32 = 10;
        const N: u32 = 100_000;

        let mut sum: u32 = 0;
        let mut rng = rng(111);
        for _ in 0..N {
            if rng.gen_ratio(NUM, DENOM) {
                sum += 1;
            }
        }
        // Have Binomial(N, NUM/DENOM) distribution
        let expected = (NUM * N) / DENOM;   // exact integer
        assert!(((sum - expected) as i32).abs() < 500);
    }
}
