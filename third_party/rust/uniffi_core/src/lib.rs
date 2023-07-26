/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Runtime support code for uniffi
//!
//! This crate provides the small amount of runtime code that is required by the generated uniffi
//! component scaffolding in order to transfer data back and forth across the C-style FFI layer,
//! as well as some utilities for testing the generated bindings.
//!
//! The key concept here is the [`FfiConverter`] trait, which is responsible for converting between
//! a Rust type and a low-level C-style type that can be passed across the FFI:
//!
//!  * How to [represent](FfiConverter::FfiType) values of the Rust type in the low-level C-style type
//!    system of the FFI layer.
//!  * How to ["lower"](FfiConverter::lower) values of the Rust type into an appropriate low-level
//!    FFI value.
//!  * How to ["lift"](FfiConverter::try_lift) low-level FFI values back into values of the Rust
//!    type.
//!  * How to [write](FfiConverter::write) values of the Rust type into a buffer, for cases
//!    where they are part of a compound data structure that is serialized for transfer.
//!  * How to [read](FfiConverter::try_read) values of the Rust type from buffer, for cases
//!    where they are received as part of a compound data structure that was serialized for transfer.
//!  * How to [return](FfiConverter::lower_return) values of the Rust type from scaffolding
//!    functions.
//!
//! This logic encapsulates the Rust-side handling of data transfer. Each foreign-language binding
//! must also implement a matching set of data-handling rules for each data type.
//!
//! In addition to the core `FfiConverter` trait, we provide a handful of struct definitions useful
//! for passing core rust types over the FFI, such as [`RustBuffer`].

#![warn(rust_2018_idioms, unused_qualifications)]

use anyhow::bail;
use bytes::buf::Buf;

// Make Result<> public to support external impls of FfiConverter
pub use anyhow::Result;

pub mod ffi;
mod ffi_converter_impls;
pub mod metadata;

pub use ffi::*;
pub use metadata::*;

// Re-export the libs that we use in the generated code,
// so the consumer doesn't have to depend on them directly.
pub mod deps {
    pub use anyhow;
    #[cfg(feature = "tokio")]
    pub use async_compat;
    pub use bytes;
    pub use log;
    pub use static_assertions;
}

mod panichook;

const PACKAGE_VERSION: &str = env!("CARGO_PKG_VERSION");

// For the significance of this magic number 10 here, and the reason that
// it can't be a named constant, see the `check_compatible_version` function.
static_assertions::const_assert!(PACKAGE_VERSION.as_bytes().len() < 10);

/// Check whether the uniffi runtime version is compatible a given uniffi_bindgen version.
///
/// The result of this check may be used to ensure that generated Rust scaffolding is
/// using a compatible version of the uniffi runtime crate. It's a `const fn` so that it
/// can be used to perform such a check at compile time.
#[allow(clippy::len_zero)]
pub const fn check_compatible_version(bindgen_version: &'static str) -> bool {
    // While UniFFI is still under heavy development, we require that
    // the runtime support crate be precisely the same version as the
    // build-time bindgen crate.
    //
    // What we want to achieve here is checking two strings for equality.
    // Unfortunately Rust doesn't yet support calling the `&str` equals method
    // in a const context. We can hack around that by doing a byte-by-byte
    // comparison of the underlying bytes.
    let package_version = PACKAGE_VERSION.as_bytes();
    let bindgen_version = bindgen_version.as_bytes();
    // What we want to achieve here is a loop over the underlying bytes,
    // something like:
    // ```
    //  if package_version.len() != bindgen_version.len() {
    //      return false
    //  }
    //  for i in 0..package_version.len() {
    //      if package_version[i] != bindgen_version[i] {
    //          return false
    //      }
    //  }
    //  return true
    // ```
    // Unfortunately stable Rust doesn't allow `if` or `for` in const contexts,
    // so code like the above would only work in nightly. We can hack around it by
    // statically asserting that the string is shorter than a certain length
    // (currently 10 bytes) and then manually unrolling that many iterations of the loop.
    //
    // Yes, I am aware that this is horrific, but the externally-visible
    // behaviour is quite nice for consumers!
    package_version.len() == bindgen_version.len()
        && (package_version.len() == 0 || package_version[0] == bindgen_version[0])
        && (package_version.len() <= 1 || package_version[1] == bindgen_version[1])
        && (package_version.len() <= 2 || package_version[2] == bindgen_version[2])
        && (package_version.len() <= 3 || package_version[3] == bindgen_version[3])
        && (package_version.len() <= 4 || package_version[4] == bindgen_version[4])
        && (package_version.len() <= 5 || package_version[5] == bindgen_version[5])
        && (package_version.len() <= 6 || package_version[6] == bindgen_version[6])
        && (package_version.len() <= 7 || package_version[7] == bindgen_version[7])
        && (package_version.len() <= 8 || package_version[8] == bindgen_version[8])
        && (package_version.len() <= 9 || package_version[9] == bindgen_version[9])
        && package_version.len() < 10
}

