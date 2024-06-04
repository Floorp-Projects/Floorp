/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::ffi::{rust_call, ForeignBytes, RustCallStatus};

/// Support for passing an allocated-by-Rust buffer of bytes over the FFI.
///
/// We can pass a `Vec<u8>` to foreign language code by decomposing it into
/// its raw parts (buffer pointer, length, and capacity) and passing those
/// around as a struct. Naturally, this can be tremendously unsafe! So here
/// are the details:
///
///   * `RustBuffer` structs must only ever be constructed from a `Vec<u8>`,
///     either explicitly via `RustBuffer::from_vec` or indirectly by calling
///     one of the `RustBuffer::new*` constructors.
///
///   * `RustBuffer` structs do not implement `Drop`, since they are intended
///     to be passed to foreign-language code outside of the control of Rust's
///     ownership system. To avoid memory leaks they *must* passed back into
///     Rust and either explicitly destroyed using `RustBuffer::destroy`, or
///     converted back to a `Vec<u8>` using `RustBuffer::destroy_into_vec`
///     (which will then be dropped via Rust's usual ownership-tracking system).
///
/// Foreign-language code should not construct `RustBuffer` structs other than
/// by receiving them from a call into the Rust code, and should not modify them
/// apart from the following safe operations:
///
///   * Writing bytes into the buffer pointed to by `data`, without writing
///     beyond the indicated `capacity`.
///
///   * Adjusting the `len` property to indicate the amount of data written,
///     while ensuring that 0 <= `len` <= `capacity`.
///
///   * As a special case, constructing a `RustBuffer` with zero capacity, zero
///     length, and a null `data` pointer to indicate an empty buffer.
///
/// In particular, it is not safe for foreign-language code to construct a `RustBuffer`
/// that points to its own allocated memory; use the `ForeignBytes` struct to
/// pass a view of foreign-owned memory in to Rust code.
///
/// Implementation note: all the fields of this struct are private, so you can't
/// manually construct instances that don't come from a `Vec<u8>`. If you've got
/// a `RustBuffer` then it either came from a public constructor (all of which
/// are safe) or it came from foreign-language code (which should have in turn
/// received it by calling some Rust function, and should be respecting the
/// invariants listed above).
///
/// This struct is based on `ByteBuffer` from the `ffi-support` crate, but modified
/// to retain unallocated capacity rather than truncating to the occupied length.
#[repr(C)]
#[derive(Debug)]
pub struct RustBuffer {
    /// The allocated capacity of the underlying `Vec<u8>`.
    /// In Rust this is a `usize`, but we use an `u64` to keep the foreign binding code simple.
    capacity: u64,
    /// The occupied length of the underlying `Vec<u8>`.
    /// In Rust this is a `usize`, but we use an `u64` to keep the foreign binding code simple.
    len: u64,
    /// The pointer to the allocated buffer of the `Vec<u8>`.
    data: *mut u8,
}

// Mark `RustBuffer` as safe to send between threads, despite the `u8` pointer.  The only mutable
// use of that pointer is in `destroy_into_vec()` which requires a &mut on the `RustBuffer`.  This
// is required to send `RustBuffer` inside a `RustFuture`
unsafe impl Send for RustBuffer {}

impl RustBuffer {
    /// Creates an empty `RustBuffer`.
    ///
    /// The buffer will not allocate.
    /// The resulting vector will not be automatically dropped; you must
    /// arrange to call `destroy` or `destroy_into_vec` when finished with it.
    pub fn new() -> Self {
        Self::from_vec(Vec::new())
    }

    /// Creates a `RustBuffer` from its constituent fields.
    ///
    /// This is intended mainly as an internal convenience function and should not
    /// be used outside of this module.
    ///
    /// # Safety
    ///
    /// You must ensure that the raw parts uphold the documented invariants of this class.
    pub unsafe fn from_raw_parts(data: *mut u8, len: u64, capacity: u64) -> Self {
        Self {
            capacity,
            len,
            data,
        }
    }

    /// Get the current length of the buffer, as a `usize`.
    ///
    /// This is mostly a helper function to convert the `i32` length field
    /// into a `usize`, which is what Rust code usually expects.
    ///
    /// # Panics
    ///
    /// Panics if called on an invalid struct obtained from foreign-language code,
    /// in which the `len` field is negative.
    pub fn len(&self) -> usize {
        self.len
            .try_into()
            .expect("buffer length negative or overflowed")
    }

