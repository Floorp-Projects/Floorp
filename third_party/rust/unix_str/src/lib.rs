//! Strings that are compatible wuth Unix-like operating systems.
//!
//! * [`UnixString`] and [`UnixStr`] are useful when you need to with Unix strings.
//! Conversions between [`UnixString`], [`UnixStr`] and Rust strings work similarly
//! to those for `CString` and `CStr`.
//!
//! * [`UnixString`] represents an owned string in Unix's preferred
//! representation.
//!
//! * [`UnixStr`] represents a borrowed reference to a string in a format that
//! can be passed to a Unix-lie operating system. It can be converted into
//! a UTF-8 Rust string slice in a similar way to [`UnixString`].
//!
//! # Conversions
//!
//! [`UnixStr`] implements two methods, [`from_bytes`] and [`as_bytes`].
//! These do inexpensive conversions from and to UTF-8 byte slices.
//!
//! Additionally, [`UnixString`] provides [`from_vec`] and [`into_vec`] methods
//! that consume their arguments, and take or produce vectors of [`u8`].
//!
//! [`UnixString`]: struct.UnixString.html
//! [`UnixStr`]: struct.UnixStr.html
//! [`from_vec`]: struct.UnixString.html#method.from_vec
//! [`into_vec`]: struct.UnixString.html#method.into_vec
//! [`from_bytes`]: struct.UnixStrExt.html#method.from_bytes
//! [`as_bytes`]: struct.UnixStrExt.html#method.as_bytes

#![cfg_attr(feature = "shrink_to", feature(shrink_to))]
#![cfg_attr(feature = "toowned_clone_into", feature(toowned_clone_into))]
#![no_std]

#[cfg(feature = "alloc")]
extern crate alloc;

use core::cmp;
use core::fmt;
use core::hash::{Hash, Hasher};
use core::mem;

#[cfg(feature = "alloc")]
use alloc::borrow::{Borrow, Cow, ToOwned};
#[cfg(feature = "alloc")]
use alloc::boxed::Box;
#[cfg(feature = "alloc")]
use alloc::rc::Rc;
#[cfg(feature = "alloc")]
use alloc::string::String;
#[cfg(feature = "alloc")]
use alloc::sync::Arc;
#[cfg(feature = "alloc")]
use alloc::vec::Vec;
#[cfg(feature = "alloc")]
use core::ops;
#[cfg(feature = "alloc")]
use core::str::FromStr;

mod lossy;

mod sys;
#[cfg(feature = "alloc")]
use sys::Buf;
use sys::Slice;

mod sys_common;
use sys_common::AsInner;
#[cfg(feature = "alloc")]
use sys_common::{FromInner, IntoInner};

/// A type that can represent owned, mutable Unix strings, but is cheaply
/// inter-convertible with Rust strings.
///
/// The need for this type arises from the fact that:
///
/// * On Unix systems, strings are often arbitrary sequences of non-zero
///   bytes, in many cases interpreted as UTF-8.
///
/// * In Rust, strings are always valid UTF-8, which may contain zeros.
///
/// `UnixString` and [`UnixStr`] bridge this gap by simultaneously representing
/// Rust and platform-native string values, and in particular allowing a Rust
/// string to be converted into a “Unix” string with no cost if possible.
/// A consequence of this is that `UnixString` instances are *not* `NULL`
/// terminated; in order to pass to e.g., Unix system call, you should create
/// a `CStr`.
///
/// `UnixString` is to [`&UnixStr`] as `String` is to `&str`: the former
/// in each pair are owned strings; the latter are borrowed references.
///
/// Note, `UnixString` and [`UnixStr`] internally do not hold in the form native
/// to the platform: `UnixString`s are stored as a sequence of 8-bit values.
///
/// # Creating an `UnixString`
///
/// **From a Rust string**: `UnixString` implements `From<String>`, so you can
/// use `my_string.from` to create an `UnixString` from a normal Rust string.
///
/// **From slices:** Just like you can start with an empty Rust [`String`]
/// and then [`push_str`][String.push_str] `&str` sub-string slices into it,
/// you can create an empty `UnixString` with the [`new`] method and then push
/// string slices into it with the [`push`] method.
///
/// # Extracting a borrowed reference to the whole OS string
///
/// You can use the [`as_unix_str`] method to get a [`&UnixStr`] from
/// a `UnixString`; this is effectively a borrowed reference to the whole
/// string.
///
/// # Conversions
///
/// See the [module's toplevel documentation about conversions][conversions]
/// for a discussion on the traits which `UnixString` implements for
/// [conversions] from/to native representations.
///
/// [`UnixStr`]: struct.UnixStr.html
/// [`&UnixStr`]: struct.UnixStr.html
/// [`CStr`]: struct.CStr.html
/// [`new`]: #method.new
/// [`push`]: #method.push
/// [`as_unix_str`]: #method.as_unix_str
/// [conversions]: index.html#conversions
#[derive(Clone)]
#[cfg(feature = "alloc")]
pub struct UnixString {
    inner: Buf,
}