/// Assert that the uniffi runtime version matches an expected value.
///
/// This is a helper hook for the generated Rust scaffolding, to produce a compile-time
/// error if the version of `uniffi_bindgen` used to generate the scaffolding was
/// incompatible with the version of `uniffi` being used at runtime.
#[macro_export]
macro_rules! assert_compatible_version {
    ($v:expr $(,)?) => {
        uniffi::deps::static_assertions::const_assert!(uniffi::check_compatible_version($v));
    };
}

/// Trait defining how to transfer values via the FFI layer.
///
/// The `FfiConverter` trait defines how to pass values of a particular type back-and-forth over
/// the uniffi generated FFI layer, both as standalone argument or return values, and as
/// part of serialized compound data structures. `FfiConverter` is mainly used in generated code.
/// The goal is to make it easy for the code generator to name the correct FFI-related function for
/// a given type.
///
/// FfiConverter has a generic parameter, that's filled in with a type local to the UniFFI consumer crate.
/// This allows us to work around the Rust orphan rules for remote types. See
/// `https://mozilla.github.io/uniffi-rs/internals/lifting_and_lowering.html#code-generation-and-the-fficonverter-trait`
/// for details.
///
/// ## Scope
///
/// It could be argued that FfiConverter handles too many concerns and should be split into
/// separate traits (like `FfiLiftAndLower`, `FfiSerialize`, `FfiReturn`).  However, there are good
/// reasons to keep all the functionality in one trait.
///
/// The main reason is that splitting the traits requires fairly complex blanket implementations,
/// for example `impl<UT, T> FfiReturn<UT> for T: FfiLiftAndLower<UT>`.  Writing these impls often
/// means fighting with `rustc` over what counts as a conflicting implementation.  In fact, as of
/// Rust 1.66, that previous example conflicts with `impl<UT> FfiReturn<UT> for ()`, since other
/// crates can implement `impl FfiReturn<MyLocalType> for ()`. In general, this path gets
/// complicated very quickly and that distracts from the logic that we want to define, which is
/// fairly simple.
///
/// The main downside of having a single `FfiConverter` trait is that we need to implement it for
/// types that only support some of the functionality.  For example, `Result<>` supports returning
/// values, but not lifting/lowering/serializing them.  This is a bit weird, but works okay --
/// especially since `FfiConverter` is rarely used outside of generated code.
///
/// ## Safety
///
/// This is an unsafe trait (implementing it requires `unsafe impl`) because we can't guarantee
/// that it's safe to pass your type out to foreign-language code and back again. Buggy
/// implementations of this trait might violate some assumptions made by the generated code,
/// or might not match with the corresponding code in the generated foreign-language bindings.
///
/// In general, you should not need to implement this trait by hand, and should instead rely on
/// implementations generated from your component UDL via the `uniffi-bindgen scaffolding` command.
pub unsafe trait FfiConverter<UT>: Sized {
    /// The low-level type used for passing values of this type over the FFI.
    ///
    /// This must be a C-compatible type (e.g. a numeric primitive, a `#[repr(C)]` struct) into
    /// which values of the target rust type can be converted.
    ///
    /// For complex data types, we currently recommend using `RustBuffer` and serializing
    /// the data for transfer. In theory it could be possible to build a matching
    /// `#[repr(C)]` struct for a complex data type and pass that instead, but explicit
    /// serialization is simpler and safer as a starting point.
    type FfiType;

