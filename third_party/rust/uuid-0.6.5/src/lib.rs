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
//! unique 128-bit number, stored as 16 octets.  UUIDs are used to  assign
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
//!   using an implementation of `UuidV1ClockSequence` (usually `UuidV1Context`)
//!   and a timestamp from `time::timespec`.
//! * `v3` - adds the `Uuid::new_v3` function and the ability to create a V3
//!   UUID based on the MD5 hash of some data.
//! * `v4` - adds the `Uuid::new_v4` function and the ability to randomly
//!   generate a `Uuid`.
//! * `v5` - adds the `Uuid::new_v5` function and the ability to create a V5
//!   UUID based on the SHA1 hash of some data.
//! * `serde` - adds the ability to serialize and deserialize a `Uuid` using the
//!   `serde` crate.
//!
//! By default, `uuid` can be depended on with:
//!
//! ```toml
//! [dependencies]
//! uuid = "0.6"
//! ```
//!
//! To activate various features, use syntax like:
//!
//! ```toml
//! [dependencies]
//! uuid = { version = "0.6", features = ["serde", "v4"] }
//! ```
//!
//! You can disable default features with:
//!
//! ```toml
//! [dependencies]
//! uuid = { version = "0.6", default-features = false }
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
//!     let my_uuid = Uuid::parse_str("936DA01F9ABD4d9d80C702AF85C822A8").unwrap();
//!     println!("{}", my_uuid.urn());
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
//! * [Wikipedia: Universally Unique Identifier](
//!     http://en.wikipedia.org/wiki/Universally_unique_identifier)
//! * [RFC4122: A Universally Unique IDentifier (UUID) URN Namespace](
//!     http://tools.ietf.org/html/rfc4122)

#![doc(
    html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk-v2.png",
    html_favicon_url = "https://www.rust-lang.org/favicon.ico",
    html_root_url = "https://docs.rs/uuid"
)]
#![deny(warnings)]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(feature = "const_fn", feature(const_fn))]

#[cfg(feature = "byteorder")]
extern crate byteorder;
#[macro_use]
extern crate cfg_if;
#[cfg(feature = "std")]
extern crate core;

cfg_if! {
    if #[cfg(feature = "md5")] {
        extern crate md5;
    }
}

cfg_if! {
    if #[cfg(feature = "rand")] {
        extern crate rand;
    }
}

cfg_if! {
    if #[cfg(feature = "serde")] {
        extern crate serde;
    }
}

cfg_if! {
    if #[cfg(feature = "sha1")] {
        extern crate sha1;
    }
}

cfg_if! {
    if #[cfg(all(feature = "slog", not(test)))] {
        extern crate slog;
    } else if #[cfg(all(feature = "slog", test))] {
        #[macro_use]
        extern crate slog;
    }
}

cfg_if! {
    if #[cfg(feature = "std")] {
        use std::fmt;
        use std::str;

        cfg_if! {
            if #[cfg(feature = "v1")] {
                use std::sync::atomic;
            }
        }
    } else if #[cfg(not(feature = "std"))] {
        use core::fmt;
        use core::str;

        cfg_if! {
            if #[cfg(feature = "v1")] {
                use core::sync::atomic;
            }
        }
    }
}

pub mod prelude;

mod adapter;
mod core_support;
#[cfg(feature = "u128")]
mod u128_support;

cfg_if! {
    if #[cfg(feature = "serde")] {
        mod serde_support;
    }
}

cfg_if! {
    if #[cfg(feature = "slog")] {
        mod slog_support;
    }
}

cfg_if! {
    if #[cfg(feature = "std")] {
        mod std_support;
    }
}

cfg_if! {
    if #[cfg(test)] {
        mod test_util;
    }
}

/// A 128-bit (16 byte) buffer containing the ID.
pub type UuidBytes = [u8; 16];

/// The version of the UUID, denoting the generating algorithm.
#[derive(Debug, PartialEq, Copy, Clone)]
pub enum UuidVersion {
    /// Version 1: MAC address
    Mac = 1,
    /// Version 2: DCE Security
    Dce = 2,
    /// Version 3: MD5 hash
    Md5 = 3,
    /// Version 4: Random
    Random = 4,
    /// Version 5: SHA-1 hash
    Sha1 = 5,
}

/// The reserved variants of UUIDs.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum UuidVariant {
    /// Reserved by the NCS for backward compatibility
    NCS,
    /// As described in the RFC4122 Specification (default)
    RFC4122,
    /// Reserved by Microsoft for backward compatibility
    Microsoft,
    /// Reserved for future expansion
    Future,
}

/// A Universally Unique Identifier (UUID).
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Uuid {
    /// The 128-bit number stored in 16 bytes
    bytes: UuidBytes,
}

/// An adaptor for formatting a `Uuid` as a simple string.
pub struct Simple<'a> {
    inner: &'a Uuid,
}

/// An adaptor for formatting a `Uuid` as a hyphenated string.
pub struct Hyphenated<'a> {
    inner: &'a Uuid,
}

/// A UUID of the namespace of fully-qualified domain names
pub const NAMESPACE_DNS: Uuid = Uuid {
    bytes: [
        0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30,
        0xc8,
    ],
};

/// A UUID of the namespace of URLs
pub const NAMESPACE_URL: Uuid = Uuid {
    bytes: [
        0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30,
        0xc8,
    ],
};

/// A UUID of the namespace of ISO OIDs
pub const NAMESPACE_OID: Uuid = Uuid {
    bytes: [
        0x6b, 0xa7, 0xb8, 0x12, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30,
        0xc8,
    ],
};

/// A UUID of the namespace of X.500 DNs (in DER or a text output format)
pub const NAMESPACE_X500: Uuid = Uuid {
    bytes: [
        0x6b, 0xa7, 0xb8, 0x14, 0x9d, 0xad, 0x11, 0xd1, 0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30,
        0xc8,
    ],
};

/// The number of 100 ns ticks between
/// the UUID epoch 1582-10-15 00:00:00 and the Unix epoch 1970-01-01 00:00:00
#[cfg(feature = "v1")]
const UUID_TICKS_BETWEEN_EPOCHS: u64 = 0x01B2_1DD2_1381_4000;

/// An adaptor for formatting a `Uuid` as a URN string.
pub struct Urn<'a> {
    inner: &'a Uuid,
}

