/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// This module contains builtin `FFIConverter` implementations.  These cover:
///   - Simple privitive types: u8, i32, String, Arc<T>, etc
///   - Composite types: Vec<T>, Option<T>, etc.
///   - SystemTime and Duration, which maybe shouldn`t be built-in, but have been historically and
///     we want to continue to support them for now.
///
/// As described in
/// https://mozilla.github.io/uniffi-rs/internals/lifting_and_lowering.html#code-generation-and-the-fficonverter-trait,
/// we use the following system:
///
///   - Each UniFFIed crate defines a unit struct named `UniFfiTag`
///   - We define an `impl FFIConverter<UniFfiTag> for Type` for each type that we want to pass
///     across the FFI.
///   - When generating the code, we use the `<T as ::uniffi::FFIConverter<crate::UniFfiTag>>` impl
///     to lift/lower/serialize types for a crate.
///
/// This crate needs to implement `FFIConverter<UT>` on `UniFfiTag` instances for all UniFFI
/// consumer crates.  To do this, it defines blanket impls like `impl<UT> FFIConverter<UT> for u8`.
/// "UT" means an abitrary `UniFfiTag` type.
use crate::{
    check_remaining, ffi_converter_default_return, ffi_converter_rust_buffer_lift_and_lower,
    lower_into_rust_buffer, metadata, try_lift_from_rust_buffer, FfiConverter, FutureCallback,
    Interface, MetadataBuffer, Result, RustBuffer, RustCallStatus, UnexpectedUniFFICallbackError,
};
use anyhow::bail;
use bytes::buf::{Buf, BufMut};
use paste::paste;
use std::{
    collections::HashMap,
    convert::{Infallible, TryFrom},
    time::{Duration, SystemTime},
};

/// Blanket implementation of `FfiConverter` for numeric primitives.
///
/// Numeric primitives have a straightforward mapping into C-compatible numeric types,
/// sice they are themselves a C-compatible numeric type!
macro_rules! impl_ffi_converter_for_num_primitive {
    ($T:ty, $type_code:expr) => {
        paste! {
            unsafe impl<UT> FfiConverter<UT> for $T {
                ffi_converter_default_return!(UT);

                type FfiType = $T;

                fn lower(obj: $T) -> Self::FfiType {
                    obj
                }

                fn try_lift(v: Self::FfiType) -> Result<$T> {
                    Ok(v)
                }

                fn write(obj: $T, buf: &mut Vec<u8>) {
                    buf.[<put_ $T>](obj);
                }

                fn try_read(buf: &mut &[u8]) -> Result<$T> {
                    check_remaining(buf, std::mem::size_of::<$T>())?;
                    Ok(buf.[<get_ $T>]())
                }

                const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code($type_code);
            }
        }
    };
}

impl_ffi_converter_for_num_primitive!(u8, metadata::codes::TYPE_U8);
impl_ffi_converter_for_num_primitive!(i8, metadata::codes::TYPE_I8);
impl_ffi_converter_for_num_primitive!(u16, metadata::codes::TYPE_U16);
impl_ffi_converter_for_num_primitive!(i16, metadata::codes::TYPE_I16);
impl_ffi_converter_for_num_primitive!(u32, metadata::codes::TYPE_U32);
impl_ffi_converter_for_num_primitive!(i32, metadata::codes::TYPE_I32);
impl_ffi_converter_for_num_primitive!(u64, metadata::codes::TYPE_U64);
impl_ffi_converter_for_num_primitive!(i64, metadata::codes::TYPE_I64);
impl_ffi_converter_for_num_primitive!(f32, metadata::codes::TYPE_F32);
impl_ffi_converter_for_num_primitive!(f64, metadata::codes::TYPE_F64);

/// Support for passing boolean values via the FFI.
///
/// Booleans are passed as an `i8` in order to avoid problems with handling
/// C-compatible boolean values on JVM-based languages.
unsafe impl<UT> FfiConverter<UT> for bool {
    ffi_converter_default_return!(UT);

