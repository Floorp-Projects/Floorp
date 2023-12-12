/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Traits that define how to transfer values via the FFI layer.
//!
//! These traits define how to pass values over the FFI in various ways: as arguments or as return
//! values, from Rust to the foreign side and vice-versa.  These traits are mainly used by the
//! proc-macro generated code.  The goal is to allow the proc-macros to go from a type name to the
//! correct function for a given FFI operation.
//!
//! The traits form a sort-of tree structure from general to specific:
//! ```ignore
//!
//!                   [FfiConverter]
//!                        |
//!           -----------------------------
//!           |                           |
//!       [Lower]                      [Lift]
//!           |                           |
//!           |                       --------------
//!           |                       |            |
//!       [LowerReturn]           [LiftRef]  [LiftReturn]
//! ```
//!
//! The `derive_ffi_traits` macro can be used to derive the specific traits from the general ones.
//! Here's the main ways we implement these traits:
//!
//! * For most types we implement [FfiConverter] and use [derive_ffi_traits] to implement the rest
//! * If a type can only be lifted/lowered, then we implement [Lift] or [Lower] and use
//!   [derive_ffi_traits] to implement the rest
//! * If a type needs special-case handling, like `Result<>` and `()`, we implement the traits
//!   directly.
//!
//! FfiConverter has a generic parameter, that's filled in with a type local to the UniFFI consumer crate.
//! This allows us to work around the Rust orphan rules for remote types. See
//! `https://mozilla.github.io/uniffi-rs/internals/lifting_and_lowering.html#code-generation-and-the-fficonverter-trait`
//! for details.
//!
//! ## Safety
//!
//! All traits are unsafe (implementing it requires `unsafe impl`) because we can't guarantee
//! that it's safe to pass your type out to foreign-language code and back again. Buggy
//! implementations of this trait might violate some assumptions made by the generated code,
//! or might not match with the corresponding code in the generated foreign-language bindings.
//! These traits should not be used directly, only in generated code, and the generated code should
//! have fixture tests to test that everything works correctly together.

use std::{borrow::Borrow, sync::Arc};

use anyhow::bail;
use bytes::Buf;

use crate::{FfiDefault, MetadataBuffer, Result, RustBuffer, UnexpectedUniFFICallbackError};

/// Generalized FFI conversions
///
/// This trait is not used directly by the code generation, but implement this and calling
/// [derive_ffi_traits] is a simple way to implement all the traits that are.
///
/// ## Safety
///
/// All traits are unsafe (implementing it requires `unsafe impl`) because we can't guarantee
/// that it's safe to pass your type out to foreign-language code and back again. Buggy
/// implementations of this trait might violate some assumptions made by the generated code,
/// or might not match with the corresponding code in the generated foreign-language bindings.
/// These traits should not be used directly, only in generated code, and the generated code should
/// have fixture tests to test that everything works correctly together.
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
    ///
    /// If a type implements multiple FFI traits, `FfiType` must be the same for all of them.
    type FfiType: FfiDefault;

    /// Lower a rust value of the target type, into an FFI value of type Self::FfiType.
    ///
    /// This trait method is used for sending data from rust to the foreign language code,
    /// by (hopefully cheaply!) converting it into something that can be passed over the FFI
    /// and reconstructed on the other side.
    ///
    /// Note that this method takes an owned value; this allows it to transfer ownership in turn to
    /// the foreign language code, e.g. by boxing the value and passing a pointer.
    fn lower(obj: Self) -> Self::FfiType;

    /// Lift a rust value of the target type, from an FFI value of type Self::FfiType.
    ///
    /// This trait method is used for receiving data from the foreign language code in rust,
    /// by (hopefully cheaply!) converting it from a low-level FFI value of type Self::FfiType
    /// into a high-level rust value of the target type.
    ///
    /// Since we cannot statically guarantee that the foreign-language code will send valid
    /// values of type Self::FfiType, this method is fallible.
    fn try_lift(v: Self::FfiType) -> Result<Self>;

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

    /// Type ID metadata, serialized into a [MetadataBuffer].
    ///
    /// If a type implements multiple FFI traits, `TYPE_ID_META` must be the same for all of them.
    const TYPE_ID_META: MetadataBuffer;
}