/// Borrowed reference to a Unix string (see [`UnixString`]).
///
/// This type represents a borrowed reference to a string in Unix's preferred
/// representation.
///
/// `&UnixStr` is to [`UnixString`] as `&str` is to `String`: the former
/// in each pair are borrowed references; the latter are owned strings.
///
/// See the [module's toplevel documentation about conversions][conversions]
/// for a discussion on the traits which `UnixStr` implements for [conversions]
/// from/to native representations.
///
/// [`UnixString`]: struct.UnixString.html
/// [conversions]: index.html#conversions
// FIXME:
// `UnixStr::from_inner` current implementation relies on `UnixStr` being
// layout-compatible with `Slice`. When attribute privacy is implemented,
// `UnixStr` should be annotated as `#[repr(transparent)]`. Anyway, `UnixStr`
// representation and layout are considered implementation detail, are
// not documented and must not be relied upon.
pub struct UnixStr {
    inner: Slice,
}

#[cfg(feature = "alloc")]
impl UnixString {
    /// Constructs a new empty `UnixString`.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let unix_string = UnixString::new();
    /// ```
    pub fn new() -> Self {
        Self {
            inner: Buf::from_string(String::new()),
        }
    }

    /// Converts to an [`UnixStr`] slice.
    ///
    /// [`UnixStr`]: struct.UnixStr.html
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::{UnixString, UnixStr};
    ///
    /// let unix_string = UnixString::from("foo");
    /// let unix_str = UnixStr::new("foo");
    /// assert_eq!(unix_string.as_unix_str(), unix_str);
    /// ```
    pub fn as_unix_str(&self) -> &UnixStr {
        self
    }

    /// Converts the `UnixString` into a `String` if it contains valid Unicode data.
    ///
    /// On failure, ownership of the original `UnixString` is returned.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let unix_string = UnixString::from("foo");
    /// let string = unix_string.into_string();
    /// assert_eq!(string, Ok(String::from("foo")));
    /// ```
    pub fn into_string(self) -> Result<String, UnixString> {
        self.inner
            .into_string()
            .map_err(|buf| UnixString { inner: buf })
    }

    /// Extends the string with the given [`&UnixStr`] slice.
    ///
    /// [`&UnixStr`]: struct.UnixStr.html
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let mut unix_string = UnixString::from("foo");
    /// unix_string.push("bar");
    /// assert_eq!(&unix_string, "foobar");
    /// ```
    pub fn push<T: AsRef<UnixStr>>(&mut self, s: T) {
        self.inner.push_slice(&s.as_ref().inner)
    }

    /// Creates a new `UnixString` with the given capacity.
    ///
    /// The string will be able to hold exactly `capacity` length units of other
    /// OS strings without reallocating. If `capacity` is 0, the string will not
    /// allocate.
    ///
    /// See main `UnixString` documentation information about encoding.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let mut unix_string = UnixString::with_capacity(10);
    /// let capacity = unix_string.capacity();
    ///
    /// // This push is done without reallocating
    /// unix_string.push("foo");
    ///
    /// assert_eq!(capacity, unix_string.capacity());
    /// ```
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            inner: Buf::with_capacity(capacity),
        }
    }

    /// Truncates the `UnixString` to zero length.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let mut unix_string = UnixString::from("foo");
    /// assert_eq!(&unix_string, "foo");
    ///
    /// unix_string.clear();
    /// assert_eq!(&unix_string, "");
    /// ```
    pub fn clear(&mut self) {
        self.inner.clear()
    }

    /// Returns the capacity this `UnixString` can hold without reallocating.
    ///
    /// See `UnixString` introduction for information about encoding.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let unix_string = UnixString::with_capacity(10);
    /// assert!(unix_string.capacity() >= 10);
    /// ```
    pub fn capacity(&self) -> usize {
        self.inner.capacity()
    }

    /// Reserves capacity for at least `additional` more capacity to be inserted
    /// in the given `UnixString`.
    ///
    /// The collection may reserve more space to avoid frequent reallocations.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let mut s = UnixString::new();
    /// s.reserve(10);
    /// assert!(s.capacity() >= 10);
    /// ```
    pub fn reserve(&mut self, additional: usize) {
        self.inner.reserve(additional)
    }

    /// Reserves the minimum capacity for exactly `additional` more capacity to
    /// be inserted in the given `UnixString`. Does nothing if the capacity is
    /// already sufficient.
    ///
    /// Note that the allocator may give the collection more space than it
    /// requests. Therefore, capacity can not be relied upon to be precisely
    /// minimal. Prefer reserve if future insertions are expected.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let mut s = UnixString::new();
    /// s.reserve_exact(10);
    /// assert!(s.capacity() >= 10);
    /// ```
    pub fn reserve_exact(&mut self, additional: usize) {
        self.inner.reserve_exact(additional)
    }

    /// Shrinks the capacity of the `UnixString` to match its length.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let mut s = UnixString::from("foo");
    ///
    /// s.reserve(100);
    /// assert!(s.capacity() >= 100);
    ///
    /// s.shrink_to_fit();
    /// assert_eq!(3, s.capacity());
    /// ```
    pub fn shrink_to_fit(&mut self) {
        self.inner.shrink_to_fit()
    }

    /// Shrinks the capacity of the `UnixString` with a lower bound.
    ///
    /// The capacity will remain at least as large as both the length
    /// and the supplied value.
    ///
    /// Panics if the current capacity is smaller than the supplied
    /// minimum capacity.
    ///
    /// # Examples
    ///
    /// ```
    /// #![feature(shrink_to)]
    /// use std::ffi::UnixString;
    ///
    /// let mut s = UnixString::from("foo");
    ///
    /// s.reserve(100);
    /// assert!(s.capacity() >= 100);
    ///
    /// s.shrink_to(10);
    /// assert!(s.capacity() >= 10);
    /// s.shrink_to(0);
    /// assert!(s.capacity() >= 3);
    /// ```
    #[inline]
    #[cfg(feature = "shrink_to")]
    pub fn shrink_to(&mut self, min_capacity: usize) {
        self.inner.shrink_to(min_capacity)
    }

    /// Converts this `UnixString` into a boxed [`UnixStr`].
    ///
    /// [`UnixStr`]: struct.UnixStr.html
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::{UnixString, UnixStr};
    ///
    /// let s = UnixString::from("hello");
    ///
    /// let b: Box<UnixStr> = s.into_boxed_unix_str();
    /// ```
    pub fn into_boxed_unix_str(self) -> Box<UnixStr> {
        let rw = Box::into_raw(self.inner.into_box()) as *mut UnixStr;
        unsafe { Box::from_raw(rw) }
    }

    /// Creates a `UnixString` from a byte vector.
    ///
    /// See the module documentation for an example.
    ///
    pub fn from_vec(vec: Vec<u8>) -> Self {
        FromInner::from_inner(Buf { inner: vec })
    }

    /// Yields the underlying byte vector of this `UnixString`.
    ///
    /// See the module documentation for an example.
    pub fn into_vec(self) -> Vec<u8> {
        self.into_inner().inner
    }
}

