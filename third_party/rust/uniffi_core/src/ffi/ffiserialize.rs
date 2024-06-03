/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{Handle, RustBuffer, RustCallStatus, RustCallStatusCode};
use std::{mem::MaybeUninit, ptr::NonNull};

/// FFIBuffer element
///
/// This is the union of all possible primitive FFI types.
/// Composite FFI types like `RustBuffer` and `RustCallStatus` are stored using multiple elements.
#[repr(C)]
#[derive(Clone, Copy)]
pub union FfiBufferElement {
    pub u8: u8,
    pub i8: i8,
    pub u16: u16,
    pub i16: i16,
    pub u32: u32,
    pub i32: i32,
    pub u64: u64,
    pub i64: i64,
    pub float: std::ffi::c_float,
    pub double: std::ffi::c_double,
    pub ptr: *const std::ffi::c_void,
}

impl Default for FfiBufferElement {
    fn default() -> Self {
        Self { u64: 0 }
    }
}

/// Serialize a FFI value to a buffer
///
/// This trait allows FFI types to be read from/written to FFIBufferElement slices.
/// It's similar, to the [crate::Lift::read] and [crate::Lower::write] methods, but implemented on the FFI types rather than Rust types.
/// It's useful to compare the two:
///
/// - [crate::Lift] and [crate::Lower] are implemented on Rust types like String and user-defined records.
/// - [FfiSerialize] is implemented on the FFI types like RustBuffer, RustCallStatus, and vtable structs.
/// - All 3 traits are implemented for simple cases where the FFI type and Rust type are the same, for example numeric types.
/// - [FfiSerialize] uses FFIBuffer elements rather than u8 elements.  Using a union eliminates the need to cast values and creates better alignment.
/// - [FfiSerialize] uses a constant size to store each type.
///
/// [FfiSerialize] is used to generate alternate forms of the scaffolding functions that simplify work needed to implement the bindings on the other side.
/// This is currently only used in the gecko-js bindings for Firefox, but could maybe be useful for other external bindings or even some of the builtin bindings like Python/Kotlin.
///
/// The FFI-buffer version of the scaffolding functions:
///   - Input two pointers to ffi buffers, one to read arguments from and one to write the return value to.
///   - Rather than inputting an out pointer for `RustCallStatus` it's written to the return buffer after the normal return value.
///
pub trait FfiSerialize: Sized {
    /// Number of elements required to store this FFI type
    const SIZE: usize;

    /// Get a value from a ffi buffer
    ///
    /// Note: `buf` should be thought of as `&[FFIBufferElement; Self::SIZE]`, but it can't be spelled out that way
    /// since Rust doesn't support that usage of const generics yet.
    fn get(buf: &[FfiBufferElement]) -> Self;

    /// Put a value to a ffi buffer
    ///
    /// Note: `buf` should be thought of as `&[FFIBufferElement; Self::SIZE]`, but it can't be spelled out that way
    /// since Rust doesn't support that usage of const generics yet.
    fn put(buf: &mut [FfiBufferElement], value: Self);

    /// Read a value from a ffi buffer ref and advance it
    ///
    /// buf must have a length of at least `Self::Size`
    fn read(buf: &mut &[FfiBufferElement]) -> Self {
        let value = Self::get(buf);
        *buf = &buf[Self::SIZE..];
        value
    }

    /// Write a value to a ffi buffer ref and advance it
    ///
    /// buf must have a length of at least `Self::Size`
    fn write(buf: &mut &mut [FfiBufferElement], value: Self) {
        Self::put(buf, value);
        // Lifetime dance taken from `bytes::BufMut`
        let (_, new_buf) = core::mem::take(buf).split_at_mut(Self::SIZE);
        *buf = new_buf;
    }
}

/// Get the FFI buffer size for list of types
#[macro_export]
macro_rules! ffi_buffer_size {
    ($($T:ty),* $(,)?) => {
        (
            0
            $(
                + <$T as $crate::FfiSerialize>::SIZE
            )*
        )
    }
}

macro_rules! define_ffi_serialize_simple_cases {
    ($(($name: ident, $T:ty)),* $(,)?) => {
        $(
            impl FfiSerialize for $T {
                const SIZE: usize = 1;

                fn get(buf: &[FfiBufferElement]) -> Self {
                    // Safety: the foreign bindings are responsible for sending us the correct data.
                    unsafe { buf[0].$name }
                }

                fn put(buf: &mut[FfiBufferElement], value: Self) {
                    buf[0].$name = value
                }
            }
        )*
    };
}

define_ffi_serialize_simple_cases! {
    (i8, i8),
    (u8, u8),
    (i16, i16),
    (u16, u16),
    (i32, i32),
    (u32, u32),
    (i64, i64),
    (u64, u64),
    (ptr, *const std::ffi::c_void),
}

impl FfiSerialize for f32 {
    const SIZE: usize = 1;

