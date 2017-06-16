/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::borrow::{Borrow, Cow};
use std::cmp;
use std::fmt;
use std::hash;
use std::marker::PhantomData;
use std::mem;
use std::ops::Deref;
use std::slice;
use std::str;

// All bits set except the highest
const MAX_LEN: usize = !0 >> 1;

// Only the highest bit
const OWNED_TAG: usize = MAX_LEN + 1;

/// Like `Cow<'a, str>`, but with smaller `std::mem::size_of`. (Two words instead of four.)
pub struct CompactCowStr<'a> {
    // `tagged_len` is a tag in its highest bit, and the string length in the rest of the bits.
    //
    // * If the tag is 1, the memory pointed to by `ptr` is owned
    //   and the lifetime parameter is irrelevant.
    //   `ptr` and `len` are the components of a `Box<str>`.
    //
    // * If the tag is 0, the memory is borrowed.
    //   `ptr` and `len` are the components of a `&'a str`.

    // FIXME: https://github.com/rust-lang/rust/issues/27730 use NonZero or Shared
    ptr: *const u8,
    tagged_len: usize,
    phantom: PhantomData<&'a str>,
}

impl<'a> From<&'a str> for CompactCowStr<'a> {
    #[inline]
    fn from(s: &'a str) -> Self {
        let len = s.len();
        assert!(len <= MAX_LEN);
        CompactCowStr {
            ptr: s.as_ptr(),
            tagged_len: len,
            phantom: PhantomData,
        }
    }
}

impl<'a> From<Box<str>> for CompactCowStr<'a> {
    #[inline]
    fn from(s: Box<str>) -> Self {
        let ptr = s.as_ptr();
        let len = s.len();
        assert!(len <= MAX_LEN);
        mem::forget(s);
        CompactCowStr {
            ptr: ptr,
            tagged_len: len | OWNED_TAG,
            phantom: PhantomData,
        }
    }
}

impl<'a> CompactCowStr<'a> {
    /// Whether this string refers to borrowed memory
    /// (as opposed to owned, which would be freed when `CompactCowStr` goes out of scope).
    #[inline]
    pub fn is_borrowed(&self) -> bool {
        (self.tagged_len & OWNED_TAG) == 0
    }

    /// The length of this string
    #[inline]
    pub fn len(&self) -> usize {
        self.tagged_len & !OWNED_TAG
    }

    // Intentionally private since it is easy to use incorrectly.
    #[inline]
    fn as_raw_str(&self) -> *const str {
        unsafe {
            str::from_utf8_unchecked(slice::from_raw_parts(self.ptr, self.len()))
        }
    }

    /// If this string is borrowed, return a slice with the original lifetime,
    /// not borrowing `self`.
    ///
    /// (`Deref` is implemented unconditionally, but returns a slice with a shorter lifetime.)
    #[inline]
    pub fn as_str(&self) -> Option<&'a str> {
        if self.is_borrowed() {
            Some(unsafe { &*self.as_raw_str() })
        } else {
            None
        }
    }

    /// Convert into `String`, re-using the memory allocation if it was already owned.
    #[inline]
    pub fn into_owned(self) -> String {
        unsafe {
            let raw = self.as_raw_str();
            let is_borrowed = self.is_borrowed();
            mem::forget(self);
            if is_borrowed {
                String::from(&*raw)
            } else {
                Box::from_raw(raw as *mut str).into_string()
            }
        }
    }
}

impl<'a> Clone for CompactCowStr<'a> {
    #[inline]
    fn clone(&self) -> Self {
        if self.is_borrowed() {
            CompactCowStr { ..*self }
        } else {
            Self::from(String::from(&**self).into_boxed_str())
        }
    }
}

impl<'a> Drop for CompactCowStr<'a> {
    #[inline]
    fn drop(&mut self) {
        if !self.is_borrowed() {
            unsafe {
                Box::from_raw(self.as_raw_str() as *mut str);
            }
        }
    }
}

impl<'a> Deref for CompactCowStr<'a> {
    type Target = str;

    #[inline]
    fn deref(&self) -> &str {
        unsafe {
            &*self.as_raw_str()
        }
    }
}

impl<'a> From<CompactCowStr<'a>> for Cow<'a, str> {
    #[inline]
    fn from(cow: CompactCowStr<'a>) -> Self {
        unsafe {
            let raw = cow.as_raw_str();
            let is_borrowed = cow.is_borrowed();
            mem::forget(cow);
            if is_borrowed {
                Cow::Borrowed(&*raw)
            } else {
                Cow::Owned(Box::from_raw(raw as *mut str).into_string())
            }
        }
    }
}

impl<'a> From<String> for CompactCowStr<'a> {
    #[inline]
    fn from(s: String) -> Self {
        Self::from(s.into_boxed_str())
    }
}

impl<'a> From<Cow<'a, str>> for CompactCowStr<'a> {
    #[inline]
    fn from(s: Cow<'a, str>) -> Self {
        match s {
            Cow::Borrowed(s) => Self::from(s),
            Cow::Owned(s) => Self::from(s),
        }
    }
}

impl<'a> AsRef<str> for CompactCowStr<'a> {
    #[inline]
    fn as_ref(&self) -> &str {
        self
    }
}

impl<'a> Borrow<str> for CompactCowStr<'a> {
    #[inline]
    fn borrow(&self) -> &str {
        self
    }
}

impl<'a> Default for CompactCowStr<'a> {
    #[inline]
    fn default() -> Self {
        Self::from("")
    }
}

impl<'a> hash::Hash for CompactCowStr<'a> {
    #[inline]
    fn hash<H: hash::Hasher>(&self, hasher: &mut H) {
        str::hash(self, hasher)
    }
}

impl<'a, T: AsRef<str>> PartialEq<T> for CompactCowStr<'a> {
    #[inline]
    fn eq(&self, other: &T) -> bool {
        str::eq(self, other.as_ref())
    }
}

impl<'a, T: AsRef<str>> PartialOrd<T> for CompactCowStr<'a> {
    #[inline]
    fn partial_cmp(&self, other: &T) -> Option<cmp::Ordering> {
        str::partial_cmp(self, other.as_ref())
    }
}

impl<'a> Eq for CompactCowStr<'a> {}

impl<'a> Ord for CompactCowStr<'a> {
    #[inline]
    fn cmp(&self, other: &Self) -> cmp::Ordering {
        str::cmp(self, other)
    }
}

impl<'a> fmt::Display for CompactCowStr<'a> {
    #[inline]
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        str::fmt(self, formatter)
    }
}

impl<'a> fmt::Debug for CompactCowStr<'a> {
    #[inline]
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        str::fmt(self, formatter)
    }
}
