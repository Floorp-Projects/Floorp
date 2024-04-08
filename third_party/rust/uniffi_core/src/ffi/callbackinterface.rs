/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Callback interfaces are traits specified in UDL which can be implemented by foreign languages.
//!
//! # Using callback interfaces
//!
//! 1. Define a Rust trait.
//!
//! This toy example defines a way of Rust accessing a key-value store exposed
//! by the host operating system (e.g. the key chain).
//!
//! ```
//! trait Keychain: Send {
//!   fn get(&self, key: String) -> Option<String>;
//!   fn put(&self, key: String, value: String);
//! }
//! ```
//!
//! 2. Define a callback interface in the UDL
//!
//! ```idl
//! callback interface Keychain {
//!     string? get(string key);
//!     void put(string key, string data);
//! };
//! ```
//!
//! 3. And allow it to be passed into Rust.
//!
//! Here, we define a constructor to pass the keychain to rust, and then another method
//! which may use it.
//!
//! In UDL:
//! ```idl
//! object Authenticator {
//!     constructor(Keychain keychain);
//!     void login();
//! }
//! ```
//!
//! In Rust:
//!
//! ```
//!# trait Keychain: Send {
//!#  fn get(&self, key: String) -> Option<String>;
//!#  fn put(&self, key: String, value: String);
//!# }
//! struct Authenticator {
//!   keychain: Box<dyn Keychain>,
//! }
//!
//! impl Authenticator {
//!   pub fn new(keychain: Box<dyn Keychain>) -> Self {
//!     Self { keychain }
//!   }
//!   pub fn login(&self) {
//!     let username = self.keychain.get("username".into());
//!     let password = self.keychain.get("password".into());
//!   }
//! }
//! ```
//! 4. Create an foreign language implementation of the callback interface.
//!
//! In this example, here's a Kotlin implementation.
//!
//! ```kotlin
//! class AndroidKeychain: Keychain {
//!     override fun get(key: String): String? {
//!         // … elide the implementation.
//!         return value
//!     }
//!     override fun put(key: String) {
//!         // … elide the implementation.
//!     }
//! }
//! ```
//! 5. Pass the implementation to Rust.
//!
//! Again, in Kotlin
//!
//! ```kotlin
//! val authenticator = Authenticator(AndroidKeychain())
//! authenticator.login()
//! ```
//!
//! # How it works.
//!
//! ## High level
//!
//! Uniffi generates a protocol or interface in client code in the foreign language must implement.
//!
//! For each callback interface, a `CallbackInternals` (on the Foreign Language side) and `ForeignCallbackInternals`
//! (on Rust side) manages the process through a `ForeignCallback`. There is one `ForeignCallback` per callback interface.
//!
//! Passing a callback interface implementation from foreign language (e.g. `AndroidKeychain`) into Rust causes the
//! `KeychainCallbackInternals` to store the instance in a handlemap.
//!
//! The object handle is passed over to Rust, and used to instantiate a struct `KeychainProxy` which implements
//! the trait. This proxy implementation is generate by Uniffi. The `KeychainProxy` object is then passed to
//! client code as `Box<dyn Keychain>`.
//!
//! Methods on `KeychainProxy` objects (e.g. `self.keychain.get("username".into())`) encode the arguments into a `RustBuffer`.
//! Using the `ForeignCallback`, it calls the `CallbackInternals` object on the foreign language side using the
//! object handle, and the method selector.
//!
//! The `CallbackInternals` object unpacks the arguments from the passed buffer, gets the object out from the handlemap,
//! and calls the actual implementation of the method.
//!
//! If there's a return value, it is packed up in to another `RustBuffer` and used as the return value for
//! `ForeignCallback`. The caller of `ForeignCallback`, the `KeychainProxy` unpacks the returned buffer into the correct
//! type and then returns to client code.
//!

use crate::{ForeignCallback, ForeignCallbackCell, Lift, LiftReturn, RustBuffer};
use std::fmt;

/// The method index used by the Drop trait to communicate to the foreign language side that Rust has finished with it,
/// and it can be deleted from the handle map.
pub const IDX_CALLBACK_FREE: u32 = 0;

/// Result of a foreign callback invocation
#[repr(i32)]
#[derive(Debug, PartialEq, Eq)]
pub enum CallbackResult {
    /// Successful call.
    /// The return value is serialized to `buf_ptr`.
    Success = 0,
    /// Expected error.
    /// This is returned when a foreign method throws an exception that corresponds to the Rust Err half of a Result.
    /// The error value is serialized to `buf_ptr`.
    Error = 1,
    /// Unexpected error.
    /// An error message string is serialized to `buf_ptr`.
    UnexpectedError = 2,
}

impl TryFrom<i32> for CallbackResult {
    // On errors we return the unconverted value
    type Error = i32;

    fn try_from(value: i32) -> Result<Self, i32> {
        match value {
            0 => Ok(Self::Success),
            1 => Ok(Self::Error),
            2 => Ok(Self::UnexpectedError),
            n => Err(n),
        }
    }
}

/// Struct to hold a foreign callback.
pub struct ForeignCallbackInternals {
    callback_cell: ForeignCallbackCell,
}