cfg_if! {
    if #[cfg(feature = "v1")] {

        /// A trait that abstracts over generation of Uuid v1 "Clock Sequence"
        /// values.
        pub trait UuidV1ClockSequence {
            /// Return a 16-bit number that will be used as the "clock
            /// sequence" in the Uuid. The number must be different if the
            /// time has changed since the last time a clock sequence was
            /// requested.
            fn generate_sequence(&self, seconds: u64, nano_seconds: u32) -> u16;
        }

        /// A thread-safe, stateful context for the v1 generator to help
        /// ensure process-wide uniqueness.
        pub struct UuidV1Context {
            count: atomic::AtomicUsize,
        }

        impl UuidV1Context {
            /// Creates a thread-safe, internally mutable context to help
            /// ensure uniqueness.
            ///
            /// This is a context which can be shared across threads. It
            /// maintains an internal counter that is incremented at every
            /// request, the value ends up in the clock_seq portion of the
            /// Uuid (the fourth group). This will improve the probability
            /// that the Uuid is unique across the process.
            pub fn new(count: u16) -> UuidV1Context {
                UuidV1Context {
                    count: atomic::AtomicUsize::new(count as usize),
                }
            }
        }

        impl UuidV1ClockSequence for UuidV1Context {
            fn generate_sequence(&self, _: u64, _: u32) -> u16 {
                (self.count.fetch_add(1, atomic::Ordering::SeqCst) & 0xffff) as u16
            }
        }
    }
}

/// Error details for string parsing failures.
#[allow(missing_docs)]
#[derive(PartialEq, Eq, Copy, Clone, Debug)]
pub enum ParseError {
    InvalidLength(usize),
    InvalidCharacter(char, usize),
    InvalidGroups(usize),
    InvalidGroupLength(usize, usize, u8),
}

/// Converts a `ParseError` to a string.
impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ParseError::InvalidLength(found) => write!(
                f,
                "Invalid length; expecting {} or {} chars, found {}",
                adapter::UUID_SIMPLE_LENGTH,
                adapter::UUID_HYPHENATED_LENGTH,
                found
            ),
            ParseError::InvalidCharacter(found, pos) => write!(
                f,
                "Invalid character; found `{}` (0x{:02x}) at offset {}",
                found, found as usize, pos
            ),
            ParseError::InvalidGroups(found) => write!(
                f,
                "Malformed; wrong number of groups: expected 1 or 5, found {}",
                found
            ),
            ParseError::InvalidGroupLength(group, found, expecting) => write!(
                f,
                "Malformed; length of group {} was {}, expecting {}",
                group, found, expecting
            ),
        }
    }
}

// Length of each hyphenated group in hex digits.
const GROUP_LENS: [u8; 5] = [8, 4, 4, 4, 12];
// Accumulated length of each hyphenated group in hex digits.
const ACC_GROUP_LENS: [u8; 5] = [8, 12, 16, 20, 32];

