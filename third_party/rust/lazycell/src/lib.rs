// Original work Copyright (c) 2014 The Rust Project Developers
// Modified work Copyright (c) 2016 Nikita Pekin and the lazycell contributors
// See the README.md file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(missing_docs)]
#![cfg_attr(feature = "nightly", feature(plugin))]
#![cfg_attr(feature = "clippy", plugin(clippy))]

//! This crate provides a `LazyCell` struct which acts as a lazily filled
//! `Cell`, but with frozen contents.
//!
//! With a `RefCell`, the inner contents cannot be borrowed for the lifetime of
//! the entire object, but only of the borrows returned. A `LazyCell` is a
//! variation on `RefCell` which allows borrows to be tied to the lifetime of
//! the outer object.
//!
//! The limitation of a `LazyCell` is that after it is initialized, it can never
//! be modified.
//!
//! # Example
//!
//! The following example shows a quick example of the basic functionality of
//! `LazyCell`.
//!
//! ```
//! use lazycell::LazyCell;
//!
//! let lazycell = LazyCell::new();
//!
//! assert_eq!(lazycell.borrow(), None);
//! assert!(!lazycell.filled());
//! lazycell.fill(1).ok();
//! assert!(lazycell.filled());
//! assert_eq!(lazycell.borrow(), Some(&1));
//! assert_eq!(lazycell.into_inner(), Some(1));
//! ```
//!
//! `AtomicLazyCell` is a variant that uses an atomic variable to manage
//! coordination in a thread-safe fashion.

use std::cell::UnsafeCell;
use std::sync::atomic::{AtomicUsize, Ordering};

/// A lazily filled `Cell`, with frozen contents.
pub struct LazyCell<T> {
    inner: UnsafeCell<Option<T>>,
}

impl<T> LazyCell<T> {
    /// Creates a new, empty, `LazyCell`.
    pub fn new() -> LazyCell<T> {
        LazyCell { inner: UnsafeCell::new(None) }
    }

    /// Put a value into this cell.
    ///
    /// This function will return Err(value) is the cell is already full.
    pub fn fill(&self, t: T) -> Result<(), T> {
        let mut slot = unsafe { &mut *self.inner.get() };
        if slot.is_some() {
	    return Err(t);
        }
        *slot = Some(t);

	Ok(())
    }

    /// Test whether this cell has been previously filled.
    pub fn filled(&self) -> bool {
        self.borrow().is_some()
    }

    /// Borrows the contents of this lazy cell for the duration of the cell
    /// itself.
    ///
    /// This function will return `Some` if the cell has been previously
    /// initialized, and `None` if it has not yet been initialized.
    pub fn borrow(&self) -> Option<&T> {
        unsafe { &*self.inner.get() }.as_ref()
    }

    /// Consumes this `LazyCell`, returning the underlying value.
    pub fn into_inner(self) -> Option<T> {
        unsafe { self.inner.into_inner() }
    }
}

// Tracks the AtomicLazyCell inner state
const NONE: usize = 0;
const LOCK: usize = 1;
const SOME: usize = 2;

/// A lazily filled `Cell`, with frozen contents.
pub struct AtomicLazyCell<T> {
    inner: UnsafeCell<Option<T>>,
    state: AtomicUsize,
}

impl<T> AtomicLazyCell<T> {
    /// Creates a new, empty, `AtomicLazyCell`.
    pub fn new() -> AtomicLazyCell<T> {
        AtomicLazyCell {
            inner: UnsafeCell::new(None),
            state: AtomicUsize::new(NONE),
        }
    }

    /// Put a value into this cell.
    ///
    /// This function will return Err(value) is the cell is already full.
    pub fn fill(&self, t: T) -> Result<(), T> {
        if NONE != self.state.compare_and_swap(NONE, LOCK, Ordering::Acquire) {
            return Err(t);
        }

        unsafe { *self.inner.get() = Some(t) };

        if LOCK != self.state.compare_and_swap(LOCK, SOME, Ordering::Release) {
            panic!("unable to release lock");
        }

        Ok(())
    }

    /// Test whether this cell has been previously filled.
    pub fn filled(&self) -> bool {
        self.state.load(Ordering::Acquire) == SOME
    }

    /// Borrows the contents of this lazy cell for the duration of the cell
    /// itself.
    ///
    /// This function will return `Some` if the cell has been previously
    /// initialized, and `None` if it has not yet been initialized.
    pub fn borrow(&self) -> Option<&T> {
        match self.state.load(Ordering::Acquire) {
            SOME => unsafe { &*self.inner.get() }.as_ref(),
            _ => None,
        }
    }

    /// Consumes this `LazyCell`, returning the underlying value.
    pub fn into_inner(self) -> Option<T> {
        unsafe { self.inner.into_inner() }
    }
}

unsafe impl<T: Sync> Sync for AtomicLazyCell<T> { }
unsafe impl<T: Send> Send for AtomicLazyCell<T> { }

#[cfg(test)]
mod tests {
    use super::{LazyCell, AtomicLazyCell};

    #[test]
    fn test_borrow_from_empty() {
        let lazycell: LazyCell<usize> = LazyCell::new();

        let value = lazycell.borrow();
        assert_eq!(value, None);
    }

    #[test]
    fn test_fill_and_borrow() {
        let lazycell = LazyCell::new();

        assert!(!lazycell.filled());
        lazycell.fill(1).unwrap();
        assert!(lazycell.filled());

        let value = lazycell.borrow();
        assert_eq!(value, Some(&1));
    }

    #[test]
    fn test_already_filled_error() {
        let lazycell = LazyCell::new();

        lazycell.fill(1).unwrap();
        assert_eq!(lazycell.fill(1), Err(1));
    }

    #[test]
    fn test_into_inner() {
        let lazycell = LazyCell::new();

        lazycell.fill(1).unwrap();
        let value = lazycell.into_inner();
        assert_eq!(value, Some(1));
    }

    #[test]
    fn test_atomic_borrow_from_empty() {
        let lazycell: AtomicLazyCell<usize> = AtomicLazyCell::new();

        let value = lazycell.borrow();
        assert_eq!(value, None);
    }

    #[test]
    fn test_atomic_fill_and_borrow() {
        let lazycell = AtomicLazyCell::new();

        assert!(!lazycell.filled());
        lazycell.fill(1).unwrap();
        assert!(lazycell.filled());

        let value = lazycell.borrow();
        assert_eq!(value, Some(&1));
    }

    #[test]
    fn test_atomic_already_filled_panic() {
        let lazycell = AtomicLazyCell::new();

        lazycell.fill(1).unwrap();
        assert_eq!(1, lazycell.fill(1).unwrap_err());
    }

    #[test]
    fn test_atomic_into_inner() {
        let lazycell = AtomicLazyCell::new();

        lazycell.fill(1).unwrap();
        let value = lazycell.into_inner();
        assert_eq!(value, Some(1));
    }
}
