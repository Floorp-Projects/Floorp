//! # aHash
//!
//! This hashing algorithm is intended to be a high performance, (hardware specific), keyed hash function.
//! This can be seen as a DOS resistant alternative to `FxHash`, or a fast equivalent to `SipHash`.
//! It provides a high speed hash algorithm, but where the result is not predictable without knowing a Key.
//! This allows it to be used in a `HashMap` without allowing for the possibility that an malicious user can
//! induce a collision.
//!
//! # How aHash works
//!
//! aHash uses the hardware AES instruction on x86 processors to provide a keyed hash function.
//! It uses two rounds of AES per hash. So it should not be considered cryptographically secure.
#![deny(clippy::correctness, clippy::complexity, clippy::perf)]
#![allow(clippy::pedantic, clippy::cast_lossless, clippy::unreadable_literal)]
#![cfg_attr(all(not(test), not(feature = "std")), no_std)]

#[macro_use]
mod convert;

#[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "aes"))]
mod aes_hash;
mod fallback_hash;
#[cfg(test)]
mod hash_quality_test;

mod folded_multiply;
#[cfg(feature = "std")]
mod hash_map;
#[cfg(feature = "std")]
mod hash_set;
mod random_state;

#[cfg(feature = "compile-time-rng")]
use const_random::const_random;

#[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "aes"))]
pub use crate::aes_hash::AHasher;

#[cfg(not(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "aes")))]
pub use crate::fallback_hash::AHasher;
pub use crate::random_state::RandomState;

#[cfg(feature = "std")]
pub use crate::hash_map::AHashMap;
#[cfg(feature = "std")]
pub use crate::hash_set::AHashSet;

/// Provides a default [Hasher] compile time generated constants for keys.
/// This is typically used in conjunction with [`BuildHasherDefault`] to create
/// [AHasher]s in order to hash the keys of the map.
///
/// # Example
/// ```
/// use std::hash::BuildHasherDefault;
/// use ahash::{AHasher, RandomState};
/// use std::collections::HashMap;
///
/// let mut map: HashMap<i32, i32, RandomState> = HashMap::default();
/// map.insert(12, 34);
/// ```
///
/// [BuildHasherDefault]: std::hash::BuildHasherDefault
/// [Hasher]: std::hash::Hasher
/// [HashMap]: std::collections::HashMap
#[cfg(feature = "compile-time-rng")]
impl Default for AHasher {
    /// Constructs a new [AHasher] with compile time generated constants for keys.
    /// This means the keys will be the same from one instance to another,
    /// but different from build to the next. So if it is possible for a potential
    /// attacker to have access to the compiled binary it would be better
    /// to specify keys generated at runtime.
    ///
    /// This is defined only if the `compile-time-rng` feature is enabled.
    ///
    /// # Examples
    ///
    /// ```
    /// use ahash::AHasher;
    /// use std::hash::Hasher;
    ///
    /// let mut hasher_1 = AHasher::default();
    /// let mut hasher_2 = AHasher::default();
    ///
    /// hasher_1.write_u32(1234);
    /// hasher_2.write_u32(1234);
    ///
    /// assert_eq!(hasher_1.finish(), hasher_2.finish());
    /// ```
    #[inline]
    fn default() -> AHasher {
        AHasher::new_with_keys(const_random!(u64), const_random!(u64))
    }
}

//#[inline(never)]
//pub fn hash_test(input: &[u8]) -> u64 {
//    use std::hash::Hasher;
//    let mut a = AHasher::new_with_keys(67, 87);
//    a.write(input);
//    a.finish()
//}

#[cfg(test)]
mod test {
    use crate::convert::Convert;
    use crate::*;
    use core::hash::BuildHasherDefault;
    use std::collections::HashMap;

    #[test]
    fn test_default_builder() {
        let mut map = HashMap::<u32, u64, BuildHasherDefault<AHasher>>::default();
        map.insert(1, 3);
    }
    #[test]
    fn test_builder() {
        let mut map = HashMap::<u32, u64, RandomState>::default();
        map.insert(1, 3);
    }

    #[test]
    fn test_conversion() {
        let input: &[u8] = b"dddddddd";
        let bytes: u64 = as_array!(input, 8).convert();
        assert_eq!(bytes, 0x6464646464646464);
    }

    #[test]
    fn test_ahasher_construction() {
        let _ = AHasher::new_with_keys(1245, 5678);
    }
}