/// FfiConverter for Arc-types
///
/// This trait gets around the orphan rule limitations, which prevent library crates from
/// implementing `FfiConverter` on an Arc. When this is implemented for T, we generate an
/// `FfiConverter` impl for Arc<T>.
///
/// Note: There's no need for `FfiConverterBox`, since Box is a fundamental type.
///
/// ## Safety
///
/// All traits are unsafe (implementing it requires `unsafe impl`) because we can't guarantee
/// that it's safe to pass your type out to foreign-language code and back again. Buggy
/// implementations of this trait might violate some assumptions made by the generated code,
/// or might not match with the corresponding code in the generated foreign-language bindings.
/// These traits should not be used directly, only in generated code, and the generated code should
/// have fixture tests to test that everything works correctly together.
pub unsafe trait FfiConverterArc<UT>: Send + Sync {
    type FfiType: FfiDefault;

    fn lower(obj: Arc<Self>) -> Self::FfiType;
    fn try_lift(v: Self::FfiType) -> Result<Arc<Self>>;
    fn write(obj: Arc<Self>, buf: &mut Vec<u8>);
    fn try_read(buf: &mut &[u8]) -> Result<Arc<Self>>;

    const TYPE_ID_META: MetadataBuffer;
}

unsafe impl<T, UT> FfiConverter<UT> for Arc<T>
where
    T: FfiConverterArc<UT> + ?Sized,
{
    type FfiType = T::FfiType;

    fn lower(obj: Self) -> Self::FfiType {
        T::lower(obj)
    }

    fn try_lift(v: Self::FfiType) -> Result<Self> {
        T::try_lift(v)
    }

    fn write(obj: Self, buf: &mut Vec<u8>) {
        T::write(obj, buf)
    }

    fn try_read(buf: &mut &[u8]) -> Result<Self> {
        T::try_read(buf)
    }

    const TYPE_ID_META: MetadataBuffer = T::TYPE_ID_META;
}

/// Lift values passed by the foreign code over the FFI into Rust values
///
/// This is used by the code generation to handle arguments.  It's usually derived from
/// [FfiConverter], except for types that only support lifting but not lowering.
///
/// See [FfiConverter] for a discussion of the methods
///
/// ## Safety
///
/// All traits are unsafe (implementing it requires `unsafe impl`) because we can't guarantee
/// that it's safe to pass your type out to foreign-language code and back again. Buggy
/// implementations of this trait might violate some assumptions made by the generated code,
/// or might not match with the corresponding code in the generated foreign-language bindings.
/// These traits should not be used directly, only in generated code, and the generated code should
/// have fixture tests to test that everything works correctly together.
pub unsafe trait Lift<UT>: Sized {
    type FfiType;

    fn try_lift(v: Self::FfiType) -> Result<Self>;

    fn try_read(buf: &mut &[u8]) -> Result<Self>;

    /// Convenience method
    fn try_lift_from_rust_buffer(v: RustBuffer) -> Result<Self> {
        let vec = v.destroy_into_vec();
        let mut buf = vec.as_slice();
        let value = Self::try_read(&mut buf)?;
        match Buf::remaining(&buf) {
            0 => Ok(value),
            n => bail!("junk data left in buffer after lifting (count: {n})",),
        }
    }

    const TYPE_ID_META: MetadataBuffer;
}

/// Lower Rust values to pass them to the foreign code
///
/// This is used to pass arguments to callback interfaces. It's usually derived from
/// [FfiConverter], except for types that only support lowering but not lifting.
///
/// See [FfiConverter] for a discussion of the methods
///
/// ## Safety
///
/// All traits are unsafe (implementing it requires `unsafe impl`) because we can't guarantee
/// that it's safe to pass your type out to foreign-language code and back again. Buggy
/// implementations of this trait might violate some assumptions made by the generated code,
/// or might not match with the corresponding code in the generated foreign-language bindings.
/// These traits should not be used directly, only in generated code, and the generated code should
/// have fixture tests to test that everything works correctly together.
pub unsafe trait Lower<UT>: Sized {
    type FfiType: FfiDefault;

    fn lower(obj: Self) -> Self::FfiType;

    fn write(obj: Self, buf: &mut Vec<u8>);

    /// Convenience method
    fn lower_into_rust_buffer(obj: Self) -> RustBuffer {
        let mut buf = ::std::vec::Vec::new();
        Self::write(obj, &mut buf);
        RustBuffer::from_vec(buf)
    }

    const TYPE_ID_META: MetadataBuffer;
}

/// Return Rust values to the foreign code
///
/// This is usually derived from [Lift], but we special case types like `Result<>` and `()`.
///
/// ## Safety
///
/// All traits are unsafe (implementing it requires `unsafe impl`) because we can't guarantee
/// that it's safe to pass your type out to foreign-language code and back again. Buggy
/// implementations of this trait might violate some assumptions made by the generated code,
/// or might not match with the corresponding code in the generated foreign-language bindings.
/// These traits should not be used directly, only in generated code, and the generated code should
/// have fixture tests to test that everything works correctly together.
pub unsafe trait LowerReturn<UT>: Sized {
    /// The type that should be returned by scaffolding functions for this type.
    ///
    /// When derived, it's the same as `FfiType`.
    type ReturnType: FfiDefault;