impl Uuid {
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
    /// assert_eq!(uuid.hyphenated().to_string(),
    ///            "00000000-0000-0000-0000-000000000000");
    /// ```
    #[cfg(feature = "const_fn")]
    pub const fn nil() -> Self {
        Uuid { bytes: [0; 16] }
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
    /// assert_eq!(uuid.hyphenated().to_string(),
    ///            "00000000-0000-0000-0000-000000000000");
    /// ```
    #[cfg(not(feature = "const_fn"))]
    pub fn nil() -> Uuid {
        Uuid { bytes: [0; 16] }
    }

    /// Creates a new `Uuid`.
    ///
    /// Note that not all versions can be generated currently and `None` will be
    /// returned if the specified version cannot be generated.
    ///
    /// To generate a random UUID (`UuidVersion::Md5`), then the `v3`
    /// feature must be enabled for this crate.
    ///
    /// To generate a random UUID (`UuidVersion::Random`), then the `v4`
    /// feature must be enabled for this crate.
    ///
    /// To generate a random UUID (`UuidVersion::Sha1`), then the `v5`
    /// feature must be enabled for this crate.
    pub fn new(v: UuidVersion) -> Option<Uuid> {
        // Why 23? Ascii has roughly 6bit randomness per 8bit.
        // So to reach 128bit at-least 21.333 (128/6) Bytes are required.
        #[cfg(any(feature = "v3", feature = "v5"))]
        let iv: String = {
            use rand::Rng;
            rand::thread_rng().gen_ascii_chars().take(23).collect()
        };

        match v {
            #[cfg(feature = "v3")]
            UuidVersion::Md5 => Some(Uuid::new_v3(&NAMESPACE_DNS, &*iv)),
            #[cfg(feature = "v4")]
            UuidVersion::Random => Some(Uuid::new_v4()),
            #[cfg(feature = "v5")]
            UuidVersion::Sha1 => Some(Uuid::new_v5(&NAMESPACE_DNS, &*iv)),
            _ => None,
        }
    }

    /// Creates a new `Uuid` (version 1 style) using a time value + seq + NodeID.
    ///
    /// This expects two values representing a monotonically increasing value
    /// as well as a unique 6 byte NodeId, and an implementation of `UuidV1ClockSequence`.
    /// This function is only guaranteed to produce unique values if the following conditions hold:
    ///
    /// 1. The NodeID is unique for this process,
    /// 2. The Context is shared across all threads which are generating V1 UUIDs,
    /// 3. The `UuidV1ClockSequence` implementation reliably returns unique clock sequences
    ///    (this crate provides `UuidV1Context` for this purpose).
    ///
    /// The NodeID must be exactly 6 bytes long. If the NodeID is not a valid length
    /// this will return a `ParseError::InvalidLength`.
    ///
    /// The function is not guaranteed to produce monotonically increasing values
    /// however.  There is a slight possibility that two successive equal time values
    /// could be supplied and the sequence counter wraps back over to 0.
    ///
    /// If uniqueness and monotonicity is required, the user is responsibile for ensuring
    /// that the time value always increases between calls
    /// (including between restarts of the process and device).
    ///
    /// Note that usage of this method requires the `v1` feature of this crate
    /// to be enabled.
    ///
    /// # Examples
    /// Basic usage:
    ///
    /// ```
    /// use uuid::{Uuid, UuidV1Context};
    ///
    /// let ctx = UuidV1Context::new(42);
    /// let v1uuid = Uuid::new_v1(&ctx, 1497624119, 1234, &[1,2,3,4,5,6]).unwrap();
    ///
    /// assert_eq!(v1uuid.hyphenated().to_string(), "f3b4958c-52a1-11e7-802a-010203040506");
    /// ```
    #[cfg(feature = "v1")]
    pub fn new_v1<T: UuidV1ClockSequence>(
        context: &T,
        seconds: u64,
        nsecs: u32,
        node: &[u8],
    ) -> Result<Uuid, ParseError> {
        if node.len() != 6 {
            return Err(ParseError::InvalidLength(node.len()));
        }
        let count = context.generate_sequence(seconds, nsecs);
        let timestamp = seconds * 10_000_000 + u64::from(nsecs / 100);
        let uuidtime = timestamp + UUID_TICKS_BETWEEN_EPOCHS;
        let time_low: u32 = (uuidtime & 0xFFFF_FFFF) as u32;
        let time_mid: u16 = ((uuidtime >> 32) & 0xFFFF) as u16;
        let time_hi_and_ver: u16 = (((uuidtime >> 48) & 0x0FFF) as u16) | (1 << 12);
        let mut d4 = [0_u8; 8];
        d4[0] = (((count & 0x3F00) >> 8) as u8) | 0x80;
        d4[1] = (count & 0xFF) as u8;
        d4[2..].copy_from_slice(node);
        Uuid::from_fields(time_low, time_mid, time_hi_and_ver, &d4)
    }

    /// Creates a UUID using a name from a namespace, based on the MD5 hash.
    ///
    /// A number of namespaces are available as constants in this crate:
    ///
    /// * `NAMESPACE_DNS`
    /// * `NAMESPACE_URL`
    /// * `NAMESPACE_OID`
    /// * `NAMESPACE_X500`
    ///
    /// Note that usage of this method requires the `v3` feature of this crate
    /// to be enabled.
    #[cfg(feature = "v3")]
    pub fn new_v3(namespace: &Uuid, name: &str) -> Uuid {
        let mut ctx = md5::Context::new();
        ctx.consume(namespace.as_bytes());
        ctx.consume(name.as_bytes());
        let mut uuid = Uuid {
            bytes: ctx.compute().into(),
        };
        uuid.set_variant(UuidVariant::RFC4122);
        uuid.set_version(UuidVersion::Md5);
        uuid
    }

    /// Creates a random `Uuid`.
    ///
    /// This uses the `rand` crate's default task RNG as the source of random numbers.
    /// If you'd like to use a custom generator, don't use this method: use the
    /// [`rand::Rand trait`]'s `rand()` method instead.
    ///
    /// [`rand::Rand trait`]: ../../rand/rand/trait.Rand.html#tymethod.rand
    ///
    /// Note that usage of this method requires the `v4` feature of this crate
    /// to be enabled.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::new_v4();
    /// ```
    #[cfg(feature = "v4")]
    pub fn new_v4() -> Uuid {
        use rand::Rng;

        let mut rng = rand::thread_rng();

        let mut bytes = [0; 16];
        rng.fill_bytes(&mut bytes);

        Uuid::from_random_bytes(bytes)
    }

    /// Creates a UUID using a name from a namespace, based on the SHA-1 hash.
    ///
    /// A number of namespaces are available as constants in this crate:
    ///
    /// * `NAMESPACE_DNS`
    /// * `NAMESPACE_URL`
    /// * `NAMESPACE_OID`
    /// * `NAMESPACE_X500`
    ///
    /// Note that usage of this method requires the `v5` feature of this crate
    /// to be enabled.
    #[cfg(feature = "v5")]
    pub fn new_v5(namespace: &Uuid, name: &str) -> Uuid {
        let mut hash = sha1::Sha1::new();
        hash.update(namespace.as_bytes());
        hash.update(name.as_bytes());
        let buffer = hash.digest().bytes();
        let mut uuid = Uuid { bytes: [0; 16] };
        uuid.bytes.copy_from_slice(&buffer[..16]);
        uuid.set_variant(UuidVariant::RFC4122);
        uuid.set_version(UuidVersion::Sha1);
        uuid
    }

    /// Creates a `Uuid` from four field values.
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
    /// let uuid = uuid.map(|uuid| uuid.hyphenated().to_string());
    ///
    /// let expected_uuid = Ok(String::from("0000002a-000c-0005-0c03-0938362b0809"));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    ///
    /// An invalid length:
    ///
    /// ```
    /// use uuid::Uuid;
    /// use uuid::ParseError;
    ///
    /// let d4 = [12];
    ///
    /// let uuid = Uuid::from_fields(42, 12, 5, &d4);
    ///
    /// let expected_uuid = Err(ParseError::InvalidLength(1));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    pub fn from_fields(d1: u32, d2: u16, d3: u16, d4: &[u8]) -> Result<Uuid, ParseError> {
        if d4.len() != 8 {
            return Err(ParseError::InvalidLength(d4.len()));
        }

        Ok(Uuid {
            bytes: [
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
            ],
        })
    }

    /// Creates a `Uuid` using the supplied bytes.
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
    /// let bytes = [4, 54, 67, 12, 43, 2, 98, 76,
    ///              32, 50, 87, 5, 1, 33, 43, 87];
    ///
    /// let uuid = Uuid::from_bytes(&bytes);
    /// let uuid = uuid.map(|uuid| uuid.hyphenated().to_string());
    ///
    /// let expected_uuid = Ok(String::from("0436430c-2b02-624c-2032-570501212b57"));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    ///
    /// An incorrect number of bytes:
    ///
    /// ```
    /// use uuid::Uuid;
    /// use uuid::ParseError;
    ///
    /// let bytes = [4, 54, 67, 12, 43, 2, 98, 76];
    ///
    /// let uuid = Uuid::from_bytes(&bytes);
    ///
    /// let expected_uuid = Err(ParseError::InvalidLength(8));
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    pub fn from_bytes(b: &[u8]) -> Result<Uuid, ParseError> {
        let len = b.len();
        if len != 16 {
            return Err(ParseError::InvalidLength(len));
        }

        let mut uuid = Uuid { bytes: [0; 16] };
        uuid.bytes.copy_from_slice(b);
        Ok(uuid)
    }

    /// Creates a `Uuid` using the supplied bytes.
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Uuid;
    /// use uuid::UuidBytes;
    ///
    /// let bytes:UuidBytes = [70, 235, 208, 238, 14, 109, 67, 201, 185, 13, 204, 195, 90, 145, 63, 62];
    ///
    /// let uuid = Uuid::from_uuid_bytes(bytes);
    /// let uuid = uuid.hyphenated().to_string();
    ///
    /// let expected_uuid = String::from("46ebd0ee-0e6d-43c9-b90d-ccc35a913f3e");
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    ///
    /// An incorrect number of bytes:
    ///
    /// ```compile_fail
    /// use uuid::Uuid;
    /// use uuid::UuidBytes;
    ///
    /// let bytes:UuidBytes = [4, 54, 67, 12, 43, 2, 98, 76]; // doesn't compile
    ///
    /// let uuid = Uuid::from_uuid_bytes(bytes);
    /// ```
    #[cfg(not(feature = "const_fn"))]
    pub fn from_uuid_bytes(bytes: UuidBytes) -> Uuid {
        Uuid { bytes }
    }