    type FfiType = i8;

    fn lower(obj: bool) -> Self::FfiType {
        i8::from(obj)
    }

    fn try_lift(v: Self::FfiType) -> Result<bool> {
        Ok(match v {
            0 => false,
            1 => true,
            _ => bail!("unexpected byte for Boolean"),
        })
    }

    fn write(obj: bool, buf: &mut Vec<u8>) {
        buf.put_i8(<Self as FfiConverter<UT>>::lower(obj));
    }

    fn try_read(buf: &mut &[u8]) -> Result<bool> {
        check_remaining(buf, 1)?;
        <Self as FfiConverter<UT>>::try_lift(buf.get_i8())
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_BOOL);
}

/// Support unit-type returns via the FFI.
unsafe impl<UT> FfiConverter<UT> for () {
    // This actually isn't used, but we need to specify something
    type FfiType = ();
    // Returning `()` is FFI-safe, since it gets translated into a void return
    type ReturnType = ();
    // However, we can't use `FutureCallback<()>` since passing `()` as an argument is not
    // FFI-safe.  So we used an arbitrary non-ZST type instead.
    type FutureCallback = FutureCallback<u8>;

    fn try_lift(_: Self::FfiType) -> Result<()> {
        Ok(())
    }

    fn lower(_: ()) -> Self::FfiType {}

    fn write(_: (), _: &mut Vec<u8>) {}

    fn try_read(_: &mut &[u8]) -> Result<()> {
        Ok(())
    }

    fn lower_return(_: ()) -> Result<Self::ReturnType, RustBuffer> {
        Ok(())
    }

    fn invoke_future_callback(
        callback: Self::FutureCallback,
        callback_data: *const (),
        _return_value: (),
        call_status: RustCallStatus,
    ) {
        callback(callback_data, 0, call_status)
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_UNIT);
}

unsafe impl<UT> FfiConverter<UT> for Infallible {
    ffi_converter_default_return!(UT);

    type FfiType = RustBuffer;

    fn try_lift(_: Self::FfiType) -> Result<Infallible> {
        unreachable!()
    }

    fn lower(_: Infallible) -> Self::FfiType {
        unreachable!()
    }

    fn write(_: Infallible, _: &mut Vec<u8>) {
        unreachable!()
    }

    fn try_read(_: &mut &[u8]) -> Result<Infallible> {
        unreachable!()
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::new();
}

/// Support for passing Strings via the FFI.
///
/// Unlike many other implementations of `FfiConverter`, this passes a struct containing
/// a raw pointer rather than copying the data from one side to the other. This is a
/// safety hazard, but turns out to be pretty nice for useability. This struct
/// *must* be a valid `RustBuffer` and it *must* contain valid utf-8 data (in other
/// words, it *must* be a `Vec<u8>` suitable for use as an actual rust `String`).
///
/// When serialized in a buffer, strings are represented as a i32 byte length
/// followed by utf8-encoded bytes. (It's a signed integer because unsigned types are
/// currently experimental in Kotlin).
unsafe impl<UT> FfiConverter<UT> for String {
    ffi_converter_default_return!(UT);

    type FfiType = RustBuffer;

    // This returns a struct with a raw pointer to the underlying bytes, so it's very
    // important that it consume ownership of the String, which is relinquished to the
    // foreign language code (and can be restored by it passing the pointer back).
    fn lower(obj: String) -> Self::FfiType {
        RustBuffer::from_vec(obj.into_bytes())
    }

    // The argument here *must* be a uniquely-owned `RustBuffer` previously obtained
    // from `lower` above, and hence must be the bytes of a valid rust string.
    fn try_lift(v: Self::FfiType) -> Result<String> {
        let v = v.destroy_into_vec();
        // This turns the buffer back into a `String` without copying the data
        // and without re-checking it for validity of the utf8. If the `RustBuffer`
        // came from a valid String then there's no point in re-checking the utf8,
        // and if it didn't then bad things are probably going to happen regardless
        // of whether we check for valid utf8 data or not.
        Ok(unsafe { String::from_utf8_unchecked(v) })
    }

