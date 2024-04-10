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
//! For each callback interface, UniFFI defines a VTable.
//! This is a `repr(C)` struct where each field is a `repr(C)` callback function pointer.
//! There is one field for each method, plus an extra field for the `uniffi_free` method.
//! The foreign code registers one VTable per callback interface with Rust.
//!
//! VTable methods have a similar signature to Rust scaffolding functions.
//! The one difference is that values are returned via an out pointer to work around a Python bug (https://bugs.python.org/issue5710).
//!
//! The foreign object that implements the interface is represented by an opaque handle.
//! UniFFI generates a struct that implements the trait by calling VTable methods, passing the handle as the first parameter.
//! When the struct is dropped, the `uniffi_free` method is called.

use std::fmt;

/// Used when internal/unexpected error happened when calling a foreign callback, for example when
/// a unknown exception is raised
///
/// User callback error types must implement a From impl from this type to their own error type.
#[derive(Debug)]
pub struct UnexpectedUniFFICallbackError {
    pub reason: String,
}

impl UnexpectedUniFFICallbackError {
    pub fn new(reason: impl fmt::Display) -> Self {
        Self {
            reason: reason.to_string(),
        }
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
