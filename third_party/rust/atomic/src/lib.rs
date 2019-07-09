// Copyright 2016 Amanieu d'Antras
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

//! Generic `Atomic<T>` wrapper type
//!
//! Atomic types provide primitive shared-memory communication between
//! threads, and are the building blocks of other concurrent types.
//!
//! This library defines a generic atomic wrapper type `Atomic<T>` for all
//! `T: Copy` types.
//! Atomic types present operations that, when used correctly, synchronize
//! updates between threads.
//!
//! Each method takes an `Ordering` which represents the strength of
//! the memory barrier for that operation. These orderings are the
//! same as [LLVM atomic orderings][1].
//!
//! [1]: http://llvm.org/docs/LangRef.html#memory-model-for-concurrent-operations
//!
//! Atomic variables are safe to share between threads (they implement `Sync`)
//! but they do not themselves provide the mechanism for sharing. The most
//! common way to share an atomic variable is to put it into an `Arc` (an
//! atomically-reference-counted shared pointer).
//!
//! Most atomic types may be stored in static variables, initialized using
//! the `const fn` constructors (only available on nightly). Atomic statics
//! are often used for lazy global initialization.

#![warn(missing_docs)]
#![no_std]
#![cfg_attr(
    feature = "nightly", feature(const_fn, cfg_target_has_atomic, atomic_min_max)
)]

#[cfg(test)]
#[macro_use]
extern crate std;

// Re-export some useful definitions from libcore
pub use core::sync::atomic::{fence, Ordering};

use core::cell::UnsafeCell;
use core::fmt;

mod fallback;
mod ops;

/// A generic atomic wrapper type which allows an object to be safely shared
/// between threads.
pub struct Atomic<T: Copy> {
    v: UnsafeCell<T>,
}

// Atomic<T> is only Sync if T is Send
unsafe impl<T: Copy + Send> Sync for Atomic<T> {}

impl<T: Copy + Default> Default for Atomic<T> {
    #[inline]
    fn default() -> Self {
        Self::new(Default::default())
    }
}

impl<T: Copy + fmt::Debug> fmt::Debug for Atomic<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_tuple("Atomic")
            .field(&self.load(Ordering::SeqCst))
            .finish()
    }
}

impl<T: Copy> Atomic<T> {
    /// Creates a new `Atomic`.
    #[inline]
    #[cfg(feature = "nightly")]
    pub const fn new(v: T) -> Atomic<T> {
        Atomic {
            v: UnsafeCell::new(v),
        }
    }

    /// Creates a new `Atomic`.
    #[inline]
    #[cfg(not(feature = "nightly"))]
    pub fn new(v: T) -> Atomic<T> {
        Atomic {
            v: UnsafeCell::new(v),
        }
    }

    /// Checks if `Atomic` objects of this type are lock-free.
    ///
    /// If an `Atomic` is not lock-free then it may be implemented using locks
    /// internally, which makes it unsuitable for some situations (such as
    /// communicating with a signal handler).
    #[inline]
    #[cfg(feature = "nightly")]
    pub const fn is_lock_free() -> bool {
        ops::atomic_is_lock_free::<T>()
    }

    /// Checks if `Atomic` objects of this type are lock-free.
    ///
    /// If an `Atomic` is not lock-free then it may be implemented using locks
    /// internally, which makes it unsuitable for some situations (such as
    /// communicating with a signal handler).
    #[inline]
    #[cfg(not(feature = "nightly"))]
    pub fn is_lock_free() -> bool {
        ops::atomic_is_lock_free::<T>()
    }

    /// Returns a mutable reference to the underlying type.
    ///
    /// This is safe because the mutable reference guarantees that no other threads are
    /// concurrently accessing the atomic data.
    #[inline]
    pub fn get_mut(&mut self) -> &mut T {
        unsafe { &mut *self.v.get() }
    }

    /// Consumes the atomic and returns the contained value.
    ///
    /// This is safe because passing `self` by value guarantees that no other threads are
    /// concurrently accessing the atomic data.
    #[inline]
    pub fn into_inner(self) -> T {
        self.v.into_inner()
    }

    /// Loads a value from the `Atomic`.
    ///
    /// `load` takes an `Ordering` argument which describes the memory ordering
    /// of this operation.
    ///
    /// # Panics
    ///
    /// Panics if `order` is `Release` or `AcqRel`.
    #[inline]
    pub fn load(&self, order: Ordering) -> T {
        unsafe { ops::atomic_load(self.v.get(), order) }
    }

