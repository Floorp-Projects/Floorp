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
/// "UT" means an arbitrary `UniFfiTag` type.
use crate::{
    check_remaining, derive_ffi_traits, ffi_converter_rust_buffer_lift_and_lower, metadata,
    ConvertError, FfiConverter, Lift, LiftRef, LiftReturn, Lower, LowerReturn, MetadataBuffer,
    Result, RustBuffer, UnexpectedUniFFICallbackError,
};
use anyhow::bail;
use bytes::buf::{Buf, BufMut};
use paste::paste;
use std::{
    collections::HashMap,
    convert::TryFrom,
    error::Error,
    sync::Arc,
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

// Support for passing optional values via the FFI.
//
// Optional values are currently always passed by serializing to a buffer.
// We write either a zero byte for `None`, or a one byte followed by the containing
// item for `Some`.
//
// In future we could do the same optimization as rust uses internally, where the
// `None` option is represented as a null pointer and the `Some` as a valid pointer,
// but that seems more fiddly and less safe in the short term, so it can wait.

unsafe impl<UT, T: Lower<UT>> Lower<UT> for Option<T> {
    type FfiType = RustBuffer;

    fn write(obj: Option<T>, buf: &mut Vec<u8>) {
        match obj {
            None => buf.put_i8(0),
            Some(v) => {
                buf.put_i8(1);
                T::write(v, buf);
            }
        }
    }

    fn lower(obj: Option<T>) -> RustBuffer {
        Self::lower_into_rust_buffer(obj)
    }

    const TYPE_ID_META: MetadataBuffer =
        MetadataBuffer::from_code(metadata::codes::TYPE_OPTION).concat(T::TYPE_ID_META);
}

unsafe impl<UT, T: Lift<UT>> Lift<UT> for Option<T> {
    type FfiType = RustBuffer;

    fn try_read(buf: &mut &[u8]) -> Result<Option<T>> {
        check_remaining(buf, 1)?;
        Ok(match buf.get_i8() {
            0 => None,
            1 => Some(T::try_read(buf)?),
            _ => bail!("unexpected tag byte for Option"),
        })
    }

    fn try_lift(buf: RustBuffer) -> Result<Option<T>> {
        Self::try_lift_from_rust_buffer(buf)
    }

    const TYPE_ID_META: MetadataBuffer =
        MetadataBuffer::from_code(metadata::codes::TYPE_OPTION).concat(T::TYPE_ID_META);
}

// Support for passing vectors of values via the FFI.
//
// Vectors are currently always passed by serializing to a buffer.
// We write a `i32` item count followed by each item in turn.
// (It's a signed type due to limits of the JVM).
//
// Ideally we would pass `Vec<u8>` directly as a `RustBuffer` rather
// than serializing, and perhaps even pass other vector types using a
// similar struct. But that's for future work.

unsafe impl<UT, T: Lower<UT>> Lower<UT> for Vec<T> {
    type FfiType = RustBuffer;

    fn write(obj: Vec<T>, buf: &mut Vec<u8>) {
        // TODO: would be nice not to panic here :-/
        let len = i32::try_from(obj.len()).unwrap();
        buf.put_i32(len); // We limit arrays to i32::MAX items
        for item in obj {
            <T as Lower<UT>>::write(item, buf);
        }
    }

    fn lower(obj: Vec<T>) -> RustBuffer {
        Self::lower_into_rust_buffer(obj)
    }

    const TYPE_ID_META: MetadataBuffer =
        MetadataBuffer::from_code(metadata::codes::TYPE_VEC).concat(T::TYPE_ID_META);
}

/// Support for associative arrays via the FFI - `record<u32, u64>` in UDL.
/// HashMaps are currently always passed by serializing to a buffer.
/// We write a `i32` entries count followed by each entry (string
/// key followed by the value) in turn.
/// (It's a signed type due to limits of the JVM).
unsafe impl<UT, T: Lift<UT>> Lift<UT> for Vec<T> {
    type FfiType = RustBuffer;

    fn try_read(buf: &mut &[u8]) -> Result<Vec<T>> {
        check_remaining(buf, 4)?;
        let len = usize::try_from(buf.get_i32())?;
        let mut vec = Vec::with_capacity(len);
        for _ in 0..len {
            vec.push(<T as Lift<UT>>::try_read(buf)?)
        }
        Ok(vec)
    }

    fn try_lift(buf: RustBuffer) -> Result<Vec<T>> {
        Self::try_lift_from_rust_buffer(buf)
    }

    const TYPE_ID_META: MetadataBuffer =
        MetadataBuffer::from_code(metadata::codes::TYPE_VEC).concat(T::TYPE_ID_META);
}

unsafe impl<K, V, UT> Lower<UT> for HashMap<K, V>
where
    K: Lower<UT> + std::hash::Hash + Eq,
    V: Lower<UT>,
{
    type FfiType = RustBuffer;

    fn write(obj: HashMap<K, V>, buf: &mut Vec<u8>) {
        // TODO: would be nice not to panic here :-/
        let len = i32::try_from(obj.len()).unwrap();
        buf.put_i32(len); // We limit HashMaps to i32::MAX entries
        for (key, value) in obj {
            <K as Lower<UT>>::write(key, buf);
            <V as Lower<UT>>::write(value, buf);
        }
    }

    fn lower(obj: HashMap<K, V>) -> RustBuffer {
        Self::lower_into_rust_buffer(obj)
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_HASH_MAP)
        .concat(K::TYPE_ID_META)
        .concat(V::TYPE_ID_META);
}

unsafe impl<K, V, UT> Lift<UT> for HashMap<K, V>
where
    K: Lift<UT> + std::hash::Hash + Eq,
    V: Lift<UT>,
{
    type FfiType = RustBuffer;

    fn try_read(buf: &mut &[u8]) -> Result<HashMap<K, V>> {
        check_remaining(buf, 4)?;
        let len = usize::try_from(buf.get_i32())?;
        let mut map = HashMap::with_capacity(len);
        for _ in 0..len {
            let key = <K as Lift<UT>>::try_read(buf)?;
            let value = <V as Lift<UT>>::try_read(buf)?;
            map.insert(key, value);
        }
        Ok(map)
    }

    fn try_lift(buf: RustBuffer) -> Result<HashMap<K, V>> {
        Self::try_lift_from_rust_buffer(buf)
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_HASH_MAP)
        .concat(K::TYPE_ID_META)
        .concat(V::TYPE_ID_META);
}

derive_ffi_traits!(blanket u8);
derive_ffi_traits!(blanket i8);
derive_ffi_traits!(blanket u16);
derive_ffi_traits!(blanket i16);
derive_ffi_traits!(blanket u32);
derive_ffi_traits!(blanket i32);
derive_ffi_traits!(blanket u64);
derive_ffi_traits!(blanket i64);
derive_ffi_traits!(blanket f32);
derive_ffi_traits!(blanket f64);
derive_ffi_traits!(blanket bool);
derive_ffi_traits!(blanket String);
derive_ffi_traits!(blanket Duration);
derive_ffi_traits!(blanket SystemTime);

// For composite types, derive LowerReturn, LiftReturn, etc, from Lift/Lower.
//
// Note that this means we don't get specialized return handling.  For example, if we could return
// an `Option<Result<>>` we would always return that type directly and never throw.
derive_ffi_traits!(impl<T, UT> LowerReturn<UT> for Option<T> where Option<T>: Lower<UT>);
derive_ffi_traits!(impl<T, UT> LiftReturn<UT> for Option<T> where Option<T>: Lift<UT>);
derive_ffi_traits!(impl<T, UT> LiftRef<UT> for Option<T> where Option<T>: Lift<UT>);

derive_ffi_traits!(impl<T, UT> LowerReturn<UT> for Vec<T> where Vec<T>: Lower<UT>);
derive_ffi_traits!(impl<T, UT> LiftReturn<UT> for Vec<T> where Vec<T>: Lift<UT>);
derive_ffi_traits!(impl<T, UT> LiftRef<UT> for Vec<T> where Vec<T>: Lift<UT>);

derive_ffi_traits!(impl<K, V, UT> LowerReturn<UT> for HashMap<K, V> where HashMap<K, V>: Lower<UT>);
derive_ffi_traits!(impl<K, V, UT> LiftReturn<UT> for HashMap<K, V> where HashMap<K, V>: Lift<UT>);
derive_ffi_traits!(impl<K, V, UT> LiftRef<UT> for HashMap<K, V> where HashMap<K, V>: Lift<UT>);

// For Arc we derive all the traits, but have to write it all out because we need an unsized T bound
derive_ffi_traits!(impl<T, UT> Lower<UT> for Arc<T> where Arc<T>: FfiConverter<UT>, T: ?Sized);
derive_ffi_traits!(impl<T, UT> Lift<UT> for Arc<T> where Arc<T>: FfiConverter<UT>, T: ?Sized);
derive_ffi_traits!(impl<T, UT> LowerReturn<UT> for Arc<T> where Arc<T>: Lower<UT>, T: ?Sized);
derive_ffi_traits!(impl<T, UT> LiftReturn<UT> for Arc<T> where Arc<T>: Lift<UT>, T: ?Sized);
derive_ffi_traits!(impl<T, UT> LiftRef<UT> for Arc<T> where Arc<T>: Lift<UT>, T: ?Sized);

// Implement LowerReturn/LiftReturn for the unit type (void returns)

unsafe impl<UT> LowerReturn<UT> for () {
    type ReturnType = ();

    fn lower_return(_: ()) -> Result<Self::ReturnType, RustBuffer> {
        Ok(())
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_UNIT);
}

unsafe impl<UT> LiftReturn<UT> for () {
    type ReturnType = ();

    fn try_lift_successful_return(_: ()) -> Result<Self> {
        Ok(())
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_UNIT);
}

// Implement LowerReturn/LiftReturn for `Result<R, E>`.  This is where we handle exceptions/Err
// results.

unsafe impl<UT, R, E> LowerReturn<UT> for Result<R, E>
where
    R: LowerReturn<UT>,
    E: Lower<UT> + Error + Send + Sync + 'static,
{
    type ReturnType = R::ReturnType;

    fn lower_return(v: Self) -> Result<Self::ReturnType, RustBuffer> {
        match v {
            Ok(r) => R::lower_return(r),
            Err(e) => Err(E::lower_into_rust_buffer(e)),
        }
    }

    fn handle_failed_lift(arg_name: &str, err: anyhow::Error) -> Self {
        match err.downcast::<E>() {
            Ok(actual_error) => Err(actual_error),
            Err(ohno) => panic!("Failed to convert arg '{arg_name}': {ohno}"),
        }
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_RESULT)
        .concat(R::TYPE_ID_META)
        .concat(E::TYPE_ID_META);
}

unsafe impl<UT, R, E> LiftReturn<UT> for Result<R, E>
where
    R: LiftReturn<UT>,
    E: Lift<UT, FfiType = RustBuffer> + ConvertError<UT>,
{
    type ReturnType = R::ReturnType;

    fn try_lift_successful_return(v: R::ReturnType) -> Result<Self> {
        R::try_lift_successful_return(v).map(Ok)
    }

    fn lift_error(buf: RustBuffer) -> Self {
        match E::try_lift_from_rust_buffer(buf) {
            Ok(lifted_error) => Err(lifted_error),
            Err(anyhow_error) => {
                Self::handle_callback_unexpected_error(UnexpectedUniFFICallbackError {
                    reason: format!("Error lifting from rust buffer: {anyhow_error}"),
                })
            }
        }
    }

    fn handle_callback_unexpected_error(e: UnexpectedUniFFICallbackError) -> Self {
        Err(E::try_convert_unexpected_callback_error(e).unwrap_or_else(|e| panic!("{e}")))
    }

    const TYPE_ID_META: MetadataBuffer = MetadataBuffer::from_code(metadata::codes::TYPE_RESULT)
        .concat(R::TYPE_ID_META)
        .concat(E::TYPE_ID_META);
}

unsafe impl<T, UT> LiftRef<UT> for [T]
where
    T: Lift<UT>,
{
    type LiftType = Vec<T>;
}

unsafe impl<UT> LiftRef<UT> for str {
    type LiftType = String;
}
