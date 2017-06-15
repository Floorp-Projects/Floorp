//! Epoch-based garbage collection.
//!
//! # Pointers
//!
//! Concurrent collections are built using atomic pointers. This module provides [`Atomic`], which
//! is just a shared atomic pointer to a heap-allocated object. Loading an [`Atomic`] yields a
//! [`Ptr`], which is an epoch-protected pointer through which the loaded object can be safely
//! read.
//!
//! # Pinning
//!
//! Before an [`Atomic`] can be loaded, the current thread must be pinned. By pinning a thread we
//! declare that any object that gets removed from now on must not be destructed just yet. Garbage
//! collection of newly removed objects is suspended until the thread gets unpinned.
//!
//! # Garbage
//!
//! Objects that get removed from concurrent collections must be stashed away until all currently
//! pinned threads get unpinned. Such objects can be stored into a [`Garbage`], where they are kept
//! until the right time for their destruction comes.
//!
//! There is a global shared instance of [`Garbage`], which can only deallocate memory. It cannot
//! drop objects or run arbitrary destruction procedures. Removed objects can be stored into it by
//! calling [`defer_free`].
//!
//! [`Atomic`]: struct.Atomic.html
//! [`Garbage`]: struct.Garbage.html
//! [`Ptr`]: struct.Ptr.html
//! [`defer_free`]: fn.defer_free.html

mod atomic;
mod garbage;
mod thread;

pub use self::atomic::{Atomic, Ptr};
pub use self::garbage::Garbage;
pub use self::thread::{Pin, defer_free, flush, is_pinned, pin};

#[cfg(feature = "internals")]
pub use self::garbage::destroy_global;
