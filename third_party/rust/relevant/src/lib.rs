//! Defines `Relevant` type to use in types that requires
//! custom destruction.
//! Crate supports 3 main mechnisms for error reporting:
//! * "panic" feature makes `Relevant` panic on drop
//! * "log" feature uses `log` crate and `Relevant` will emit `log::error!` on drop.
//! * otherwise `Relevant` will print into stderr using `eprintln!` on drop.
//!
//! "backtrace" feature will add backtrace to the error unless it is reported via panicking.
//! "message" feature will add custom message (specified when value was created) to the error.
//!

#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(not(feature = "std"))]
use core as std;

/// Values of this type can't be automatically dropped.
/// If struct or enum has field with type `Relevant`,
/// it can't be automatically dropped either. And so considered relevant too.
/// User has to deconstruct such values and call `Relevant::dispose`.
/// If relevant field is private it means that user has to move value into some public method.
///
/// # Panics
///
/// With "panic" feature enabled this value will always panic on drop.
///
#[derive(Clone, Debug, PartialOrd, PartialEq, Ord, Eq, Hash)]
#[cfg_attr(feature = "serde-1", derive(serde::Serialize, serde::Deserialize))]
pub struct Relevant;

impl Relevant {
    /// Dispose this value.
    pub fn dispose(self) {
        std::mem::forget(self)
    }
}

impl Drop for Relevant {
    fn drop(&mut self) {
        dropped()
    }
}

cfg_if::cfg_if! {
    if #[cfg(feature = "panic")] {
        macro_rules! sink {
            ($($x:tt)*) => { panic!($($x)*) };
        }
    } else if #[cfg(feature = "log")] {
        macro_rules! sink {
            ($($x:tt)*) => { log::error!($($x)*) };
        }
    } else if #[cfg(feature = "std")] {
        macro_rules! sink {
            ($($x:tt)*) => { eprintln!($($x)*) };
        }
    } else {
        macro_rules! sink {
            ($($x:tt)*) => { panic!($($x)*) };
        }
    }
}

cfg_if::cfg_if! {
    if #[cfg(all(not(feature = "panic"), any(feature = "std", feature = "log"), feature = "backtrace"))] {
        fn whine() {
            let backtrace = backtrace::Backtrace::new();
            sink!("Values of this type can't be dropped!. Trace: {:#?}", backtrace)
        }
    } else {
        fn whine()  {
            sink!("Values of this type can't be dropped!")
        }
    }
}

cfg_if::cfg_if! {
    if #[cfg(feature = "std")] {
        fn dropped() {
            if !std::thread::panicking() {
                whine()
            }
        }
    } else {
        fn dropped()  {
            whine()
        }
    }
}