#[cfg(feature = "alloc")]
impl From<String> for UnixString {
    /// Converts a `String` into a [`UnixString`].
    ///
    /// The conversion copies the data, and includes an allocation on the heap.
    ///
    /// [`UnixString`]: ../../std/ffi/struct.UnixString.html
    fn from(s: String) -> Self {
        UnixString {
            inner: Buf::from_string(s),
        }
    }
}

#[cfg(feature = "alloc")]
impl<T: ?Sized + AsRef<UnixStr>> From<&T> for UnixString {
    fn from(s: &T) -> Self {
        s.as_ref().to_unix_string()
    }
}

#[cfg(feature = "alloc")]
impl ops::Index<ops::RangeFull> for UnixString {
    type Output = UnixStr;

    #[inline]
    fn index(&self, _index: ops::RangeFull) -> &UnixStr {
        UnixStr::from_inner(self.inner.as_slice())
    }
}

#[cfg(feature = "alloc")]
impl ops::IndexMut<ops::RangeFull> for UnixString {
    #[inline]
    fn index_mut(&mut self, _index: ops::RangeFull) -> &mut UnixStr {
        UnixStr::from_inner_mut(self.inner.as_mut_slice())
    }
}

#[cfg(feature = "alloc")]
impl ops::Deref for UnixString {
    type Target = UnixStr;

    #[inline]
    fn deref(&self) -> &UnixStr {
        &self[..]
    }
}

#[cfg(feature = "alloc")]
impl ops::DerefMut for UnixString {
    #[inline]
    fn deref_mut(&mut self) -> &mut UnixStr {
        &mut self[..]
    }
}

#[cfg(feature = "alloc")]
impl Default for UnixString {
    /// Constructs an empty `UnixString`.
    #[inline]
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(feature = "alloc")]
impl fmt::Debug for UnixString {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&**self, formatter)
    }
}

#[cfg(feature = "alloc")]
impl PartialEq for UnixString {
    fn eq(&self, other: &Self) -> bool {
        &**self == &**other
    }
}

#[cfg(feature = "alloc")]
impl PartialEq<str> for UnixString {
    fn eq(&self, other: &str) -> bool {
        &**self == other
    }
}

#[cfg(feature = "alloc")]
impl PartialEq<UnixString> for str {
    fn eq(&self, other: &UnixString) -> bool {
        &**other == self
    }
}

#[cfg(feature = "alloc")]
impl PartialEq<&str> for UnixString {
    fn eq(&self, other: &&str) -> bool {
        **self == **other
    }
}

#[cfg(feature = "alloc")]
impl<'a> PartialEq<UnixString> for &'a str {
    fn eq(&self, other: &UnixString) -> bool {
        **other == **self
    }
}

#[cfg(feature = "alloc")]
impl Eq for UnixString {}

#[cfg(feature = "alloc")]
impl PartialOrd for UnixString {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        (&**self).partial_cmp(&**other)
    }
    #[inline]
    fn lt(&self, other: &Self) -> bool {
        &**self < &**other
    }
    #[inline]
    fn le(&self, other: &Self) -> bool {
        &**self <= &**other
    }
    #[inline]
    fn gt(&self, other: &Self) -> bool {
        &**self > &**other
    }
    #[inline]
    fn ge(&self, other: &Self) -> bool {
        &**self >= &**other
    }
}