    fn write(obj: String, buf: &mut Vec<u8>) {
        // N.B. `len()` gives us the length in bytes, not in chars or graphemes.
        // TODO: it would be nice not to panic here.
        let len = i32::try_from(obj.len()).unwrap();
        buf.put_i32(len); // We limit strings to u32::MAX bytes
        buf.put(obj.as_bytes());
    }

    fn try_read(buf: &mut &[u8]) -> Result<String> {
        check_remaining(buf, 4)?;
        let len = usize::try_from(buf.get_i32())?;
        check_remaining(buf, len)?;
        // N.B: In the general case `Buf::chunk()` may return partial data.
        // But in the specific case of `<&[u8] as Buf>` it returns the full slice,
        // so there is no risk of having less than `len` bytes available here.
        let bytes = &buf.chunk()[..len];
        let res = String::from_utf8(bytes.to_vec())?;
        buf.advance(len);
        Ok(res)
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_STRING);
}

/// Support for passing timestamp values via the FFI.
///
/// Timestamps values are currently always passed by serializing to a buffer.
///
/// Timestamps are represented on the buffer by an i64 that indicates the
/// direction and the magnitude in seconds of the offset from epoch, and a
/// u32 that indicates the nanosecond portion of the offset magnitude. The
/// nanosecond portion is expected to be between 0 and 999,999,999.
///
/// To build an epoch offset the absolute value of the seconds portion of the
/// offset should be combined with the nanosecond portion. This is because
/// the sign of the seconds portion represents the direction of the offset
/// overall. The sign of the seconds portion can then be used to determine
/// if the total offset should be added to or subtracted from the unix epoch.
unsafe impl<UT> FfiConverter<UT> for SystemTime {
    ffi_converter_rust_buffer_lift_and_lower!(UT);
    ffi_converter_default_return!(UT);

    fn write(obj: SystemTime, buf: &mut Vec<u8>) {
        let mut sign = 1;
        let epoch_offset = obj
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap_or_else(|error| {
                sign = -1;
                error.duration()
            });
        // This panic should never happen as SystemTime typically stores seconds as i64
        let seconds = sign
            * i64::try_from(epoch_offset.as_secs())
                .expect("SystemTime overflow, seconds greater than i64::MAX");

        buf.put_i64(seconds);
        buf.put_u32(epoch_offset.subsec_nanos());
    }

    fn try_read(buf: &mut &[u8]) -> Result<SystemTime> {
        check_remaining(buf, 12)?;
        let seconds = buf.get_i64();
        let nanos = buf.get_u32();
        let epoch_offset = Duration::new(seconds.wrapping_abs() as u64, nanos);

        if seconds >= 0 {
            Ok(SystemTime::UNIX_EPOCH + epoch_offset)
        } else {
            Ok(SystemTime::UNIX_EPOCH - epoch_offset)
        }
    }

    const TYPE_ID_META: MetadataBuffer =
        MetadataBuffer::from_code(metadata::codes::TYPE_SYSTEM_TIME);
}

/// Support for passing duration values via the FFI.
///
/// Duration values are currently always passed by serializing to a buffer.
///
/// Durations are represented on the buffer by a u64 that indicates the
/// magnitude in seconds, and a u32 that indicates the nanosecond portion
/// of the magnitude. The nanosecond portion is expected to be between 0
/// and 999,999,999.
unsafe impl<UT> FfiConverter<UT> for Duration {
    ffi_converter_rust_buffer_lift_and_lower!(UT);
    ffi_converter_default_return!(UT);

    fn write(obj: Duration, buf: &mut Vec<u8>) {
        buf.put_u64(obj.as_secs());
        buf.put_u32(obj.subsec_nanos());
    }