    #[cfg(feature = "const_fn")]
    pub const fn from_uuid_bytes(bytes: UuidBytes) -> Uuid {
        Uuid { bytes }
    }

    /// Creates a v4 Uuid from random bytes (e.g. bytes supplied from `Rand` crate)
    ///
    /// # Examples
    ///
    /// Basic usage:
    ///
    /// ```
    /// use uuid::Uuid;
    /// use uuid::UuidBytes;
    ///
    ///
    /// let bytes:UuidBytes = [70, 235, 208, 238, 14, 109, 67, 201, 185, 13, 204, 195, 90, 145, 63, 62];
    /// let uuid = Uuid::from_random_bytes(bytes);
    /// let uuid = uuid.hyphenated().to_string();
    ///
    /// let expected_uuid = String::from("46ebd0ee-0e6d-43c9-b90d-ccc35a913f3e");
    ///
    /// assert_eq!(expected_uuid, uuid);
    /// ```
    ///
    pub fn from_random_bytes(b: [u8; 16]) -> Uuid {
        let mut uuid = Uuid { bytes: b };
        uuid.set_variant(UuidVariant::RFC4122);
        uuid.set_version(UuidVersion::Random);
        uuid
    }

    /// Specifies the variant of the UUID structure
    #[allow(dead_code)]
    fn set_variant(&mut self, v: UuidVariant) {
        // Octet 8 contains the variant in the most significant 3 bits
        self.bytes[8] = match v {
            UuidVariant::NCS => self.bytes[8] & 0x7f, // b0xx...
            UuidVariant::RFC4122 => (self.bytes[8] & 0x3f) | 0x80, // b10x...
            UuidVariant::Microsoft => (self.bytes[8] & 0x1f) | 0xc0, // b110...
            UuidVariant::Future => (self.bytes[8] & 0x1f) | 0xe0, // b111...
        }
    }

    /// Returns the variant of the `Uuid` structure.
    ///
    /// This determines the interpretation of the structure of the UUID.
    /// Currently only the RFC4122 variant is generated by this module.
    ///
    /// * [Variant Reference](http://tools.ietf.org/html/rfc4122#section-4.1.1)
    pub fn get_variant(&self) -> Option<UuidVariant> {
        match self.bytes[8] {
            x if x & 0x80 == 0x00 => Some(UuidVariant::NCS),
            x if x & 0xc0 == 0x80 => Some(UuidVariant::RFC4122),
            x if x & 0xe0 == 0xc0 => Some(UuidVariant::Microsoft),
            x if x & 0xe0 == 0xe0 => Some(UuidVariant::Future),
            _ => None,
        }
    }

    /// Specifies the version number of the `Uuid`.
    #[allow(dead_code)]
    fn set_version(&mut self, v: UuidVersion) {
        self.bytes[6] = (self.bytes[6] & 0xF) | ((v as u8) << 4);
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
        (self.bytes[6] >> 4) as usize
    }

    /// Returns the version of the `Uuid`.
    ///
    /// This represents the algorithm used to generate the contents
    pub fn get_version(&self) -> Option<UuidVersion> {
        let v = self.bytes[6] >> 4;
        match v {
            1 => Some(UuidVersion::Mac),
            2 => Some(UuidVersion::Dce),
            3 => Some(UuidVersion::Md5),
            4 => Some(UuidVersion::Random),
            5 => Some(UuidVersion::Sha1),
            _ => None,
        }
    }

    /// Returns the four field values of the UUID.
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
    /// * The third field value represents the third group of (four) hex
    ///   digits, taken as a big-endian `u16` value.  The 4 most significant
    ///   bits give the UUID version, and for V1 UUIDs, the last 12 bits
    ///   represent the high 12 bits of the timestamp.
    /// * The last field value represents the last two groups of four and
    ///   twelve hex digits, taken in order.  The first 1-3 bits of this
    ///   indicate the UUID variant, and for V1 UUIDs, the next 13-15 bits
    ///   indicate the clock sequence and the last 48 bits indicate the node
    ///   ID.
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
    /// assert_eq!(uuid.as_fields(),
    ///            (0x936DA01F, 0x9ABD, 0x4D9D, b"\x80\xC7\x02\xAF\x85\xC8\x22\xA8"));
    /// ```
    pub fn as_fields(&self) -> (u32, u16, u16, &[u8; 8]) {
        let d1 = u32::from(self.bytes[0]) << 24
            | u32::from(self.bytes[1]) << 16
            | u32::from(self.bytes[2]) << 8
            | u32::from(self.bytes[3]);

        let d2 = u16::from(self.bytes[4]) << 8 | u16::from(self.bytes[5]);

        let d3 = u16::from(self.bytes[6]) << 8 | u16::from(self.bytes[7]);

        let d4: &[u8; 8] = unsafe { &*(self.bytes[8..16].as_ptr() as *const [u8; 8]) };
        (d1, d2, d3, d4)
    }

    /// Returns an array of 16 octets containing the UUID data.
    ///
    /// # Examples
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    /// assert_eq!(uuid.as_bytes(), &[0; 16]);
    ///
    /// let uuid = Uuid::parse_str("936DA01F9ABD4d9d80C702AF85C822A8").unwrap();
    /// assert_eq!(uuid.as_bytes(),
    ///            &[147, 109, 160, 31, 154, 189, 77, 157,
    ///              128, 199, 2, 175, 133, 200, 34, 168]);
    /// ```
    #[cfg(feature = "const_fn")]
    pub const fn as_bytes(&self) -> &[u8; 16] {
        &self.bytes
    }

    /// Returns an array of 16 octets containing the UUID data.
    ///
    /// # Examples
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    /// assert_eq!(uuid.as_bytes(), &[0; 16]);
    ///
    /// let uuid = Uuid::parse_str("936DA01F9ABD4d9d80C702AF85C822A8").unwrap();
    /// assert_eq!(uuid.as_bytes(),
    ///            &[147, 109, 160, 31, 154, 189, 77, 157,
    ///              128, 199, 2, 175, 133, 200, 34, 168]);
    /// ```
    #[cfg(not(feature = "const_fn"))]
    pub fn as_bytes(&self) -> &[u8; 16] {
        &self.bytes
    }

