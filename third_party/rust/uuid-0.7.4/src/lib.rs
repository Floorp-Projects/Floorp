// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Generate and parse UUIDs.
//!
//! Provides support for Universally Unique Identifiers (UUIDs). A UUID is a
//! unique 128-bit number, stored as 16 octets. UUIDs are used to  assign
//! unique identifiers to entities without requiring a central allocating
//! authority.
//!
//! They are particularly useful in distributed systems, though can be used in
//! disparate areas, such as databases and network protocols.  Typically a UUID
//! is displayed in a readable string form as a sequence of hexadecimal digits,
//! separated into groups by hyphens.
//!
//! The uniqueness property is not strictly guaranteed, however for all
//! practical purposes, it can be assumed that an unintentional collision would
//! be extremely unlikely.
//!
//! # Dependencies
//!
//! By default, this crate depends on nothing but `std` and cannot generate
//! [`Uuid`]s. You need to enable the following Cargo features to enable
//! various pieces of functionality:
//!
//! * `v1` - adds the `Uuid::new_v1` function and the ability to create a V1
//!   using an implementation of `uuid::v1::ClockSequence` (usually
//! `uuid::v1::Context`) and a timestamp from `time::timespec`.
//! * `v3` - adds the `Uuid::new_v3` function and the ability to create a V3
//!   UUID based on the MD5 hash of some data.
//! * `v4` - adds the `Uuid::new_v4` function and the ability to randomly
//!   generate a `Uuid`.
//! * `v5` - adds the `Uuid::new_v5` function and the ability to create a V5
//!   UUID based on the SHA1 hash of some data.
//! * `serde` - adds the ability to serialize and deserialize a `Uuid` using the
//!   `serde` crate.
//!
//! You need to enable one of the following Cargo features together with
//! `v3`, `v4` or `v5` feature if you're targeting `wasm32` architecture:
//!
//! * `stdweb` - enables support for `OsRng` on `wasm32-unknown-unknown` via
//!   `stdweb` combined with `cargo-web`
//! * `wasm-bindgen` - `wasm-bindgen` enables support for `OsRng` on
//!   `wasm32-unknown-unknown` via [`wasm-bindgen`]
//!
//! By default, `uuid` can be depended on with:
//!
//! ```toml
//! [dependencies]
//! uuid = "0.7"
//! ```
//!
//! To activate various features, use syntax like:
//!
//! ```toml
//! [dependencies]
//! uuid = { version = "0.7", features = ["serde", "v4"] }
//! ```
//!
//! You can disable default features with:
//!
//! ```toml
//! [dependencies]
//! uuid = { version = "0.7", default-features = false }
//! ```
//!
//! # Examples
//!
//! To parse a UUID given in the simple format and print it as a urn:
//!
//! ```rust
//! use uuid::Uuid;
//!
//! fn main() {
//!     let my_uuid =
//!         Uuid::parse_str("936DA01F9ABD4d9d80C702AF85C822A8").unwrap();
//!     println!("{}", my_uuid.to_urn());
//! }
//! ```
//!
//! To create a new random (V4) UUID and print it out in hexadecimal form:
//!
//! ```ignore,rust
//! // Note that this requires the `v4` feature enabled in the uuid crate.
//!
//! use uuid::Uuid;
//!
//! fn main() {
//!     let my_uuid = Uuid::new_v4();
//!     println!("{}", my_uuid);
//! }
//! ```
//!
//! # Strings
//!
//! Examples of string representations:
//!
//! * simple: `936DA01F9ABD4d9d80C702AF85C822A8`
//! * hyphenated: `550e8400-e29b-41d4-a716-446655440000`
//! * urn: `urn:uuid:F9168C5E-CEB2-4faa-B6BF-329BF39FA1E4`
//!
//! # References
//!
//! * [Wikipedia: Universally Unique Identifier](     http://en.wikipedia.org/wiki/Universally_unique_identifier)
//! * [RFC4122: A Universally Unique IDentifier (UUID) URN Namespace](     http://tools.ietf.org/html/rfc4122)
//!
//! [`wasm-bindgen`]: https://github.com/rustwasm/wasm-bindgen

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(feature = "const_fn", feature(const_fn))]
#![deny(
    missing_copy_implementations,
    missing_debug_implementations,
    missing_docs
)]
#![doc(
    html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk-v2.png",
    html_favicon_url = "https://www.rust-lang.org/favicon.ico",
    html_root_url = "https://docs.rs/uuid/0.7.4"
)]

#[cfg(feature = "byteorder")]
extern crate byteorder;
#[cfg(feature = "std")]
extern crate core;
#[cfg(feature = "md5")]
extern crate md5;
#[cfg(feature = "rand")]
extern crate rand;
#[cfg(feature = "serde")]
extern crate serde;
#[cfg(all(feature = "serde", test))]
extern crate serde_test;
#[cfg(all(feature = "serde", test))]
#[macro_use]
extern crate serde_derive;
#[cfg(feature = "sha1")]
extern crate sha1;
#[cfg(feature = "slog")]
#[cfg_attr(test, macro_use)]
extern crate slog;
#[cfg(feature = "winapi")]
extern crate winapi;

pub mod adapter;
pub mod builder;
pub mod parser;
pub mod prelude;
#[cfg(feature = "v1")]
pub mod v1;

pub use builder::Builder;

mod core_support;
#[cfg(feature = "serde")]
mod serde_support;
#[cfg(feature = "slog")]
mod slog_support;
#[cfg(feature = "std")]
mod std_support;
#[cfg(test)]
mod test_util;
#[cfg(feature = "u128")]
mod u128_support;
#[cfg(all(
    feature = "v3",
    any(
        not(target_arch = "wasm32"),
        all(
            target_arch = "wasm32",
            any(feature = "stdweb", feature = "wasm-bindgen")
        )
    )
))]
mod v3;
#[cfg(all(
    feature = "v4",
    any(
        not(target_arch = "wasm32"),
        all(
            target_arch = "wasm32",
            any(feature = "stdweb", feature = "wasm-bindgen")
        )
    )
))]
mod v4;
#[cfg(all(
    feature = "v5",
    any(
        not(target_arch = "wasm32"),
        all(
            target_arch = "wasm32",
            any(feature = "stdweb", feature = "wasm-bindgen")
        )
    )
))]
mod v5;
#[cfg(all(windows, feature = "winapi"))]
mod winapi_support;