    /// Stores a value into the `Atomic`.
    ///
    /// `store` takes an `Ordering` argument which describes the memory ordering
    /// of this operation.
    ///
    /// # Panics
    ///
    /// Panics if `order` is `Acquire` or `AcqRel`.
    #[inline]
    pub fn store(&self, val: T, order: Ordering) {
        unsafe {
            ops::atomic_store(self.v.get(), val, order);
        }
    }

    /// Stores a value into the `Atomic`, returning the old value.
    ///
    /// `swap` takes an `Ordering` argument which describes the memory ordering
    /// of this operation.
    #[inline]
    pub fn swap(&self, val: T, order: Ordering) -> T {
        unsafe { ops::atomic_swap(self.v.get(), val, order) }
    }

    /// Stores a value into the `Atomic` if the current value is the same as the
    /// `current` value.
    ///
    /// The return value is a result indicating whether the new value was
    /// written and containing the previous value. On success this value is
    /// guaranteed to be equal to `new`.
    ///
    /// `compare_exchange` takes two `Ordering` arguments to describe the memory
    /// ordering of this operation. The first describes the required ordering if
    /// the operation succeeds while the second describes the required ordering
    /// when the operation fails. The failure ordering can't be `Acquire` or
    /// `AcqRel` and must be equivalent or weaker than the success ordering.
    #[inline]
    pub fn compare_exchange(
        &self,
        current: T,
        new: T,
        success: Ordering,
        failure: Ordering,
    ) -> Result<T, T> {
        unsafe { ops::atomic_compare_exchange(self.v.get(), current, new, success, failure) }
    }

    /// Stores a value into the `Atomic` if the current value is the same as the
    /// `current` value.
    ///
    /// Unlike `compare_exchange`, this function is allowed to spuriously fail
    /// even when the comparison succeeds, which can result in more efficient
    /// code on some platforms. The return value is a result indicating whether
    /// the new value was written and containing the previous value.
    ///
    /// `compare_exchange` takes two `Ordering` arguments to describe the memory
    /// ordering of this operation. The first describes the required ordering if
    /// the operation succeeds while the second describes the required ordering
    /// when the operation fails. The failure ordering can't be `Acquire` or
    /// `AcqRel` and must be equivalent or weaker than the success ordering.
    /// success ordering.
    #[inline]
    pub fn compare_exchange_weak(
        &self,
        current: T,
        new: T,
        success: Ordering,
        failure: Ordering,
    ) -> Result<T, T> {
        unsafe { ops::atomic_compare_exchange_weak(self.v.get(), current, new, success, failure) }
    }
}

impl Atomic<bool> {
    /// Logical "and" with a boolean value.
    ///
    /// Performs a logical "and" operation on the current value and the argument
    /// `val`, and sets the new value to the result.
    ///
    /// Returns the previous value.
    #[inline]
    pub fn fetch_and(&self, val: bool, order: Ordering) -> bool {
        unsafe { ops::atomic_and(self.v.get(), val, order) }
    }

    /// Logical "or" with a boolean value.
    ///
    /// Performs a logical "or" operation on the current value and the argument
    /// `val`, and sets the new value to the result.
    ///
    /// Returns the previous value.
    #[inline]
    pub fn fetch_or(&self, val: bool, order: Ordering) -> bool {
        unsafe { ops::atomic_or(self.v.get(), val, order) }
    }

    /// Logical "xor" with a boolean value.
    ///
    /// Performs a logical "xor" operation on the current value and the argument
    /// `val`, and sets the new value to the result.
    ///
    /// Returns the previous value.
    #[inline]
    pub fn fetch_xor(&self, val: bool, order: Ordering) -> bool {
        unsafe { ops::atomic_xor(self.v.get(), val, order) }
    }
}

