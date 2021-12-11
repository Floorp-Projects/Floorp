#![deny(missing_docs, missing_debug_implementations)]
//! This library defines the `PinCell` type, a pinning variant of the standard
//! library's `RefCell`.
//!
//! It is not safe to "pin project" through a `RefCell` - getting a pinned
//! reference to something inside the `RefCell` when you have a pinned
//! refernece to the `RefCell` - because `RefCell` is too powerful.
//!
//! A `PinCell` is slightly less powerful than `RefCell`: unlike a `RefCell`,
//! one cannot get a mutable reference into a `PinCell`, only a pinned mutable
//! reference (`Pin<&mut T>`). This makes pin projection safe, allowing you
//! to use interior mutability with the knowledge that `T` will never actually
//! be moved out of the `RefCell` that wraps it.

mod pin_mut;
mod pin_ref;

use core::cell::{BorrowMutError, RefCell, RefMut};
use core::pin::Pin;

pub use pin_mut::PinMut;
pub use pin_ref::PinRef;

/// A mutable memory location with dynamically checked borrow rules
///
/// Unlike `RefCell`, this type only allows *pinned* mutable access to the
/// inner value, enabling a "pin-safe" version of interior mutability.
///
/// See the standard library documentation for more information.
#[derive(Default, Clone, Ord, PartialOrd, Eq, PartialEq, Debug)]
pub struct PinCell<T: ?Sized> {
    inner: RefCell<T>,
}

impl<T> PinCell<T> {
    /// Creates a new `PinCell` containing `value`.
    pub const fn new(value: T) -> PinCell<T> {
        PinCell {
            inner: RefCell::new(value),
        }
    }
}

impl<T: ?Sized> PinCell<T> {
    /// Mutably borrows the wrapped value, preserving its pinnedness.
    ///
    /// The borrow lasts until the returned `PinMut` or all `PinMut`s derived
    /// from it exit scope. The value cannot be borrowed while this borrow is
    /// active.
    pub fn borrow_mut(self: Pin<&Self>) -> PinMut<'_, T> {
        self.try_borrow_mut().expect("already borrowed")
    }

    /// Mutably borrows the wrapped value, preserving its pinnedness,
    /// returning an error if the value is currently borrowed.
    ///
    /// The borrow lasts until the returned `PinMut` or all `PinMut`s derived
    /// from it exit scope. The value cannot be borrowed while this borrow is
    /// active.
    ///
    /// This is the non-panicking variant of `borrow_mut`.
    pub fn try_borrow_mut<'a>(self: Pin<&'a Self>) -> Result<PinMut<'a, T>, BorrowMutError> {
        let ref_mut: RefMut<'a, T> = Pin::get_ref(self).inner.try_borrow_mut()?;

        // this is a pin projection from Pin<&PinCell<T>> to Pin<RefMut<T>>
        // projecting is safe because:
        //
        // - for<T: ?Sized> (PinCell<T>: Unpin) imples (RefMut<T>: Unpin)
        //   holds true
        // - PinCell does not implement Drop
        //
        // see discussion on tracking issue #49150 about pin projection
        // invariants
        let pin_ref_mut: Pin<RefMut<'a, T>> = unsafe { Pin::new_unchecked(ref_mut) };

        Ok(PinMut { inner: pin_ref_mut })
    }
}

impl<T> From<T> for PinCell<T> {
    fn from(value: T) -> PinCell<T> {
        PinCell::new(value)
    }
}

impl<T> From<RefCell<T>> for PinCell<T> {
    fn from(cell: RefCell<T>) -> PinCell<T> {
        PinCell { inner: cell }
    }
}

impl<T> From<PinCell<T>> for RefCell<T> {
    fn from(input: PinCell<T>) -> Self {
        input.inner
    }
}
// TODO CoerceUnsized