/// A 128-bit (16 byte) buffer containing the ID.
pub type Bytes = [u8; 16];

/// The error that can occur when creating a [`Uuid`].
///
/// [`Uuid`]: struct.Uuid.html
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BytesError {
    expected: usize,
    found: usize,
}

/// A general error that can occur when handling [`Uuid`]s.
///
/// Although specialized error types exist in the crate,
/// sometimes where particular error type occurred is hidden
/// until errors need to be handled. This allows to enumerate
/// the errors.
///
/// [`Uuid`]: struct.Uuid.html
// TODO: improve the doc
// BODY: This detail should be fine for initial merge

// TODO: write tests for Error
// BODY: not immediately blocking, but should be covered for 1.0
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Error {
    /// An error occurred while handling [`Uuid`] bytes.
    ///
    /// See [`BytesError`]
    ///
    /// [`BytesError`]: struct.BytesError.html
    /// [`Uuid`]: struct.Uuid.html
    Bytes(BytesError),

    /// An error occurred while parsing a [`Uuid`] string.
    ///
    /// See [`parser::ParseError`]
    ///
    /// [`parser::ParseError`]: parser/enum.ParseError.html
    /// [`Uuid`]: struct.Uuid.html
    Parse(parser::ParseError),
}

/// The version of the UUID, denoting the generating algorithm.
#[derive(Debug, PartialEq, Copy, Clone)]
#[repr(C)]
pub enum Version {
    /// Special case for `nil` [`Uuid`].
    ///
    /// [`Uuid`]: struct.Uuid.html
    Nil = 0,
    /// Version 1: MAC address
    Mac,
    /// Version 2: DCE Security
    Dce,
    /// Version 3: MD5 hash
    Md5,
    /// Version 4: Random
    Random,
    /// Version 5: SHA-1 hash
    Sha1,
}

/// The reserved variants of UUIDs.
#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub enum Variant {
    /// Reserved by the NCS for backward compatibility
    NCS = 0,
    /// As described in the RFC4122 Specification (default)
    RFC4122,
    /// Reserved by Microsoft for backward compatibility
    Microsoft,
    /// Reserved for future expansion
    Future,
}

/// A Universally Unique Identifier (UUID).
#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Uuid(Bytes);

impl BytesError {
    /// The expected number of bytes.
    #[cfg(feature = "const_fn")]
    #[inline]
    pub const fn expected(&self) -> usize {
        self.expected
    }

    /// The expected number of bytes.
    #[cfg(not(feature = "const_fn"))]
    #[inline]
    pub fn expected(&self) -> usize {
        self.expected
    }

    /// The number of bytes found.
    #[cfg(feature = "const_fn")]
    #[inline]
    pub const fn found(&self) -> usize {
        self.found
    }

    /// The number of bytes found.
    #[cfg(not(feature = "const_fn"))]
    #[inline]
    pub fn found(&self) -> usize {
        self.found
    }

    /// Create a new [`UuidError`].
    ///
    /// [`UuidError`]: struct.UuidError.html
    #[cfg(feature = "const_fn")]
    #[inline]
    pub const fn new(expected: usize, found: usize) -> Self {
        BytesError { expected, found }
    }

    /// Create a new [`UuidError`].
    ///
    /// [`UuidError`]: struct.UuidError.html
    #[cfg(not(feature = "const_fn"))]
    #[inline]
    pub fn new(expected: usize, found: usize) -> Self {
        BytesError { expected, found }
    }
}

impl Uuid {
    /// [`Uuid`] namespace for Domain Name System (DNS).
    ///
    /// [`Uuid`]: struct.Uuid.html
    pub const NAMESPACE_DNS: Self = Uuid([
        0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0,
        0x4f, 0xd4, 0x30, 0xc8,
    ]);

    /// [`Uuid`] namespace for ISO Object Identifiers (OIDs).
    ///
    /// [`Uuid`]: struct.Uuid.html
    pub const NAMESPACE_OID: Self = Uuid([
        0x6b, 0xa7, 0xb8, 0x12, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0,
        0x4f, 0xd4, 0x30, 0xc8,
    ]);

    /// [`Uuid`] namespace for Uniform Resource Locators (URLs).
    ///
    /// [`Uuid`]: struct.Uuid.html
    pub const NAMESPACE_URL: Self = Uuid([
        0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0,
        0x4f, 0xd4, 0x30, 0xc8,
    ]);

    /// [`Uuid`] namespace for X.500 Distinguished Names (DNs).
    ///
    /// [`Uuid`]: struct.Uuid.html
    pub const NAMESPACE_X500: Self = Uuid([
        0x6b, 0xa7, 0xb8, 0x14, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0,
        0x4f, 0xd4, 0x30, 0xc8,
    ]);

    /// The 'nil UUID'.
    ///
    /// The nil UUID is special form of UUID that is specified to have all
    /// 128 bits set to zero, as defined in [IETF RFC 4122 Section 4.1.7][RFC].
    ///
    /// [RFC]: https://tools.ietf.org/html/rfc4122.html#section-4.1.7
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    ///
    /// assert_eq!(
    ///     uuid.to_hyphenated().to_string(),
    ///     "00000000-0000-0000-0000-000000000000"
    /// );
    /// ```
    #[cfg(feature = "const_fn")]
    pub const fn nil() -> Self {
        Uuid::from_bytes([0; 16])
    }

