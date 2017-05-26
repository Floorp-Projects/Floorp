// Copyright 2013-2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Generate and parse UUIDs
//!
//! Provides support for Universally Unique Identifiers (UUIDs). A UUID is a
//! unique 128-bit number, stored as 16 octets.  UUIDs are used to  assign unique
//! identifiers to entities without requiring a central allocating authority.
//!
//! They are particularly useful in distributed systems, though can be used in
//! disparate areas, such as databases and network protocols.  Typically a UUID is
//! displayed in a readable string form as a sequence of hexadecimal digits,
//! separated into groups by hyphens.
//!
//! The uniqueness property is not strictly guaranteed, however for all practical
//! purposes, it can be assumed that an unintentional collision would be extremely
//! unlikely.
//!
//! # Examples
//!
//! To create a new random (V4) UUID and print it out in hexadecimal form:
//!
//! ```rust
//! use uuid::Uuid;
//!
//! fn main() {
//!     let my_uuid = Uuid::new_v4();
//!     println!("{}", my_uuid);
//! }
//! ```
//!
//! To parse parse a UUID in the simple format and print it as a urn:
//!
//! ```rust
//! use uuid::Uuid;
//!
//! fn main() {
//!     let my_uuid = Uuid::parse_str("936DA01F9ABD4d9d80C702AF85C822A8").unwrap();
//!     println!("{}", my_uuid.to_urn_string());
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

#![doc(html_logo_url = "https://www.rust-lang.org/logos/rust-logo-128x128-blk-v2.png",
       html_favicon_url = "https://www.rust-lang.org/favicon.ico",
       html_root_url = "https://doc.rust-lang.org/uuid/")]

#![cfg_attr(test, deny(warnings))]

extern crate rustc_serialize;
#[cfg(feature = "serde")]
extern crate serde;
extern crate rand;

use std::default::Default;
use std::error::Error;
use std::fmt;
use std::hash;
use std::iter::repeat;
use std::mem::{transmute, transmute_copy};
use std::str::FromStr;

use rand::Rng;
use rustc_serialize::{Encoder, Encodable, Decoder, Decodable};
#[cfg(feature = "serde")]
use serde::{de, Deserialize, Deserializer, Serialize, Serializer};

/// A 128-bit (16 byte) buffer containing the ID
pub type UuidBytes = [u8; 16];

/// The version of the UUID, denoting the generating algorithm
#[derive(PartialEq, Copy, Clone)]
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

/// The reserved variants of UUIDs
#[derive(PartialEq, Copy, Clone)]
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

/// A Universally Unique Identifier (UUID)
#[derive(Copy, Clone)]
pub struct Uuid {
    /// The 128-bit number stored in 16 bytes
    bytes: UuidBytes,
}

impl hash::Hash for Uuid {
    fn hash<S: hash::Hasher>(&self, state: &mut S) {
        self.bytes.hash(state)
    }
}

/// A UUID stored as fields (identical to UUID, used only for conversions)
#[derive(Copy, Clone)]
struct UuidFields {
    /// First field, 32-bit word
    data1: u32,
    /// Second field, 16-bit short
    data2: u16,
    /// Third field, 16-bit short
    data3: u16,
    /// Fourth field, 8 bytes
    data4: [u8; 8],
}

/// Error details for string parsing failures
#[allow(missing_docs)]
#[derive(PartialEq, Eq, Copy, Clone, Debug)]
pub enum ParseError {
    InvalidLength(usize),
    InvalidCharacter(char, usize),
    InvalidGroups(usize),
    InvalidGroupLength(usize, usize, u8),
}

/// Converts a ParseError to a string
impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ParseError::InvalidLength(found) =>
                write!(f,
                       "Invalid length; expecting 32 or 36 chars, found {}",
                       found),
            ParseError::InvalidCharacter(found, pos) =>
                write!(f,
                       "Invalid character; found `{}` (0x{:02x}) at offset {}",
                       found,
                       found as usize,
                       pos),
            ParseError::InvalidGroups(found) =>
                write!(f,
                       "Malformed; wrong number of groups: expected 1 or 5, found {}",
                       found),
            ParseError::InvalidGroupLength(group, found, expecting) =>
                write!(f,
                       "Malformed; length of group {} was {}, expecting {}",
                       group,
                       found,
                       expecting),
        }
    }
}