impl ForeignCallbackInternals {
    pub const fn new() -> Self {
        ForeignCallbackInternals {
            callback_cell: ForeignCallbackCell::new(),
        }
    }

    pub fn set_callback(&self, callback: ForeignCallback) {
        self.callback_cell.set(callback);
    }

    /// Invoke a callback interface method on the foreign side and return the result
    pub fn invoke_callback<R, UniFfiTag>(&self, handle: u64, method: u32, args: RustBuffer) -> R
    where
        R: LiftReturn<UniFfiTag>,
    {
        let mut ret_rbuf = RustBuffer::new();
        let callback = self.callback_cell.get();
        let raw_result = unsafe {
            callback(
                handle,
                method,
                args.data_pointer(),
                args.len() as i32,
                &mut ret_rbuf,
            )
        };
        let result = CallbackResult::try_from(raw_result)
            .unwrap_or_else(|code| panic!("Callback failed with unexpected return code: {code}"));
        match result {
            CallbackResult::Success => R::lift_callback_return(ret_rbuf),
            CallbackResult::Error => R::lift_callback_error(ret_rbuf),
            CallbackResult::UnexpectedError => {
                let reason = if !ret_rbuf.is_empty() {
                    match <String as Lift<UniFfiTag>>::try_lift(ret_rbuf) {
                        Ok(s) => s,
                        Err(e) => {
                            log::error!("{{ trait_name }} Error reading ret_buf: {e}");
                            String::from("[Error reading reason]")
                        }
                    }
                } else {
                    RustBuffer::destroy(ret_rbuf);
                    String::from("[Unknown Reason]")
                };
                R::handle_callback_unexpected_error(UnexpectedUniFFICallbackError { reason })
            }
        }
    }
}

/// Used when internal/unexpected error happened when calling a foreign callback, for example when
/// a unknown exception is raised
///
/// User callback error types must implement a From impl from this type to their own error type.
#[derive(Debug)]
pub struct UnexpectedUniFFICallbackError {
    pub reason: String,
}

impl UnexpectedUniFFICallbackError {
    pub fn from_reason(reason: String) -> Self {
        Self { reason }
    }
}

impl fmt::Display for UnexpectedUniFFICallbackError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "UnexpectedUniFFICallbackError(reason: {:?})",
            self.reason
        )
    }
}

impl std::error::Error for UnexpectedUniFFICallbackError {}

// Autoref-based specialization for converting UnexpectedUniFFICallbackError into error types.
//
// For more details, see:
// https://github.com/dtolnay/case-studies/blob/master/autoref-specialization/README.md

// Define two ZST types:
//   - One implements `try_convert_unexpected_callback_error` by always returning an error value.
//   - The specialized version implements it using `From<UnexpectedUniFFICallbackError>`

#[doc(hidden)]
#[derive(Debug)]
pub struct UnexpectedUniFFICallbackErrorConverterGeneric;

impl UnexpectedUniFFICallbackErrorConverterGeneric {
    pub fn try_convert_unexpected_callback_error<E>(
        &self,
        e: UnexpectedUniFFICallbackError,
    ) -> anyhow::Result<E> {
        Err(e.into())
    }
}

#[doc(hidden)]
#[derive(Debug)]
pub struct UnexpectedUniFFICallbackErrorConverterSpecialized;

impl UnexpectedUniFFICallbackErrorConverterSpecialized {
    pub fn try_convert_unexpected_callback_error<E>(
        &self,
        e: UnexpectedUniFFICallbackError,
    ) -> anyhow::Result<E>
    where
        E: From<UnexpectedUniFFICallbackError>,
    {
        Ok(E::from(e))
    }
}

// Macro to convert an UnexpectedUniFFICallbackError value for a particular type.  This is used in
// the `ConvertError` implementation.
#[doc(hidden)]
#[macro_export]
macro_rules! convert_unexpected_error {
    ($error:ident, $ty:ty) => {{
        // Trait for generic conversion, implemented for all &T.
        pub trait GetConverterGeneric {
            fn get_converter(&self) -> $crate::UnexpectedUniFFICallbackErrorConverterGeneric;
        }

        impl<T> GetConverterGeneric for &T {
            fn get_converter(&self) -> $crate::UnexpectedUniFFICallbackErrorConverterGeneric {
                $crate::UnexpectedUniFFICallbackErrorConverterGeneric
            }
        }
        // Trait for specialized conversion, implemented for all T that implements
        // `Into<ErrorType>`.  I.e. it's implemented for UnexpectedUniFFICallbackError when
        // ErrorType implements From<UnexpectedUniFFICallbackError>.
        pub trait GetConverterSpecialized {
            fn get_converter(&self) -> $crate::UnexpectedUniFFICallbackErrorConverterSpecialized;
        }

        impl<T: Into<$ty>> GetConverterSpecialized for T {
            fn get_converter(&self) -> $crate::UnexpectedUniFFICallbackErrorConverterSpecialized {
                $crate::UnexpectedUniFFICallbackErrorConverterSpecialized
            }
        }
        // Here's the hack.  Because of the auto-ref rules, this will use `GetConverterSpecialized`
        // if it's implemented and `GetConverterGeneric` if not.
        (&$error)
            .get_converter()
            .try_convert_unexpected_callback_error($error)
    }};
}
