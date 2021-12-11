// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! ByteBuffer is a struct that represents an array of bytes to be sent over the FFI boundaries.
//!
//! This is a copy of the same struct from [ffi-support],
//! with the difference that the length is encoded as a `i32`.
//!
//! [ffi-support]: https://docs.rs/ffi-support/0.4.0/src/ffi_support/lib.rs.html#390-393

/// ByteBuffer is a struct that represents an array of bytes to be sent over the FFI boundaries.
/// There are several cases when you might want to use this, but the primary one for us
/// is for returning protobuf-encoded data to Swift and Java. The type is currently rather
/// limited (implementing almost no functionality), however in the future it may be
/// more expanded.
///
/// ## Caveats
///
/// Note that the order of the fields is `len` (an i32) then `data` (a `*mut u8`), getting
/// this wrong on the other side of the FFI will cause memory corruption and crashes.
/// `i32` is used for the length instead of `u64` and `usize` because JNA has interop
/// issues with both these types.
///
/// ByteBuffer does not implement Drop. This is intentional. Memory passed into it will
/// be leaked if it is not explicitly destroyed by calling [`ByteBuffer::destroy`]. This
/// is because in the future, we may allow it's use for passing data into Rust code.
/// ByteBuffer assuming ownership of the data would make this a problem.
///
/// ## Layout/fields
///
/// This struct's field are not `pub` (mostly so that we can soundly implement `Send`, but also so
/// that we can verify Rust users are constructing them appropriately), the fields, their types, and
/// their order are *very much* a part of the public API of this type. Consumers on the other side
/// of the FFI will need to know its layout.
///
/// If this were a C struct, it would look like
///
/// ```c,no_run
/// struct ByteBuffer {
///     int64_t len;
///     uint8_t *data; // note: nullable
/// };
/// ```
///
/// In Rust, there are two fields, in this order: `len: i32`, and `data: *mut u8`.
///
/// ### Description of fields
///
/// `data` is a pointer to an array of `len` bytes. Not that data can be a null pointer and therefore
/// should be checked.
///
/// The bytes array is allocated on the heap and must be freed on it as well. Critically, if there
/// are multiple rust packages using being used in the same application, it *must be freed on the
/// same heap that allocated it*, or you will corrupt both heaps.
#[repr(C)]
pub struct ByteBuffer {
    len: i32,
    data: *mut u8,
}

impl From<Vec<u8>> for ByteBuffer {
    #[inline]
    fn from(bytes: Vec<u8>) -> Self {
        Self::from_vec(bytes)
    }
}

impl ByteBuffer {
    /// Creates a `ByteBuffer` of the requested size, zero-filled.
    ///
    /// The contents of the vector will not be dropped. Instead, `destroy` must
    /// be called later to reclaim this memory or it will be leaked.
    ///
    /// ## Caveats
    ///
    /// This will panic if the buffer length (`usize`) cannot fit into a `i32`.
    #[inline]
    pub fn new_with_size(size: usize) -> Self {
        let mut buf = vec![];
        buf.reserve_exact(size);
        buf.resize(size, 0);
        ByteBuffer::from_vec(buf)
    }

    /// Creates a `ByteBuffer` instance from a `Vec` instance.
    ///
    /// The contents of the vector will not be dropped. Instead, `destroy` must
    /// be called later to reclaim this memory or it will be leaked.
    ///
    /// ## Caveats
    ///
    /// This will panic if the buffer length (`usize`) cannot fit into a `i32`.
    #[inline]
    pub fn from_vec(bytes: Vec<u8>) -> Self {
        use std::convert::TryFrom;
        let mut buf = bytes.into_boxed_slice();
        let data = buf.as_mut_ptr();
        let len = i32::try_from(buf.len()).expect("buffer length cannot fit into a i32.");
        std::mem::forget(buf);
        Self { len, data }
    }

    /// Convert this `ByteBuffer` into a Vec<u8>. This is the only way
    /// to access the data from inside the buffer.
    #[inline]
    pub fn into_vec(self) -> Vec<u8> {
        if self.data.is_null() {
            vec![]
        } else {
            // This is correct because we convert to a Box<[u8]> first, which is
            // a design constraint of RawVec.
            unsafe { Vec::from_raw_parts(self.data, self.len as usize, self.len as usize) }
        }
    }

    /// Reclaim memory stored in this ByteBuffer.
    ///
    /// ## Caveats
    ///
    /// This is safe so long as the buffer is empty, or the data was allocated
    /// by Rust code, e.g. this is a ByteBuffer created by
    /// `ByteBuffer::from_vec` or `Default::default`.
    ///
    /// If the ByteBuffer were passed into Rust (which you shouldn't do, since
    /// theres no way to see the data in Rust currently), then calling `destroy`
    /// is fundamentally broken.
    #[inline]
    pub fn destroy(self) {
        drop(self.into_vec())
    }
}

impl Default for ByteBuffer {
    #[inline]
    fn default() -> Self {
        Self {
            len: 0,
            data: std::ptr::null_mut(),
        }
    }
}
