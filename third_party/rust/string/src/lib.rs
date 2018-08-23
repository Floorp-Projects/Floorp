//! A UTF-8 encoded string with configurable byte storage.
//!
//! This crate provides `String`, a type similar to its std counterpart, but
//! with one significant difference: the underlying byte storage is
//! configurable. In other words, `String<T>` is a marker type wrapping `T`,
//! indicating that it represents a UTF-8 encoded string.
//!
//! For example, one can represent small strings (stack allocated) by wrapping
//! an array:
//!
//! ```
//! # use string::*;
//! let s: String<[u8; 2]> = String::try_from([b'h', b'i']).unwrap();
//! assert_eq!(&s[..], "hi");
//! ```

#![deny(warnings, missing_docs, missing_debug_implementations)]
#![doc(html_root_url = "https://docs.rs/string/0.1.1")]

use std::{fmt, ops, str};

/// A UTF-8 encoded string with configurable byte storage.
///
/// This type differs from `std::String` in that it is generic over the
/// underlying byte storage, enabling it to use `Vec<[u8]>`, `&[u8]`, or third
/// party types, such as [`Bytes`].
///
/// [`Bytes`]: https://docs.rs/bytes/0.4.8/bytes/struct.Bytes.html
#[derive(Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Default)]
pub struct String<T = Vec<u8>> {
    value: T,
}

impl<T> String<T> {
    /// Get a reference to the underlying byte storage.
    ///
    /// # Examples
    ///
    /// ```
    /// # use string::*;
    /// let s = String::new();
    /// let vec = s.get_ref();
    /// ```
    pub fn get_ref(&self) -> &T {
        &self.value
    }

    /// Get a mutable reference to the underlying byte storage.
    ///
    /// It is inadvisable to directly manipulate the byte storage. This function
    /// is unsafe as the bytes could no longer be valid UTF-8 after mutation.
    ///
    /// # Examples
    ///
    /// ```
    /// # use string::*;
    /// let mut s = String::new();
    ///
    /// unsafe {
    ///     let vec = s.get_mut();
    /// }
    /// ```
    pub unsafe fn get_mut(&mut self) -> &mut T {
        &mut self.value
    }

    /// Unwraps this `String`, returning the underlying byte storage.
    ///
    /// # Examples
    ///
    /// ```
    /// # use string::*;
    /// let s = String::new();
    /// let vec = s.into_inner();
    /// ```
    pub fn into_inner(self) -> T {
        self.value
    }
}

impl String {
    /// Creates a new empty `String`.
    ///
    /// Given that the `String` is empty, this will not allocate.
    ///
    /// # Examples
    ///
    /// Basic usage
    ///
    /// ```
    /// let s = String::new();
    /// assert_eq!(s, "");
    /// ```
    pub fn new() -> String {
        String::default()
    }
}

impl<T> String<T>
    where T: AsRef<[u8]>,
{
    /// Converts the provided value to a `String` without checking that the
    /// given value is valid UTF-8.
    ///
    /// Use `TryFrom` for a safe conversion.
    pub unsafe fn from_utf8_unchecked(value: T) -> String<T> {
        String { value }
    }
}

impl<T> ops::Deref for String<T>
    where T: AsRef<[u8]>
{
    type Target = str;

    #[inline]
    fn deref(&self) -> &str {
        let b = self.value.as_ref();
        unsafe { str::from_utf8_unchecked(b) }
    }
}

impl From<::std::string::String> for String<::std::string::String> {
    fn from(value: ::std::string::String) -> Self {
        String { value }
    }
}

impl<'a> From<&'a str> for String<&'a str> {
    fn from(value: &'a str) -> Self {
        String { value }
    }
}

impl<T> TryFrom<T> for String<T>
    where T: AsRef<[u8]>
{
    type Error = str::Utf8Error;

    fn try_from(value: T) -> Result<Self, Self::Error> {
        let _ = str::from_utf8(value.as_ref())?;
        Ok(String { value })
    }
}

impl<T: AsRef<[u8]>> fmt::Debug for String<T> {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        (**self).fmt(fmt)
    }
}

/// Attempt to construct `Self` via a conversion.
///
/// This trait will be deprecated in favor of [std::convert::TryFrom] once it
/// reaches stable Rust.
pub trait TryFrom<T>: Sized + sealed::Sealed {
    /// The type returned in the event of a conversion error.
    type Error;

    /// Performs the conversion.
    fn try_from(value: T) -> Result<Self, Self::Error>;
}

impl<T> sealed::Sealed for String<T> {}

mod sealed {
    /// Private trait to this crate to prevent traits from being implemented in
    /// downstream crates.
    pub trait Sealed {}
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_from_std_string() {
        let s: String<_> = "hello".to_string().into();
        assert_eq!(&s[..], "hello");
    }
}