    /// The type that should be returned by scaffolding functions for this type.
    ///
    /// This is usually the same as `FfiType`, but `Result<>` has specialized handling.
    type ReturnType: FfiDefault;

    /// The `FutureCallback<T>` type used for async functions
    ///
    /// This is almost always `FutureCallback<Self::ReturnType>`.  The one exception is the
    /// unit type, see that `FfiConverter` impl for details.
    type FutureCallback: Copy;

    /// Lower a rust value of the target type, into an FFI value of type Self::FfiType.
    ///
    /// This trait method is used for sending data from rust to the foreign language code,
    /// by (hopefully cheaply!) converting it into something that can be passed over the FFI
    /// and reconstructed on the other side.
    ///
    /// Note that this method takes an owned value; this allows it to transfer ownership in turn to
    /// the foreign language code, e.g. by boxing the value and passing a pointer.
    fn lower(obj: Self) -> Self::FfiType;

    /// Lower this value for scaffolding function return
    ///
    /// This method converts values into the `Result<>` type that [rust_call] expects. For
    /// successful calls, return `Ok(lower_return)`.  For errors that should be translated into
    /// thrown exceptions on the foreign code, serialize the error into a RustBuffer and return
    /// `Err(buf)`
    fn lower_return(obj: Self) -> Result<Self::ReturnType, RustBuffer>;

    /// Lift a rust value of the target type, from an FFI value of type Self::FfiType.
    ///
    /// This trait method is used for receiving data from the foreign language code in rust,
    /// by (hopefully cheaply!) converting it from a low-level FFI value of type Self::FfiType
    /// into a high-level rust value of the target type.
    ///
    /// Since we cannot statically guarantee that the foreign-language code will send valid
    /// values of type Self::FfiType, this method is fallible.
    fn try_lift(v: Self::FfiType) -> Result<Self>;

    /// Lift a Rust value for a callback interface method result
    fn lift_callback_return(buf: RustBuffer) -> Self {
        try_lift_from_rust_buffer(buf).expect("Error reading callback interface result")
    }

    /// Lift a Rust value for a callback interface method error result
    ///
    /// This is called for "expected errors" -- the callback method returns a Result<> type and the
    /// foreign code throws an exception that corresponds to the error type.
    fn lift_callback_error(_buf: RustBuffer) -> Self {
        panic!("Callback interface method returned unexpected error")
    }

    /// Lift a Rust value for an unexpected callback interface error
    ///
    /// The main reason this is called is when the callback interface throws an error type that
    /// doesn't match the Rust trait definition.  It's also called for corner cases, like when the
    /// foreign code doesn't follow the FFI contract.
    ///
    /// The default implementation panics unconditionally.  Errors used in callback interfaces
    /// handle this using the `From<UnexpectedUniFFICallbackError>` impl that the library author
    /// must provide.
    fn handle_callback_unexpected_error(_e: UnexpectedUniFFICallbackError) -> Self {
        panic!("Callback interface method returned unexpected error")
    }

    /// Write a rust value into a buffer, to send over the FFI in serialized form.
    ///
    /// This trait method can be used for sending data from rust to the foreign language code,
    /// in cases where we're not able to use a special-purpose FFI type and must fall back to
    /// sending serialized bytes.
    ///
    /// Note that this method takes an owned value because it's transferring ownership
    /// to the foreign language code via the RustBuffer.
    fn write(obj: Self, buf: &mut Vec<u8>);

