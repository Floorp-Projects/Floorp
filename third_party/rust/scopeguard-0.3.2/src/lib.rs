//! A scope guard will run a given closure when it goes out of scope,
//! even if the code between panics.
//! (as long as panic doesn't abort)

#![cfg_attr(not(any(test, feature = "use_std")), no_std)]

//!
//!
//! Crate features:
//!
//! - `use_std`
//!   + Enabled by default. Enables the `OnUnwind` strategy.
//!   + Disable to use `no_std`.

#[cfg(not(any(test, feature = "use_std")))]
extern crate core as std;

use std::fmt;
use std::marker::PhantomData;
use std::ops::{Deref, DerefMut};

pub trait Strategy {
    /// Return `true` if the guard’s associated code should run
    /// (in the context where this method is called).
    fn should_run() -> bool;
}

/// Always run on scope exit.
///
/// “Always” run: on regular exit from a scope or on unwinding from a panic.
/// Can not run on abort, process exit, and other catastrophic events where
/// destructors don’t run.
#[derive(Debug)]
pub enum Always {}

/// Run on scope exit through unwinding.
///
/// Requires crate feature `use_std`.
#[cfg(feature = "use_std")]
#[derive(Debug)]
pub enum OnUnwind {}

/// Run on regular scope exit, when not unwinding.
///
/// Requires crate feature `use_std`.
#[cfg(feature = "use_std")]
#[derive(Debug)]
#[cfg(test)]
enum OnSuccess {}

impl Strategy for Always {
    #[inline(always)]
    fn should_run() -> bool { true }
}

#[cfg(feature = "use_std")]
impl Strategy for OnUnwind {
    #[inline(always)]
    fn should_run() -> bool { std::thread::panicking() }
}

#[cfg(feature = "use_std")]
#[cfg(test)]
impl Strategy for OnSuccess {
    #[inline(always)]
    fn should_run() -> bool { !std::thread::panicking() }
}

/// Macro to create a `ScopeGuard` (always run).
///
/// The macro takes one expression `$e`, which is the body of a closure
/// that will run when the scope is exited. The expression can
/// be a whole block.
#[macro_export]
macro_rules! defer {
    ($e:expr) => {
        let _guard = $crate::guard((), |_| $e);
    }
}

/// Macro to create a `ScopeGuard` (run on successful scope exit).
///
/// The macro takes one expression `$e`, which is the body of a closure
/// that will run when the scope is exited. The expression can
/// be a whole block.
///
/// Requires crate feature `use_std`.
#[cfg(test)]
macro_rules! defer_on_success {
    ($e:expr) => {
        let _guard = $crate::guard_on_success((), |_| $e);
    }
}

/// Macro to create a `ScopeGuard` (run on unwinding from panic).
///
/// The macro takes one expression `$e`, which is the body of a closure
/// that will run when the scope is exited. The expression can
/// be a whole block.
///
/// Requires crate feature `use_std`.
#[macro_export]
macro_rules! defer_on_unwind {
    ($e:expr) => {
        let _guard = $crate::guard_on_unwind((), |_| $e);
    }
}

/// `ScopeGuard` is a scope guard that may own a protected value.
///
/// If you place a guard in a local variable, the closure can
/// run regardless how you leave the scope — through regular return or panic
/// (except if panic or other code aborts; so as long as destructors run).
/// It is run only once.
///
/// The `S` parameter for [`Strategy`](Strategy.t.html) determines if
/// the closure actually runs.
///
/// The guard's closure will be called with a mut ref to the held value
/// in the destructor. It's called only once.
///
/// The `ScopeGuard` implements `Deref` so that you can access the inner value.
pub struct ScopeGuard<T, F, S: Strategy = Always>
    where F: FnMut(&mut T)
{
    __dropfn: F,
    __value: T,
    strategy: PhantomData<S>,
}
impl<T, F, S> ScopeGuard<T, F, S>
    where F: FnMut(&mut T),
          S: Strategy,
{
    /// Create a `ScopeGuard` that owns `v` (accessible through deref) and calls
    /// `dropfn` when its destructor runs.
    ///
    /// The `Strategy` decides whether the scope guard's closure should run.
    pub fn with_strategy(v: T, dropfn: F) -> ScopeGuard<T, F, S> {
        ScopeGuard {
            __value: v,
            __dropfn: dropfn,
            strategy: PhantomData,
        }
    }
}