    /// Returns a wrapper which when formatted via `fmt::Display` will format a
    /// string of 32 hexadecimal digits.
    ///
    /// # Examples
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    /// assert_eq!(uuid.simple().to_string(),
    ///            "00000000000000000000000000000000");
    /// ```
    pub fn simple(&self) -> Simple {
        Simple { inner: self }
    }

    /// Returns a wrapper which when formatted via `fmt::Display` will format a
    /// string of hexadecimal digits separated into groups with a hyphen.
    ///
    /// # Examples
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    /// assert_eq!(uuid.hyphenated().to_string(),
    ///            "00000000-0000-0000-0000-000000000000");
    /// ```
    pub fn hyphenated(&self) -> Hyphenated {
        Hyphenated { inner: self }
    }

    /// Returns a wrapper which when formatted via `fmt::Display` will format a
    /// string of the UUID as a full URN string.
    ///
    /// # Examples
    ///
    /// ```
    /// use uuid::Uuid;
    ///
    /// let uuid = Uuid::nil();
    /// assert_eq!(uuid.urn().to_string(),
    ///            "urn:uuid:00000000-0000-0000-0000-000000000000");
    /// ```
    pub fn urn(&self) -> Urn {
        Urn { inner: self }
    }

    /// Returns an Optional Tuple of (u64, u16) representing the timestamp and
    /// counter portion of a V1 UUID.  If the supplied UUID is not V1, this
    /// will return None
    pub fn to_timestamp(&self) -> Option<(u64, u16)> {
        if self
            .get_version()
            .map(|v| v != UuidVersion::Mac)
            .unwrap_or(true)
        {
            return None;
        }

        let ts: u64 = u64::from(self.bytes[6] & 0x0F) << 56
            | u64::from(self.bytes[7]) << 48
            | u64::from(self.bytes[4]) << 40
            | u64::from(self.bytes[5]) << 32
            | u64::from(self.bytes[0]) << 24
            | u64::from(self.bytes[1]) << 16
            | u64::from(self.bytes[2]) << 8
            | u64::from(self.bytes[3]);

        let count: u16 = u16::from(self.bytes[8] & 0x3F) << 8 | u16::from(self.bytes[9]);

        Some((ts, count))
    }

    /// Parses a `Uuid` from a string of hexadecimal digits with optional hyphens.
    ///
    /// Any of the formats generated by this module (simple, hyphenated, urn) are
    /// supported by this parsing function.
    pub fn parse_str(mut input: &str) -> Result<Uuid, ParseError> {
        // Ensure length is valid for any of the supported formats
        let len = input.len();
        if len == (adapter::UUID_HYPHENATED_LENGTH + 9) && input.starts_with("urn:uuid:") {
            input = &input[9..];
        } else if len != adapter::UUID_SIMPLE_LENGTH && len != adapter::UUID_HYPHENATED_LENGTH {
            return Err(ParseError::InvalidLength(len));
        }

        // `digit` counts only hexadecimal digits, `i_char` counts all chars.
        let mut digit = 0;
        let mut group = 0;
        let mut acc = 0;
        let mut buffer = [0u8; 16];

        for (i_char, chr) in input.bytes().enumerate() {
            if digit as usize >= adapter::UUID_SIMPLE_LENGTH && group != 4 {
                if group == 0 {
                    return Err(ParseError::InvalidLength(len));
                }
                return Err(ParseError::InvalidGroups(group + 1));
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
                        if ACC_GROUP_LENS[group] != digit {
                            // Calculate how many digits this group consists of in the input.
                            let found = if group > 0 {
                                digit - ACC_GROUP_LENS[group - 1]
                            } else {
                                digit
                            };
                            return Err(ParseError::InvalidGroupLength(
                                group,
                                found as usize,
                                GROUP_LENS[group],
                            ));
                        }
                        // Next group, decrement digit, it is incremented again at the bottom.
                        group += 1;
                        digit -= 1;
                    }
                    _ => {
                        return Err(ParseError::InvalidCharacter(
                            input[i_char..].chars().next().unwrap(),
                            i_char,
                        ))
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
                            digit - ACC_GROUP_LENS[group - 1]
                        } else {
                            digit
                        };
                        return Err(ParseError::InvalidGroupLength(
                            group,
                            found as usize,
                            GROUP_LENS[group],
                        ));
                    }
                    _ => {
                        return Err(ParseError::InvalidCharacter(
                            input[i_char..].chars().next().unwrap(),
                            i_char,
                        ))
                    }
                }
                buffer[(digit / 2) as usize] = acc;
            }
            digit += 1;
        }

        // Now check the last group.
        if ACC_GROUP_LENS[4] != digit {
            return Err(ParseError::InvalidGroupLength(
                group,
                (digit - ACC_GROUP_LENS[3]) as usize,
                GROUP_LENS[4],
            ));
        }

        Ok(Uuid::from_bytes(&buffer).unwrap())
    }

    /// Tests if the UUID is nil
    pub fn is_nil(&self) -> bool {
        self.bytes.iter().all(|&b| b == 0)
    }
}

impl<'a> fmt::Display for Simple<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl<'a> fmt::UpperHex for Simple<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for byte in &self.inner.bytes {
            write!(f, "{:02X}", byte)?;
        }
        Ok(())
    }
}

impl<'a> fmt::LowerHex for Simple<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for byte in &self.inner.bytes {
            write!(f, "{:02x}", byte)?;
        }
        Ok(())
    }
}

impl<'a> fmt::Display for Hyphenated<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

macro_rules! hyphenated_write {
    ($f:expr, $format:expr, $bytes:expr) => {{
        let data1 = u32::from($bytes[0]) << 24
            | u32::from($bytes[1]) << 16
            | u32::from($bytes[2]) << 8
            | u32::from($bytes[3]);

        let data2 = u16::from($bytes[4]) << 8 | u16::from($bytes[5]);

        let data3 = u16::from($bytes[6]) << 8 | u16::from($bytes[7]);

        write!(
            $f,
            $format,
            data1,
            data2,
            data3,
            $bytes[8],
            $bytes[9],
            $bytes[10],
            $bytes[11],
            $bytes[12],
            $bytes[13],
            $bytes[14],
            $bytes[15]
        )
    }};
}