    /// The 'nil UUID'.
    ///
    /// The nil UUID is special form of UUID that is specified to have all
    /// 128 bits set to zero, as defined in [IETF RFC 4122 Section 4.1.7][RFC].
    ///
    /// [RFC]: https://tools.ietf.org/html/rfc4122.html#section-4.1.7
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    ///
    /// assert_eq!(
    ///     uuid.to_hyphenated().to_string(),
    ///     "00000000-0000-0000-0000-000000000000"
    /// );
    /// ```
    #[cfg(not(feature = "const_fn"))]
    pub fn nil() -> Uuid {
        Uuid::from_bytes([0; 16])
    }

    /// Creates a `Uuid` from four field values in big-endian order.
    ///
    /// # Errors
    ///
    /// This function will return an error if `d4`'s length is not 8 bytes.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let d4 = [12, 3, 9, 56, 54, 43, 8, 9];
    ///
    /// let uuid = Uuid::from_fields(42, 12, 5, &d4);
    /// let uuid = uuid.map(|uuid| uuid.to_hyphenated().to_string());
    ///
    /// let expected_uuid =
    ///     Ok(String::from("0000002a-000c-0005-0c03-0938362b0809"));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    ///
    /// An invalid length:
    ///
    /// ```
    /// use uuid::prelude::*;
    ///
    /// let d4 = [12];
    ///
    /// let uuid = uuid::Uuid::from_fields(42, 12, 5, &d4);
    ///
    /// let expected_uuid = Err(uuid::BytesError::new(8, d4.len()));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    pub fn from_fields(
        d1: u32,
        d2: u16,
        d3: u16,
        d4: &[u8],
    ) -> Result<Uuid, BytesError> {
        const D4_LEN: usize = 8;

        let len = d4.len();

        if len != D4_LEN {
            return Err(BytesError::new(D4_LEN, len));
        }

        Ok(Uuid::from_bytes([
            (d1 >> 24) as u8,
            (d1 >> 16) as u8,
            (d1 >> 8) as u8,
            d1 as u8,
            (d2 >> 8) as u8,
            d2 as u8,
            (d3 >> 8) as u8,
            d3 as u8,
            d4[0],
            d4[1],
            d4[2],
            d4[3],
            d4[4],
            d4[5],
            d4[6],
            d4[7],
        ]))
    }

    /// Creates a `Uuid` from four field values in little-endian order.
    ///
    /// The bytes in the `d1`, `d2` and `d3` fields will
    /// be converted into big-endian order.
    ///
    /// # Examples
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let d1 = 0xAB3F1097u32;
    /// let d2 = 0x501Eu16;
    /// let d3 = 0xB736u16;
    /// let d4 = [12, 3, 9, 56, 54, 43, 8, 9];
    ///
    /// let uuid = Uuid::from_fields_le(d1, d2, d3, &d4);
    /// let uuid = uuid.map(|uuid| uuid.to_hyphenated().to_string());
    ///
    /// let expected_uuid =
    ///     Ok(String::from("97103fab-1e50-36b7-0c03-0938362b0809"));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    pub fn from_fields_le(
        d1: u32,
        d2: u16,
        d3: u16,
        d4: &[u8],
    ) -> Result<Uuid, BytesError> {
        const D4_LEN: usize = 8;

        let len = d4.len();

        if len != D4_LEN {
            return Err(BytesError::new(D4_LEN, len));
        }

        Ok(Uuid::from_bytes([
            d1 as u8,
            (d1 >> 8) as u8,
            (d1 >> 16) as u8,
            (d1 >> 24) as u8,
            (d2) as u8,
            (d2 >> 8) as u8,
            d3 as u8,
            (d3 >> 8) as u8,
            d4[0],
            d4[1],
            d4[2],
            d4[3],
            d4[4],
            d4[5],
            d4[6],
            d4[7],
        ]))
    }

    /// Creates a `Uuid` using the supplied big-endian bytes.
    ///
    /// # Errors
    ///
    /// This function will return an error if `b` has any length other than 16.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let bytes = [4, 54, 67, 12, 43, 2, 98, 76, 32, 50, 87, 5, 1, 33, 43, 87];
    ///
    /// let uuid = Uuid::from_slice(&bytes);
    /// let uuid = uuid.map(|uuid| uuid.to_hyphenated().to_string());
    ///
    /// let expected_uuid =
    ///     Ok(String::from("0436430c-2b02-624c-2032-570501212b57"));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    ///
    /// An incorrect number of bytes:
    ///
    /// ```
    /// use uuid::prelude::*;
    ///
    /// let bytes = [4, 54, 67, 12, 43, 2, 98, 76];
    ///
    /// let uuid = Uuid::from_slice(&bytes);
    ///
    /// let expected_uuid = Err(uuid::BytesError::new(16, 8));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    pub fn from_slice(b: &[u8]) -> Result<Uuid, BytesError> {
        const BYTES_LEN: usize = 16;

        let len = b.len();

        if len != BYTES_LEN {
            return Err(BytesError::new(BYTES_LEN, len));
        }

        let mut bytes: Bytes = [0; 16];
        bytes.copy_from_slice(b);
        Ok(Uuid::from_bytes(bytes))
    }

    /// Creates a `Uuid` using the supplied big-endian bytes.
    #[cfg(not(feature = "const_fn"))]
    pub fn from_bytes(bytes: Bytes) -> Uuid {
        Uuid(bytes)
    }

    /// Creates a `Uuid` using the supplied big-endian bytes.
    #[cfg(feature = "const_fn")]
    pub const fn from_bytes(bytes: Bytes) -> Uuid {
        Uuid(bytes)
    }

    /// Creates a v4 Uuid from random bytes (e.g. bytes supplied from `Rand`
    /// crate)
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Bytes;
    /// use uuid::Uuid;
    ///
    /// let bytes: Bytes = [
    ///     70, 235, 208, 238, 14, 109, 67, 201, 185, 13, 204, 195, 90, 145, 63, 62,
    /// ];
    /// let uuid = Uuid::from_random_bytes(bytes);
    /// let uuid = uuid.to_hyphenated().to_string();
    ///
    /// let expected_uuid = String::from("46ebd0ee-0e6d-43c9-b90d-ccc35a913f3e");
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    #[deprecated(
        since = "0.7.2",
        note = "please use the `uuid::Builder` instead to set the variant and version"
    )]
    pub fn from_random_bytes(bytes: Bytes) -> Uuid {
        let mut uuid = Uuid::from_bytes(bytes);
        uuid.set_variant(Variant::RFC4122);
        uuid.set_version(Version::Random);
        uuid
    }

    /// Specifies the variant of the UUID structure
    fn set_variant(&mut self, v: Variant) {
        // Octet 8 contains the variant in the most significant 3 bits
        self.0[8] = match v {
            Variant::NCS => self.as_bytes()[8] & 0x7f, // b0xx...
            Variant::RFC4122 => (self.as_bytes()[8] & 0x3f) | 0x80, // b10x...
            Variant::Microsoft => (self.as_bytes()[8] & 0x1f) | 0xc0, // b110...
            Variant::Future => (self.as_bytes()[8] & 0x1f) | 0xe0, // b111...
        }
    }

    /// Returns the variant of the `Uuid` structure.
    ///
    /// This determines the interpretation of the structure of the UUID.
    /// Currently only the RFC4122 variant is generated by this module.
    ///
    /// * [Variant Reference](http://tools.ietf.org/html/rfc4122#section-4.1.1)
    pub fn get_variant(&self) -> Option<Variant> {
        match self.as_bytes()[8] {
            x if x & 0x80 == 0x00 => Some(Variant::NCS),
            x if x & 0xc0 == 0x80 => Some(Variant::RFC4122),
            x if x & 0xe0 == 0xc0 => Some(Variant::Microsoft),
            x if x & 0xe0 == 0xe0 => Some(Variant::Future),
            _ => None,
        }
    }

    /// Specifies the version number of the `Uuid`.
    fn set_version(&mut self, v: Version) {
        self.0[6] = (self.as_bytes()[6] & 0xF) | ((v as u8) << 4);
    }

    /// Returns the version number of the `Uuid`.
    ///
    /// This represents the algorithm used to generate the contents.
    ///
    /// Currently only the Random (V4) algorithm is supported by this
    /// module.  There are security and privacy implications for using
    /// older versions - see [Wikipedia: Universally Unique Identifier](
    /// http://en.wikipedia.org/wiki/Universally_unique_identifier) for
    /// details.
    ///
    /// * [Version Reference](http://tools.ietf.org/html/rfc4122#section-4.1.3)
    pub fn get_version_num(&self) -> usize {
        (self.as_bytes()[6] >> 4) as usize
    }

    /// Returns the version of the `Uuid`.
    ///
    /// This represents the algorithm used to generate the contents
    pub fn get_version(&self) -> Option<Version> {
        let v = self.as_bytes()[6] >> 4;
        match v {
            0 if self.is_nil() => Some(Version::Nil),
            1 => Some(Version::Mac),
            2 => Some(Version::Dce),
            3 => Some(Version::Md5),
            4 => Some(Version::Random),
            5 => Some(Version::Sha1),
            _ => None,
        }
    }

    /// Returns the four field values of the UUID in big-endian order.
    ///
    /// These values can be passed to the `from_fields()` method to get the
    /// original `Uuid` back.
    ///
    /// * The first field value represents the first group of (eight) hex
    ///   digits, taken as a big-endian `u32` value.  For V1 UUIDs, this field
    ///   represents the low 32 bits of the timestamp.
    /// * The second field value represents the second group of (four) hex
    ///   digits, taken as a big-endian `u16` value.  For V1 UUIDs, this field
    ///   represents the middle 16 bits of the timestamp.
    /// * The third field value represents the third group of (four) hex digits,
    ///   taken as a big-endian `u16` value.  The 4 most significant bits give
    ///   the UUID version, and for V1 UUIDs, the last 12 bits represent the
    ///   high 12 bits of the timestamp.
    /// * The last field value represents the last two groups of four and twelve
    ///   hex digits, taken in order.  The first 1-3 bits of this indicate the
    ///   UUID variant, and for V1 UUIDs, the next 13-15 bits indicate the clock
    ///   sequence and the last 48 bits indicate the node ID.
    ///
    /// # Examples
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    /// assert_eq!(uuid.as_fields(), (0, 0, 0, &[0u8; 8]));
    ///
    /// let uuid = Uuid::parse_str("936DA01F-9ABD-4D9D-80C7-02AF85C822A8").unwrap();
    /// assert_eq!(
    ///     uuid.as_fields(),
    ///     (
    ///         0x936DA01F,
    ///         0x9ABD,
    ///         0x4D9D,
    ///         b"\x80\xC7\x02\xAF\x85\xC8\x22\xA8"
    ///     )
    /// );
    /// ```
    pub fn as_fields(&self) -> (u32, u16, u16, &[u8; 8]) {
        let d1 = u32::from(self.as_bytes()[0]) << 24
            | u32::from(self.as_bytes()[1]) << 16
            | u32::from(self.as_bytes()[2]) << 8
            | u32::from(self.as_bytes()[3]);

        let d2 =
            u16::from(self.as_bytes()[4]) << 8 | u16::from(self.as_bytes()[5]);

        let d3 =
            u16::from(self.as_bytes()[6]) << 8 | u16::from(self.as_bytes()[7]);

        let d4: &[u8; 8] =
            unsafe { &*(self.as_bytes()[8..16].as_ptr() as *const [u8; 8]) };
        (d1, d2, d3, d4)
    }

    /// Returns the four field values of the UUID in little-endian order.
    ///
    /// The bytes in the returned integer fields will
    /// be converted from big-endian order.
    ///
    /// # Examples
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::parse_str("936DA01F-9ABD-4D9D-80C7-02AF85C822A8").unwrap();
    /// assert_eq!(
    ///     uuid.to_fields_le(),
    ///     (
    ///         0x1FA06D93,
    ///         0xBD9A,
    ///         0x9D4D,
    ///         b"\x80\xC7\x02\xAF\x85\xC8\x22\xA8"
    ///     )
    /// );
    /// ```
    pub fn to_fields_le(&self) -> (u32, u16, u16, &[u8; 8]) {
        let d1 = u32::from(self.as_bytes()[0])
            | u32::from(self.as_bytes()[1]) << 8
            | u32::from(self.as_bytes()[2]) << 16
            | u32::from(self.as_bytes()[3]) << 24;

        let d2 =
            u16::from(self.as_bytes()[4]) | u16::from(self.as_bytes()[5]) << 8;

        let d3 =
            u16::from(self.as_bytes()[6]) | u16::from(self.as_bytes()[7]) << 8;

        let d4: &[u8; 8] =
            unsafe { &*(self.as_bytes()[8..16].as_ptr() as *const [u8; 8]) };
        (d1, d2, d3, d4)
    }

    /// Returns an array of 16 octets containing the UUID data.
    /// This method wraps [`Uuid::as_bytes`]
    #[cfg(feature = "const_fn")]
    pub const fn as_bytes(&self) -> &Bytes {
        &self.0
    }

    /// Returns an array of 16 octets containing the UUID data.
    /// This method wraps [`Uuid::as_bytes`]
    #[cfg(not(feature = "const_fn"))]
    pub fn as_bytes(&self) -> &Bytes {
        &self.0
    }

    /// Returns an Optional Tuple of (u64, u16) representing the timestamp and
    /// counter portion of a V1 UUID.  If the supplied UUID is not V1, this
    /// will return None
    pub fn to_timestamp(&self) -> Option<(u64, u16)> {
        if self
            .get_version()
            .map(|v| v != Version::Mac)
            .unwrap_or(true)
        {
            return None;
        }

        let ts: u64 = u64::from(self.as_bytes()[6] & 0x0F) << 56
            | u64::from(self.as_bytes()[7]) << 48
            | u64::from(self.as_bytes()[4]) << 40
            | u64::from(self.as_bytes()[5]) << 32
            | u64::from(self.as_bytes()[0]) << 24
            | u64::from(self.as_bytes()[1]) << 16
            | u64::from(self.as_bytes()[2]) << 8
            | u64::from(self.as_bytes()[3]);

        let count: u16 = u16::from(self.as_bytes()[8] & 0x3F) << 8
            | u16::from(self.as_bytes()[9]);

        Some((ts, count))
    }

    /// Parses a `Uuid` from a string of hexadecimal digits with optional
    /// hyphens.
    ///
    /// Any of the formats generated by this module (simple, hyphenated, urn)
    /// are supported by this parsing function.
    pub fn parse_str(mut input: &str) -> Result<Uuid, parser::ParseError> {
        // Ensure length is valid for any of the supported formats
        let len = input.len();

        if len == adapter::Urn::LENGTH && input.starts_with("urn:uuid:") {
            input = &input[9..];
        } else if !parser::len_matches_any(
            len,
            &[adapter::Hyphenated::LENGTH, adapter::Simple::LENGTH],
        ) {
            return Err(parser::ParseError::InvalidLength {
                expected: parser::Expected::Any(&[
                    adapter::Hyphenated::LENGTH,
                    adapter::Simple::LENGTH,
                ]),
                found: len,
            });
        }

        // `digit` counts only hexadecimal digits, `i_char` counts all chars.
        let mut digit = 0;
        let mut group = 0;
        let mut acc = 0;
        let mut buffer = [0u8; 16];

        for (i_char, chr) in input.bytes().enumerate() {
            if digit as usize >= adapter::Simple::LENGTH && group != 4 {
                if group == 0 {
                    return Err(parser::ParseError::InvalidLength {
                        expected: parser::Expected::Any(&[
                            adapter::Hyphenated::LENGTH,
                            adapter::Simple::LENGTH,
                        ]),
                        found: len,
                    });
                }

                return Err(parser::ParseError::InvalidGroupCount {
                    expected: parser::Expected::Any(&[1, 5]),
                    found: group + 1,
                });
            }

            if digit % 2 == 0 {
                // First digit of the byte.
                match chr {
                    // Calulate upper half.
                    b'0'...b'9' => acc = chr - b'0',
                    b'a'...b'f' => acc = chr - b'a' + 10,
                    b'A'...b'F' => acc = chr - b'A' + 10,
                    // Found a group delimiter
                    b'-' => {
                        // TODO: remove the u8 cast
                        // BODY: this only needed until we switch to
                        //       ParseError
                        if parser::ACC_GROUP_LENS[group] as u8 != digit {
                            // Calculate how many digits this group consists of
                            // in the input.
                            let found = if group > 0 {
                                // TODO: remove the u8 cast
                                // BODY: this only needed until we switch to
                                //       ParseError
                                digit - parser::ACC_GROUP_LENS[group - 1] as u8
                            } else {
                                digit
                            };

                            return Err(
                                parser::ParseError::InvalidGroupLength {
                                    expected: parser::Expected::Exact(
                                        parser::GROUP_LENS[group],
                                    ),
                                    found: found as usize,
                                    group,
                                },
                            );
                        }
                        // Next group, decrement digit, it is incremented again
                        // at the bottom.
                        group += 1;
                        digit -= 1;
                    }
                    _ => {
                        return Err(parser::ParseError::InvalidCharacter {
                            expected: "0123456789abcdefABCDEF-",
                            found: input[i_char..].chars().next().unwrap(),
                            index: i_char,
                        });
                    }
                }
            } else {
                // Second digit of the byte, shift the upper half.
                acc *= 16;
                match chr {
                    b'0'...b'9' => acc += chr - b'0',
                    b'a'...b'f' => acc += chr - b'a' + 10,
                    b'A'...b'F' => acc += chr - b'A' + 10,
                    b'-' => {
                        // The byte isn't complete yet.
                        let found = if group > 0 {
                            // TODO: remove the u8 cast
                            // BODY: this only needed until we switch to
                            //       ParseError
                            digit - parser::ACC_GROUP_LENS[group - 1] as u8
                        } else {
                            digit
                        };

                        return Err(parser::ParseError::InvalidGroupLength {
                            expected: parser::Expected::Exact(
                                parser::GROUP_LENS[group],
                            ),
                            found: found as usize,
                            group,
                        });
                    }
                    _ => {
                        return Err(parser::ParseError::InvalidCharacter {
                            expected: "0123456789abcdefABCDEF-",
                            found: input[i_char..].chars().next().unwrap(),
                            index: i_char,
                        });
                    }
                }
                buffer[(digit / 2) as usize] = acc;
            }
            digit += 1;
        }

        // Now check the last group.
        // TODO: remove the u8 cast
        // BODY: this only needed until we switch to
        //       ParseError
        if parser::ACC_GROUP_LENS[4] as u8 != digit {
            return Err(parser::ParseError::InvalidGroupLength {
                expected: parser::Expected::Exact(parser::GROUP_LENS[4]),
                found: (digit as usize - parser::ACC_GROUP_LENS[3]),
                group,
            });
        }

        Ok(Uuid::from_bytes(buffer))
    }

    /// Tests if the UUID is nil
    pub fn is_nil(&self) -> bool {
        self.as_bytes().iter().all(|&b| b == 0)
    }

    /// A buffer that can be used for `encode_...` calls, that is
    /// guaranteed to be long enough for any of the adapters.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    ///
    /// assert_eq!(
    ///     uuid.to_simple().encode_lower(&mut Uuid::encode_buffer()),
    ///     "00000000000000000000000000000000"
    /// );
    ///
    /// assert_eq!(
    ///     uuid.to_hyphenated()
    ///         .encode_lower(&mut Uuid::encode_buffer()),
    ///     "00000000-0000-0000-0000-000000000000"
    /// );
    ///
    /// assert_eq!(
    ///     uuid.to_urn().encode_lower(&mut Uuid::encode_buffer()),
    ///     "urn:uuid:00000000-0000-0000-0000-000000000000"
    /// );
    /// ```
    pub fn encode_buffer() -> [u8; adapter::Urn::LENGTH] {
        [0; adapter::Urn::LENGTH]
    }
}