#[cfg(feature = "alloc")]
impl PartialOrd<str> for UnixString {
    #[inline]
    fn partial_cmp(&self, other: &str) -> Option<cmp::Ordering> {
        (&**self).partial_cmp(other)
    }
}

#[cfg(feature = "alloc")]
impl Ord for UnixString {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        (&**self).cmp(&**other)
    }
}

#[cfg(feature = "alloc")]
impl Hash for UnixString {
    #[inline]
    fn hash<H: Hasher>(&self, state: &mut H) {
        (&**self).hash(state)
    }
}

impl UnixStr {
    /// Coerces into an `UnixStr` slice.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixStr;
    ///
    /// let unix_str = UnixStr::new("foo");
    /// ```
    #[inline]
    pub fn new<S: AsRef<UnixStr> + ?Sized>(s: &S) -> &UnixStr {
        s.as_ref()
    }

    #[inline]
    fn from_inner(inner: &Slice) -> &UnixStr {
        // Safety: UnixStr is just a wrapper of Slice,
        // therefore converting &Slice to &UnixStr is safe.
        unsafe { &*(inner as *const Slice as *const UnixStr) }
    }

    #[inline]
    #[cfg(feature = "alloc")]
    fn from_inner_mut(inner: &mut Slice) -> &mut UnixStr {
        // Safety: UnixStr is just a wrapper of Slice,
        // therefore converting &mut Slice to &mut UnixStr is safe.
        // Any method that mutates UnixStr must be careful not to
        // break platform-specific encoding, in particular Wtf8 on Windows.
        unsafe { &mut *(inner as *mut Slice as *mut UnixStr) }
    }

    /// Yields a `&str` slice if the `UnixStr` is valid Unicode.
    ///
    /// This conversion may entail doing a check for UTF-8 validity.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixStr;
    ///
    /// let unix_str = UnixStr::new("foo");
    /// assert_eq!(unix_str.to_str(), Some("foo"));
    /// ```
    pub fn to_str(&self) -> Option<&str> {
        self.inner.to_str()
    }

