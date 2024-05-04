#[cfg(all(feature = "unstable", not(feature = "rust_1_57")))]
pub use alloc::collections::TryReserveError;
#[cfg(all(feature = "hashmap", not(all(feature = "unstable", not(feature = "rust_1_57")))))]
pub use hashbrown::TryReserveError;

/// The error type for `try_reserve` methods.
#[cfg(all(not(feature = "hashmap"), not(all(feature = "unstable", not(feature = "rust_1_57")))))]
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum TryReserveError {
    /// Error due to the computed capacity exceeding the collection's maximum
    /// (usually `isize::MAX` bytes).
    CapacityOverflow,

    /// The memory allocator returned an error
    AllocError {
        /// The layout of the allocation request that failed.
        layout: alloc::alloc::Layout,
    },
}