    /// Get a pointer to the data
    pub fn data_pointer(&self) -> *const u8 {
        self.data
    }

    /// Returns true if the length of the buffer is 0.
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    /// Creates a `RustBuffer` zero-filed to the requested size.
    ///
    /// The resulting vector will not be automatically dropped; you must
    /// arrange to call `destroy` or `destroy_into_vec` when finished with it.
    ///
    /// # Panics
    ///
    /// Panics if the requested size is too large to fit in an `i32`, and
    /// hence would risk incompatibility with some foreign-language code.
    pub fn new_with_size(size: u64) -> Self {
        Self::from_vec(vec![0u8; size as usize])
    }

    /// Consumes a `Vec<u8>` and returns its raw parts as a `RustBuffer`.
    ///
    /// The resulting vector will not be automatically dropped; you must
    /// arrange to call `destroy` or `destroy_into_vec` when finished with it.
    ///
    /// # Panics
    ///
    /// Panics if the vector's length or capacity are too large to fit in an `i32`,
    /// and hence would risk incompatibility with some foreign-language code.
    pub fn from_vec(v: Vec<u8>) -> Self {
        let capacity = u64::try_from(v.capacity()).expect("buffer capacity cannot fit into a u64.");
        let len = u64::try_from(v.len()).expect("buffer length cannot fit into a u64.");
        let mut v = std::mem::ManuallyDrop::new(v);
        unsafe { Self::from_raw_parts(v.as_mut_ptr(), len, capacity) }
    }

    /// Converts this `RustBuffer` back into an owned `Vec<u8>`.
    ///
    /// This restores ownership of the underlying buffer to Rust, meaning it will
    /// be dropped when the `Vec<u8>` is dropped. The `RustBuffer` *must* have been
    /// previously obtained from a valid `Vec<u8>` owned by this Rust code.
    ///
    /// # Panics
    ///
    /// Panics if called on an invalid struct obtained from foreign-language code,
    /// which does not respect the invairiants on `len` and `capacity`.
    pub fn destroy_into_vec(self) -> Vec<u8> {
        // Rust will never give us a null `data` pointer for a `Vec`, but
        // foreign-language code can use it to cheaply pass an empty buffer.
        if self.data.is_null() {
            assert!(self.capacity == 0, "null RustBuffer had non-zero capacity");
            assert!(self.len == 0, "null RustBuffer had non-zero length");
            vec![]
        } else {
            let capacity: usize = self
                .capacity
                .try_into()
                .expect("buffer capacity negative or overflowed");
            let len: usize = self
                .len
                .try_into()
                .expect("buffer length negative or overflowed");
            assert!(len <= capacity, "RustBuffer length exceeds capacity");
            unsafe { Vec::from_raw_parts(self.data, len, capacity) }
        }
    }

    /// Reclaim memory stored in this `RustBuffer`.
    ///
    /// # Panics
    ///
    /// Panics if called on an invalid struct obtained from foreign-language code,
    /// which does not respect the invairiants on `len` and `capacity`.
    pub fn destroy(self) {
        drop(self.destroy_into_vec());
    }
}

impl Default for RustBuffer {
    fn default() -> Self {
        Self::new()
    }
}

// Functions for the RustBuffer functionality.
//
// The scaffolding code re-exports these functions, prefixed with the component name and UDL hash
// This creates a separate set of functions for each UniFFIed component, which is needed in the
// case where we create multiple dylib artifacts since each dylib will have its own allocator.

/// This helper allocates a new byte buffer owned by the Rust code, and returns it
/// to the foreign-language code as a `RustBuffer` struct. Callers must eventually
/// free the resulting buffer, either by explicitly calling [`uniffi_rustbuffer_free`] defined
/// below, or by passing ownership of the buffer back into Rust code.
pub fn uniffi_rustbuffer_alloc(size: u64, call_status: &mut RustCallStatus) -> RustBuffer {
    rust_call(call_status, || Ok(RustBuffer::new_with_size(size)))
}

/// This helper copies bytes owned by the foreign-language code into a new byte buffer owned
/// by the Rust code, and returns it as a `RustBuffer` struct. Callers must eventually
/// free the resulting buffer, either by explicitly calling the destructor defined below,
/// or by passing ownership of the buffer back into Rust code.
///
/// # Safety
/// This function will dereference a provided pointer in order to copy bytes from it, so
/// make sure the `ForeignBytes` struct contains a valid pointer and length.
pub fn uniffi_rustbuffer_from_bytes(
    bytes: ForeignBytes,
    call_status: &mut RustCallStatus,
) -> RustBuffer {
    rust_call(call_status, || {
        let bytes = bytes.as_slice();
        Ok(RustBuffer::from_vec(bytes.to_vec()))
    })
}

