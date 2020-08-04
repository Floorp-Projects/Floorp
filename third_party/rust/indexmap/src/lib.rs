#![deny(unsafe_code)]
#![doc(html_root_url = "https://docs.rs/indexmap/1/")]
#![cfg_attr(not(has_std), no_std)]

//! [`IndexMap`] is a hash table where the iteration order of the key-value
//! pairs is independent of the hash values of the keys.
//!
//! [`IndexSet`] is a corresponding hash set using the same implementation and
//! with similar properties.
//!
//! [`IndexMap`]: map/struct.IndexMap.html
//! [`IndexSet`]: set/struct.IndexSet.html
//!
//!
//! ### Feature Highlights
//!
//! [`IndexMap`] and [`IndexSet`] are drop-in compatible with the std `HashMap`
//! and `HashSet`, but they also have some features of note:
//!
//! - The ordering semantics (see their documentation for details)
//! - Sorting methods and the [`.pop()`][IndexMap::pop] methods.
//! - The [`Equivalent`] trait, which offers more flexible equality definitions
//!   between borrowed and owned versions of keys.
//! - The [`MutableKeys`][map::MutableKeys] trait, which gives opt-in mutable
//!   access to hash map keys.
//!
//! ### Rust Version
//!
//! This version of indexmap requires Rust 1.18 or later, or 1.32+ for
//! development builds, and Rust 1.36+ for using with `alloc` (without `std`),
//! see below.
//!
//! The indexmap 1.x release series will use a carefully considered version
//! upgrade policy, where in a later 1.x version, we will raise the minimum
//! required Rust version.
//!
//! ## No Standard Library Targets
//!
//! From Rust 1.36, this crate supports being built without `std`, requiring
//! `alloc` instead. This is enabled automatically when it is detected that
//! `std` is not available. There is no crate feature to enable/disable to
//! trigger this. It can be tested by building for a std-less target.
//!
//! - Creating maps and sets using [`new`][IndexMap::new] and
//! [`with_capacity`][IndexMap::with_capacity] is unavailable without `std`.  
//!   Use methods [`IndexMap::default`][def],
//!   [`with_hasher`][IndexMap::with_hasher],
//!   [`with_capacity_and_hasher`][IndexMap::with_capacity_and_hasher] instead.
//!   A no-std compatible hasher will be needed as well, for example
//!   from the crate `twox-hash`.
//! - Macros [`indexmap!`] and [`indexset!`] are unavailable without `std`.
//!
//! [def]: map/struct.IndexMap.html#impl-Default

#[cfg(not(has_std))]
#[macro_use(vec)]
extern crate alloc;

#[cfg(not(has_std))]
pub(crate) mod std {
    pub use core::*;
    pub mod alloc {
        pub use alloc::*;
    }
    pub mod collections {
        pub use alloc::collections::*;
    }
    pub use alloc::vec;
}

#[cfg(not(has_std))]
use std::vec::Vec;

#[macro_use]
mod macros;
mod equivalent;
mod mutable_keys;
#[cfg(feature = "serde-1")]
mod serde;
mod util;

pub mod map;
pub mod set;

// Placed after `map` and `set` so new `rayon` methods on the types
// are documented after the "normal" methods.
#[cfg(feature = "rayon")]
mod rayon;

pub use equivalent::Equivalent;
pub use map::IndexMap;
pub use set::IndexSet;

// shared private items

/// Hash value newtype. Not larger than usize, since anything larger
/// isn't used for selecting position anyway.
#[derive(Copy, Debug)]
struct HashValue(usize);

impl HashValue {
    #[inline(always)]
    fn get(self) -> usize {
        self.0
    }
}

impl Clone for HashValue {
    #[inline]
    fn clone(&self) -> Self {
        *self
    }
}
impl PartialEq for HashValue {
    #[inline]
    fn eq(&self, rhs: &Self) -> bool {
        self.0 == rhs.0
    }
}

#[derive(Copy, Clone, Debug)]
struct Bucket<K, V> {
    hash: HashValue,
    key: K,
    value: V,
}

impl<K, V> Bucket<K, V> {
    // field accessors -- used for `f` instead of closures in `.map(f)`
    fn key_ref(&self) -> &K {
        &self.key
    }
    fn value_ref(&self) -> &V {
        &self.value
    }
    fn value_mut(&mut self) -> &mut V {
        &mut self.value
    }
    fn key(self) -> K {
        self.key
    }
    fn key_value(self) -> (K, V) {
        (self.key, self.value)
    }
    fn refs(&self) -> (&K, &V) {
        (&self.key, &self.value)
    }
    fn ref_mut(&mut self) -> (&K, &mut V) {
        (&self.key, &mut self.value)
    }
    fn muts(&mut self) -> (&mut K, &mut V) {
        (&mut self.key, &mut self.value)
    }
}

trait Entries {
    type Entry;
    fn into_entries(self) -> Vec<Self::Entry>;
    fn as_entries(&self) -> &[Self::Entry];
    fn as_entries_mut(&mut self) -> &mut [Self::Entry];
    fn with_entries<F>(&mut self, f: F)
    where
        F: FnOnce(&mut [Self::Entry]);
}