impl Error for ParseError {
    fn description(&self) -> &str {
        "UUID parse error"
    }
}

// Length of each hyphenated group in hex digits.
const GROUP_LENS: [u8; 5] = [8, 4, 4, 4, 12];
// Accumulated length of each hyphenated group in hex digits.
const ACC_GROUP_LENS: [u8; 5] = [8, 12, 16, 20, 32];

/// UUID support
impl Uuid {
    /// Returns a nil or empty UUID (containing all zeroes)
    pub fn nil() -> Uuid {
        Uuid { bytes: [0; 16] }
    }

    /// Create a new UUID of the specified version
    pub fn new(v: UuidVersion) -> Option<Uuid> {
        match v {
            UuidVersion::Random => Some(Uuid::new_v4()),
            _ => None,
        }
    }

    /// Creates a new random UUID
    ///
    /// Uses the `rand` module's default RNG task as the source
    /// of random numbers. Use the rand::Rand trait to supply
    /// a custom generator if required.
    pub fn new_v4() -> Uuid {
        let ub = rand::thread_rng().gen_iter::<u8>().take(16).collect::<Vec<_>>();
        let mut uuid = Uuid { bytes: [0; 16] };
        copy_memory(&mut uuid.bytes, &ub);
        uuid.set_variant(UuidVariant::RFC4122);
        uuid.set_version(UuidVersion::Random);
        uuid
    }

    /// Creates a UUID using the supplied field values
    ///
    /// # Arguments
    /// * `d1` A 32-bit word
    /// * `d2` A 16-bit word
    /// * `d3` A 16-bit word
    /// * `d4` Array of 8 octets
    pub fn from_fields(d1: u32, d2: u16, d3: u16, d4: &[u8]) -> Uuid {
        // First construct a temporary field-based struct
        let mut fields = UuidFields {
            data1: 0,
            data2: 0,
            data3: 0,
            data4: [0; 8],
        };

        fields.data1 = d1.to_be();
        fields.data2 = d2.to_be();
        fields.data3 = d3.to_be();
        copy_memory(&mut fields.data4, d4);

        unsafe { transmute(fields) }
    }

    /// Creates a UUID using the supplied bytes
    ///
    /// # Arguments
    /// * `b` An array or slice of 16 bytes
    pub fn from_bytes(b: &[u8]) -> Option<Uuid> {
        if b.len() != 16 {
            return None
        }

        let mut uuid = Uuid { bytes: [0; 16] };
        copy_memory(&mut uuid.bytes, b);
        Some(uuid)
    }

    /// Specifies the variant of the UUID structure
    fn set_variant(&mut self, v: UuidVariant) {
        // Octet 8 contains the variant in the most significant 3 bits
        self.bytes[8] = match v {
            UuidVariant::NCS => self.bytes[8] & 0x7f,                // b0xx...
            UuidVariant::RFC4122 => (self.bytes[8] & 0x3f) | 0x80,   // b10x...
            UuidVariant::Microsoft => (self.bytes[8] & 0x1f) | 0xc0, // b110...
            UuidVariant::Future => (self.bytes[8] & 0x1f) | 0xe0,    // b111...
        }
    }

    /// Returns the variant of the UUID structure
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

    /// Specifies the version number of the UUID
    fn set_version(&mut self, v: UuidVersion) {
        self.bytes[6] = (self.bytes[6] & 0xF) | ((v as u8) << 4);
    }

    /// Returns the version number of the UUID
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

    /// Returns the version of the UUID
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

