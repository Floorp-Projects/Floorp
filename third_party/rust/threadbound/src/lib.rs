//! [![github]](https://github.com/dtolnay/threadbound)&ensp;[![crates-io]](https://crates.io/crates/threadbound)&ensp;[![docs-rs]](https://docs.rs/threadbound)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs
//!
//! <br>
//!
//! [`ThreadBound<T>`] is a wrapper that binds a value to its original thread.
//! The wrapper gets to be [`Sync`] and [`Send`] but only the original thread on
//! which the ThreadBound was constructed can retrieve the underlying value.
//!
//! [`ThreadBound<T>`]: struct.ThreadBound.html
//! [`Sync`]: https://doc.rust-lang.org/std/marker/trait.Sync.html
//! [`Send`]: https://doc.rust-lang.org/std/marker/trait.Send.html
//!
//! # Example
//!
//! ```
//! use std::marker::PhantomData;
//! use std::rc::Rc;
//! use std::sync::Arc;
//! use threadbound::ThreadBound;
//!
//! // Neither Send nor Sync. Maybe the index points into a
//! // thread-local interner.
//! #[derive(Copy, Clone)]
//! struct Span {
//!     index: u32,
//!     marker: PhantomData<Rc<()>>,
//! }
//!
//! // Error types are always supposed to be Send and Sync.
//! // We can use ThreadBound to make it so.
//! struct Error {
//!     span: ThreadBound<Span>,
//!     message: String,
//! }
//!
//! fn main() {
//!     let err = Error {
//!         span: ThreadBound::new(Span {
//!             index: 99,
//!             marker: PhantomData,
//!         }),
//!         message: "fearless concurrency".to_owned(),
//!     };
//!
//!     // Original thread can see the contents.
//!     assert_eq!(err.span.get_ref().unwrap().index, 99);
//!
//!     let err = Arc::new(err);
//!     let err2 = err.clone();
//!     std::thread::spawn(move || {
//!         // Other threads cannot get access. Maybe they use
//!         // a default value or a different codepath.
//!         assert!(err2.span.get_ref().is_none());
//!     });
//!
//!     // Original thread can still see the contents.
//!     assert_eq!(err.span.get_ref().unwrap().index, 99);
//! }
//! ```

#![doc(html_root_url = "https://docs.rs/threadbound/0.1.4")]
#![allow(clippy::doc_markdown)]

use std::fmt::{self, Debug};
use std::thread::{self, ThreadId};

/// ThreadBound is a Sync-maker and Send-maker that allows accessing a value
/// of type T only from the original thread on which the ThreadBound was
/// constructed.
///
/// Refer to the [crate-level documentation] for a usage example.
///
/// [crate-level documentation]: index.html
pub struct ThreadBound<T> {
    value: T,
    thread_id: ThreadId,
}

unsafe impl<T> Sync for ThreadBound<T> {}

// Send bound requires Copy, as otherwise Drop could run in the wrong place.
unsafe impl<T: Copy> Send for ThreadBound<T> {}

impl<T> ThreadBound<T> {
    /// Binds a value to the current thread. The wrapper can be sent around to
    /// other threads, but no other threads will be able to access the
    /// underlying value.
    pub fn new(value: T) -> Self {
        ThreadBound {
            value,
            thread_id: thread::current().id(),
        }
    }

    /// Accesses a reference to the underlying value if this is its original
    /// thread, otherwise `None`.
    pub fn get_ref(&self) -> Option<&T> {
        if thread::current().id() == self.thread_id {
            Some(&self.value)
        } else {
            None
        }
    }

    /// Accesses a mutable reference to the underlying value if this is its
    /// original thread, otherwise `None`.
    pub fn get_mut(&mut self) -> Option<&mut T> {
        if thread::current().id() == self.thread_id {
            Some(&mut self.value)
        } else {
            None
        }
    }

    /// Extracts ownership of the underlying value if this is its original
    /// thread, otherwise `None`.
    pub fn into_inner(self) -> Option<T> {
        if thread::current().id() == self.thread_id {
            Some(self.value)
        } else {
            None
        }
    }
}

impl<T: Default> Default for ThreadBound<T> {
    fn default() -> Self {
        ThreadBound::new(Default::default())
    }
}

impl<T: Debug> Debug for ThreadBound<T> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        match self.get_ref() {
            Some(value) => Debug::fmt(value, formatter),
            None => formatter.write_str("unknown"),
        }
    }
}