macro_rules! atomic_ops_common {
    ($($t:ty)*) => ($(
        impl Atomic<$t> {
            /// Add to the current value, returning the previous value.
            #[inline]
            pub fn fetch_add(&self, val: $t, order: Ordering) -> $t {
                unsafe { ops::atomic_add(self.v.get(), val, order) }
            }

            /// Subtract from the current value, returning the previous value.
            #[inline]
            pub fn fetch_sub(&self, val: $t, order: Ordering) -> $t {
                unsafe { ops::atomic_sub(self.v.get(), val, order) }
            }

            /// Bitwise and with the current value, returning the previous value.
            #[inline]
            pub fn fetch_and(&self, val: $t, order: Ordering) -> $t {
                unsafe { ops::atomic_and(self.v.get(), val, order) }
            }

            /// Bitwise or with the current value, returning the previous value.
            #[inline]
            pub fn fetch_or(&self, val: $t, order: Ordering) -> $t {
                unsafe { ops::atomic_or(self.v.get(), val, order) }
            }

            /// Bitwise xor with the current value, returning the previous value.
            #[inline]
            pub fn fetch_xor(&self, val: $t, order: Ordering) -> $t {
                unsafe { ops::atomic_xor(self.v.get(), val, order) }
            }
        }
    )*);
}
macro_rules! atomic_ops_signed {
    ($($t:ty)*) => (
        atomic_ops_common!{ $($t)* }
        $(
            impl Atomic<$t> {
                /// Minimum with the current value.
                #[inline]
                pub fn fetch_min(&self, val: $t, order: Ordering) -> $t {
                    unsafe { ops::atomic_min(self.v.get(), val, order) }
                }

                /// Maximum with the current value.
                #[inline]
                pub fn fetch_max(&self, val: $t, order: Ordering) -> $t {
                    unsafe { ops::atomic_max(self.v.get(), val, order) }
                }
            }
        )*
    );
}
macro_rules! atomic_ops_unsigned {
    ($($t:ty)*) => (
        atomic_ops_common!{ $($t)* }
        $(
            impl Atomic<$t> {
                /// Minimum with the current value.
                #[inline]
                pub fn fetch_min(&self, val: $t, order: Ordering) -> $t {
                    unsafe { ops::atomic_umin(self.v.get(), val, order) }
                }

                /// Maximum with the current value.
                #[inline]
                pub fn fetch_max(&self, val: $t, order: Ordering) -> $t {
                    unsafe { ops::atomic_umax(self.v.get(), val, order) }
                }
            }
        )*
    );
}
atomic_ops_signed!{ i8 i16 i32 i64 isize i128 }
atomic_ops_unsigned!{ u8 u16 u32 u64 usize u128 }

#[cfg(test)]
mod tests {
    use core::mem;
    use Atomic;
    use Ordering::*;

    #[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
    struct Foo(u8, u8);
    #[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
    struct Bar(u64, u64);
    #[derive(Copy, Clone, Eq, PartialEq, Debug, Default)]
    struct Quux(u32);

    #[test]
    fn atomic_bool() {
        let a = Atomic::new(false);
        assert_eq!(Atomic::<bool>::is_lock_free(), cfg!(feature = "nightly"));
        assert_eq!(format!("{:?}", a), "Atomic(false)");
        assert_eq!(a.load(SeqCst), false);
        a.store(true, SeqCst);
        assert_eq!(a.swap(false, SeqCst), true);
        assert_eq!(a.compare_exchange(true, false, SeqCst, SeqCst), Err(false));
        assert_eq!(a.compare_exchange(false, true, SeqCst, SeqCst), Ok(false));
        assert_eq!(a.fetch_and(false, SeqCst), true);
        assert_eq!(a.fetch_or(true, SeqCst), false);
        assert_eq!(a.fetch_xor(false, SeqCst), true);
        assert_eq!(a.load(SeqCst), true);
    }