    /// Return an array of 16 octets containing the UUID data
    pub fn as_bytes<'a>(&'a self) -> &'a [u8] {
        &self.bytes
    }

    /// Returns the UUID as a string of 32 hexadecimal digits
    ///
    /// Example: `936DA01F9ABD4d9d80C702AF85C822A8`
    pub fn to_simple_string(&self) -> String {
        let mut s = repeat(0u8).take(32).collect::<Vec<_>>();
        for i in 0..16 {
            let digit = format!("{:02x}", self.bytes[i] as usize);
            s[i * 2 + 0] = digit.as_bytes()[0];
            s[i * 2 + 1] = digit.as_bytes()[1];
        }
        String::from_utf8(s).unwrap()
    }

    /// Returns a string of hexadecimal digits, separated into groups with a hyphen.
    ///
    /// Example: `550e8400-e29b-41d4-a716-446655440000`
    pub fn to_hyphenated_string(&self) -> String {
        // Convert to field-based struct as it matches groups in output.
        // Ensure fields are in network byte order, as per RFC.
        let mut uf: UuidFields;
        unsafe {
            uf = transmute_copy(&self.bytes);
        }
        uf.data1 = uf.data1.to_be();
        uf.data2 = uf.data2.to_be();
        uf.data3 = uf.data3.to_be();
        let s = format!("{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                        uf.data1,
                        uf.data2,
                        uf.data3,
                        uf.data4[0],
                        uf.data4[1],
                        uf.data4[2],
                        uf.data4[3],
                        uf.data4[4],
                        uf.data4[5],
                        uf.data4[6],
                        uf.data4[7]);
        s
    }

    /// Returns the UUID formatted as a full URN string
    ///
    /// This is the same as the hyphenated format, but with the "urn:uuid:" prefix.
    ///
    /// Example: `urn:uuid:F9168C5E-CEB2-4faa-B6BF-329BF39FA1E4`
    pub fn to_urn_string(&self) -> String {
        format!("urn:uuid:{}", self.to_hyphenated_string())
    }

    /// Parses a UUID from a string of hexadecimal digits with optional hyphens
    ///
    /// Any of the formats generated by this module (simple, hyphenated, urn) are
    /// supported by this parsing function.
    pub fn parse_str(mut input: &str) -> Result<Uuid, ParseError> {
        // Ensure length is valid for any of the supported formats
        let len = input.len();
        if len == 45 && input.starts_with("urn:uuid:") {
            input = &input[9..];
        } else if len != 32 && len != 36 {
            return Err(ParseError::InvalidLength(len));
        }

        // `digit` counts only hexadecimal digits, `i_char` counts all chars.
        let mut digit = 0;
        let mut group = 0;
        let mut acc = 0;
        let mut buffer = [0u8; 16];

        for (i_char, chr) in input.chars().enumerate() {
            if digit % 2 == 0 {
                // First digit of the byte.
                match chr {
                    // Calulate upper half.
                    '0'...'9' => acc = chr as u8 - '0' as u8,
                    'a'...'f' => acc = chr as u8 - 'a' as u8 + 10,
                    'A'...'F' => acc = chr as u8 - 'A' as u8 + 10,
                    // Found a group delimiter
                    '-' => {
                        if ACC_GROUP_LENS[group] != digit {
                            // Calculate how many digits this group consists of in the input.
                            let found = if group > 0 {
                                digit - ACC_GROUP_LENS[group - 1]
                            } else {
                                digit
                            };
                            return Err(ParseError::InvalidGroupLength(group,
                                                                      found as usize,
                                                                      GROUP_LENS[group]));
                        }
                        // Next group, decrement digit, it is incremented again at the bottom.
                        group += 1;
                        digit -= 1;
                    }
                    _ => return Err(ParseError::InvalidCharacter(chr, i_char)),
                }
            } else {
                // Second digit of the byte, shift the upper half.
                acc *= 16;
                match chr {
                    '0'...'9' => acc += chr as u8 - '0' as u8,
                    'a'...'f' => acc += chr as u8 - 'a' as u8 + 10,
                    'A'...'F' => acc += chr as u8 - 'A' as u8 + 10,
                    '-' => {
                        // The byte isn't complete yet.
                        let found = if group > 0 {
                            digit - ACC_GROUP_LENS[group - 1]
                        } else {
                            digit
                        };
                        return Err(ParseError::InvalidGroupLength(group,
                                                                  found as usize,
                                                                  GROUP_LENS[group]));
                    }
                    _ => return Err(ParseError::InvalidCharacter(chr, i_char)),
                }
                buffer[(digit / 2) as usize] = acc;
            }
            digit += 1;
        }

        // Now check the last group.
        if group != 0 && group != 4 {
            return Err(ParseError::InvalidGroups(group + 1));
        } else if ACC_GROUP_LENS[4] != digit {
            return Err(ParseError::InvalidGroupLength(group,
                                                      (digit - ACC_GROUP_LENS[3]) as usize,
                                                      GROUP_LENS[4]));
        }

        Ok(Uuid::from_bytes(&mut buffer).unwrap())
    }

    /// Tests if the UUID is nil
    pub fn is_nil(&self) -> bool {
        self.bytes.iter().all(|&b| b == 0)
    }
}

