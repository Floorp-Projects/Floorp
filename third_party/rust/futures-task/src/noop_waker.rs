//! Utilities for creating zero-cost wakers that don't do anything.

use core::task::{RawWaker, RawWakerVTable, Waker};
use core::ptr::null;
#[cfg(feature = "std")]
use core::cell::UnsafeCell;

unsafe fn noop_clone(_data: *const ()) -> RawWaker {
    noop_raw_waker()
}

unsafe fn noop(_data: *const ()) {}

const NOOP_WAKER_VTABLE: RawWakerVTable = RawWakerVTable::new(noop_clone, noop, noop, noop);

fn noop_raw_waker() -> RawWaker {
    RawWaker::new(null(), &NOOP_WAKER_VTABLE)
}

/// Create a new [`Waker`] which does
/// nothing when `wake()` is called on it.
///
/// # Examples
///
/// ```
/// use futures::task::noop_waker;
/// let waker = noop_waker();
/// waker.wake();
/// ```
#[inline]
pub fn noop_waker() -> Waker {
    unsafe {
        Waker::from_raw(noop_raw_waker())
    }
}

/// Get a static reference to a [`Waker`] which
/// does nothing when `wake()` is called on it.
///
/// # Examples
///
/// ```
/// use futures::task::noop_waker_ref;
/// let waker = noop_waker_ref();
/// waker.wake_by_ref();
/// ```
#[inline]
#[cfg(feature = "std")]
pub fn noop_waker_ref() -> &'static Waker {
    thread_local! {
        static NOOP_WAKER_INSTANCE: UnsafeCell<Waker> =
            UnsafeCell::new(noop_waker());
    }
    NOOP_WAKER_INSTANCE.with(|l| unsafe { &*l.get() })
}