    /// Converts an `UnixStr` to a `Cow<str>`.
    ///
    /// Any non-Unicode sequences are replaced with
    /// `U+FFFD REPLACEMENT CHARACTER`.
    ///
    ///
    /// # Examples
    ///
    /// Calling `to_string_lossy` on an `UnixStr` with invalid unicode:
    ///
    /// ```
    /// use unix_str::UnixStr;
    ///
    /// // Here, the values 0x66 and 0x6f correspond to 'f' and 'o'
    /// // respectively. The value 0x80 is a lone continuation byte, invalid
    /// // in a UTF-8 sequence.
    /// let source = [0x66, 0x6f, 0x80, 0x6f];
    /// let unix_str = UnixStr::from_bytes(&source[..]);
    ///
    /// assert_eq!(unix_str.to_string_lossy(), "fo�o");
    /// ```
    #[cfg(feature = "alloc")]
    pub fn to_string_lossy(&self) -> Cow<'_, str> {
        self.inner.to_string_lossy()
    }

    /// Copies the slice into an owned [`UnixString`].
    ///
    /// [`UnixString`]: struct.UnixString.html
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::{UnixStr, UnixString};
    ///
    /// let unix_str = UnixStr::new("foo");
    /// let unix_string = unix_str.to_unix_string();
    /// assert_eq!(unix_string, UnixString::from("foo"));
    /// ```
    #[cfg(feature = "alloc")]
    pub fn to_unix_string(&self) -> UnixString {
        UnixString {
            inner: self.inner.to_owned(),
        }
    }

    /// Checks whether the `UnixStr` is empty.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixStr;
    ///
    /// let unix_str = UnixStr::new("");
    /// assert!(unix_str.is_empty());
    ///
    /// let unix_str = UnixStr::new("foo");
    /// assert!(!unix_str.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.inner.inner.is_empty()
    }

    /// Returns the length of this `UnixStr`.
    ///
    /// Note that this does **not** return the number of bytes in the string in
    /// OS string form.
    ///
    /// The length returned is that of the underlying storage used by `UnixStr`.
    /// As discussed in the [`UnixString`] introduction, [`UnixString`] and
    /// `UnixStr` store strings in a form best suited for cheap inter-conversion
    /// between native-platform and Rust string forms, which may differ
    /// significantly from both of them, including in storage size and encoding.
    ///
    /// This number is simply useful for passing to other methods, like
    /// [`UnixString::with_capacity`] to avoid reallocations.
    ///
    /// [`UnixString`]: struct.UnixString.html
    /// [`UnixString::with_capacity`]: struct.UnixString.html#method.with_capacity
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixStr;
    ///
    /// let unix_str = UnixStr::new("");
    /// assert_eq!(unix_str.len(), 0);
    ///
    /// let unix_str = UnixStr::new("foo");
    /// assert_eq!(unix_str.len(), 3);
    /// ```
    pub fn len(&self) -> usize {
        self.inner.inner.len()
    }

    /// Converts a `Box<UnixStr>` into an [`UnixString`] without copying
    /// allocating.
    ///
    /// [`UnixString`]: struct.UnixString.html
    #[cfg(feature = "alloc")]
    pub fn into_unix_string(self: Box<UnixStr>) -> UnixString {
        let boxed = unsafe { Box::from_raw(Box::into_raw(self) as *mut Slice) };
        UnixString {
            inner: Buf::from_box(boxed),
        }
    }

    /// Gets the underlying byte representation.
    ///
    /// Note: it is *crucial* that this API is private, to avoid
    /// revealing the internal, platform-specific encodings.
    #[inline]
    fn bytes(&self) -> &[u8] {
        unsafe { &*(&self.inner as *const _ as *const [u8]) }
    }

    /// Converts this string to its ASCII lower case equivalent in-place.
    ///
    /// ASCII letters 'A' to 'Z' are mapped to 'a' to 'z', but non-ASCII letters
    /// are unchanged.
    ///
    /// To return a new lowercased value without modifying the existing one, use
    /// [`to_ascii_lowercase`].
    ///
    /// [`to_ascii_lowercase`]: #method.to_ascii_lowercase
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let mut s = UnixString::from("GRÜßE, JÜRGEN ❤");
    ///
    /// s.make_ascii_lowercase();
    ///
    /// assert_eq!("grÜße, jÜrgen ❤", s);
    /// ```
    #[cfg(feature = "unixstring_ascii")]
    pub fn make_ascii_lowercase(&mut self) {
        self.inner.make_ascii_lowercase()
    }

    /// Converts this string to its ASCII upper case equivalent in-place.
    ///
    /// ASCII letters 'a' to 'z' are mapped to 'A' to 'Z',
    /// but non-ASCII letters are unchanged.
    ///
    /// To return a new uppercased value without modifying the existing one, use
    /// [`to_ascii_uppercase`].
    ///
    /// [`to_ascii_uppercase`]: #method.to_ascii_uppercase
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let mut s = UnixString::from("Grüße, Jürgen ❤");
    ///
    /// s.make_ascii_uppercase();
    ///
    /// assert_eq!("GRüßE, JüRGEN ❤", s);
    /// ```
    #[cfg(feature = "unixstring_ascii")]
    pub fn make_ascii_uppercase(&mut self) {
        self.inner.make_ascii_uppercase()
    }

    /// Returns a copy of this string where each character is mapped to its
    /// ASCII lower case equivalent.
    ///
    /// ASCII letters 'A' to 'Z' are mapped to 'a' to 'z',
    /// but non-ASCII letters are unchanged.
    ///
    /// To lowercase the value in-place, use [`make_ascii_lowercase`].
    ///
    /// [`make_ascii_lowercase`]: #method.make_ascii_lowercase
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    /// let s = UnixString::from("Grüße, Jürgen ❤");
    ///
    /// assert_eq!("grüße, jürgen ❤", s.to_ascii_lowercase());
    /// ```
    #[cfg(all(feature = "alloc", feature = "unixstring_ascii"))]
    pub fn to_ascii_lowercase(&self) -> UnixString {
        UnixString::from_inner(self.inner.to_ascii_lowercase())
    }

    /// Returns a copy of this string where each character is mapped to its
    /// ASCII upper case equivalent.
    ///
    /// ASCII letters 'a' to 'z' are mapped to 'A' to 'Z',
    /// but non-ASCII letters are unchanged.
    ///
    /// To uppercase the value in-place, use [`make_ascii_uppercase`].
    ///
    /// [`make_ascii_uppercase`]: #method.make_ascii_uppercase
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    /// let s = UnixString::from("Grüße, Jürgen ❤");
    ///
    /// assert_eq!("GRüßE, JüRGEN ❤", s.to_ascii_uppercase());
    /// ```
    #[cfg(all(feature = "alloc", feature = "unixstring_ascii"))]
    pub fn to_ascii_uppercase(&self) -> UnixString {
        UnixString::from_inner(self.inner.to_ascii_uppercase())
    }

    /// Checks if all characters in this string are within the ASCII range.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// let ascii = UnixString::from("hello!\n");
    /// let non_ascii = UnixString::from("Grüße, Jürgen ❤");
    ///
    /// assert!(ascii.is_ascii());
    /// assert!(!non_ascii.is_ascii());
    /// ```
    #[cfg(feature = "unixstring_ascii")]
    pub fn is_ascii(&self) -> bool {
        self.inner.is_ascii()
    }

    /// Checks that two strings are an ASCII case-insensitive match.
    ///
    /// Same as `to_ascii_lowercase(a) == to_ascii_lowercase(b)`,
    /// but without allocating and copying temporaries.
    ///
    /// # Examples
    ///
    /// ```
    /// use unix_str::UnixString;
    ///
    /// assert!(UnixString::from("Ferris").eq_ignore_ascii_case("FERRIS"));
    /// assert!(UnixString::from("Ferrös").eq_ignore_ascii_case("FERRöS"));
    /// assert!(!UnixString::from("Ferrös").eq_ignore_ascii_case("FERRÖS"));
    /// ```
    #[cfg(feature = "unixstring_ascii")]
    pub fn eq_ignore_ascii_case<S: ?Sized + AsRef<UnixStr>>(&self, other: &S) -> bool {
        self.inner.eq_ignore_ascii_case(&other.as_ref().inner)
    }

    /// Creates a `UnixStr` from a byte slice.
    ///
    /// See the module documentation for an example.
    pub fn from_bytes(slice: &[u8]) -> &Self {
        unsafe { mem::transmute(slice) }
    }

    /// Gets the underlying byte view of the `UnixStr` slice.
    ///
    /// See the module documentation for an example.
    pub fn as_bytes(&self) -> &[u8] {
        &self.as_inner().inner
    }
}