fn copy_memory(dst: &mut [u8], src: &[u8]) {
    for (slot, val) in dst.iter_mut().zip(src.iter()) {
        *slot = *val;
    }
}

impl Default for Uuid {
    /// Returns the nil UUID, which is all zeroes
    fn default() -> Uuid {
        Uuid::nil()
    }
}

impl FromStr for Uuid {
    type Err = ParseError;

    /// Parse a hex string and interpret as a UUID
    ///
    /// Accepted formats are a sequence of 32 hexadecimal characters,
    /// with or without hyphens (grouped as 8, 4, 4, 4, 12).
    fn from_str(us: &str) -> Result<Uuid, ParseError> {
        Uuid::parse_str(us)
    }
}

/// Convert the UUID to a hexadecimal-based string representation wrapped in `Uuid()`
impl fmt::Debug for Uuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Uuid(\"{}\")", self.to_hyphenated_string())
    }
}

/// Convert the UUID to a hexadecimal-based string representation
impl fmt::Display for Uuid {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.to_hyphenated_string())
    }
}

/// Test two UUIDs for equality
///
/// UUIDs are equal only when they are byte-for-byte identical
impl PartialEq for Uuid {
    fn eq(&self, other: &Uuid) -> bool {
        self.bytes == other.bytes
    }
}

impl Eq for Uuid {}

// FIXME #9845: Test these more thoroughly
impl Encodable for Uuid {
    /// Encode a UUID as a hyphenated string
    fn encode<E: Encoder>(&self, e: &mut E) -> Result<(), E::Error> {
        e.emit_str(&self.to_hyphenated_string())
    }
}

impl Decodable for Uuid {
    /// Decode a UUID from a string
    fn decode<D: Decoder>(d: &mut D) -> Result<Uuid, D::Error> {
        let string = try!(d.read_str());
        string.parse().map_err(|err| d.error(&format!("{}", err)))
    }
}

#[cfg(feature = "serde")]
impl Serialize for Uuid {
    fn serialize<S: Serializer>(&self, serializer: &mut S) -> Result<(), S::Error> {
        serializer.visit_str(&self.to_hyphenated_string())
    }
}

#[cfg(feature = "serde")]
impl Deserialize for Uuid {
    fn deserialize<D: Deserializer>(deserializer: &mut D) -> Result<Self, D::Error> {
        struct UuidVisitor;

        impl de::Visitor for UuidVisitor {
            type Value = Uuid;

            fn visit_str<E: de::Error>(&mut self, value: &str) -> Result<Uuid, E> {
                value.parse().map_err(|err| E::syntax(&format!("{}", err)))
            }

            fn visit_bytes<E: de::Error>(&mut self, value: &[u8]) -> Result<Uuid, E> {
                Uuid::from_bytes(value).ok_or(E::syntax("Expected 16 bytes."))
            }
        }

        deserializer.visit(UuidVisitor)
    }
}

/// Generates a random instance of UUID (V4 conformant)
impl rand::Rand for Uuid {
    #[inline]
    fn rand<R: rand::Rng>(rng: &mut R) -> Uuid {
        let ub = rng.gen_iter::<u8>().take(16).collect::<Vec<_>>();
        let mut uuid = Uuid { bytes: [0; 16] };
        copy_memory(&mut uuid.bytes, &ub);
        uuid.set_variant(UuidVariant::RFC4122);
        uuid.set_version(UuidVersion::Random);
        uuid
    }
}

#[cfg(test)]
mod tests {
    use super::{Uuid, UuidVariant, UuidVersion};
    use rand;