    /// Lower this value for scaffolding function return
    ///
    /// This method converts values into the `Result<>` type that [rust_call] expects. For
    /// successful calls, return `Ok(lower_return)`.  For errors that should be translated into
    /// thrown exceptions on the foreign code, serialize the error into a RustBuffer and return
    /// `Err(buf)`
    fn lower_return(obj: Self) -> Result<Self::ReturnType, RustBuffer>;

    /// If possible, get a serialized error for failed argument lifts
    ///
    /// By default, we just panic and let `rust_call` handle things.  However, for `Result<_, E>`
    /// returns, if the anyhow error can be downcast to `E`, then serialize that and return it.
    /// This results in the foreign code throwing a "normal" exception, rather than an unexpected
    /// exception.
    fn handle_failed_lift(arg_name: &str, e: anyhow::Error) -> Self {
        panic!("Failed to convert arg '{arg_name}': {e}")
    }

    const TYPE_ID_META: MetadataBuffer;
}

/// Return foreign values to Rust
///
/// This is usually derived from [Lower], but we special case types like `Result<>` and `()`.
///
/// ## Safety
///
/// All traits are unsafe (implementing it requires `unsafe impl`) because we can't guarantee
/// that it's safe to pass your type out to foreign-language code and back again. Buggy
/// implementations of this trait might violate some assumptions made by the generated code,
/// or might not match with the corresponding code in the generated foreign-language bindings.
/// These traits should not be used directly, only in generated code, and the generated code should
/// have fixture tests to test that everything works correctly together.
pub unsafe trait LiftReturn<UT>: Sized {
    /// Lift a Rust value for a callback interface method result
    fn lift_callback_return(buf: RustBuffer) -> Self;

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
    fn handle_callback_unexpected_error(e: UnexpectedUniFFICallbackError) -> Self {
        panic!("Callback interface failure: {e}")
    }

    const TYPE_ID_META: MetadataBuffer;
}

/// Lift references
///
/// This is usually derived from [Lift] and also implemented for the inner `T` value of smart
/// pointers.  For example, if `Lift` is implemented for `Arc<T>`, then we implement this to lift
///
/// ## Safety
///
/// All traits are unsafe (implementing it requires `unsafe impl`) because we can't guarantee
/// that it's safe to pass your type out to foreign-language code and back again. Buggy
/// implementations of this trait might violate some assumptions made by the generated code,
/// or might not match with the corresponding code in the generated foreign-language bindings.
/// These traits should not be used directly, only in generated code, and the generated code should
/// have fixture tests to test that everything works correctly together.
/// `&T` using the Arc.
pub unsafe trait LiftRef<UT> {
    type LiftType: Lift<UT> + Borrow<Self>;
}

pub trait ConvertError<UT>: Sized {
    fn try_convert_unexpected_callback_error(e: UnexpectedUniFFICallbackError) -> Result<Self>;
}