    /// Read a rust value from a buffer, received over the FFI in serialized form.
    ///
    /// This trait method can be used for receiving data from the foreign language code in rust,
    /// in cases where we're not able to use a special-purpose FFI type and must fall back to
    /// receiving serialized bytes.
    ///
    /// Since we cannot statically guarantee that the foreign-language code will send valid
    /// serialized bytes for the target type, this method is fallible.
    ///
    /// Note the slightly unusual type here - we want a mutable reference to a slice of bytes,
    /// because we want to be able to advance the start of the slice after reading an item
    /// from it (but will not mutate the actual contents of the slice).
    fn try_read(buf: &mut &[u8]) -> Result<Self>;

    /// Invoke a `FutureCallback` to complete a async call
    fn invoke_future_callback(
        callback: Self::FutureCallback,
        callback_data: *const (),
        return_value: Self::ReturnType,
        call_status: RustCallStatus,
    );

    /// Type ID metadata, serialized into a [MetadataBuffer]
    const TYPE_ID_META: MetadataBuffer;
}

/// Implemented for exported interface types
///
/// Like, FfiConverter this has a generic parameter, that's filled in with a type local to the
/// UniFFI consumer crate.
pub trait Interface<UT>: Send + Sync + Sized {
    const NAME: &'static str;
}

/// Struct to use when we want to lift/lower/serialize types inside the `uniffi` crate.
struct UniFfiTag;

/// A helper function to ensure we don't read past the end of a buffer.
///
/// Rust won't actually let us read past the end of a buffer, but the `Buf` trait does not support
/// returning an explicit error in this case, and will instead panic. This is a look-before-you-leap
/// helper function to instead return an explicit error, to help with debugging.
pub fn check_remaining(buf: &[u8], num_bytes: usize) -> Result<()> {
    if buf.remaining() < num_bytes {
        bail!(
            "not enough bytes remaining in buffer ({} < {num_bytes})",
            buf.remaining(),
        );
    }
    Ok(())
}

/// Helper function to lower an `anyhow::Error` that's wrapping an error type
pub fn lower_anyhow_error_or_panic<UT, E>(err: anyhow::Error, arg_name: &str) -> RustBuffer
where
    E: 'static + FfiConverter<UT> + Sync + Send + std::fmt::Debug + std::fmt::Display,
{
    match err.downcast::<E>() {
        Ok(actual_error) => lower_into_rust_buffer(actual_error),
        Err(ohno) => panic!("Failed to convert arg '{arg_name}': {ohno}"),
    }
}

/// Helper function to create a RustBuffer with a single value
pub fn lower_into_rust_buffer<T: FfiConverter<UT>, UT>(obj: T) -> RustBuffer {
    let mut buf = ::std::vec::Vec::new();
    T::write(obj, &mut buf);
    RustBuffer::from_vec(buf)
}

/// Helper function to deserialize a RustBuffer with a single value
pub fn try_lift_from_rust_buffer<T: FfiConverter<UT>, UT>(v: RustBuffer) -> Result<T> {
    let vec = v.destroy_into_vec();
    let mut buf = vec.as_slice();
    let value = T::try_read(&mut buf)?;
    match Buf::remaining(&buf) {
        0 => Ok(value),
        n => bail!("junk data left in buffer after lifting (count: {n})",),
    }
}

/// Macro to implement returning values by simply lowering them and returning them
///
/// This is what we use for all FfiConverters except for `Result`.  This would be nicer as a
/// trait default, but Rust doesn't support defaults on associated types.
#[macro_export]
macro_rules! ffi_converter_default_return {
    ($uniffi_tag:ty) => {
        type ReturnType = <Self as $crate::FfiConverter<$uniffi_tag>>::FfiType;
        type FutureCallback = $crate::FutureCallback<Self::ReturnType>;

        fn lower_return(v: Self) -> ::std::result::Result<Self::FfiType, $crate::RustBuffer> {
            Ok(<Self as $crate::FfiConverter<$uniffi_tag>>::lower(v))
        }

        fn invoke_future_callback(
            callback: Self::FutureCallback,
            callback_data: *const (),
            return_value: Self::ReturnType,
            call_status: $crate::RustCallStatus,
        ) {
            callback(callback_data, return_value, call_status);
        }
    };
}