#[cfg(test)]
mod tests {
    extern crate std;

    use self::std::prelude::v1::*;
    use super::test_util;
    use prelude::*;

    #[test]
    fn test_nil() {
        let nil = Uuid::nil();
        let not_nil = test_util::new();
        let from_bytes = Uuid::from_bytes([
            4, 54, 67, 12, 43, 2, 2, 76, 32, 50, 87, 5, 1, 33, 43, 87,
        ]);

        assert_eq!(from_bytes.get_version(), None);

        assert!(nil.is_nil());
        assert!(!not_nil.is_nil());

        assert_eq!(nil.get_version(), Some(Version::Nil));
        assert_eq!(not_nil.get_version(), Some(Version::Random))
    }

    #[test]
    fn test_predefined_namespaces() {
        assert_eq!(
            Uuid::NAMESPACE_DNS.to_hyphenated().to_string(),
            "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
        );
        assert_eq!(
            Uuid::NAMESPACE_URL.to_hyphenated().to_string(),
            "6ba7b811-9dad-11d1-80b4-00c04fd430c8"
        );
        assert_eq!(
            Uuid::NAMESPACE_OID.to_hyphenated().to_string(),
            "6ba7b812-9dad-11d1-80b4-00c04fd430c8"
        );
        assert_eq!(
            Uuid::NAMESPACE_X500.to_hyphenated().to_string(),
            "6ba7b814-9dad-11d1-80b4-00c04fd430c8"
        );
    }