    fn get(buf: &[FfiBufferElement]) -> Self {
        // Safety: the foreign bindings are responsible for sending us the correct data.
        unsafe { buf[0].float as Self }
    }

    fn put(buf: &mut [FfiBufferElement], value: Self) {
        // Use a cast since it's theoretically possible for float to not be f32 on some systems.
        buf[0].float = value as std::ffi::c_float;
    }
}

impl FfiSerialize for f64 {
    const SIZE: usize = 1;

    fn get(buf: &[FfiBufferElement]) -> Self {
        // Safety: the foreign bindings are responsible for sending us the correct data.
        unsafe { buf[0].double as Self }
    }

    fn put(buf: &mut [FfiBufferElement], value: Self) {
        // Use a cast since it's theoretically possible for double to not be f64 on some systems.
        buf[0].double = value as std::ffi::c_double;
    }
}

impl FfiSerialize for bool {
    const SIZE: usize = 1;

    fn get(buf: &[FfiBufferElement]) -> Self {
        // Safety: the foreign bindings are responsible for sending us the correct data.
        unsafe { buf[0].i8 == 1 }
    }

    fn put(buf: &mut [FfiBufferElement], value: Self) {
        buf[0].i8 = if value { 1 } else { 0 }
    }
}

impl FfiSerialize for () {
    const SIZE: usize = 0;

    fn get(_buf: &[FfiBufferElement]) -> Self {}

    fn put(_buf: &mut [FfiBufferElement], _value: Self) {}
}

impl<T> FfiSerialize for NonNull<T> {
    const SIZE: usize = 1;

    fn get(buf: &[FfiBufferElement]) -> Self {
        // Safety: this relies on the foreign code passing us valid pointers
        unsafe { Self::new_unchecked(buf[0].ptr as *mut T) }
    }

    fn put(buf: &mut [FfiBufferElement], value: Self) {
        buf[0].ptr = value.as_ptr() as *const std::ffi::c_void
    }
}

impl FfiSerialize for Handle {
    const SIZE: usize = 1;

    fn get(buf: &[FfiBufferElement]) -> Self {
        unsafe { Handle::from_raw_unchecked(buf[0].u64) }
    }

    fn put(buf: &mut [FfiBufferElement], value: Self) {
        buf[0].u64 = value.as_raw()
    }
}

impl FfiSerialize for RustBuffer {
    const SIZE: usize = 3;

    fn get(buf: &[FfiBufferElement]) -> Self {
        // Safety: the foreign bindings are responsible for sending us the correct data.
        let (capacity, len, data) = unsafe { (buf[0].u64, buf[1].u64, buf[2].ptr as *mut u8) };
        unsafe { crate::RustBuffer::from_raw_parts(data, len, capacity) }
    }

    fn put(buf: &mut [FfiBufferElement], value: Self) {
        buf[0].u64 = value.capacity;
        buf[1].u64 = value.len;
        buf[2].ptr = value.data as *const std::ffi::c_void;
    }
}

impl FfiSerialize for RustCallStatus {
    const SIZE: usize = 4;

    fn get(buf: &[FfiBufferElement]) -> Self {
        // Safety: the foreign bindings are responsible for sending us the correct data.
        let code = unsafe { buf[0].i8 };
        Self {
            code: RustCallStatusCode::try_from(code).unwrap_or(RustCallStatusCode::UnexpectedError),
            error_buf: MaybeUninit::new(RustBuffer::get(&buf[1..])),
        }
    }