/// Macro to implement lowering/lifting using a `RustBuffer`
///
/// For complex types where it's too fiddly or too unsafe to convert them into a special-purpose
/// C-compatible value, you can use this trait to implement `lower()` in terms of `write()` and
/// `lift` in terms of `read()`.
///
/// This macro implements the boilerplate needed to define `lower`, `lift` and `FFIType`.
#[macro_export]
macro_rules! ffi_converter_rust_buffer_lift_and_lower {
    ($uniffi_tag:ty) => {
        type FfiType = $crate::RustBuffer;

        fn lower(v: Self) -> $crate::RustBuffer {
            $crate::lower_into_rust_buffer::<Self, $uniffi_tag>(v)
        }

        fn try_lift(buf: $crate::RustBuffer) -> $crate::Result<Self> {
            $crate::try_lift_from_rust_buffer::<Self, $uniffi_tag>(buf)
        }
    };
}

/// Macro to implement `FfiConverter<T>` for a type by forwording all calls to another type
///
/// This is used for external types
#[macro_export]
macro_rules! ffi_converter_forward {
    ($T:ty, $existing_impl_tag:ty, $new_impl_tag:ty) => {
        unsafe impl $crate::FfiConverter<$new_impl_tag> for $T {
            type FfiType = <$T as $crate::FfiConverter<$existing_impl_tag>>::FfiType;
            type ReturnType = <$T as $crate::FfiConverter<$existing_impl_tag>>::FfiType;
            type FutureCallback = <$T as $crate::FfiConverter<$existing_impl_tag>>::FutureCallback;

            fn lower(obj: $T) -> Self::FfiType {
                <$T as $crate::FfiConverter<$existing_impl_tag>>::lower(obj)
            }

            fn lower_return(
                v: Self,
            ) -> ::std::result::Result<Self::ReturnType, $crate::RustBuffer> {
                <$T as $crate::FfiConverter<$existing_impl_tag>>::lower_return(v)
            }

            fn try_lift(v: Self::FfiType) -> $crate::Result<$T> {
                <$T as $crate::FfiConverter<$existing_impl_tag>>::try_lift(v)
            }

            fn write(obj: $T, buf: &mut Vec<u8>) {
                <$T as $crate::FfiConverter<$existing_impl_tag>>::write(obj, buf)
            }

            fn try_read(buf: &mut &[u8]) -> $crate::Result<$T> {
                <$T as $crate::FfiConverter<$existing_impl_tag>>::try_read(buf)
            }

            fn invoke_future_callback(
                callback: Self::FutureCallback,
                callback_data: *const (),
                return_value: Self::ReturnType,
                call_status: $crate::RustCallStatus,
            ) {
                <$T as $crate::FfiConverter<$existing_impl_tag>>::invoke_future_callback(
                    callback,
                    callback_data,
                    return_value,
                    call_status,
                )
            }

            const TYPE_ID_META: ::uniffi::MetadataBuffer =
                <$T as $crate::FfiConverter<$existing_impl_tag>>::TYPE_ID_META;
        }
    };
}

