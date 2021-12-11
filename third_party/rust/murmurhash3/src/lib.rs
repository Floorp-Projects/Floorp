#![cfg_attr(feature = "nightly", feature(hashmap_hasher))]
#![cfg_attr(feature = "nightly", feature(test))]
#![cfg_attr(feature = "nightly", feature(vec_push_all))]

mod mmh3_128;
mod mmh3_32;

#[cfg(feature="nightly")]
mod hasher;

pub use mmh3_128::murmurhash3_x64_128;
pub use mmh3_32::murmurhash3_x86_32;

#[cfg(feature="nightly")]
pub use hasher::Murmur3HashState;