#[cfg(feature = "alloc")]
impl From<&UnixStr> for Box<UnixStr> {
    fn from(s: &UnixStr) -> Self {
        let rw = Box::into_raw(s.inner.into_box()) as *mut UnixStr;
        unsafe { Box::from_raw(rw) }
    }
}

#[cfg(feature = "alloc")]
impl From<Cow<'_, UnixStr>> for Box<UnixStr> {
    #[inline]
    fn from(cow: Cow<'_, UnixStr>) -> Self {
        match cow {
            Cow::Borrowed(s) => Box::from(s),
            Cow::Owned(s) => Box::from(s),
        }
    }
}

#[cfg(feature = "alloc")]
impl From<Box<UnixStr>> for UnixString {
    /// Converts a `Box<UnixStr>` into a `UnixString` without copying or
    /// allocating.
    ///
    /// [`UnixStr`]: ../ffi/struct.UnixStr.html
    fn from(boxed: Box<UnixStr>) -> Self {
        boxed.into_unix_string()
    }
}

#[cfg(feature = "alloc")]
impl From<UnixString> for Box<UnixStr> {
    /// Converts a [`UnixString`] into a `Box<UnixStr>` without copying or
    /// allocating.
    ///
    /// [`UnixString`]: ../ffi/struct.UnixString.html
    fn from(s: UnixString) -> Self {
        s.into_boxed_unix_str()
    }
}

#[cfg(feature = "alloc")]
impl Clone for Box<UnixStr> {
    #[inline]
    fn clone(&self) -> Self {
        self.to_unix_string().into_boxed_unix_str()
    }
}

#[cfg(feature = "alloc")]
impl From<UnixString> for Arc<UnixStr> {
    /// Converts a [`UnixString`] into a `Arc<UnixStr>` without copying or
    /// allocating.
    ///
    /// [`UnixString`]: ../ffi/struct.UnixString.html
    #[inline]
    fn from(s: UnixString) -> Self {
        let arc = s.inner.into_arc();
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const UnixStr) }
    }
}

#[cfg(feature = "alloc")]
impl From<&UnixStr> for Arc<UnixStr> {
    #[inline]
    fn from(s: &UnixStr) -> Self {
        let arc = s.inner.into_arc();
        unsafe { Arc::from_raw(Arc::into_raw(arc) as *const UnixStr) }
    }
}

#[cfg(feature = "alloc")]
impl From<UnixString> for Rc<UnixStr> {
    /// Converts a [`UnixString`] into a `Rc<UnixStr>` without copying or
    /// allocating.
    ///
    /// [`UnixString`]: ../ffi/struct.UnixString.html
    #[inline]
    fn from(s: UnixString) -> Self {
        let rc = s.inner.into_rc();
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const UnixStr) }
    }
}

#[cfg(feature = "alloc")]
impl From<&UnixStr> for Rc<UnixStr> {
    #[inline]
    fn from(s: &UnixStr) -> Self {
        let rc = s.inner.into_rc();
        unsafe { Rc::from_raw(Rc::into_raw(rc) as *const UnixStr) }
    }
}

#[cfg(feature = "alloc")]
impl<'a> From<UnixString> for Cow<'a, UnixStr> {
    #[inline]
    fn from(s: UnixString) -> Self {
        Cow::Owned(s)
    }
}

#[cfg(feature = "alloc")]
impl<'a> From<&'a UnixStr> for Cow<'a, UnixStr> {
    #[inline]
    fn from(s: &'a UnixStr) -> Self {
        Cow::Borrowed(s)
    }
}

#[cfg(feature = "alloc")]
impl<'a> From<&'a UnixString> for Cow<'a, UnixStr> {
    #[inline]
    fn from(s: &'a UnixString) -> Self {
        Cow::Borrowed(s.as_unix_str())
    }
}

#[cfg(feature = "alloc")]
impl<'a> From<Cow<'a, UnixStr>> for UnixString {
    #[inline]
    fn from(s: Cow<'a, UnixStr>) -> Self {
        s.into_owned()
    }
}

#[cfg(feature = "alloc")]
impl Default for Box<UnixStr> {
    fn default() -> Self {
        let rw = Box::into_raw(Slice::empty_box()) as *mut UnixStr;
        unsafe { Box::from_raw(rw) }
    }
}

impl Default for &UnixStr {
    /// Creates an empty `UnixStr`.
    #[inline]
    fn default() -> Self {
        UnixStr::new("")
    }
}