    fn try_read(buf: &mut &[u8]) -> Result<Duration> {
        check_remaining(buf, 12)?;
        Ok(Duration::new(buf.get_u64(), buf.get_u32()))
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_DURATION);
}

/// Support for passing optional values via the FFI.
///
/// Optional values are currently always passed by serializing to a buffer.
/// We write either a zero byte for `None`, or a one byte followed by the containing
/// item for `Some`.
///
/// In future we could do the same optimization as rust uses internally, where the
/// `None` option is represented as a null pointer and the `Some` as a valid pointer,
/// but that seems more fiddly and less safe in the short term, so it can wait.
unsafe impl<UT, T: FfiConverter<UT>> FfiConverter<UT> for Option<T> {
    ffi_converter_rust_buffer_lift_and_lower!(UT);
    ffi_converter_default_return!(UT);

    fn write(obj: Option<T>, buf: &mut Vec<u8>) {
        match obj {
            None => buf.put_i8(0),
            Some(v) => {
                buf.put_i8(1);
                T::write(v, buf);
            }
        }
    }

    fn try_read(buf: &mut &[u8]) -> Result<Option<T>> {
        check_remaining(buf, 1)?;
        Ok(match buf.get_i8() {
            0 => None,
            1 => Some(T::try_read(buf)?),
            _ => bail!("unexpected tag byte for Option"),
        })
    }

    const TYPE_ID_META: MetadataBuffer =
        MetadataBuffer::from_code(metadata::codes::TYPE_OPTION).concat(T::TYPE_ID_META);
}

/// Support for passing vectors of values via the FFI.
///
/// Vectors are currently always passed by serializing to a buffer.
/// We write a `i32` item count followed by each item in turn.
/// (It's a signed type due to limits of the JVM).
///
/// Ideally we would pass `Vec<u8>` directly as a `RustBuffer` rather
/// than serializing, and perhaps even pass other vector types using a
/// similar struct. But that's for future work.
unsafe impl<UT, T: FfiConverter<UT>> FfiConverter<UT> for Vec<T> {
    ffi_converter_rust_buffer_lift_and_lower!(UT);
    ffi_converter_default_return!(UT);

    fn write(obj: Vec<T>, buf: &mut Vec<u8>) {
        // TODO: would be nice not to panic here :-/
        let len = i32::try_from(obj.len()).unwrap();
        buf.put_i32(len); // We limit arrays to i32::MAX items
        for item in obj {
            <T as FfiConverter<UT>>::write(item, buf);
        }
    }

    fn try_read(buf: &mut &[u8]) -> Result<Vec<T>> {
        check_remaining(buf, 4)?;
        let len = usize::try_from(buf.get_i32())?;
        let mut vec = Vec::with_capacity(len);
        for _ in 0..len {
            vec.push(<T as FfiConverter<UT>>::try_read(buf)?)
        }
        Ok(vec)
    }

    const TYPE_ID_META: MetadataBuffer =
        MetadataBuffer::from_code(metadata::codes::TYPE_VEC).concat(T::TYPE_ID_META);
}

/// Support for associative arrays via the FFI.
/// Note that because of webidl limitations,
/// the key must always be of the String type.
///
/// HashMaps are currently always passed by serializing to a buffer.
/// We write a `i32` entries count followed by each entry (string
/// key followed by the value) in turn.
/// (It's a signed type due to limits of the JVM).
unsafe impl<K, V, UT> FfiConverter<UT> for HashMap<K, V>
where
    K: FfiConverter<UT> + std::hash::Hash + Eq,
    V: FfiConverter<UT>,
{
    ffi_converter_rust_buffer_lift_and_lower!(UT);
    ffi_converter_default_return!(UT);

    fn write(obj: HashMap<K, V>, buf: &mut Vec<u8>) {
        // TODO: would be nice not to panic here :-/
        let len = i32::try_from(obj.len()).unwrap();
        buf.put_i32(len); // We limit HashMaps to i32::MAX entries
        for (key, value) in obj {
            <K as FfiConverter<UT>>::write(key, buf);
            <V as FfiConverter<UT>>::write(value, buf);
        }
    }

    fn try_read(buf: &mut &[u8]) -> Result<HashMap<K, V>> {
        check_remaining(buf, 4)?;
        let len = usize::try_from(buf.get_i32())?;
        let mut map = HashMap::with_capacity(len);
        for _ in 0..len {
            let key = <K as FfiConverter<UT>>::try_read(buf)?;
            let value = <V as FfiConverter<UT>>::try_read(buf)?;
            map.insert(key, value);
        }
        Ok(map)
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_HASH_MAP)
        .concat(K::TYPE_ID_META)
        .concat(V::TYPE_ID_META);
}

/// Support for passing reference-counted shared objects via the FFI.
///
/// To avoid dealing with complex lifetime semantics over the FFI, any data passed
/// by reference must be encapsulated in an `Arc`, and must be safe to share
/// across threads.
unsafe impl<UT, T: Interface<UT>> FfiConverter<UT> for std::sync::Arc<T> {
    // Don't use a pointer to <T> as that requires a `pub <T>`
    type FfiType = *const std::os::raw::c_void;

