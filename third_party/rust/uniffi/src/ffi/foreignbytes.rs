/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::convert::TryInto;

/// Support for reading a slice of foreign-language-allocated bytes over the FFI.
///
/// Foreign language code can pass a slice of bytes by providing a data pointer
/// and length, and this struct provides a convenient wrapper for working with
/// that pair. Naturally, this can be tremendously unsafe! So here are the details:
///
///   * The foreign language code must ensure the provided buffer stays alive
///     and unchanged for the duration of the call to which the `ForeignBytes`
///     struct was provided.
///
/// To work with the bytes in Rust code, use `as_slice()` to view the data
/// as a `&[u8]`.
///
/// Implementation note: all the fields of this struct are private and it has no
/// constructors, so consuming crates cant create instances of it. If you've
/// got a `ForeignBytes`, then you received it over the FFI and are assuming that
/// the foreign language code is upholding the above invariants.
///
/// This struct is based on `ByteBuffer` from the `ffi-support` crate, but modified
/// to give a read-only view of externally-provided bytes.
#[repr(C)]
pub struct ForeignBytes {
    /// The length of the pointed-to data.
    /// We use an `i32` for compatibility with JNA.
    len: i32,
    /// The pointer to the foreign-owned bytes.
    data: *const u8,
}

impl ForeignBytes {
    /// Creates a `ForeignBytes` from its constituent fields.
    ///
    /// This is intended mainly as an internal convenience function and should not
    /// be used outside of this module.
    ///
    /// # Safety
    ///
    /// You must ensure that the raw parts uphold the documented invariants of this class.
    pub unsafe fn from_raw_parts(data: *const u8, len: i32) -> Self {
        Self { len, data }
    }

    /// View the foreign bytes as a `&[u8]`.
    ///
    /// # Panics
    ///
    /// Panics if the provided struct has a null pointer but non-zero length.
    /// Panics if the provided length is negative.
    pub fn as_slice(&self) -> &[u8] {
        if self.data.is_null() {
            assert!(self.len == 0, "null ForeignBytes had non-zero length");
            &[]
        } else {
            unsafe { std::slice::from_raw_parts(self.data, self.len()) }
        }
    }

    /// Get the length of this slice of bytes.
    ///
    /// # Panics
    ///
    /// Panics if the provided length is negative.
    pub fn len(&self) -> usize {
        self.len
            .try_into()
            .expect("bytes length negative or overflowed")
    }

    /// Returns true if the length of this slice of bytes is 0.
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_foreignbytes_access() {
        let v = vec![1u8, 2, 3];
        let fbuf = unsafe { ForeignBytes::from_raw_parts(v.as_ptr(), 3) };
        assert_eq!(fbuf.len(), 3);
        assert_eq!(fbuf.as_slice(), &[1u8, 2, 3]);
    }

    #[test]
    fn test_foreignbytes_empty() {
        let v = Vec::<u8>::new();
        let fbuf = unsafe { ForeignBytes::from_raw_parts(v.as_ptr(), 0) };
        assert_eq!(fbuf.len(), 0);
        assert_eq!(fbuf.as_slice(), &[0u8; 0]);
    }

    #[test]
    fn test_foreignbytes_null_means_empty() {
        let fbuf = unsafe { ForeignBytes::from_raw_parts(std::ptr::null_mut(), 0) };
        assert_eq!(fbuf.as_slice(), &[0u8; 0]);
    }

    #[test]
    #[should_panic]
    fn test_foreignbytes_null_must_have_zero_length() {
        let fbuf = unsafe { ForeignBytes::from_raw_parts(std::ptr::null_mut(), 12) };
        fbuf.as_slice();
    }

    #[test]
    #[should_panic]
    fn test_foreignbytes_provided_len_must_be_non_negative() {
        let v = vec![0u8, 1, 2];
        let fbuf = unsafe { ForeignBytes::from_raw_parts(v.as_ptr(), -1) };
        fbuf.as_slice();
    }
}