impl PartialEq for UnixStr {
    #[inline]
    fn eq(&self, other: &UnixStr) -> bool {
        self.bytes().eq(other.bytes())
    }
}

impl PartialEq<str> for UnixStr {
    #[inline]
    fn eq(&self, other: &str) -> bool {
        *self == *UnixStr::new(other)
    }
}

impl PartialEq<UnixStr> for str {
    #[inline]
    fn eq(&self, other: &UnixStr) -> bool {
        *other == *UnixStr::new(self)
    }
}

impl Eq for UnixStr {}

impl PartialOrd for UnixStr {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<cmp::Ordering> {
        self.bytes().partial_cmp(other.bytes())
    }
    #[inline]
    fn lt(&self, other: &Self) -> bool {
        self.bytes().lt(other.bytes())
    }
    #[inline]
    fn le(&self, other: &Self) -> bool {
        self.bytes().le(other.bytes())
    }
    #[inline]
    fn gt(&self, other: &Self) -> bool {
        self.bytes().gt(other.bytes())
    }
    #[inline]
    fn ge(&self, other: &Self) -> bool {
        self.bytes().ge(other.bytes())
    }
}

impl PartialOrd<str> for UnixStr {
    #[inline]
    fn partial_cmp(&self, other: &str) -> Option<cmp::Ordering> {
        self.partial_cmp(Self::new(other))
    }
}

// FIXME (#19470): cannot provide PartialOrd<UnixStr> for str until we
// have more flexible coherence rules.

impl Ord for UnixStr {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        self.bytes().cmp(other.bytes())
    }
}

#[cfg(feature = "alloc")]
macro_rules! impl_cmp {
    ($lhs:ty, $rhs: ty) => {
        impl<'a, 'b> PartialEq<$rhs> for $lhs {
            #[inline]
            fn eq(&self, other: &$rhs) -> bool {
                <UnixStr as PartialEq>::eq(self, other)
            }
        }

        impl<'a, 'b> PartialEq<$lhs> for $rhs {
            #[inline]
            fn eq(&self, other: &$lhs) -> bool {
                <UnixStr as PartialEq>::eq(self, other)
            }
        }

        impl<'a, 'b> PartialOrd<$rhs> for $lhs {
            #[inline]
            fn partial_cmp(&self, other: &$rhs) -> Option<cmp::Ordering> {
                <UnixStr as PartialOrd>::partial_cmp(self, other)
            }
        }

        impl<'a, 'b> PartialOrd<$lhs> for $rhs {
            #[inline]
            fn partial_cmp(&self, other: &$lhs) -> Option<cmp::Ordering> {
                <UnixStr as PartialOrd>::partial_cmp(self, other)
            }
        }
    };
}

