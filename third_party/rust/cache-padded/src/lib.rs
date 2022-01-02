//! Prevent false sharing by padding and aligning to the length of a cache line.
//!
//! In concurrent programming, sometimes it is desirable to make sure commonly accessed shared data
//! is not all placed into the same cache line. Updating an atomic value invalides the whole cache
//! line it belongs to, which makes the next access to the same cache line slower for other CPU
//! cores. Use [`CachePadded`] to ensure updating one piece of data doesn't invalidate other cached
//! data.
//!
//! # Size and alignment
//!
//! Cache lines are assumed to be N bytes long, depending on the architecture:
//!
//! * On x86-64 and aarch64, N = 128.
//! * On all others, N = 64.
//!
//! Note that N is just a reasonable guess and is not guaranteed to match the actual cache line
//! length of the machine the program is running on.
//!
//! The size of `CachePadded<T>` is the smallest multiple of N bytes large enough to accommodate
//! a value of type `T`.
//!
//! The alignment of `CachePadded<T>` is the maximum of N bytes and the alignment of `T`.
//!
//! # Examples
//!
//! Alignment and padding:
//!
//! ```
//! use cache_padded::CachePadded;
//!
//! let array = [CachePadded::new(1i8), CachePadded::new(2i8)];
//! let addr1 = &*array[0] as *const i8 as usize;
//! let addr2 = &*array[1] as *const i8 as usize;
//!
//! assert!(addr2 - addr1 >= 64);
//! assert_eq!(addr1 % 64, 0);
//! assert_eq!(addr2 % 64, 0);
//! ```
//!
//! When building a concurrent queue with a head and a tail index, it is wise to place indices in
//! different cache lines so that concurrent threads pushing and popping elements don't invalidate
//! each other's cache lines:
//!
//! ```
//! use cache_padded::CachePadded;
//! use std::sync::atomic::AtomicUsize;
//!
//! struct Queue<T> {
//!     head: CachePadded<AtomicUsize>,
//!     tail: CachePadded<AtomicUsize>,
//!     buffer: *mut T,
//! }
//! ```

#![no_std]
#![forbid(unsafe_code)]
#![warn(missing_docs, missing_debug_implementations, rust_2018_idioms)]

use core::fmt;
use core::ops::{Deref, DerefMut};

/// Pads and aligns data to the length of a cache line.
// Starting from Intel's Sandy Bridge, spatial prefetcher is now pulling pairs of 64-byte cache
// lines at a time, so we have to align to 128 bytes rather than 64.
//
// Sources:
// - https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-optimization-manual.pdf
// - https://github.com/facebook/folly/blob/1b5288e6eea6df074758f877c849b6e73bbb9fbb/folly/lang/Align.h#L107
//
// ARM's big.LITTLE architecture has asymmetric cores and "big" cores have 128-byte cache lines.
//
// Sources:
// - https://www.mono-project.com/news/2016/09/12/arm64-icache/
//
#[cfg_attr(any(target_arch = "x86_64", target_arch = "aarch64"), repr(align(128)))]
#[cfg_attr(
    not(any(target_arch = "x86_64", target_arch = "aarch64")),
    repr(align(64))
)]
#[derive(Clone, Copy, Default, Hash, PartialEq, Eq)]
pub struct CachePadded<T>(T);

impl<T> CachePadded<T> {
    /// Pads and aligns a piece of data to the length of a cache line.
    ///
    /// # Examples
    ///
    /// ```
    /// use cache_padded::CachePadded;
    ///
    /// let padded = CachePadded::new(1);
    /// ```
    pub const fn new(t: T) -> CachePadded<T> {
        CachePadded(t)
    }

    /// Returns the inner data.
    ///
    /// # Examples
    ///
    /// ```
    /// use cache_padded::CachePadded;
    ///
    /// let padded = CachePadded::new(7);
    /// let data = padded.into_inner();
    /// assert_eq!(data, 7);
    /// ```
    pub fn into_inner(self) -> T {
        self.0
    }
}

impl<T> Deref for CachePadded<T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.0
    }
}

impl<T> DerefMut for CachePadded<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.0
    }
}

impl<T: fmt::Debug> fmt::Debug for CachePadded<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_tuple("CachePadded").field(&self.0).finish()
    }
}

impl<T> From<T> for CachePadded<T> {
    fn from(t: T) -> Self {
        CachePadded::new(t)
    }
}