    /// When lowering, we have an owned `Arc<T>` and we transfer that ownership
    /// to the foreign-language code, "leaking" it out of Rust's ownership system
    /// as a raw pointer. This works safely because we have unique ownership of `self`.
    /// The foreign-language code is responsible for freeing this by calling the
    /// `ffi_object_free` FFI function provided by the corresponding UniFFI type.
    ///
    /// Safety: when freeing the resulting pointer, the foreign-language code must
    /// call the destructor function specific to the type `T`. Calling the destructor
    /// function for other types may lead to undefined behaviour.
    fn lower(obj: std::sync::Arc<T>) -> Self::FfiType {
        std::sync::Arc::into_raw(obj) as Self::FfiType
    }

    /// When lifting, we receive a "borrow" of the `Arc<T>` that is owned by
    /// the foreign-language code, and make a clone of it for our own use.
    ///
    /// Safety: the provided value must be a pointer previously obtained by calling
    /// the `lower()` or `write()` method of this impl.
    fn try_lift(v: Self::FfiType) -> Result<std::sync::Arc<T>> {
        let v = v as *const T;
        // We musn't drop the `Arc<T>` that is owned by the foreign-language code.
        let foreign_arc = std::mem::ManuallyDrop::new(unsafe { std::sync::Arc::<T>::from_raw(v) });
        // Take a clone for our own use.
        Ok(std::sync::Arc::clone(&*foreign_arc))
    }

    /// When writing as a field of a complex structure, make a clone and transfer ownership
    /// of it to the foreign-language code by writing its pointer into the buffer.
    /// The foreign-language code is responsible for freeing this by calling the
    /// `ffi_object_free` FFI function provided by the corresponding UniFFI type.
    ///
    /// Safety: when freeing the resulting pointer, the foreign-language code must
    /// call the destructor function specific to the type `T`. Calling the destructor
    /// function for other types may lead to undefined behaviour.
    fn write(obj: std::sync::Arc<T>, buf: &mut Vec<u8>) {
        static_assertions::const_assert!(std::mem::size_of::<*const std::ffi::c_void>() <= 8);
        buf.put_u64(<Self as FfiConverter<UT>>::lower(obj) as u64);
    }

    /// When reading as a field of a complex structure, we receive a "borrow" of the `Arc<T>`
    /// that is owned by the foreign-language code, and make a clone for our own use.
    ///
    /// Safety: the buffer must contain a pointer previously obtained by calling
    /// the `lower()` or `write()` method of this impl.
    fn try_read(buf: &mut &[u8]) -> Result<std::sync::Arc<T>> {
        static_assertions::const_assert!(std::mem::size_of::<*const std::ffi::c_void>() <= 8);
        check_remaining(buf, 8)?;
        <Self as FfiConverter<UT>>::try_lift(buf.get_u64() as Self::FfiType)
    }

