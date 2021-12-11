//! A byte-offset based string table.
//! Commonly used in ELF binaries, Unix archives, and even PE binaries.

use core::ops::Index;
use core::slice;
use core::str;
use core::fmt;
use scroll::{ctx, Pread};
if_alloc! {
    use crate::error;
    use alloc::vec::Vec;
}

/// A common string table format which is indexed by byte offsets (and not
/// member index). Constructed using [`parse`](#method.parse)
/// with your choice of delimiter. Please be careful.
pub struct Strtab<'a> {
    bytes: &'a[u8],
    delim: ctx::StrCtx,
}

#[inline(always)]
fn get_str(offset: usize, bytes: &[u8], delim: ctx::StrCtx) -> scroll::Result<&str> {
    bytes.pread_with::<&str>(offset, delim)
}

impl<'a> Strtab<'a> {
    /// Construct a new strtab with `bytes` as the backing string table, using `delim` as the delimiter between entries
    pub fn new (bytes: &'a [u8], delim: u8) -> Self {
        Strtab { delim: ctx::StrCtx::Delimiter(delim), bytes }
    }
    /// Construct a strtab from a `ptr`, and a `size`, using `delim` as the delimiter
    pub unsafe fn from_raw(ptr: *const u8, size: usize, delim: u8) -> Strtab<'a> {
        Strtab { delim: ctx::StrCtx::Delimiter(delim), bytes: slice::from_raw_parts(ptr, size) }
    }
    #[cfg(feature = "alloc")]
    /// Parses a strtab from `bytes` at `offset` with `len` size as the backing string table, using `delim` as the delimiter
    pub fn parse(bytes: &'a [u8], offset: usize, len: usize, delim: u8) -> error::Result<Strtab<'a>> {
        let (end, overflow) = offset.overflowing_add(len);
        if overflow || end > bytes.len () {
            return Err(error::Error::Malformed(format!("Strtable size ({}) + offset ({}) is out of bounds for {} #bytes. Overflowed: {}", len, offset, bytes.len(), overflow)));
        }
        Ok(Strtab { bytes: &bytes[offset..end], delim: ctx::StrCtx::Delimiter(delim) })
    }
    #[cfg(feature = "alloc")]
    /// Converts the string table to a vector, with the original `delim` used to separate the strings
    pub fn to_vec(&self) -> error::Result<Vec<&'a str>> {
        let len = self.bytes.len();
        let mut strings = Vec::with_capacity(len);
        let mut i = 0;
        while i < len {
            let string = self.get(i).unwrap()?;
            i = i + string.len() + 1;
            strings.push(string);
        }
        Ok(strings)
    }
    /// Safely parses and gets a str reference from the backing bytes starting at byte `offset`.
    /// If the index is out of bounds, `None` is returned.
    /// Requires `feature = "alloc"`
    #[cfg(feature = "alloc")]
    pub fn get(&self, offset: usize) -> Option<error::Result<&'a str>> {
        if offset >= self.bytes.len() {
            None
        } else {
            Some(get_str(offset, self.bytes, self.delim).map_err(core::convert::Into::into))
        }
    }
    /// Gets a str reference from the backing bytes starting at byte `offset`.
    /// If the index is out of bounds, `None` is returned. Panics if bytes are invalid UTF-8.
    pub fn get_unsafe(&self, offset: usize) -> Option<&'a str> {
        if offset >= self.bytes.len() {
            None
        } else {
            Some(get_str(offset, self.bytes, self.delim).unwrap())
        }
    }
}

impl<'a> fmt::Debug for Strtab<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Strtab")
            .field("delim", &self.delim)
            .field("bytes", &str::from_utf8(self.bytes))
            .finish()
    }
}

impl<'a> Default for Strtab<'a> {
    fn default() -> Strtab<'a> {
        Strtab { bytes: &[], delim: ctx::StrCtx::default() }
    }
}

impl<'a> Index<usize> for Strtab<'a> {
    type Output = str;
    /// Gets str reference at starting at byte `offset`.
    /// **NB**: this will panic if the underlying bytes are not valid utf8, or the offset is invalid
    #[inline(always)]
    fn index(&self, offset: usize) -> &Self::Output {
        // This can't delegate to get() because get() requires #[cfg(features = "alloc")]
        // It's also slightly less useful than get() because the lifetime -- specified by the Index
        // trait -- matches &self, even though we could return &'a instead
        get_str(offset, self.bytes, self.delim).unwrap()
    }
}

#[test]
fn as_vec_no_final_null() {
    let bytes = b"\0printf\0memmove\0busta";
    let strtab = unsafe { Strtab::from_raw(bytes.as_ptr(), bytes.len(), 0x0) };
    let vec = strtab.to_vec().unwrap();
    assert_eq!(vec.len(), 4);
    assert_eq!(vec, vec!["", "printf", "memmove", "busta"]);
}

#[test]
fn as_vec_no_first_null_no_final_null() {
    let bytes = b"printf\0memmove\0busta";
    let strtab = unsafe { Strtab::from_raw(bytes.as_ptr(), bytes.len(), 0x0) };
    let vec = strtab.to_vec().unwrap();
    assert_eq!(vec.len(), 3);
    assert_eq!(vec, vec!["printf", "memmove", "busta"]);
}

#[test]
fn to_vec_final_null() {
    let bytes = b"\0printf\0memmove\0busta\0";
    let strtab = unsafe { Strtab::from_raw(bytes.as_ptr(), bytes.len(), 0x0) };
    let vec = strtab.to_vec().unwrap();
    assert_eq!(vec.len(), 4);
    assert_eq!(vec, vec!["", "printf", "memmove", "busta"]);
}

#[test]
fn to_vec_newline_delim() {
    let bytes = b"\nprintf\nmemmove\nbusta\n";
    let strtab = unsafe { Strtab::from_raw(bytes.as_ptr(), bytes.len(), b'\n') };
    let vec = strtab.to_vec().unwrap();
    assert_eq!(vec.len(), 4);
    assert_eq!(vec, vec!["", "printf", "memmove", "busta"]);
}
