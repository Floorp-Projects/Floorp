use crate::AHasher;
use core::hash::BuildHasher;
use core::sync::atomic::AtomicUsize;
use core::sync::atomic::Ordering;

#[cfg(feature = "compile-time-rng")]
use const_random::const_random;

///This constant come from Kunth's prng
pub(crate) const MULTIPLE: u64 = 6364136223846793005;

// Const random provides randomized starting key with no runtime cost.
#[cfg(feature = "compile-time-rng")]
static SEED: AtomicUsize = AtomicUsize::new(const_random!(u64));

#[cfg(not(feature = "compile-time-rng"))]
static SEED: AtomicUsize = AtomicUsize::new(MULTIPLE as usize);

/// Provides a [Hasher] factory. This is typically used (e.g. by [`HashMap`]) to create
/// [AHasher]s in order to hash the keys of the map. See `build_hasher` below.
///
/// [build_hasher]: ahash::
/// [Hasher]: std::hash::Hasher
/// [BuildHasher]: std::hash::BuildHasher
/// [HashMap]: std::collections::HashMap
#[derive(Clone)]
pub struct RandomState {
    pub(crate) k0: u64,
    pub(crate) k1: u64,
}

impl RandomState {
    #[inline]
    pub fn new() -> RandomState {
        //Using a self pointer. When running with ASLR this is a random value.
        let previous = SEED.load(Ordering::Relaxed) as u64;
        let stack_mem_loc = &previous as *const _ as u64;
        //This is similar to the update function in the fallback.
        //only one multiply is needed because memory locations are not under an attackers control.
        let current_seed = previous
            .wrapping_mul(MULTIPLE)
            .wrapping_add(stack_mem_loc)
            .rotate_left(31);
        SEED.store(current_seed as usize, Ordering::Relaxed);
        let (k0, k1) = scramble_keys(&SEED as *const _ as u64, current_seed);
        RandomState { k0, k1 }
    }
}

pub(crate) fn scramble_keys(k0: u64, k1: u64) -> (u64, u64) {
    //Scramble seeds (based on xoroshiro128+)
    //This is intentionally not similar the hash algorithm
    let result1 = k0.wrapping_add(k1);
    let k1 = k1 ^ k0;
    let k0 = k0.rotate_left(24) ^ k1 ^ (k1.wrapping_shl(16));
    let result2 = k0.wrapping_add(k1.rotate_left(37));
    (result2, result1)
}

impl Default for RandomState {
    #[inline]
    fn default() -> Self {
        Self::new()
    }
}

impl BuildHasher for RandomState {
    type Hasher = AHasher;

    /// Constructs a new [AHasher] with keys based on compile time generated constants** and the location
    /// of the this object in memory. This means that two different [BuildHasher]s will will generate
    /// [AHasher]s that will return different hashcodes, but [Hasher]s created from the same [BuildHasher]
    /// will generate the same hashes for the same input data.
    ///
    /// ** - only if the `compile-time-rng` feature is enabled.
    ///
    /// # Examples
    ///
    /// ```
    /// use ahash::{AHasher, RandomState};
    /// use std::hash::{Hasher, BuildHasher};
    ///
    /// let build_hasher = RandomState::new();
    /// let mut hasher_1 = build_hasher.build_hasher();
    /// let mut hasher_2 = build_hasher.build_hasher();
    ///
    /// hasher_1.write_u32(1234);
    /// hasher_2.write_u32(1234);
    ///
    /// assert_eq!(hasher_1.finish(), hasher_2.finish());
    ///
    /// let other_build_hasher = RandomState::new();
    /// let mut different_hasher = other_build_hasher.build_hasher();
    /// different_hasher.write_u32(1234);
    /// assert_ne!(different_hasher.finish(), hasher_1.finish());
    /// ```
    /// [Hasher]: std::hash::Hasher
    /// [BuildHasher]: std::hash::BuildHasher
    /// [HashMap]: std::collections::HashMap
    #[inline]
    fn build_hasher(&self) -> AHasher {
        AHasher::new_with_keys(self.k0, self.k1)
    }
}