/// Free a byte buffer that had previously been passed to the foreign language code.
///
/// # Safety
/// The argument *must* be a uniquely-owned `RustBuffer` previously obtained from a call
/// into the Rust code that returned a buffer, or you'll risk freeing unowned memory or
/// corrupting the allocator state.
pub fn uniffi_rustbuffer_free(buf: RustBuffer, call_status: &mut RustCallStatus) {
    rust_call(call_status, || {
        RustBuffer::destroy(buf);
        Ok(())
    })
}

/// Reserve additional capacity in a byte buffer that had previously been passed to the
/// foreign language code.
///
/// The first argument *must* be a uniquely-owned `RustBuffer` previously
/// obtained from a call into the Rust code that returned a buffer. Its underlying data pointer
/// will be reallocated if necessary and returned in a new `RustBuffer` struct.
///
/// The second argument must be the minimum number of *additional* bytes to reserve
/// capacity for in the buffer; it is likely to reserve additional capacity in practice
/// due to amortized growth strategy of Rust vectors.
///
/// # Safety
/// The first argument *must* be a uniquely-owned `RustBuffer` previously obtained from a call
/// into the Rust code that returned a buffer, or you'll risk freeing unowned memory or
/// corrupting the allocator state.
pub fn uniffi_rustbuffer_reserve(
    buf: RustBuffer,
    additional: u64,
    call_status: &mut RustCallStatus,
) -> RustBuffer {
    rust_call(call_status, || {
        let additional: usize = additional
            .try_into()
            .expect("additional buffer length negative or overflowed");
        let mut v = buf.destroy_into_vec();
        v.reserve(additional);
        Ok(RustBuffer::from_vec(v))
    })
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn test_rustbuffer_from_vec() {
        let rbuf = RustBuffer::from_vec(vec![1u8, 2, 3]);
        assert_eq!(rbuf.len(), 3);
        assert_eq!(rbuf.destroy_into_vec(), vec![1u8, 2, 3]);
    }

    #[test]
    fn test_rustbuffer_empty() {
        let rbuf = RustBuffer::new();
        assert_eq!(rbuf.len(), 0);
        // Rust will never give us a null pointer, even for an empty buffer.
        assert!(!rbuf.data.is_null());
        assert_eq!(rbuf.destroy_into_vec(), Vec::<u8>::new());
    }

    #[test]
    fn test_rustbuffer_new_with_size() {
        let rbuf = RustBuffer::new_with_size(5);
        assert_eq!(rbuf.destroy_into_vec().as_slice(), &[0u8, 0, 0, 0, 0]);

        let rbuf = RustBuffer::new_with_size(0);
        assert!(!rbuf.data.is_null());
        assert_eq!(rbuf.destroy_into_vec().as_slice(), &[0u8; 0]);
    }

    #[test]
    fn test_rustbuffer_null_means_empty() {
        // This is how foreign-language code might cheaply indicate an empty buffer.
        let rbuf = unsafe { RustBuffer::from_raw_parts(std::ptr::null_mut(), 0, 0) };
        assert_eq!(rbuf.destroy_into_vec().as_slice(), &[0u8; 0]);
    }

    #[test]
    #[should_panic]
    fn test_rustbuffer_null_must_have_no_capacity() {
        // We guard against foreign-language code providing this kind of invalid struct.
        let rbuf = unsafe { RustBuffer::from_raw_parts(std::ptr::null_mut(), 0, 1) };
        rbuf.destroy_into_vec();
    }
    #[test]
    #[should_panic]
    fn test_rustbuffer_null_must_have_zero_length() {
        // We guard against foreign-language code providing this kind of invalid struct.
        let rbuf = unsafe { RustBuffer::from_raw_parts(std::ptr::null_mut(), 12, 0) };
        rbuf.destroy_into_vec();
    }

    #[test]
    #[should_panic]
    fn test_rustbuffer_provided_len_must_not_exceed_capacity() {
        // We guard against foreign-language code providing this kind of invalid struct.
        let mut v = vec![0u8, 1, 2];
        let rbuf = unsafe { RustBuffer::from_raw_parts(v.as_mut_ptr(), 3, 2) };
        rbuf.destroy_into_vec();
    }
}