impl<'a> fmt::UpperHex for Hyphenated<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        hyphenated_write!(
            f,
            "{:08X}-\
             {:04X}-\
             {:04X}-\
             {:02X}{:02X}-\
             {:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
            self.inner.bytes
        )
    }
}

impl<'a> fmt::LowerHex for Hyphenated<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        hyphenated_write!(
            f,
            "{:08x}-\
             {:04x}-\
             {:04x}-\
             {:02x}{:02x}-\
             {:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
            self.inner.bytes
        )
    }
}

impl<'a> fmt::Display for Urn<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "urn:uuid:{}", self.inner.hyphenated())
    }
}

#[cfg(test)]
mod tests {
    extern crate std;

    use self::std::prelude::v1::*;

    use super::test_util;

    use super::{NAMESPACE_X500, NAMESPACE_DNS, NAMESPACE_OID, NAMESPACE_URL};
    use super::{Uuid, UuidVariant, UuidVersion};

    #[cfg(feature = "v3")]
    static FIXTURE_V3: &'static [(&'static Uuid, &'static str, &'static str)] = &[
        (
            &NAMESPACE_DNS,
            "example.org",
            "04738bdf-b25a-3829-a801-b21a1d25095b",
        ),
        (
            &NAMESPACE_DNS,
            "rust-lang.org",
            "c6db027c-615c-3b4d-959e-1a917747ca5a",
        ),
        (&NAMESPACE_DNS, "42", "5aab6e0c-b7d3-379c-92e3-2bfbb5572511"),
        (
            &NAMESPACE_DNS,
            "lorem ipsum",
            "4f8772e9-b59c-3cc9-91a9-5c823df27281",
        ),
        (
            &NAMESPACE_URL,
            "example.org",
            "39682ca1-9168-3da2-a1bb-f4dbcde99bf9",
        ),
        (
            &NAMESPACE_URL,
            "rust-lang.org",
            "7ed45aaf-e75b-3130-8e33-ee4d9253b19f",
        ),
        (&NAMESPACE_URL, "42", "08998a0c-fcf4-34a9-b444-f2bfc15731dc"),
        (
            &NAMESPACE_URL,
            "lorem ipsum",
            "e55ad2e6-fb89-34e8-b012-c5dde3cd67f0",
        ),
        (
            &NAMESPACE_OID,
            "example.org",
            "f14eec63-2812-3110-ad06-1625e5a4a5b2",
        ),
        (
            &NAMESPACE_OID,
            "rust-lang.org",
            "6506a0ec-4d79-3e18-8c2b-f2b6b34f2b6d",
        ),
        (&NAMESPACE_OID, "42", "ce6925a5-2cd7-327b-ab1c-4b375ac044e4"),
        (
            &NAMESPACE_OID,
            "lorem ipsum",
            "5dd8654f-76ba-3d47-bc2e-4d6d3a78cb09",
        ),
        (
            &NAMESPACE_X500,
            "example.org",
            "64606f3f-bd63-363e-b946-fca13611b6f7",
        ),
        (
            &NAMESPACE_X500,
            "rust-lang.org",
            "bcee7a9c-52f1-30c6-a3cc-8c72ba634990",
        ),
        (
            &NAMESPACE_X500,
            "42",
            "c1073fa2-d4a6-3104-b21d-7a6bdcf39a23",
        ),
        (
            &NAMESPACE_X500,
            "lorem ipsum",
            "02f09a3f-1624-3b1d-8409-44eff7708208",
        ),
    ];

    #[cfg(feature = "v5")]
    static FIXTURE_V5: &'static [(&'static Uuid, &'static str, &'static str)] = &[
        (
            &NAMESPACE_DNS,
            "example.org",
            "aad03681-8b63-5304-89e0-8ca8f49461b5",
        ),
        (
            &NAMESPACE_DNS,
            "rust-lang.org",
            "c66bbb60-d62e-5f17-a399-3a0bd237c503",
        ),
        (&NAMESPACE_DNS, "42", "7c411b5e-9d3f-50b5-9c28-62096e41c4ed"),
        (
            &NAMESPACE_DNS,
            "lorem ipsum",
            "97886a05-8a68-5743-ad55-56ab2d61cf7b",
        ),
        (
            &NAMESPACE_URL,
            "example.org",
            "54a35416-963c-5dd6-a1e2-5ab7bb5bafc7",
        ),
        (
            &NAMESPACE_URL,
            "rust-lang.org",
            "c48d927f-4122-5413-968c-598b1780e749",
        ),
        (&NAMESPACE_URL, "42", "5c2b23de-4bad-58ee-a4b3-f22f3b9cfd7d"),
        (
            &NAMESPACE_URL,
            "lorem ipsum",
            "15c67689-4b85-5253-86b4-49fbb138569f",
        ),
        (
            &NAMESPACE_OID,
            "example.org",
            "34784df9-b065-5094-92c7-00bb3da97a30",
        ),
        (
            &NAMESPACE_OID,
            "rust-lang.org",
            "8ef61ecb-977a-5844-ab0f-c25ef9b8d5d6",
        ),
        (&NAMESPACE_OID, "42", "ba293c61-ad33-57b9-9671-f3319f57d789"),
        (
            &NAMESPACE_OID,
            "lorem ipsum",
            "6485290d-f79e-5380-9e64-cb4312c7b4a6",
        ),
        (
            &NAMESPACE_X500,
            "example.org",
            "e3635e86-f82b-5bbc-a54a-da97923e5c76",
        ),
        (
            &NAMESPACE_X500,
            "rust-lang.org",
            "26c9c3e9-49b7-56da-8b9f-a0fb916a71a3",
        ),
        (
            &NAMESPACE_X500,
            "42",
            "e4b88014-47c6-5fe0-a195-13710e5f6e27",
        ),
        (
            &NAMESPACE_X500,
            "lorem ipsum",
            "b11f79a5-1e6d-57ce-a4b5-ba8531ea03d0",
        ),
    ];

    #[test]
    fn test_nil() {
        let nil = Uuid::nil();
        let not_nil = test_util::new();

        assert!(nil.is_nil());
        assert!(!not_nil.is_nil());
    }

    #[test]
    fn test_new() {
        if cfg!(feature = "v3") {
            let u = Uuid::new(UuidVersion::Md5);
            assert!(u.is_some(), "{:?}", u);
            assert_eq!(u.unwrap().get_version().unwrap(), UuidVersion::Md5);
        } else {
            assert_eq!(Uuid::new(UuidVersion::Md5), None);
        }
        if cfg!(feature = "v4") {
            let uuid1 = Uuid::new(UuidVersion::Random).unwrap();
            let s = uuid1.simple().to_string();

            assert_eq!(s.len(), 32);
            assert_eq!(uuid1.get_version().unwrap(), UuidVersion::Random);
        } else {
            assert!(Uuid::new(UuidVersion::Random).is_none());
        }
        if cfg!(feature = "v5") {
            let u = Uuid::new(UuidVersion::Sha1);
            assert!(u.is_some(), "{:?}", u);
            assert_eq!(u.unwrap().get_version().unwrap(), UuidVersion::Sha1);
        } else {
            assert_eq!(Uuid::new(UuidVersion::Sha1), None);
        }

        // Test unsupported versions
        assert_eq!(Uuid::new(UuidVersion::Mac), None);
        assert_eq!(Uuid::new(UuidVersion::Dce), None);
    }

    #[cfg(feature = "v1")]
    #[test]
    fn test_new_v1() {
        use UuidV1Context;
        let time: u64 = 1_496_854_535;
        let timefrac: u32 = 812_946_000;
        let node = [1, 2, 3, 4, 5, 6];
        let ctx = UuidV1Context::new(0);
        let uuid = Uuid::new_v1(&ctx, time, timefrac, &node[..]).unwrap();
        assert_eq!(uuid.get_version().unwrap(), UuidVersion::Mac);
        assert_eq!(uuid.get_variant().unwrap(), UuidVariant::RFC4122);
        assert_eq!(
            uuid.hyphenated().to_string(),
            "20616934-4ba2-11e7-8000-010203040506"
        );
        let uuid2 = Uuid::new_v1(&ctx, time, timefrac, &node[..]).unwrap();
        assert_eq!(
            uuid2.hyphenated().to_string(),
            "20616934-4ba2-11e7-8001-010203040506"
        );

        let ts = uuid.to_timestamp().unwrap();
        assert_eq!(ts.0 - 0x01B21DD213814000, 1_496_854_535_812_946_0);
        assert_eq!(ts.1, 0);
        assert_eq!(uuid2.to_timestamp().unwrap().1, 1);
    }

    #[cfg(feature = "v3")]
    #[test]
    fn test_new_v3() {
        for &(ref ns, ref name, _) in FIXTURE_V3 {
            let uuid = Uuid::new_v3(*ns, *name);
            assert_eq!(uuid.get_version().unwrap(), UuidVersion::Md5);
            assert_eq!(uuid.get_variant().unwrap(), UuidVariant::RFC4122);
        }
    }

    #[test]
    #[cfg(feature = "v4")]
    fn test_new_v4() {
        let uuid1 = Uuid::new_v4();

        assert_eq!(uuid1.get_version().unwrap(), UuidVersion::Random);
        assert_eq!(uuid1.get_variant().unwrap(), UuidVariant::RFC4122);
    }

    #[cfg(feature = "v5")]
    #[test]
    fn test_new_v5() {
        for &(ref ns, ref name, _) in FIXTURE_V5 {
            let uuid = Uuid::new_v5(*ns, *name);
            assert_eq!(uuid.get_version().unwrap(), UuidVersion::Sha1);
            assert_eq!(uuid.get_variant().unwrap(), UuidVariant::RFC4122);
        }
    }

    #[test]
    fn test_predefined_namespaces() {
        assert_eq!(
            NAMESPACE_DNS.hyphenated().to_string(),
            "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
        );
        assert_eq!(
            NAMESPACE_URL.hyphenated().to_string(),
            "6ba7b811-9dad-11d1-80b4-00c04fd430c8"
        );
        assert_eq!(
            NAMESPACE_OID.hyphenated().to_string(),
            "6ba7b812-9dad-11d1-80b4-00c04fd430c8"
        );
        assert_eq!(
            NAMESPACE_X500.hyphenated().to_string(),
            "6ba7b814-9dad-11d1-80b4-00c04fd430c8"
        );
    }

    #[cfg(feature = "v3")]
    #[test]
    fn test_get_version_v3() {
        let uuid = Uuid::new_v3(&NAMESPACE_DNS, "rust-lang.org");

        assert_eq!(uuid.get_version().unwrap(), UuidVersion::Md5);
        assert_eq!(uuid.get_version_num(), 3);
    }

    #[test]
    #[cfg(feature = "v4")]
    fn test_get_version_v4() {
        let uuid1 = Uuid::new_v4();

        assert_eq!(uuid1.get_version().unwrap(), UuidVersion::Random);
        assert_eq!(uuid1.get_version_num(), 4);
    }

    #[cfg(feature = "v5")]
    #[test]
    fn test_get_version_v5() {
        let uuid2 = Uuid::new_v5(&NAMESPACE_DNS, "rust-lang.org");

        assert_eq!(uuid2.get_version().unwrap(), UuidVersion::Sha1);
        assert_eq!(uuid2.get_version_num(), 5);
    }

    #[test]
    fn test_get_variant() {
        let uuid1 = test_util::new();
        let uuid2 = Uuid::parse_str("550e8400-e29b-41d4-a716-446655440000").unwrap();
        let uuid3 = Uuid::parse_str("67e55044-10b1-426f-9247-bb680e5fe0c8").unwrap();
        let uuid4 = Uuid::parse_str("936DA01F9ABD4d9dC0C702AF85C822A8").unwrap();
        let uuid5 = Uuid::parse_str("F9168C5E-CEB2-4faa-D6BF-329BF39FA1E4").unwrap();
        let uuid6 = Uuid::parse_str("f81d4fae-7dec-11d0-7765-00a0c91e6bf6").unwrap();

        assert_eq!(uuid1.get_variant().unwrap(), UuidVariant::RFC4122);
        assert_eq!(uuid2.get_variant().unwrap(), UuidVariant::RFC4122);
        assert_eq!(uuid3.get_variant().unwrap(), UuidVariant::RFC4122);
        assert_eq!(uuid4.get_variant().unwrap(), UuidVariant::Microsoft);
        assert_eq!(uuid5.get_variant().unwrap(), UuidVariant::Microsoft);
        assert_eq!(uuid6.get_variant().unwrap(), UuidVariant::NCS);
    }

    #[test]
    fn test_parse_uuid_v4() {
        use super::ParseError::*;

        // Invalid
        assert_eq!(Uuid::parse_str(""), Err(InvalidLength(0)));
        assert_eq!(Uuid::parse_str("!"), Err(InvalidLength(1)));
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF-329BF39FA1E45"),
            Err(InvalidLength(37))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-BBF-329BF39FA1E4"),
            Err(InvalidLength(35))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-BGBF-329BF39FA1E4"),
            Err(InvalidCharacter('G', 20))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2F4faaFB6BFF329BF39FA1E4"),
            Err(InvalidGroups(2))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faaFB6BFF329BF39FA1E4"),
            Err(InvalidGroups(3))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-B6BFF329BF39FA1E4"),
            Err(InvalidGroups(4))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa"),
            Err(InvalidLength(18))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faaXB6BFF329BF39FA1E4"),
            Err(InvalidCharacter('X', 18))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB-24fa-eB6BFF32-BF39FA1E4"),
            Err(InvalidGroupLength(1, 3, 4))
        );
        assert_eq!(
            Uuid::parse_str("01020304-1112-2122-3132-41424344"),
            Err(InvalidGroupLength(4, 8, 12))
        );
        assert_eq!(
            Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c"),
            Err(InvalidLength(31))
        );
        assert_eq!(
            Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c88"),
            Err(InvalidLength(33))
        );
        assert_eq!(
            Uuid::parse_str("67e5504410b1426f9247bb680e5fe0cg8"),
            Err(InvalidLength(33))
        );
        assert_eq!(
            Uuid::parse_str("67e5504410b1426%9247bb680e5fe0c8"),
            Err(InvalidCharacter('%', 15))
        );
        assert_eq!(
            Uuid::parse_str("231231212212423424324323477343246663"),
            Err(InvalidLength(36))
        );

        // Valid
        assert!(Uuid::parse_str("00000000000000000000000000000000").is_ok());
        assert!(Uuid::parse_str("67e55044-10b1-426f-9247-bb680e5fe0c8").is_ok());
        assert!(Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF-329BF39FA1E4").is_ok());
        assert!(Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c8").is_ok());
        assert!(Uuid::parse_str("01020304-1112-2122-3132-414243444546").is_ok());
        assert!(Uuid::parse_str("urn:uuid:67e55044-10b1-426f-9247-bb680e5fe0c8").is_ok());

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
            Err(InvalidLength(31))
        );
        assert_eq!(
            Uuid::parse_str("67e550X410b1426f9247bb680e5fe0cd"),
            Err(InvalidCharacter('X', 6))
        );
        assert_eq!(
            Uuid::parse_str("67e550-4105b1426f9247bb680e5fe0c"),
            Err(InvalidGroupLength(0, 6, 8))
        );
        assert_eq!(
            Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF1-02BF39FA1E4"),
            Err(InvalidGroupLength(3, 5, 4))
        );
    }

    #[test]
    fn test_to_simple_string() {
        let uuid1 = test_util::new();
        let s = uuid1.simple().to_string();

        assert_eq!(s.len(), 32);
        assert!(s.chars().all(|c| c.is_digit(16)));
    }

    #[test]
    fn test_to_hyphenated_string() {
        let uuid1 = test_util::new();
        let s = uuid1.hyphenated().to_string();

        assert!(s.len() == 36);
        assert!(s.chars().all(|c| c.is_digit(16) || c == '-'));
    }

    #[test]
    fn test_upper_lower_hex() {
        use super::fmt::Write;

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
        check!(buf, "{:X}", u.hyphenated(), 36, |c| c.is_uppercase()
            || c.is_digit(10)
            || c == '-');
        check!(buf, "{:X}", u.simple(), 32, |c| c.is_uppercase()
            || c.is_digit(10));

        check!(buf, "{:x}", u.hyphenated(), 36, |c| c.is_lowercase()
            || c.is_digit(10)
            || c == '-');
        check!(buf, "{:x}", u.simple(), 32, |c| c.is_lowercase()
            || c.is_digit(10));
    }

    #[cfg(feature = "v3")]
    #[test]
    fn test_v3_to_hypenated_string() {
        for &(ref ns, ref name, ref expected) in FIXTURE_V3 {
            let uuid = Uuid::new_v3(*ns, *name);
            assert_eq!(uuid.hyphenated().to_string(), *expected);
        }
    }

    #[cfg(feature = "v5")]
    #[test]
    fn test_v5_to_hypenated_string() {
        for &(ref ns, ref name, ref expected) in FIXTURE_V5 {
            let uuid = Uuid::new_v5(*ns, *name);
            assert_eq!(uuid.hyphenated().to_string(), *expected);
        }
    }

    #[test]
    fn test_to_urn_string() {
        let uuid1 = test_util::new();
        let ss = uuid1.urn().to_string();
        let s = &ss[9..];

        assert!(ss.starts_with("urn:uuid:"));
        assert_eq!(s.len(), 36);
        assert!(s.chars().all(|c| c.is_digit(16) || c == '-'));
    }

    #[test]
    fn test_to_simple_string_matching() {
        let uuid1 = test_util::new();

        let hs = uuid1.hyphenated().to_string();
        let ss = uuid1.simple().to_string();

        let hsn = hs.chars().filter(|&c| c != '-').collect::<String>();

        assert_eq!(hsn, ss);
    }

    #[test]
    fn test_string_roundtrip() {
        let uuid = test_util::new();

        let hs = uuid.hyphenated().to_string();
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
        let result = u.simple().to_string();
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
    fn test_from_bytes() {
        let b = [
            0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
            0xd7, 0xd8,
        ];

        let u = Uuid::from_bytes(&b).unwrap();
        let expected = "a1a2a3a4b1b2c1c2d1d2d3d4d5d6d7d8";

        assert_eq!(u.simple().to_string(), expected);
    }

    #[test]
    fn test_from_uuid_bytes() {
        let b = [
            0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
            0xd7, 0xd8,
        ];

        let u = Uuid::from_uuid_bytes(b);
        let expected = "a1a2a3a4b1b2c1c2d1d2d3d4d5d6d7d8";

        assert_eq!(u.simple().to_string(), expected);
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
        let b_in: [u8; 16] = [
            0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
            0xd7, 0xd8,
        ];

        let u = Uuid::from_bytes(&b_in).unwrap();

        let b_out = u.as_bytes();

        assert_eq!(&b_in, b_out);
    }

    #[test]
    fn test_from_random_bytes() {
        let b = [
            0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
            0xd7, 0xd8,
        ];

        let u = Uuid::from_random_bytes(b);
        let expected = "a1a2a3a4b1b241c291d2d3d4d5d6d7d8";

        assert_eq!(u.simple().to_string(), expected);
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