    #[test]
    fn test_nil() {
        let nil = Uuid::nil();
        let not_nil = Uuid::new_v4();

        assert!(nil.is_nil());
        assert!(!not_nil.is_nil());
    }

    #[test]
    fn test_new() {
        // Supported
        let uuid1 = Uuid::new(UuidVersion::Random).unwrap();
        let s = uuid1.to_simple_string();

        assert!(s.len() == 32);
        assert!(uuid1.get_version().unwrap() == UuidVersion::Random);

        // Test unsupported versions
        assert!(Uuid::new(UuidVersion::Mac) == None);
        assert!(Uuid::new(UuidVersion::Dce) == None);
        assert!(Uuid::new(UuidVersion::Md5) == None);
        assert!(Uuid::new(UuidVersion::Sha1) == None);
    }

    #[test]
    fn test_new_v4() {
        let uuid1 = Uuid::new_v4();

        assert!(uuid1.get_version().unwrap() == UuidVersion::Random);
        assert!(uuid1.get_variant().unwrap() == UuidVariant::RFC4122);
    }

    #[test]
    fn test_get_version() {
        let uuid1 = Uuid::new_v4();

        assert!(uuid1.get_version().unwrap() == UuidVersion::Random);
        assert!(uuid1.get_version_num() == 4);
    }

    #[test]
    fn test_get_variant() {
        let uuid1 = Uuid::new_v4();
        let uuid2 = Uuid::parse_str("550e8400-e29b-41d4-a716-446655440000").unwrap();
        let uuid3 = Uuid::parse_str("67e55044-10b1-426f-9247-bb680e5fe0c8").unwrap();
        let uuid4 = Uuid::parse_str("936DA01F9ABD4d9dC0C702AF85C822A8").unwrap();
        let uuid5 = Uuid::parse_str("F9168C5E-CEB2-4faa-D6BF-329BF39FA1E4").unwrap();
        let uuid6 = Uuid::parse_str("f81d4fae-7dec-11d0-7765-00a0c91e6bf6").unwrap();

        assert!(uuid1.get_variant().unwrap() == UuidVariant::RFC4122);
        assert!(uuid2.get_variant().unwrap() == UuidVariant::RFC4122);
        assert!(uuid3.get_variant().unwrap() == UuidVariant::RFC4122);
        assert!(uuid4.get_variant().unwrap() == UuidVariant::Microsoft);
        assert!(uuid5.get_variant().unwrap() == UuidVariant::Microsoft);
        assert!(uuid6.get_variant().unwrap() == UuidVariant::NCS);
    }