/// Macro to implement `FfiConverter<T>` for a trait
#[macro_export]
macro_rules! ffi_converter_trait_decl {
     ($T:ty, $name:expr, $uniffi_tag:ty) => {
        use $crate::deps::bytes::{Buf, BufMut};
        unsafe impl $crate::FfiConverter<$uniffi_tag> for std::sync::Arc<$T> {
            type FfiType = *const std::os::raw::c_void;
            $crate::ffi_converter_default_return!($uniffi_tag);
            //type ReturnType = *const std::os::raw::c_void;

            fn lower(obj: std::sync::Arc<$T>) -> Self::FfiType {
                Box::into_raw(Box::new(obj)) as *const std::os::raw::c_void
            }

            fn try_lift(v: Self::FfiType) -> $crate::Result<std::sync::Arc<$T>> {
                let foreign_arc = Box::leak(unsafe { Box::from_raw(v as *mut std::sync::Arc<$T>) });
                // Take a clone for our own use.
                Ok(std::sync::Arc::clone(foreign_arc))
            }

            fn write(obj: std::sync::Arc<$T>, buf: &mut Vec<u8>) {
                $crate::deps::static_assertions::const_assert!(std::mem::size_of::<*const std::ffi::c_void>() <= 8);
                buf.put_u64(<Self as $crate::FfiConverter<$uniffi_tag>>::lower(obj) as u64);
            }

            fn try_read(buf: &mut &[u8]) -> $crate::Result<std::sync::Arc<$T>> {
                $crate::deps::static_assertions::const_assert!(std::mem::size_of::<*const std::ffi::c_void>() <= 8);
                $crate::check_remaining(buf, 8)?;
                <Self as $crate::FfiConverter<$uniffi_tag>>::try_lift(buf.get_u64() as Self::FfiType)
            }
            const TYPE_ID_META: $crate::MetadataBuffer = $crate::MetadataBuffer::from_code($crate::metadata::codes::TYPE_INTERFACE).concat_str($name).concat_bool(true);
        }
    }
}

/// Macro to implement `FfiConverter<T>` for a callback interface
#[macro_export]
macro_rules! ffi_converter_callback_interface {
    ($trait:ident, $T:ty, $name:expr, $uniffi_tag:ty) => {
        unsafe impl ::uniffi::FfiConverter<$uniffi_tag> for Box<dyn $trait> {
            type FfiType = u64;

            // Lower and write are tricky to implement because we have a dyn trait as our type.  There's
            // probably a way to, but this carries lots of thread safety risks, down to impedance
            // mismatches between Rust and foreign languages, and our uncertainty around implementations of
            // concurrent handlemaps.
            //
            // The use case for them is also quite exotic: it's passing a foreign callback back to the foreign
            // language.
            //
            // Until we have some certainty, and use cases, we shouldn't use them.
            fn lower(_obj: Box<dyn $trait>) -> Self::FfiType {
                panic!("Lowering CallbackInterface not supported")
            }

            fn write(_obj: Box<dyn $trait>, _buf: &mut std::vec::Vec<u8>) {
                panic!("Writing CallbackInterface not supported")
            }

            fn try_lift(v: Self::FfiType) -> uniffi::deps::anyhow::Result<Box<dyn $trait>> {
                Ok(Box::new(<$T>::new(v)))
            }

            fn try_read(buf: &mut &[u8]) -> uniffi::deps::anyhow::Result<Box<dyn $trait>> {
                use uniffi::deps::bytes::Buf;
                uniffi::check_remaining(buf, 8)?;
                Self::try_lift(buf.get_u64())
            }

            ::uniffi::ffi_converter_default_return!($uniffi_tag);

            const TYPE_ID_META: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(
                ::uniffi::metadata::codes::TYPE_CALLBACK_INTERFACE,
            )
            .concat_str($name);
        }
    };
}

#[cfg(test)]
mod test {
    use super::{FfiConverter, UniFfiTag};
    use std::time::{Duration, SystemTime};

    #[test]
    fn timestamp_roundtrip_post_epoch() {
        let expected = SystemTime::UNIX_EPOCH + Duration::new(100, 100);
        let result =
            <SystemTime as FfiConverter<UniFfiTag>>::try_lift(<SystemTime as FfiConverter<
                UniFfiTag,
            >>::lower(expected))
            .expect("Failed to lift!");
        assert_eq!(expected, result)
    }

    #[test]
    fn timestamp_roundtrip_pre_epoch() {
        let expected = SystemTime::UNIX_EPOCH - Duration::new(100, 100);
        let result =
            <SystemTime as FfiConverter<UniFfiTag>>::try_lift(<SystemTime as FfiConverter<
                UniFfiTag,
            >>::lower(expected))
            .expect("Failed to lift!");
        assert_eq!(
            expected, result,
            "Expected results after lowering and lifting to be equal"
        )
    }
}