#[cfg(feature = "alloc")]
impl_cmp!(UnixString, UnixStr);
#[cfg(feature = "alloc")]
impl_cmp!(UnixString, &'a UnixStr);
#[cfg(feature = "alloc")]
impl_cmp!(Cow<'a, UnixStr>, UnixStr);
#[cfg(feature = "alloc")]
impl_cmp!(Cow<'a, UnixStr>, &'b UnixStr);
#[cfg(feature = "alloc")]
impl_cmp!(Cow<'a, UnixStr>, UnixString);

impl Hash for UnixStr {
    #[inline]
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.bytes().hash(state)
    }
}

impl fmt::Debug for UnixStr {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt::Debug::fmt(&self.inner, formatter)
    }
}

#[cfg(feature = "alloc")]
impl Borrow<UnixStr> for UnixString {
    fn borrow(&self) -> &UnixStr {
        &self[..]
    }
}

#[cfg(feature = "alloc")]
impl ToOwned for UnixStr {
    type Owned = UnixString;
    fn to_owned(&self) -> Self::Owned {
        self.to_unix_string()
    }
    #[cfg(feature = "toowned_clone_into")]
    fn clone_into(&self, target: &mut Self::Owned) {
        self.inner.clone_into(&mut target.inner)
    }
}

impl AsRef<UnixStr> for UnixStr {
    fn as_ref(&self) -> &UnixStr {
        self
    }
}

#[cfg(feature = "alloc")]
impl AsRef<UnixStr> for UnixString {
    #[inline]
    fn as_ref(&self) -> &UnixStr {
        self
    }
}

impl AsRef<UnixStr> for str {
    #[inline]
    fn as_ref(&self) -> &UnixStr {
        UnixStr::from_inner(Slice::from_str(self))
    }
}

#[cfg(feature = "alloc")]
impl AsRef<UnixStr> for String {
    #[inline]
    fn as_ref(&self) -> &UnixStr {
        (&**self).as_ref()
    }
}

#[cfg(feature = "alloc")]
impl FromInner<Buf> for UnixString {
    fn from_inner(buf: Buf) -> UnixString {
        UnixString { inner: buf }
    }
}

#[cfg(feature = "alloc")]
impl IntoInner<Buf> for UnixString {
    fn into_inner(self) -> Buf {
        self.inner
    }
}

impl AsInner<Slice> for UnixStr {
    #[inline]
    fn as_inner(&self) -> &Slice {
        &self.inner
    }
}

#[cfg(feature = "alloc")]
impl FromStr for UnixString {
    type Err = core::convert::Infallible;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(UnixString::from(s))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use sys_common::{AsInner, IntoInner};

    use alloc::rc::Rc;
    use alloc::sync::Arc;

    #[test]
    fn test_unix_string_with_capacity() {
        let unix_string = UnixString::with_capacity(0);
        assert_eq!(0, unix_string.inner.into_inner().capacity());

        let unix_string = UnixString::with_capacity(10);
        assert_eq!(10, unix_string.inner.into_inner().capacity());

        let mut unix_string = UnixString::with_capacity(0);
        unix_string.push("abc");
        assert!(unix_string.inner.into_inner().capacity() >= 3);
    }

    #[test]
    fn test_unix_string_clear() {
        let mut unix_string = UnixString::from("abc");
        assert_eq!(3, unix_string.inner.as_inner().len());

        unix_string.clear();
        assert_eq!(&unix_string, "");
        assert_eq!(0, unix_string.inner.as_inner().len());
    }

    #[test]
    fn test_unix_string_capacity() {
        let unix_string = UnixString::with_capacity(0);
        assert_eq!(0, unix_string.capacity());

        let unix_string = UnixString::with_capacity(10);
        assert_eq!(10, unix_string.capacity());

        let mut unix_string = UnixString::with_capacity(0);
        unix_string.push("abc");
        assert!(unix_string.capacity() >= 3);
    }

    #[test]
    fn test_unix_string_reserve() {
        let mut unix_string = UnixString::new();
        assert_eq!(unix_string.capacity(), 0);

        unix_string.reserve(2);
        assert!(unix_string.capacity() >= 2);

        for _ in 0..16 {
            unix_string.push("a");
        }

        assert!(unix_string.capacity() >= 16);
        unix_string.reserve(16);
        assert!(unix_string.capacity() >= 32);

        unix_string.push("a");

        unix_string.reserve(16);
        assert!(unix_string.capacity() >= 33)
    }

    #[test]
    fn test_unix_string_reserve_exact() {
        let mut unix_string = UnixString::new();
        assert_eq!(unix_string.capacity(), 0);

        unix_string.reserve_exact(2);
        assert!(unix_string.capacity() >= 2);

        for _ in 0..16 {
            unix_string.push("a");
        }

        assert!(unix_string.capacity() >= 16);
        unix_string.reserve_exact(16);
        assert!(unix_string.capacity() >= 32);

        unix_string.push("a");

        unix_string.reserve_exact(16);
        assert!(unix_string.capacity() >= 33)
    }

    #[test]
    fn test_unix_string_default() {
        let unix_string: UnixString = Default::default();
        assert_eq!("", &unix_string);
    }

    #[test]
    fn test_unix_str_is_empty() {
        let mut unix_string = UnixString::new();
        assert!(unix_string.is_empty());

        unix_string.push("abc");
        assert!(!unix_string.is_empty());

        unix_string.clear();
        assert!(unix_string.is_empty());
    }

    #[test]
    fn test_unix_str_len() {
        let mut unix_string = UnixString::new();
        assert_eq!(0, unix_string.len());

        unix_string.push("abc");
        assert_eq!(3, unix_string.len());

        unix_string.clear();
        assert_eq!(0, unix_string.len());
    }

    #[test]
    fn test_unix_str_default() {
        let unix_str: &UnixStr = Default::default();
        assert_eq!("", unix_str);
    }

    #[test]
    fn into_boxed() {
        let orig = "Hello, world!";
        let unix_str = UnixStr::new(orig);
        let boxed: Box<UnixStr> = Box::from(unix_str);
        let unix_string = unix_str.to_owned().into_boxed_unix_str().into_unix_string();
        assert_eq!(unix_str, &*boxed);
        assert_eq!(&*boxed, &*unix_string);
        assert_eq!(&*unix_string, unix_str);
    }

    #[test]
    fn boxed_default() {
        let boxed = <Box<UnixStr>>::default();
        assert!(boxed.is_empty());
    }

    #[test]
    #[cfg(feature = "toowned_clone_into")]
    fn test_unix_str_clone_into() {
        let mut unix_string = UnixString::with_capacity(123);
        unix_string.push("hello");
        let unix_str = UnixStr::new("bonjour");
        unix_str.clone_into(&mut unix_string);
        assert_eq!(unix_str, unix_string);
        assert!(unix_string.capacity() >= 123);
    }

    #[test]
    fn into_rc() {
        let orig = "Hello, world!";
        let unix_str = UnixStr::new(orig);
        let rc: Rc<UnixStr> = Rc::from(unix_str);
        let arc: Arc<UnixStr> = Arc::from(unix_str);

        assert_eq!(&*rc, unix_str);
        assert_eq!(&*arc, unix_str);

        let rc2: Rc<UnixStr> = Rc::from(unix_str.to_owned());
        let arc2: Arc<UnixStr> = Arc::from(unix_str.to_owned());

        assert_eq!(&*rc2, unix_str);
        assert_eq!(&*arc2, unix_str);
    }
}
