//! Platform-independent platform abstraction
//!
//! This is the platform-independent portion of the standard library's
//! platform abstraction layer, whereas `std::sys` is the
//! platform-specific portion.
//!
//! The relationship between `std::sys_common`, `std::sys` and the
//! rest of `std` is complex, with dependencies going in all
//! directions: `std` depending on `sys_common`, `sys_common`
//! depending on `sys`, and `sys` depending on `sys_common` and `std`.
//! Ideally `sys_common` would be split into two and the dependencies
//! between them all would form a dag, facilitating the extraction of
//! `std::sys` from the standard library.

pub mod bytestring;

/// A trait for viewing representations from std types
#[doc(hidden)]
pub trait AsInner<Inner: ?Sized> {
    fn as_inner(&self) -> &Inner;
}

/// A trait for viewing representations from std types
#[doc(hidden)]
pub trait AsInnerMut<Inner: ?Sized> {
    fn as_inner_mut(&mut self) -> &mut Inner;
}

/// A trait for extracting representations from std types
#[doc(hidden)]
pub trait IntoInner<Inner> {
    fn into_inner(self) -> Inner;
}

/// A trait for creating std types from internal representations
#[doc(hidden)]
pub trait FromInner<Inner> {
    fn from_inner(inner: Inner) -> Self;
}