/// Derive FFI traits
///
/// This can be used to derive:
///   * [Lower] and [Lift] from [FfiConverter]
///   * [LowerReturn] from [Lower]
///   * [LiftReturn] and [LiftRef] from [Lift]
///
/// Usage:
/// ```ignore
///
/// // Derive everything from [FfiConverter] for all Uniffi tags
/// ::uniffi::derive_ffi_traits!(blanket Foo)
/// // Derive everything from [FfiConverter] for the local crate::UniFfiTag
/// ::uniffi::derive_ffi_traits!(local Foo)
/// // To derive a specific trait, write out the impl item minus the actual  block
/// ::uniffi::derive_ffi_traits!(impl<T, UT> LowerReturn<UT> for Option<T>)
/// ```
#[macro_export]
#[allow(clippy::crate_in_macro_def)]
macro_rules! derive_ffi_traits {
    (blanket $ty:ty) => {
        $crate::derive_ffi_traits!(impl<UT> Lower<UT> for $ty);
        $crate::derive_ffi_traits!(impl<UT> Lift<UT> for $ty);
        $crate::derive_ffi_traits!(impl<UT> LowerReturn<UT> for $ty);
        $crate::derive_ffi_traits!(impl<UT> LiftReturn<UT> for $ty);
        $crate::derive_ffi_traits!(impl<UT> LiftRef<UT> for $ty);
        $crate::derive_ffi_traits!(impl<UT> ConvertError<UT> for $ty);
    };

    (local $ty:ty) => {
        $crate::derive_ffi_traits!(impl Lower<crate::UniFfiTag> for $ty);
        $crate::derive_ffi_traits!(impl Lift<crate::UniFfiTag> for $ty);
        $crate::derive_ffi_traits!(impl LowerReturn<crate::UniFfiTag> for $ty);
        $crate::derive_ffi_traits!(impl LiftReturn<crate::UniFfiTag> for $ty);
        $crate::derive_ffi_traits!(impl LiftRef<crate::UniFfiTag> for $ty);
        $crate::derive_ffi_traits!(impl ConvertError<crate::UniFfiTag> for $ty);
    };

    (impl $(<$($generic:ident),*>)? $(::uniffi::)? Lower<$ut:path> for $ty:ty $(where $($where:tt)*)?) => {
        unsafe impl $(<$($generic),*>)* $crate::Lower<$ut> for $ty $(where $($where)*)*
        {
            type FfiType = <Self as $crate::FfiConverter<$ut>>::FfiType;

            fn lower(obj: Self) -> Self::FfiType {
                <Self as $crate::FfiConverter<$ut>>::lower(obj)
            }

            fn write(obj: Self, buf: &mut ::std::vec::Vec<u8>) {
                <Self as $crate::FfiConverter<$ut>>::write(obj, buf)
            }

            const TYPE_ID_META: $crate::MetadataBuffer = <Self as $crate::FfiConverter<$ut>>::TYPE_ID_META;
        }
    };

    (impl $(<$($generic:ident),*>)? $(::uniffi::)? Lift<$ut:path> for $ty:ty $(where $($where:tt)*)?) => {
        unsafe impl $(<$($generic),*>)* $crate::Lift<$ut> for $ty $(where $($where)*)*
        {
            type FfiType = <Self as $crate::FfiConverter<$ut>>::FfiType;

            fn try_lift(v: Self::FfiType) -> $crate::deps::anyhow::Result<Self> {
                <Self as $crate::FfiConverter<$ut>>::try_lift(v)
            }

            fn try_read(buf: &mut &[u8]) -> $crate::deps::anyhow::Result<Self> {
                <Self as $crate::FfiConverter<$ut>>::try_read(buf)
            }

            const TYPE_ID_META: $crate::MetadataBuffer = <Self as $crate::FfiConverter<$ut>>::TYPE_ID_META;
        }
    };

    (impl $(<$($generic:ident),*>)? $(::uniffi::)? LowerReturn<$ut:path> for $ty:ty $(where $($where:tt)*)?) => {
        unsafe impl $(<$($generic),*>)* $crate::LowerReturn<$ut> for $ty $(where $($where)*)*
        {
            type ReturnType = <Self as $crate::Lower<$ut>>::FfiType;

            fn lower_return(obj: Self) -> $crate::deps::anyhow::Result<Self::ReturnType, $crate::RustBuffer> {
                Ok(<Self as $crate::Lower<$ut>>::lower(obj))
            }

            const TYPE_ID_META: $crate::MetadataBuffer =<Self as $crate::Lower<$ut>>::TYPE_ID_META;
        }
    };

    (impl $(<$($generic:ident),*>)? $(::uniffi::)? LiftReturn<$ut:path> for $ty:ty $(where $($where:tt)*)?) => {
        unsafe impl $(<$($generic),*>)* $crate::LiftReturn<$ut> for $ty $(where $($where)*)*
        {
            fn lift_callback_return(buf: $crate::RustBuffer) -> Self {
                <Self as $crate::Lift<$ut>>::try_lift_from_rust_buffer(buf)
                    .expect("Error reading callback interface result")
            }

            const TYPE_ID_META: $crate::MetadataBuffer = <Self as $crate::Lift<$ut>>::TYPE_ID_META;
        }
    };

    (impl $(<$($generic:ident),*>)? $(::uniffi::)? LiftRef<$ut:path> for $ty:ty $(where $($where:tt)*)?) => {
        unsafe impl $(<$($generic),*>)* $crate::LiftRef<$ut> for $ty $(where $($where)*)*
        {
            type LiftType = Self;
        }
    };

    (impl $(<$($generic:ident),*>)? $(::uniffi::)? ConvertError<$ut:path> for $ty:ty $(where $($where:tt)*)?) => {
        impl $(<$($generic),*>)* $crate::ConvertError<$ut> for $ty $(where $($where)*)*
        {
            fn try_convert_unexpected_callback_error(e: $crate::UnexpectedUniFFICallbackError) -> $crate::deps::anyhow::Result<Self> {
                $crate::convert_unexpected_error!(e, $ty)
            }
        }
    };
}
