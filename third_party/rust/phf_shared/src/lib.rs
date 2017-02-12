#![doc(html_root_url="https://docs.rs/phf_shared/0.7.20")]
#![cfg_attr(feature = "core", no_std)]

#[cfg(not(feature = "core"))]
extern crate std as core;

extern crate siphasher;

#[cfg(feature = "unicase")]
extern crate unicase;

use core::hash::{Hasher, Hash};
use siphasher::sip::SipHasher13;

#[inline]
pub fn displace(f1: u32, f2: u32, d1: u32, d2: u32) -> u32 {
    d2 + f1 * d1 + f2
}

#[inline]
pub fn split(hash: u64) -> (u32, u32, u32) {
    const BITS: u32 = 21;
    const MASK: u64 = (1 << BITS) - 1;

    ((hash & MASK) as u32,
     ((hash >> BITS) & MASK) as u32,
     ((hash >> (2 * BITS)) & MASK) as u32)
}

/// `key` is from `phf_generator::HashState::key`.
#[inline]
pub fn hash<T: ?Sized + PhfHash>(x: &T, key: u64) -> u64 {
    let mut hasher = SipHasher13::new_with_keys(0, key);
    x.phf_hash(&mut hasher);
    hasher.finish()
}

/// Return an index into `phf_generator::HashState::map`.
///
/// * `hash` is from `hash()` in this crate.
/// * `disps` is from `phf_generator::HashState::disps`.
/// * `len` is the length of `phf_generator::HashState::map`.
#[inline]
pub fn get_index(hash: u64, disps: &[(u32, u32)], len: usize) -> u32 {
    let (g, f1, f2) = split(hash);
    let (d1, d2) = disps[(g % (disps.len() as u32)) as usize];
    displace(f1, f2, d1, d2) % (len as u32)
}

/// A trait implemented by types which can be used in PHF data structures.
///
/// This differs from the standard library's `Hash` trait in that `PhfHash`'s
/// results must be architecture independent so that hashes will be consistent
/// between the host and target when cross compiling.
pub trait PhfHash {
    /// Feeds the value into the state given, updating the hasher as necessary.
    fn phf_hash<H: Hasher>(&self, state: &mut H);

    /// Feeds a slice of this type into the state provided.
    fn phf_hash_slice<H: Hasher>(data: &[Self], state: &mut H)
        where Self: Sized
    {
        for piece in data {
            piece.phf_hash(state);
        }
    }
}

#[cfg(not(feature = "core"))]
impl PhfHash for String {
    #[inline]
    fn phf_hash<H: Hasher>(&self, state: &mut H) {
        (**self).phf_hash(state)
    }
}

#[cfg(not(feature = "core"))]
impl PhfHash for Vec<u8> {
    #[inline]
    fn phf_hash<H: Hasher>(&self, state: &mut H) {
        (**self).phf_hash(state)
    }
}

impl<'a> PhfHash for &'a str {
    #[inline]
    fn phf_hash<H: Hasher>(&self, state: &mut H) {
        (*self).phf_hash(state)
    }
}

impl<'a> PhfHash for &'a [u8] {
    #[inline]
    fn phf_hash<H: Hasher>(&self, state: &mut H) {
        (*self).phf_hash(state)
    }
}

impl PhfHash for str {
    #[inline]
    fn phf_hash<H: Hasher>(&self, state: &mut H) {
        self.as_bytes().phf_hash(state)
    }
}

impl PhfHash for [u8] {
    #[inline]
    fn phf_hash<H: Hasher>(&self, state: &mut H) {
        state.write(self);
    }
}

#[cfg(feature = "unicase")]
impl<S> PhfHash for unicase::UniCase<S>
where unicase::UniCase<S>: Hash {
    #[inline]
    fn phf_hash<H: Hasher>(&self, state: &mut H) {
        self.hash(state)
    }
}

macro_rules! sip_impl(
    (le $t:ty) => (
        impl PhfHash for $t {
            #[inline]
            fn phf_hash<H: Hasher>(&self, state: &mut H) {
                self.to_le().hash(state);
            }
        }
    );
    ($t:ty) => (
        impl PhfHash for $t {
            #[inline]
            fn phf_hash<H: Hasher>(&self, state: &mut H) {
                self.hash(state);
            }
        }
    )
);

sip_impl!(u8);
sip_impl!(i8);
sip_impl!(le u16);
sip_impl!(le i16);
sip_impl!(le u32);
sip_impl!(le i32);
sip_impl!(le u64);
sip_impl!(le i64);
sip_impl!(bool);

impl PhfHash for char {
    #[inline]
    fn phf_hash<H: Hasher>(&self, state: &mut H) {
        (*self as u32).phf_hash(state)
    }
}

macro_rules! array_impl(
    ($t:ty, $n:expr) => (
        impl PhfHash for [$t; $n] {
            #[inline]
            fn phf_hash<H: Hasher>(&self, state: &mut H) {
                state.write(self);
            }
        }
    )
);

array_impl!(u8, 1);
array_impl!(u8, 2);
array_impl!(u8, 3);
array_impl!(u8, 4);
array_impl!(u8, 5);
array_impl!(u8, 6);
array_impl!(u8, 7);
array_impl!(u8, 8);
array_impl!(u8, 9);
array_impl!(u8, 10);
array_impl!(u8, 11);
array_impl!(u8, 12);
array_impl!(u8, 13);
array_impl!(u8, 14);
array_impl!(u8, 15);
array_impl!(u8, 16);
array_impl!(u8, 17);
array_impl!(u8, 18);
array_impl!(u8, 19);
array_impl!(u8, 20);
array_impl!(u8, 21);
array_impl!(u8, 22);
array_impl!(u8, 23);
array_impl!(u8, 24);
array_impl!(u8, 25);
array_impl!(u8, 26);
array_impl!(u8, 27);
array_impl!(u8, 28);
array_impl!(u8, 29);
array_impl!(u8, 30);
array_impl!(u8, 31);
array_impl!(u8, 32);
