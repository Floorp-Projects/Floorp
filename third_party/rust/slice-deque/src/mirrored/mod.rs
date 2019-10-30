//! Mirrored memory buffer.
mod buffer;

#[cfg(all(
    unix,
    not(all(
        any(
            target_os = "linux",
            target_os = "android",
            target_os = "macos",
            target_os = "ios"
        ),
        not(feature = "unix_sysv")
    ))
))]
mod sysv;
#[cfg(all(
    unix,
    not(all(
        any(
            target_os = "linux",
            target_os = "android",
            target_os = "macos",
            target_os = "ios"
        ),
        not(feature = "unix_sysv")
    ))
))]
pub(crate) use self::sysv::{
    allocate_mirrored, allocation_granularity, deallocate_mirrored,
};

#[cfg(all(
    any(target_os = "linux", target_os = "android"),
    not(feature = "unix_sysv")
))]
mod linux;
#[cfg(all(
    any(target_os = "linux", target_os = "android"),
    not(feature = "unix_sysv")
))]
pub(crate) use self::linux::{
    allocate_mirrored, allocation_granularity, deallocate_mirrored,
};

#[cfg(all(
    any(target_os = "macos", target_os = "ios"),
    not(feature = "unix_sysv")
))]
mod macos;

#[cfg(all(
    any(target_os = "macos", target_os = "ios"),
    not(feature = "unix_sysv")
))]
pub(crate) use self::macos::{
    allocate_mirrored, allocation_granularity, deallocate_mirrored,
};

#[cfg(target_os = "windows")]
mod winapi;

#[cfg(target_os = "windows")]
pub(crate) use self::winapi::{
    allocate_mirrored, allocation_granularity, deallocate_mirrored,
};

pub use self::buffer::Buffer;

use super::*;

/// Allocation error.
pub enum AllocError {
    /// The system is Out-of-memory.
    Oom,
    /// Other allocation errors (not out-of-memory).
    ///
    /// Race conditions, exhausted file descriptors, etc.
    Other,
}

impl crate::fmt::Debug for AllocError {
    fn fmt(&self, f: &mut crate::fmt::Formatter) -> crate::fmt::Result {
        match self {
            AllocError::Oom => write!(f, "out-of-memory"),
            AllocError::Other => write!(f, "other (not out-of-memory)"),
        }
    }
}