    #[test]
    fn test_parse_uuid_v4() {
        use super::ParseError::*;

        // Invalid
        assert_eq!(Uuid::parse_str(""), Err(InvalidLength(0)));
        assert_eq!(Uuid::parse_str("!"), Err(InvalidLength(1)));
        assert_eq!(Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF-329BF39FA1E45"),
                   Err(InvalidLength(37)));
        assert_eq!(Uuid::parse_str("F9168C5E-CEB2-4faa-BBF-329BF39FA1E4"),
                   Err(InvalidLength(35)));
        assert_eq!(Uuid::parse_str("F9168C5E-CEB2-4faa-BGBF-329BF39FA1E4"),
                   Err(InvalidCharacter('G', 20)));
        assert_eq!(Uuid::parse_str("F9168C5E-CEB2-4faa-B6BFF329BF39FA1E4"),
                   Err(InvalidGroups(4)));
        assert_eq!(Uuid::parse_str("F9168C5E-CEB2-4faa"),
                   Err(InvalidLength(18)));
        assert_eq!(Uuid::parse_str("F9168C5E-CEB2-4faaXB6BFF329BF39FA1E4"),
                   Err(InvalidCharacter('X', 18)));
        assert_eq!(Uuid::parse_str("F9168C5E-CEB-24fa-eB6BFF32-BF39FA1E4"),
                   Err(InvalidGroupLength(1, 3, 4)));
        assert_eq!(Uuid::parse_str("01020304-1112-2122-3132-41424344"),
                   Err(InvalidGroupLength(4, 8, 12)));
        assert_eq!(Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c"),
                   Err(InvalidLength(31)));
        assert_eq!(Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c88"),
                   Err(InvalidLength(33)));
        assert_eq!(Uuid::parse_str("67e5504410b1426f9247bb680e5fe0cg8"),
                   Err(InvalidLength(33)));
        assert_eq!(Uuid::parse_str("67e5504410b1426%9247bb680e5fe0c8"),
                   Err(InvalidCharacter('%', 15)));

        // Valid
        assert!(Uuid::parse_str("00000000000000000000000000000000").is_ok());
        assert!(Uuid::parse_str("67e55044-10b1-426f-9247-bb680e5fe0c8").is_ok());
        assert!(Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF-329BF39FA1E4").is_ok());
        assert!(Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c8").is_ok());
        assert!(Uuid::parse_str("01020304-1112-2122-3132-414243444546").is_ok());
        assert!(Uuid::parse_str("urn:uuid:67e55044-10b1-426f-9247-bb680e5fe0c8").is_ok());

        // Nil
        let nil = Uuid::nil();
        assert!(Uuid::parse_str("00000000000000000000000000000000").unwrap() == nil);
        assert!(Uuid::parse_str("00000000-0000-0000-0000-000000000000").unwrap() == nil);

        // Round-trip
        let uuid_orig = Uuid::new_v4();
        let orig_str = uuid_orig.to_string();
        let uuid_out = Uuid::parse_str(&orig_str).unwrap();
        assert!(uuid_orig == uuid_out);

        // Test error reporting
        assert_eq!(Uuid::parse_str("67e5504410b1426f9247bb680e5fe0c"),
                   Err(InvalidLength(31)));
        assert_eq!(Uuid::parse_str("67e550X410b1426f9247bb680e5fe0cd"),
                   Err(InvalidCharacter('X', 6)));
        assert_eq!(Uuid::parse_str("67e550-4105b1426f9247bb680e5fe0c"),
                   Err(InvalidGroupLength(0, 6, 8)));
        assert_eq!(Uuid::parse_str("F9168C5E-CEB2-4faa-B6BF1-02BF39FA1E4"),
                   Err(InvalidGroupLength(3, 5, 4)));
    }

    #[test]
    fn test_to_simple_string() {
        let uuid1 = Uuid::new_v4();
        let s = uuid1.to_simple_string();

        assert!(s.len() == 32);
        assert!(s.chars().all(|c| c.is_digit(16)));
    }

    #[test]
    fn test_to_string() {
        let uuid1 = Uuid::new_v4();
        let s = uuid1.to_string();

        assert!(s.len() == 36);
        assert!(s.chars().all(|c| c.is_digit(16) || c == '-'));
    }

    #[test]
    fn test_to_hyphenated_string() {
        let uuid1 = Uuid::new_v4();
        let s = uuid1.to_hyphenated_string();

        assert!(s.len() == 36);
        assert!(s.chars().all(|c| c.is_digit(16) || c == '-'));
    }

    #[test]
    fn test_to_urn_string() {
        let uuid1 = Uuid::new_v4();
        let ss = uuid1.to_urn_string();
        let s = &ss[9..];

        assert!(ss.starts_with("urn:uuid:"));
        assert!(s.len() == 36);
        assert!(s.chars().all(|c| c.is_digit(16) || c == '-'));
    }

    #[test]
    fn test_to_simple_string_matching() {
        let uuid1 = Uuid::new_v4();

        let hs = uuid1.to_hyphenated_string();
        let ss = uuid1.to_simple_string();

        let hsn = hs.chars().filter(|&c| c != '-').collect::<String>();

        assert!(hsn == ss);
    }

    #[test]
    fn test_string_roundtrip() {
        let uuid = Uuid::new_v4();

        let hs = uuid.to_hyphenated_string();
        let uuid_hs = Uuid::parse_str(&hs).unwrap();
        assert!(uuid_hs == uuid);

        let ss = uuid.to_string();
        let uuid_ss = Uuid::parse_str(&ss).unwrap();
        assert!(uuid_ss == uuid);
    }

    #[test]
    fn test_compare() {
        let uuid1 = Uuid::new_v4();
        let uuid2 = Uuid::new_v4();

        assert!(uuid1 == uuid1);
        assert!(uuid2 == uuid2);
        assert!(uuid1 != uuid2);
        assert!(uuid2 != uuid1);
    }

    #[test]
    fn test_from_fields() {
        let d1: u32 = 0xa1a2a3a4;
        let d2: u16 = 0xb1b2;
        let d3: u16 = 0xc1c2;
        let d4: Vec<u8> = vec!(0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8);

        let u = Uuid::from_fields(d1, d2, d3, &d4);

        let expected = "a1a2a3a4b1b2c1c2d1d2d3d4d5d6d7d8".to_string();
        let result = u.to_simple_string();
        assert!(result == expected);
    }

    #[test]
    fn test_from_bytes() {
        let b = vec!(0xa1,
                     0xa2,
                     0xa3,
                     0xa4,
                     0xb1,
                     0xb2,
                     0xc1,
                     0xc2,
                     0xd1,
                     0xd2,
                     0xd3,
                     0xd4,
                     0xd5,
                     0xd6,
                     0xd7,
                     0xd8);

        let u = Uuid::from_bytes(&b).unwrap();
        let expected = "a1a2a3a4b1b2c1c2d1d2d3d4d5d6d7d8".to_string();

        assert!(u.to_simple_string() == expected);
    }

    #[test]
    fn test_as_bytes() {
        let u = Uuid::new_v4();
        let ub = u.as_bytes();

        assert!(ub.len() == 16);
        assert!(!ub.iter().all(|&b| b == 0));
    }

    #[test]
    fn test_bytes_roundtrip() {
        let b_in: [u8; 16] = [0xa1, 0xa2, 0xa3, 0xa4, 0xb1, 0xb2, 0xc1, 0xc2, 0xd1, 0xd2, 0xd3,
                              0xd4, 0xd5, 0xd6, 0xd7, 0xd8];

        let u = Uuid::from_bytes(&b_in).unwrap();

        let b_out = u.as_bytes();

        assert_eq!(&b_in, b_out);
    }

    #[test]
    fn test_operator_eq() {
        let u1 = Uuid::new_v4();
        let u2 = u1.clone();
        let u3 = Uuid::new_v4();

        assert!(u1 == u1);
        assert!(u1 == u2);
        assert!(u2 == u1);

        assert!(u1 != u3);
        assert!(u3 != u1);
        assert!(u2 != u3);
        assert!(u3 != u2);
    }

    #[test]
    fn test_rand_rand() {
        let mut rng = rand::thread_rng();
        let u: Uuid = rand::Rand::rand(&mut rng);
        let ub = u.as_bytes();

        assert!(ub.len() == 16);
        assert!(!ub.iter().all(|&b| b == 0));
    }

    #[test]
    fn test_serialize_round_trip() {
        use rustc_serialize::json;

        let u = Uuid::new_v4();
        let s = json::encode(&u).unwrap();
        let u2 = json::decode(&s).unwrap();
        assert_eq!(u, u2);
    }

    #[test]
    fn test_iterbytes_impl_for_uuid() {
        use std::collections::HashSet;
        let mut set = HashSet::new();
        let id1 = Uuid::new_v4();
        let id2 = Uuid::new_v4();
        set.insert(id1.clone());
        assert!(set.contains(&id1));
        assert!(!set.contains(&id2));
    }
}

/*
TODO: when benchmarking is stable, re-add these
#[cfg(test)]
mod bench {
    extern crate test;
    use self::test::Bencher;
    use super::Uuid;

    #[bench]
    pub fn create_uuids(b: &mut Bencher) {
        b.iter(|| {
            Uuid::new_v4();
        })
    }

    #[bench]
    pub fn uuid_to_string(b: &mut Bencher) {
        let u = Uuid::new_v4();
        b.iter(|| {
            u.to_string();
        })
    }

    #[bench]
    pub fn parse_str(b: &mut Bencher) {
        let s = "urn:uuid:F9168C5E-CEB2-4faa-B6BF-329BF39FA1E4";
        b.iter(|| {
            Uuid::parse_str(s).unwrap();
        })
    }
}
*/
