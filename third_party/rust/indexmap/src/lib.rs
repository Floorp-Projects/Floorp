
#![deny(unsafe_code)]
#![doc(html_root_url = "https://docs.rs/indexmap/1/")]

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
//! ## Rust Version
//!
//! This version of indexmap requires Rust 1.18 or later.
//!
//! The indexmap 1.x release series will use a carefully considered version
//! upgrade policy, where in a later 1.x version, we will raise the minimum
//! required Rust version.

#[macro_use]
mod macros;
#[cfg(feature = "serde-1")]
mod serde;
mod util;
mod equivalent;
mod mutable_keys;

pub mod set;
pub mod map;

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
    fn get(self) -> usize { self.0 }
}

impl Clone for HashValue {
    #[inline]
    fn clone(&self) -> Self { *self }
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
    fn key_ref(&self) -> &K { &self.key }
    fn value_ref(&self) -> &V { &self.value }
    fn value_mut(&mut self) -> &mut V { &mut self.value }
    fn key(self) -> K { self.key }
    fn key_value(self) -> (K, V) { (self.key, self.value) }
    fn refs(&self) -> (&K, &V) { (&self.key, &self.value) }
    fn ref_mut(&mut self) -> (&K, &mut V) { (&self.key, &mut self.value) }
    fn muts(&mut self) -> (&mut K, &mut V) { (&mut self.key, &mut self.value) }
}

