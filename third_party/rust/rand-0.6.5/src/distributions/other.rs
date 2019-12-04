// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The implementations of the `Standard` distribution for other built-in types.

use core::char;
use core::num::Wrapping;

use {Rng};
use distributions::{Distribution, Standard, Uniform};

// ----- Sampling distributions -----

/// Sample a `char`, uniformly distributed over ASCII letters and numbers:
/// a-z, A-Z and 0-9.
/// 
/// # Example
///
/// ```
/// use std::iter;
/// use rand::{Rng, thread_rng};
/// use rand::distributions::Alphanumeric;
/// 
/// let mut rng = thread_rng();
/// let chars: String = iter::repeat(())
///         .map(|()| rng.sample(Alphanumeric))
///         .take(7)
///         .collect();
/// println!("Random chars: {}", chars);
/// ```
#[derive(Debug)]
pub struct Alphanumeric;


// ----- Implementations of distributions -----

impl Distribution<char> for Standard {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> char {
        // A valid `char` is either in the interval `[0, 0xD800)` or
        // `(0xDFFF, 0x11_0000)`. All `char`s must therefore be in
        // `[0, 0x11_0000)` but not in the "gap" `[0xD800, 0xDFFF]` which is
        // reserved for surrogates. This is the size of that gap.
        const GAP_SIZE: u32 = 0xDFFF - 0xD800 + 1;

        // Uniform::new(0, 0x11_0000 - GAP_SIZE) can also be used but it
        // seemed slower.
        let range = Uniform::new(GAP_SIZE, 0x11_0000);

        let mut n = range.sample(rng);
        if n <= 0xDFFF {
            n -= GAP_SIZE;
        }
        unsafe { char::from_u32_unchecked(n) }
    }
}

impl Distribution<char> for Alphanumeric {
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> char {
        const RANGE: u32 = 26 + 26 + 10;
        const GEN_ASCII_STR_CHARSET: &[u8] =
            b"ABCDEFGHIJKLMNOPQRSTUVWXYZ\
                abcdefghijklmnopqrstuvwxyz\
                0123456789";
        // We can pick from 62 characters. This is so close to a power of 2, 64,
        // that we can do better than `Uniform`. Use a simple bitshift and
        // rejection sampling. We do not use a bitmask, because for small RNGs
        // the most significant bits are usually of higher quality.
        loop {
            let var = rng.next_u32() >> (32 - 6);
            if var < RANGE {
                return GEN_ASCII_STR_CHARSET[var as usize] as char
            }
        }
    }
}

impl Distribution<bool> for Standard {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> bool {
        // We can compare against an arbitrary bit of an u32 to get a bool.
        // Because the least significant bits of a lower quality RNG can have
        // simple patterns, we compare against the most significant bit. This is
        // easiest done using a sign test.
        (rng.next_u32() as i32) < 0
    }
}

macro_rules! tuple_impl {
    // use variables to indicate the arity of the tuple
    ($($tyvar:ident),* ) => {
        // the trailing commas are for the 1 tuple
        impl< $( $tyvar ),* >
            Distribution<( $( $tyvar ),* , )>
            for Standard
            where $( Standard: Distribution<$tyvar> ),*
        {
            #[inline]
            fn sample<R: Rng + ?Sized>(&self, _rng: &mut R) -> ( $( $tyvar ),* , ) {
                (
                    // use the $tyvar's to get the appropriate number of
                    // repeats (they're not actually needed)
                    $(
                        _rng.gen::<$tyvar>()
                    ),*
                    ,
                )
            }
        }
    }
}

impl Distribution<()> for Standard {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, _: &mut R) -> () { () }
}
tuple_impl!{A}
tuple_impl!{A, B}
tuple_impl!{A, B, C}
tuple_impl!{A, B, C, D}
tuple_impl!{A, B, C, D, E}
tuple_impl!{A, B, C, D, E, F}
tuple_impl!{A, B, C, D, E, F, G}
tuple_impl!{A, B, C, D, E, F, G, H}
tuple_impl!{A, B, C, D, E, F, G, H, I}
tuple_impl!{A, B, C, D, E, F, G, H, I, J}
tuple_impl!{A, B, C, D, E, F, G, H, I, J, K}
tuple_impl!{A, B, C, D, E, F, G, H, I, J, K, L}

macro_rules! array_impl {
    // recursive, given at least one type parameter:
    {$n:expr, $t:ident, $($ts:ident,)*} => {
        array_impl!{($n - 1), $($ts,)*}

        impl<T> Distribution<[T; $n]> for Standard where Standard: Distribution<T> {
            #[inline]
            fn sample<R: Rng + ?Sized>(&self, _rng: &mut R) -> [T; $n] {
                [_rng.gen::<$t>(), $(_rng.gen::<$ts>()),*]
            }
        }
    };
    // empty case:
    {$n:expr,} => {
        impl<T> Distribution<[T; $n]> for Standard {
            fn sample<R: Rng + ?Sized>(&self, _rng: &mut R) -> [T; $n] { [] }
        }
    };
}

array_impl!{32, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,}

impl<T> Distribution<Option<T>> for Standard where Standard: Distribution<T> {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> Option<T> {
        // UFCS is needed here: https://github.com/rust-lang/rust/issues/24066
        if rng.gen::<bool>() {
            Some(rng.gen())
        } else {
            None
        }
    }
}

impl<T> Distribution<Wrapping<T>> for Standard where Standard: Distribution<T> {
    #[inline]
    fn sample<R: Rng + ?Sized>(&self, rng: &mut R) -> Wrapping<T> {
        Wrapping(rng.gen())
    }
}


#[cfg(test)]
mod tests {
    use {Rng, RngCore, Standard};
    use distributions::Alphanumeric;
    #[cfg(all(not(feature="std"), feature="alloc"))] use alloc::string::String;

    #[test]
    fn test_misc() {
        let rng: &mut RngCore = &mut ::test::rng(820);
        
        rng.sample::<char, _>(Standard);
        rng.sample::<bool, _>(Standard);
    }
    
    #[cfg(feature="alloc")]
    #[test]
    fn test_chars() {
        use core::iter;
        let mut rng = ::test::rng(805);

        // Test by generating a relatively large number of chars, so we also
        // take the rejection sampling path.
        let word: String = iter::repeat(())
                .map(|()| rng.gen::<char>()).take(1000).collect();
        assert!(word.len() != 0);
    }

    #[test]
    fn test_alphanumeric() {
        let mut rng = ::test::rng(806);

        // Test by generating a relatively large number of chars, so we also
        // take the rejection sampling path.
        let mut incorrect = false;
        for _ in 0..100 {
            let c = rng.sample(Alphanumeric);
            incorrect |= !((c >= '0' && c <= '9') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= 'a' && c <= 'z') );
        }
        assert!(incorrect == false);
    }
}