    ffi_converter_default_return!(UT);

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_INTERFACE)
        .concat_str(T::NAME)
        .concat_bool(false);
}

/// FFI support for ForeignSchedulers
///
/// These are passed over the FFI as opaque pointer-sized types representing the foreign executor.
/// The foreign bindings may use an actual pointer to the executor object, or a usized integer
/// handle.
unsafe impl<UT> FfiConverter<UT> for crate::ForeignExecutor {
    type FfiType = crate::ForeignExecutorHandle;

    // Passing these back to the foreign bindings is currently not supported
    fn lower(executor: Self) -> Self::FfiType {
        executor.handle
    }

    fn write(executor: Self, buf: &mut Vec<u8>) {
        // Use native endian when writing these values, so they can be casted to pointer values
        match std::mem::size_of::<usize>() {
            // Use native endian when reading these values, so they can be casted to pointer values
            4 => buf.put_u32_ne(executor.handle.0 as u32),
            8 => buf.put_u64_ne(executor.handle.0 as u64),
            n => panic!("Invalid usize width: {n}"),
        };
    }

    fn try_lift(executor: Self::FfiType) -> Result<Self> {
        Ok(crate::ForeignExecutor::new(executor))
    }

    fn try_read(buf: &mut &[u8]) -> Result<Self> {
        let usize_val = match std::mem::size_of::<usize>() {
            // Use native endian when reading these values, so they can be casted to pointer values
            4 => buf.get_u32_ne() as usize,
            8 => buf.get_u64_ne() as usize,
            n => panic!("Invalid usize width: {n}"),
        };
        <Self as FfiConverter<UT>>::try_lift(crate::ForeignExecutorHandle(usize_val as *const ()))
    }

    ffi_converter_default_return!(UT);

    const TYPE_ID_META: MetadataBuffer =
        MetadataBuffer::from_code(metadata::codes::TYPE_FOREIGN_EXECUTOR);
}

/// Support `Result<>` via the FFI.
///
/// This is currently supported for function returns. Lifting/lowering Result<> arguments is not
/// implemented.
unsafe impl<UT, R, E> FfiConverter<UT> for Result<R, E>
where
    R: FfiConverter<UT>,
    E: FfiConverter<UT>,
{
    type FfiType = (); // Placeholder while lower/lift/serializing is unimplemented
    type ReturnType = R::ReturnType;
    type FutureCallback = R::FutureCallback;

    fn try_lift(_: Self::FfiType) -> Result<Self> {
        unimplemented!("try_lift");
    }

    fn lower(_: Self) -> Self::FfiType {
        unimplemented!("lower");
    }

    fn write(_: Self, _: &mut Vec<u8>) {
        unimplemented!("write");
    }

    fn try_read(_: &mut &[u8]) -> Result<Self> {
        unimplemented!("try_read");
    }

    fn lower_return(v: Self) -> Result<Self::ReturnType, RustBuffer> {
        match v {
            Ok(r) => R::lower_return(r),
            Err(e) => Err(lower_into_rust_buffer(e)),
        }
    }

    fn lift_callback_return(buf: RustBuffer) -> Self {
        Ok(try_lift_from_rust_buffer::<R, UT>(buf)
            .expect("Error reading callback interface result"))
    }

    fn lift_callback_error(buf: RustBuffer) -> Self {
        Err(try_lift_from_rust_buffer::<E, UT>(buf)
            .expect("Error reading callback interface Err result"))
    }

    fn handle_callback_unexpected_error(e: UnexpectedUniFFICallbackError) -> Self {
        Err(E::handle_callback_unexpected_error(e))
    }

    fn invoke_future_callback(
        callback: Self::FutureCallback,
        callback_data: *const (),
        return_value: Self::ReturnType,
        call_status: RustCallStatus,
    ) {
        R::invoke_future_callback(callback, callback_data, return_value, call_status)
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_RESULT)
        .concat(R::TYPE_ID_META)
        .concat(E::TYPE_ID_META);
}
