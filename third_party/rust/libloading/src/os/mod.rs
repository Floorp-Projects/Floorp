//! Unsafe but flexible platform specific bindings to dynamic library loading facilities.
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

/// UNIX implementation of dynamic library loading.
#[cfg(any(unix, docsrs))]
#[cfg_attr(docsrs, doc(cfg(unix)))]
pub mod unix;

/// Windows implementation of dynamic library loading.
#[cfg(any(windows, docsrs))]
#[cfg_attr(docsrs, doc(cfg(windows)))]
pub mod windows;