    #[cfg(feature = "v3")]
    #[test]
    fn test_get_version_v3() {
        let uuid =
            Uuid::new_v3(&Uuid::NAMESPACE_DNS, "rust-lang.org".as_bytes());

        assert_eq!(uuid.get_version().unwrap(), Version::Md5);
        assert_eq!(uuid.get_version_num(), 3);
    }

    #[test]
    fn test_get_variant() {
        let uuid1 = test_util::new();
        let uuid2 =
            Uuid::parse_str("550e8400-e29b-41d4-a716-446655440000").unwrap();
        let uuid3 =
            Uuid::parse_str("67e55044-10b1-426f-9247-bb680e5fe0c8").unwrap();
        let uuid4 =
            Uuid::parse_str("936DA01F9ABD4d9dC0C702AF85C822A8").unwrap();
        let uuid5 =
            Uuid::parse_str("F9168C5E-CEB2-4faa-D6BF-329BF39FA1E4").unwrap();
        let uuid6 =
            Uuid::parse_str("f81d4fae-7dec-11d0-7765-00a0c91e6bf6").unwrap();

        assert_eq!(uuid1.get_variant().unwrap(), Variant::RFC4122);
        assert_eq!(uuid2.get_variant().unwrap(), Variant::RFC4122);
        assert_eq!(uuid3.get_variant().unwrap(), Variant::RFC4122);
        assert_eq!(uuid4.get_variant().unwrap(), Variant::Microsoft);
        assert_eq!(uuid5.get_variant().unwrap(), Variant::Microsoft);
        assert_eq!(uuid6.get_variant().unwrap(), Variant::NCS);
    }

