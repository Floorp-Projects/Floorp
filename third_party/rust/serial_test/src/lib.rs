#![cfg_attr(docsrs, feature(doc_cfg, doc_auto_cfg))]

//! # serial_test
//! `serial_test` allows for the creation of serialised Rust tests using the [serial](macro@serial) attribute
//! e.g.
//! ````
//! #[test]
//! #[serial]
//! fn test_serial_one() {
//!   // Do things
//! }
//!
//! #[test]
//! #[serial]
//! fn test_serial_another() {
//!   // Do things
//! }
//! ````
//! Multiple tests with the [serial](macro@serial) attribute are guaranteed to be executed in serial. Ordering
//! of the tests is not guaranteed however.
//!
//! For cases like doctests and integration tests where the tests are run as separate processes, we also support
//! [file_serial](macro@file_serial), with similar properties but based off file locking. Note that there are no
//! guarantees about one test with [serial](macro@serial) and another with [file_serial](macro@file_serial)
//! as they lock using different methods.
//! ````
//! #[test]
//! #[file_serial]
//! fn test_serial_three() {
//!   // Do things
//! }
//! ````
//!
//! ## Feature flags
#![cfg_attr(
    feature = "docsrs",
    cfg_attr(doc, doc = ::document_features::document_features!())
)]

mod code_lock;
#[cfg(feature = "file_locks")]
mod file_lock;

pub use code_lock::{
    local_async_serial_core, local_async_serial_core_with_return, local_serial_core,
    local_serial_core_with_return,
};

#[cfg(feature = "file_locks")]
pub use file_lock::{
    fs_async_serial_core, fs_async_serial_core_with_return, fs_serial_core,
    fs_serial_core_with_return,
};

// Re-export #[serial/file_serial].
#[allow(unused_imports)]
pub use serial_test_derive::serial;

#[cfg(feature = "file_locks")]
pub use serial_test_derive::file_serial;