    fn put(buf: &mut [FfiBufferElement], value: Self) {
        buf[0].i8 = value.code as i8;
        // Safety: This is okay even if the error buf is not initialized.  It just means we'll be
        // copying the garbage data.
        unsafe { RustBuffer::put(&mut buf[1..], value.error_buf.assume_init()) }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::{Handle, RustBuffer, RustCallStatus, RustCallStatusCode};

    #[test]
    fn test_ffi_buffer_size() {
        assert_eq!(ffi_buffer_size!(u8), 1);
        assert_eq!(ffi_buffer_size!(i8), 1);
        assert_eq!(ffi_buffer_size!(u16), 1);
        assert_eq!(ffi_buffer_size!(i16), 1);
        assert_eq!(ffi_buffer_size!(u32), 1);
        assert_eq!(ffi_buffer_size!(i32), 1);
        assert_eq!(ffi_buffer_size!(u64), 1);
        assert_eq!(ffi_buffer_size!(i64), 1);
        assert_eq!(ffi_buffer_size!(f32), 1);
        assert_eq!(ffi_buffer_size!(f64), 1);
        assert_eq!(ffi_buffer_size!(bool), 1);
        assert_eq!(ffi_buffer_size!(*const std::ffi::c_void), 1);
        assert_eq!(ffi_buffer_size!(RustBuffer), 3);
        assert_eq!(ffi_buffer_size!(RustCallStatus), 4);
        assert_eq!(ffi_buffer_size!(Handle), 1);
        assert_eq!(ffi_buffer_size!(()), 0);

        assert_eq!(ffi_buffer_size!(u8, f32, bool, Handle, (), RustBuffer), 7);
    }

    #[test]
    fn test_ffi_serialize() {
        let mut some_data = vec![1, 2, 3];
        let void_ptr = some_data.as_mut_ptr() as *const std::ffi::c_void;
        let rust_buffer = unsafe { RustBuffer::from_raw_parts(some_data.as_mut_ptr(), 2, 3) };
        let orig_rust_buffer_data = (
            rust_buffer.data_pointer(),
            rust_buffer.len(),
            rust_buffer.capacity(),
        );
        let handle = Handle::from_raw(101).unwrap();
        let rust_call_status = RustCallStatus::new();
        let rust_call_status_error_buf = unsafe { rust_call_status.error_buf.assume_init_ref() };
        let orig_rust_call_status_buffer_data = (
            rust_call_status_error_buf.data_pointer(),
            rust_call_status_error_buf.len(),
            rust_call_status_error_buf.capacity(),
        );
        let mut buf = [FfiBufferElement::default(); 21];
        let mut buf_writer = buf.as_mut_slice();
        <u8 as FfiSerialize>::write(&mut buf_writer, 0);
        <i8 as FfiSerialize>::write(&mut buf_writer, 1);
        <u16 as FfiSerialize>::write(&mut buf_writer, 2);
        <i16 as FfiSerialize>::write(&mut buf_writer, 3);
        <u32 as FfiSerialize>::write(&mut buf_writer, 4);
        <i32 as FfiSerialize>::write(&mut buf_writer, 5);
        <u64 as FfiSerialize>::write(&mut buf_writer, 6);
        <i64 as FfiSerialize>::write(&mut buf_writer, 7);
        <f32 as FfiSerialize>::write(&mut buf_writer, 0.1);
        <f64 as FfiSerialize>::write(&mut buf_writer, 0.2);
        <bool as FfiSerialize>::write(&mut buf_writer, true);
        <*const std::ffi::c_void as FfiSerialize>::write(&mut buf_writer, void_ptr);
        <RustBuffer as FfiSerialize>::write(&mut buf_writer, rust_buffer);
        <RustCallStatus as FfiSerialize>::write(&mut buf_writer, rust_call_status);
        <Handle as FfiSerialize>::write(&mut buf_writer, handle);
        #[allow(unknown_lints)]
        #[allow(clippy::needless_borrows_for_generic_args)]
        <() as FfiSerialize>::write(&mut buf_writer, ());

        let mut buf_reader = buf.as_slice();
        assert_eq!(<u8 as FfiSerialize>::read(&mut buf_reader), 0);
        assert_eq!(<i8 as FfiSerialize>::read(&mut buf_reader), 1);
        assert_eq!(<u16 as FfiSerialize>::read(&mut buf_reader), 2);
        assert_eq!(<i16 as FfiSerialize>::read(&mut buf_reader), 3);
        assert_eq!(<u32 as FfiSerialize>::read(&mut buf_reader), 4);
        assert_eq!(<i32 as FfiSerialize>::read(&mut buf_reader), 5);
        assert_eq!(<u64 as FfiSerialize>::read(&mut buf_reader), 6);
        assert_eq!(<i64 as FfiSerialize>::read(&mut buf_reader), 7);
        assert_eq!(<f32 as FfiSerialize>::read(&mut buf_reader), 0.1);
        assert_eq!(<f64 as FfiSerialize>::read(&mut buf_reader), 0.2);
        assert!(<bool as FfiSerialize>::read(&mut buf_reader));
        assert_eq!(
            <*const std::ffi::c_void as FfiSerialize>::read(&mut buf_reader),
            void_ptr
        );
        let rust_buffer2 = <RustBuffer as FfiSerialize>::read(&mut buf_reader);
        assert_eq!(
            (
                rust_buffer2.data_pointer(),
                rust_buffer2.len(),
                rust_buffer2.capacity()
            ),
            orig_rust_buffer_data,
        );

        let rust_call_status2 = <RustCallStatus as FfiSerialize>::read(&mut buf_reader);
        assert_eq!(rust_call_status2.code, RustCallStatusCode::Success);

        let rust_call_status2_error_buf = unsafe { rust_call_status2.error_buf.assume_init() };
        assert_eq!(
            (
                rust_call_status2_error_buf.data_pointer(),
                rust_call_status2_error_buf.len(),
                rust_call_status2_error_buf.capacity(),
            ),
            orig_rust_call_status_buffer_data
        );
        assert_eq!(<Handle as FfiSerialize>::read(&mut buf_reader), handle);
        // Ensure that `read` with a unit struct doesn't panic.  No need to assert anything, since
        // the return type is ().
        <() as FfiSerialize>::read(&mut buf_reader);
    }
}
