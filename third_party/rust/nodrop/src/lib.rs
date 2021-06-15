//! A wrapper type to inhibit drop (destructor).
//!
//! ***Deprecated: Use ManuallyDrop or MaybeUninit instead!***
//!
//! The **nodrop** crate has the following cargo feature flags:
//!
//! - `std`
//!   - Optional, enabled by default
//!   - Use libstd
//! - `use_needs_drop`
//!   - Optional
//!   - Requires Rust 1.21
//!   - Use `needs_drop` to skip overwriting if not necessary
//! - `use_union`
//!   - Optional
//!   - Requires nightly channel
//!   - Using untagged union, finally we have an implementation of `NoDrop` without hacks,
//!     for example the fact that `NoDrop<T>` never has a destructor anymore.
//!

#![cfg_attr(not(any(test, feature="std")), no_std)]
#[cfg(not(any(test, feature="std")))]
extern crate core as std;

#[cfg(feature = "use_union")]
extern crate nodrop_union as imp;

pub use imp::NoDrop;


#[cfg(not(feature = "use_union"))]
mod imp {
    use std::ptr;
    use std::mem;
    use std::ops::{Deref, DerefMut};

    /// repr(u8) - Make sure the non-nullable pointer optimization does not occur!
    #[repr(u8)]
    enum Flag<T> {
        Alive(T),
        // Dummy u8 field below, again to beat the enum layout opt
        Dropped(u8),
    }


    /// A type holding **T** that will not call its destructor on drop
    pub struct NoDrop<T>(Flag<T>);

    impl<T> NoDrop<T> {
        /// Create a new **NoDrop**.
        #[inline]
        pub fn new(value: T) -> NoDrop<T> {
            NoDrop(Flag::Alive(value))
        }

        /// Extract the inner value.
        ///
        /// Once extracted, the value can of course drop again.
        #[inline]
        pub fn into_inner(mut self) -> T {
            let inner = unsafe {
                ptr::read(&mut *self)
            };
            // skip Drop, so we don't even have to overwrite
            mem::forget(self);
            inner
        }
    }

    #[cfg(not(feature = "use_needs_drop"))]
    #[inline]
    fn needs_drop<T>() -> bool {
        true
    }

    #[cfg(feature = "use_needs_drop")]
    #[inline]
    fn needs_drop<T>() -> bool {
        unsafe {
            ::std::mem::needs_drop::<T>()
        }
    }

    impl<T> Drop for NoDrop<T> {
        fn drop(&mut self) {
            if needs_drop::<T>() {
                // inhibit drop
                unsafe {
                    ptr::write(&mut self.0, Flag::Dropped(0));
                }
            }
        }
    }

    impl<T> Deref for NoDrop<T> {
        type Target = T;

        // Use type invariant, always Flag::Alive.
        #[inline]
        fn deref(&self) -> &T {
            match self.0 {
                Flag::Alive(ref inner) => inner,
                _ => unsafe { debug_assert_unreachable() }
            }
        }
    }

    impl<T> DerefMut for NoDrop<T> {
        // Use type invariant, always Flag::Alive.
        #[inline]
        fn deref_mut(&mut self) -> &mut T {
            match self.0 {
                Flag::Alive(ref mut inner) => inner,
                _ => unsafe { debug_assert_unreachable() }
            }
        }
    }

    #[cfg(test)]
    #[test]
    fn test_no_nonnullable_opt() {
        // Make sure `Flag` does not apply the non-nullable pointer optimization
        // as Option would do.
        assert!(mem::size_of::<Flag<&i32>>() > mem::size_of::<&i32>());
        assert!(mem::size_of::<Flag<Vec<i32>>>() > mem::size_of::<Vec<i32>>());
    }

    // copying this code saves us microcrate deps
    #[inline]
    unsafe fn debug_assert_unreachable() -> ! {
        debug_assert!(false, "Reached unreachable section: this is a bug!");
        enum Void { }
        match *(1 as *const Void) { }
    }
}

#[cfg(test)]
mod tests {
    use super::NoDrop;

    #[test]
    fn test_drop() {
        use std::cell::Cell;

        let flag = &Cell::new(0);

        struct Bump<'a>(&'a Cell<i32>);

        impl<'a> Drop for Bump<'a> {
            fn drop(&mut self) {
                let n = self.0.get();
                self.0.set(n + 1);
            }
        }

        {
            let _ = NoDrop::new([Bump(flag), Bump(flag)]);
        }
        assert_eq!(flag.get(), 0);

        // test something with the nullable pointer optimization
        flag.set(0);

        {
            let mut array = NoDrop::new(Vec::new());
            array.push(vec![Bump(flag)]);
            array.push(vec![Bump(flag), Bump(flag)]);
            array.push(vec![]);
            array.push(vec![Bump(flag)]);
            drop(array.pop());
            assert_eq!(flag.get(), 1);
            drop(array.pop());
            assert_eq!(flag.get(), 1);
            drop(array.pop());
            assert_eq!(flag.get(), 3);
        }

        // last one didn't drop.
        assert_eq!(flag.get(), 3);

        flag.set(0);
        {
            let array = NoDrop::new(Bump(flag));
            array.into_inner();
            assert_eq!(flag.get(), 1);
        }
        assert_eq!(flag.get(), 1);
    }
}