/// Create a new `ScopeGuard` owning `v` and with deferred closure `dropfn`.
pub fn guard<T, F>(v: T, dropfn: F) -> ScopeGuard<T, F, Always>
    where F: FnMut(&mut T)
{
    ScopeGuard::with_strategy(v, dropfn)
}

#[cfg(feature = "use_std")]
/// Create a new `ScopeGuard` owning `v` and with deferred closure `dropfn`.
///
/// Requires crate feature `use_std`.
#[cfg(test)]
fn guard_on_success<T, F>(v: T, dropfn: F) -> ScopeGuard<T, F, OnSuccess>
    where F: FnMut(&mut T)
{
    ScopeGuard::with_strategy(v, dropfn)
}

#[cfg(feature = "use_std")]
/// Create a new `ScopeGuard` owning `v` and with deferred closure `dropfn`.
///
/// Requires crate feature `use_std`.
pub fn guard_on_unwind<T, F>(v: T, dropfn: F) -> ScopeGuard<T, F, OnUnwind>
    where F: FnMut(&mut T)
{
    ScopeGuard::with_strategy(v, dropfn)
}

impl<T, F, S: Strategy> Deref for ScopeGuard<T, F, S>
    where F: FnMut(&mut T)
{
    type Target = T;
    fn deref(&self) -> &T {
        &self.__value
    }

}

impl<T, F, S: Strategy> DerefMut for ScopeGuard<T, F, S>
    where F: FnMut(&mut T)
{
    fn deref_mut(&mut self) -> &mut T {
        &mut self.__value
    }
}

impl<T, F, S: Strategy> Drop for ScopeGuard<T, F, S>
    where F: FnMut(&mut T)
{
    fn drop(&mut self) {
        if S::should_run() {
            (self.__dropfn)(&mut self.__value)
        }
    }
}

impl<T, F, S> fmt::Debug for ScopeGuard<T, F, S>
    where T: fmt::Debug,
          F: FnMut(&mut T),
          S: Strategy + fmt::Debug,
{
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("ScopeGuard")
         .field("value", &self.__value)
         .finish()
    }
}

#[cfg(test)]
mod tests {
    use std::cell::Cell;
    use std::panic::catch_unwind;
    use std::panic::AssertUnwindSafe;

    #[test]
    fn test_defer() {
        let drops = Cell::new(0);
        defer!(drops.set(1000));
        assert_eq!(drops.get(), 0);
    }

    #[test]
    fn test_defer_success_1() {
        let drops = Cell::new(0);
        {
            defer_on_success!(drops.set(1));
            assert_eq!(drops.get(), 0);
        }
        assert_eq!(drops.get(), 1);
    }

    #[test]
    fn test_defer_success_2() {
        let drops = Cell::new(0);
        let _ = catch_unwind(AssertUnwindSafe(|| {
            defer_on_success!(drops.set(1));
            panic!("failure")
        }));
        assert_eq!(drops.get(), 0);
    }

    #[test]
    fn test_defer_unwind_1() {
        let drops = Cell::new(0);
        let _ = catch_unwind(AssertUnwindSafe(|| {
            defer_on_unwind!(drops.set(1));
            assert_eq!(drops.get(), 0);
            panic!("failure")
        }));
        assert_eq!(drops.get(), 1);
    }

    #[test]
    fn test_defer_unwind_2() {
        let drops = Cell::new(0);
        {
            defer_on_unwind!(drops.set(1));
        }
        assert_eq!(drops.get(), 0);
    }
}