    #[test]
    fn test_parse_uuid_v4() {
        use adapter;
        use parser;

        const EXPECTED_UUID_LENGTHS: parser::Expected =
            parser::Expected::Any(&[
                adapter::Hyphenated::LENGTH,
                adapter::Simple::LENGTH,
            ]);

        const EXPECTED_GROUP_COUNTS: parser::Expected =
            parser::Expected::Any(&[1, 5]);

        const EXPECTED_CHARS: &'static str = "0123456789abcdefABCDEF-";

        // Invalid
        assert_eq!(
            Uuid::parse_str(""),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 0,
            })
        );

        assert_eq!(
            Uuid::parse_str("!"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 1
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF-329BF39FA1E45"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 37,
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-BBF-329BF39FA1E4"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 35
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-BGBF-329BF39FA1E4"),
            Err(parser::ParseError::InvalidCharacter {
                expected: EXPECTED_CHARS,
                found: 'G',
                index: 20,
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2F4faaFB6BFF329BF39FA1E4"),
            Err(parser::ParseError::InvalidGroupCount {
                expected: EXPECTED_GROUP_COUNTS,
                found: 2
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faaFB6BFF329BF39FA1E4"),
            Err(parser::ParseError::InvalidGroupCount {
                expected: EXPECTED_GROUP_COUNTS,
                found: 3,
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-B6BFF329BF39FA1E4"),
            Err(parser::ParseError::InvalidGroupCount {
                expected: EXPECTED_GROUP_COUNTS,
                found: 4,
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 18,
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faaXB6BFF329BF39FA1E4"),
            Err(parser::ParseError::InvalidCharacter {
                expected: EXPECTED_CHARS,
                found: 'X',
                index: 18,
            })
        );

        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB-24fa-eB6BFF32-BF39FA1E4"),
            Err(parser::ParseError::InvalidGroupLength {
                expected: parser::Expected::Exact(4),
                found: 3,
                group: 1,
            })
        );
        // (group, found, expecting)
        //
        assert_eq!(
            Uuid::parse_str("01020304-1112-2122-3132-41424344"),
            Err(parser::ParseError::InvalidGroupLength {
                expected: parser::Expected::Exact(12),
                found: 8,
                group: 4,
            })
        );

        assert_eq!(
            Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 31,
            })
        );

        assert_eq!(
            Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c88"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 33,
            })
        );

        assert_eq!(
            Uuid::parse_str("67e5504410b1426f9247bb680e5fe0cg8"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 33,
            })
        );

        assert_eq!(
            Uuid::parse_str("67e5504410b1426%9247bb680e5fe0c8"),
            Err(parser::ParseError::InvalidCharacter {
                expected: EXPECTED_CHARS,
                found: '%',
                index: 15,
            })
        );

        assert_eq!(
            Uuid::parse_str("231231212212423424324323477343246663"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 36,
            })
        );

        // Valid
        assert!(Uuid::parse_str("00000000000000000000000000000000").is_ok());
        assert!(Uuid::parse_str("67e55044-10b1-426f-9247-bb680e5fe0c8").is_ok());
        assert!(Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF-329BF39FA1E4").is_ok());
        assert!(Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c8").is_ok());
        assert!(Uuid::parse_str("01020304-1112-2122-3132-414243444546").is_ok());
        assert!(Uuid::parse_str(
            "urn:uuid:67e55044-10b1-426f-9247-bb680e5fe0c8"
        )
        .is_ok());

        // Nil
        let nil = Uuid::nil();
        assert_eq!(
            Uuid::parse_str("00000000000000000000000000000000").unwrap(),
            nil
        );
        assert_eq!(
            Uuid::parse_str("00000000-0000-0000-0000-000000000000").unwrap(),
            nil
        );

        // Round-trip
        let uuid_orig = test_util::new();
        let orig_str = uuid_orig.to_string();
        let uuid_out = Uuid::parse_str(&orig_str).unwrap();
        assert_eq!(uuid_orig, uuid_out);

        // Test error reporting
        assert_eq!(
            Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c"),
            Err(parser::ParseError::InvalidLength {
                expected: EXPECTED_UUID_LENGTHS,
                found: 31,
            })
        );
        assert_eq!(
            Uuid::parse_str("67e550X410b1426f9247bb680e5fe0cd"),
            Err(parser::ParseError::InvalidCharacter {
                expected: EXPECTED_CHARS,
                found: 'X',
                index: 6,
            })
        );
        assert_eq!(
            Uuid::parse_str("67e550-4105b1426f9247bb680e5fe0c"),
            Err(parser::ParseError::InvalidGroupLength {
                expected: parser::Expected::Exact(8),
                found: 6,
                group: 0,
            })
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF1-02BF39FA1E4"),
            Err(parser::ParseError::InvalidGroupLength {
                expected: parser::Expected::Exact(4),
                found: 5,
                group: 3,
            })
        );
    }

    #[test]
    fn test_to_simple_string() {
        let uuid1 = test_util::new();
        let s = uuid1.to_simple().to_string();

        assert_eq!(s.len(), 32);
        assert!(s.chars().all(|c| c.is_digit(16)));
    }

    #[test]
    fn test_to_hyphenated_string() {
        let uuid1 = test_util::new();
        let s = uuid1.to_hyphenated().to_string();

        assert!(s.len() == 36);
        assert!(s.chars().all(|c| c.is_digit(16) || c == '-'));
    }

    #[test]
    fn test_upper_lower_hex() {
        use tests::std::fmt::Write;

        let mut buf = String::new();
        let u = test_util::new();

        macro_rules! check {
            ($buf:ident, $format:expr, $target:expr, $len:expr, $cond:expr) => {
                $buf.clear();
                write!($buf, $format, $target).unwrap();
                assert!(buf.len() == $len);
                assert!($buf.chars().all($cond), "{}", $buf);
            };
        }

        check!(buf, "{:X}", u, 36, |c| c.is_uppercase()
            || c.is_digit(10)
            || c == '-');
        check!(buf, "{:X}", u.to_hyphenated(), 36, |c| c.is_uppercase()
            || c.is_digit(10)
            || c == '-');
        check!(buf, "{:X}", u.to_simple(), 32, |c| c.is_uppercase()
            || c.is_digit(10));

        check!(buf, "{:x}", u.to_hyphenated(), 36, |c| c.is_lowercase()
            || c.is_digit(10)
            || c == '-');
        check!(buf, "{:x}", u.to_simple(), 32, |c| c.is_lowercase()
            || c.is_digit(10));
    }

    #[test]
    fn test_to_urn_string() {
        let uuid1 = test_util::new();
        let ss = uuid1.to_urn().to_string();
        let s = &ss[9..];

        assert!(ss.starts_with("urn:uuid:"));
        assert_eq!(s.len(), 36);
        assert!(s.chars().all(|c| c.is_digit(16) || c == '-'));
    }

    #[test]
    fn test_to_simple_string_matching() {
        let uuid1 = test_util::new();

        let hs = uuid1.to_hyphenated().to_string();
        let ss = uuid1.to_simple().to_string();

        let hsn = hs.chars().filter(|&c| c != '-').collect::<String>();

        assert_eq!(hsn, ss);
    }

    #[test]
    fn test_string_roundtrip() {
        let uuid = test_util::new();

        let hs = uuid.to_hyphenated().to_string();
        let uuid_hs = Uuid::parse_str(&hs).unwrap();
        assert_eq!(uuid_hs, uuid);

        let ss = uuid.to_string();
        let uuid_ss = Uuid::parse_str(&ss).unwrap();
        assert_eq!(uuid_ss, uuid);
    }

    #[test]
    fn test_from_fields() {
        let d1: u32 = 0xa1a2a3a4;
        let d2: u16 = 0xb1b2;
        let d3: u16 = 0xc1c2;
        let d4 = [0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8];

        let u = Uuid::from_fields(d1, d2, d3, &d4).unwrap();

        let expected = "a1a2a3a4b1b2c1c2d1d2d3d4d5d6d7d8";
        let result = u.to_simple().to_string();
        assert_eq!(result, expected);
    }

    #[test]
    fn test_from_fields_le() {
        let d1: u32 = 0xa4a3a2a1;
        let d2: u16 = 0xb2b1;
        let d3: u16 = 0xc2c1;
        let d4 = [0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8];

        let u = Uuid::from_fields_le(d1, d2, d3, &d4).unwrap();

        let expected = "a1a2a3a4b1b2c1c2d1d2d3d4d5d6d7d8";
        let result = u.to_simple().to_string();
        assert_eq!(result, expected);
    }

    #[test]
    fn test_as_fields() {
        let u = test_util::new();
        let (d1, d2, d3, d4) = u.as_fields();

        assert_ne!(d1, 0);
        assert_ne!(d2, 0);
        assert_ne!(d3, 0);
        assert_eq!(d4.len(), 8);
        assert!(!d4.iter().all(|&b| b == 0));
    }

    #[test]
    fn test_fields_roundtrip() {
        let d1_in: u32 = 0xa1a2a3a4;
        let d2_in: u16 = 0xb1b2;
        let d3_in: u16 = 0xc1c2;
        let d4_in = &[0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8];

        let u = Uuid::from_fields(d1_in, d2_in, d3_in, d4_in).unwrap();
        let (d1_out, d2_out, d3_out, d4_out) = u.as_fields();

        assert_eq!(d1_in, d1_out);
        assert_eq!(d2_in, d2_out);
        assert_eq!(d3_in, d3_out);
        assert_eq!(d4_in, d4_out);
    }

    #[test]
    fn test_fields_le_roundtrip() {
        let d1_in: u32 = 0xa4a3a2a1;
        let d2_in: u16 = 0xb2b1;
        let d3_in: u16 = 0xc2c1;
        let d4_in = &[0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8];

        let u = Uuid::from_fields_le(d1_in, d2_in, d3_in, d4_in).unwrap();
        let (d1_out, d2_out, d3_out, d4_out) = u.to_fields_le();

        assert_eq!(d1_in, d1_out);
        assert_eq!(d2_in, d2_out);
        assert_eq!(d3_in, d3_out);
        assert_eq!(d4_in, d4_out);
    }

    #[test]
    fn test_fields_le_are_actually_le() {
        let d1_in: u32 = 0xa1a2a3a4;
        let d2_in: u16 = 0xb1b2;
        let d3_in: u16 = 0xc1c2;
        let d4_in = &[0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8];

        let u = Uuid::from_fields(d1_in, d2_in, d3_in, d4_in).unwrap();
        let (d1_out, d2_out, d3_out, d4_out) = u.to_fields_le();

        assert_eq!(d1_in, d1_out.swap_bytes());
        assert_eq!(d2_in, d2_out.swap_bytes());
        assert_eq!(d3_in, d3_out.swap_bytes());
        assert_eq!(d4_in, d4_out);
    }

    #[test]
    fn test_from_slice() {
        let b = [
            0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3,
            0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
        ];

        let u = Uuid::from_slice(&b).unwrap();
        let expected = "a1a2a3a4b1b2c1c2d1d2d3d4d5d6d7d8";

        assert_eq!(u.to_simple().to_string(), expected);
    }

    #[test]
    fn test_from_bytes() {
        let b = [
            0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3,
            0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
        ];

        let u = Uuid::from_bytes(b);
        let expected = "a1a2a3a4b1b2c1c2d1d2d3d4d5d6d7d8";

        assert_eq!(u.to_simple().to_string(), expected);
    }

    #[test]
    fn test_as_bytes() {
        let u = test_util::new();
        let ub = u.as_bytes();

        assert_eq!(ub.len(), 16);
        assert!(!ub.iter().all(|&b| b == 0));
    }

    #[test]
    fn test_bytes_roundtrip() {
        let b_in: ::Bytes = [
            0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3,
            0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
        ];

        let u = Uuid::from_slice(&b_in).unwrap();

        let b_out = u.as_bytes();

        assert_eq!(&b_in, b_out);
    }

    #[test]
    #[allow(deprecated)]
    fn test_from_random_bytes() {
        let b = [
            0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3,
            0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
        ];

        let u = Uuid::from_random_bytes(b);
        let expected = "a1a2a3a4b1b241c291d2d3d4d5d6d7d8";

        assert_eq!(u.to_simple().to_string(), expected);
    }

    #[test]
    fn test_iterbytes_impl_for_uuid() {
        let mut set = std::collections::HashSet::new();
        let id1 = test_util::new();
        let id2 = test_util::new2();
        set.insert(id1.clone());

        assert!(set.contains(&id1));
        assert!(!set.contains(&id2));
    }
}