    #[test]
    fn atomic_i8() {
        let a = Atomic::new(0i8);
        assert_eq!(
            Atomic::<i8>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "8",
                all(feature = "nightly", target_has_atomic = "8")
            ))
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        // Make sure overflows are handled correctly
        assert_eq!(a.fetch_sub(-56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), -74);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(-25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_i16() {
        let a = Atomic::new(0i16);
        assert_eq!(
            Atomic::<i16>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "16",
                all(feature = "nightly", target_has_atomic = "16")
            ))
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(-56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 182);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(-25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_i32() {
        let a = Atomic::new(0i32);
        assert_eq!(
            Atomic::<i32>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "32",
                all(feature = "nightly", target_has_atomic = "32")
            ))
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(-56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 182);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(-25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_i64() {
        let a = Atomic::new(0i64);
        assert_eq!(
            Atomic::<i64>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "64",
                all(feature = "nightly", target_has_atomic = "64")
            )) && mem::align_of::<i64>() == 8
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(-56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 182);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(-25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_i128() {
        let a = Atomic::new(0i128);
        assert_eq!(
            Atomic::<i128>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "128",
                all(feature = "nightly", target_has_atomic = "128")
            ))
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(-56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 182);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(-25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_isize() {
        let a = Atomic::new(0isize);
        assert!(Atomic::<isize>::is_lock_free());
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(-56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 182);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(-25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_u8() {
        let a = Atomic::new(0u8);
        assert_eq!(
            Atomic::<u8>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "8",
                all(feature = "nightly", target_has_atomic = "8")
            ))
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 70);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_u16() {
        let a = Atomic::new(0u16);
        assert_eq!(
            Atomic::<u16>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "16",
                all(feature = "nightly", target_has_atomic = "16")
            ))
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 70);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_u32() {
        let a = Atomic::new(0u32);
        assert_eq!(
            Atomic::<u32>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "32",
                all(feature = "nightly", target_has_atomic = "32")
            ))
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 70);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_u64() {
        let a = Atomic::new(0u64);
        assert_eq!(
            Atomic::<u64>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "64",
                all(feature = "nightly", target_has_atomic = "64")
            )) && mem::align_of::<u64>() == 8
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 70);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_u128() {
        let a = Atomic::new(0u128);
        assert_eq!(
            Atomic::<u128>::is_lock_free(),
            cfg!(any(
                target_pointer_width = "128",
                all(feature = "nightly", target_has_atomic = "128")
            ))
        );
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 70);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_usize() {
        let a = Atomic::new(0usize);
        assert!(Atomic::<usize>::is_lock_free());
        assert_eq!(format!("{:?}", a), "Atomic(0)");
        assert_eq!(a.load(SeqCst), 0);
        a.store(1, SeqCst);
        assert_eq!(a.swap(2, SeqCst), 1);
        assert_eq!(a.compare_exchange(5, 45, SeqCst, SeqCst), Err(2));
        assert_eq!(a.compare_exchange(2, 3, SeqCst, SeqCst), Ok(2));
        assert_eq!(a.fetch_add(123, SeqCst), 3);
        assert_eq!(a.fetch_sub(56, SeqCst), 126);
        assert_eq!(a.fetch_and(7, SeqCst), 70);
        assert_eq!(a.fetch_or(64, SeqCst), 6);
        assert_eq!(a.fetch_xor(1, SeqCst), 70);
        assert_eq!(a.fetch_min(30, SeqCst), 71);
        assert_eq!(a.fetch_max(25, SeqCst), 30);
        assert_eq!(a.load(SeqCst), 30);
    }

    #[test]
    fn atomic_foo() {
        let a = Atomic::default();
        assert_eq!(Atomic::<Foo>::is_lock_free(), false);
        assert_eq!(format!("{:?}", a), "Atomic(Foo(0, 0))");
        assert_eq!(a.load(SeqCst), Foo(0, 0));
        a.store(Foo(1, 1), SeqCst);
        assert_eq!(a.swap(Foo(2, 2), SeqCst), Foo(1, 1));
        assert_eq!(
            a.compare_exchange(Foo(5, 5), Foo(45, 45), SeqCst, SeqCst),
            Err(Foo(2, 2))
        );
        assert_eq!(
            a.compare_exchange(Foo(2, 2), Foo(3, 3), SeqCst, SeqCst),
            Ok(Foo(2, 2))
        );
        assert_eq!(a.load(SeqCst), Foo(3, 3));
    }

    #[test]
    fn atomic_bar() {
        let a = Atomic::default();
        assert_eq!(Atomic::<Bar>::is_lock_free(), false);
        assert_eq!(format!("{:?}", a), "Atomic(Bar(0, 0))");
        assert_eq!(a.load(SeqCst), Bar(0, 0));
        a.store(Bar(1, 1), SeqCst);
        assert_eq!(a.swap(Bar(2, 2), SeqCst), Bar(1, 1));
        assert_eq!(
            a.compare_exchange(Bar(5, 5), Bar(45, 45), SeqCst, SeqCst),
            Err(Bar(2, 2))
        );
        assert_eq!(
            a.compare_exchange(Bar(2, 2), Bar(3, 3), SeqCst, SeqCst),
            Ok(Bar(2, 2))
        );
        assert_eq!(a.load(SeqCst), Bar(3, 3));
    }

    #[test]
    fn atomic_quxx() {
        let a = Atomic::default();
        assert_eq!(
            Atomic::<Quux>::is_lock_free(),
            cfg!(any(feature = "nightly", target_pointer_width = "32"))
        );
        assert_eq!(format!("{:?}", a), "Atomic(Quux(0))");
        assert_eq!(a.load(SeqCst), Quux(0));
        a.store(Quux(1), SeqCst);
        assert_eq!(a.swap(Quux(2), SeqCst), Quux(1));
        assert_eq!(
            a.compare_exchange(Quux(5), Quux(45), SeqCst, SeqCst),
            Err(Quux(2))
        );
        assert_eq!(
            a.compare_exchange(Quux(2), Quux(3), SeqCst, SeqCst),
            Ok(Quux(2))
        );
        assert_eq!(a.load(SeqCst), Quux(3));
    }
}
