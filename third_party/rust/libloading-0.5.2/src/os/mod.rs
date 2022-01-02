//! Unsafe, platform specific bindings to dynamic library loading facilities.
//!
//! These modules expose more extensive, powerful, less principled bindings to the dynamic
//! library loading facilities. Use of these bindings come at the cost of less (in most cases,
//! none at all) safety guarantees, which are provided by the top-level bindings.
//!
//! # Examples
//!
//! Using these modules will likely involve conditional compilation:
//!
//! ```ignore
//! # extern crate libloading;
//! #[cfg(unix)]
//! use libloading::os::unix::*;
//! #[cfg(windows)]
//! use libloading::os::windows::*;
//! ```

macro_rules! unix {
    ($item: item) => {
        /// UNIX implementation of dynamic library loading.
        ///
        /// This module should be expanded with more UNIX-specific functionality in the future.
        $item
    }
}

macro_rules! windows {
    ($item: item) => {
        /// Windows implementation of dynamic library loading.
        ///
        /// This module should be expanded with more Windows-specific functionality in the future.
        $item
    }
}

#[cfg(unix)]
unix!(pub mod unix;);
#[cfg(unix)]
windows!(pub mod windows {});

#[cfg(windows)]
windows!(pub mod windows;);
#[cfg(windows)]
unix!(pub mod unix {});
