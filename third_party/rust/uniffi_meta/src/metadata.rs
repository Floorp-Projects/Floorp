/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copied from uniffi_core/src/metadata.rs
// Due to a [Rust bug](https://github.com/rust-lang/rust/issues/113104) we don't want to pull in
// `uniffi_core`.
// This is the easy way out of that issue and is a temporary hacky solution.

/// Metadata constants, make sure to keep this in sync with copy in `uniffi_meta::reader`
pub mod codes {
    // Top-level metadata item codes
    pub const FUNC: u8 = 0;
    pub const METHOD: u8 = 1;
    pub const RECORD: u8 = 2;
    pub const ENUM: u8 = 3;
    pub const INTERFACE: u8 = 4;
    pub const ERROR: u8 = 5;
    pub const NAMESPACE: u8 = 6;
    pub const CONSTRUCTOR: u8 = 7;
    pub const UDL_FILE: u8 = 8;
    pub const CALLBACK_INTERFACE: u8 = 9;
    pub const TRAIT_METHOD: u8 = 10;
    pub const UNIFFI_TRAIT: u8 = 11;
    //pub const UNKNOWN: u8 = 255;

    // Type codes
    pub const TYPE_U8: u8 = 0;
    pub const TYPE_U16: u8 = 1;
    pub const TYPE_U32: u8 = 2;
    pub const TYPE_U64: u8 = 3;
    pub const TYPE_I8: u8 = 4;
    pub const TYPE_I16: u8 = 5;
    pub const TYPE_I32: u8 = 6;
    pub const TYPE_I64: u8 = 7;
    pub const TYPE_F32: u8 = 8;
    pub const TYPE_F64: u8 = 9;
    pub const TYPE_BOOL: u8 = 10;
    pub const TYPE_STRING: u8 = 11;
    pub const TYPE_OPTION: u8 = 12;
    pub const TYPE_RECORD: u8 = 13;
    pub const TYPE_ENUM: u8 = 14;
    // 15 no longer used.
    pub const TYPE_INTERFACE: u8 = 16;
    pub const TYPE_VEC: u8 = 17;
    pub const TYPE_HASH_MAP: u8 = 18;
    pub const TYPE_SYSTEM_TIME: u8 = 19;
    pub const TYPE_DURATION: u8 = 20;
    pub const TYPE_CALLBACK_INTERFACE: u8 = 21;
    pub const TYPE_CUSTOM: u8 = 22;
    pub const TYPE_RESULT: u8 = 23;
    //pub const TYPE_FUTURE: u8 = 24;
    pub const TYPE_FOREIGN_EXECUTOR: u8 = 25;
    pub const TYPE_UNIT: u8 = 255;

    // Literal codes
    pub const LIT_STR: u8 = 0;
    pub const LIT_INT: u8 = 1;
    pub const LIT_FLOAT: u8 = 2;
    pub const LIT_BOOL: u8 = 3;
    pub const LIT_NULL: u8 = 4;
}

// Create a checksum for a MetadataBuffer
//
// This is used by the bindings code to verify that the library they link to is the same one
// that the bindings were generated from.
pub const fn checksum_metadata(buf: &[u8]) -> u16 {
    calc_checksum(buf, buf.len())
}

const fn calc_checksum(bytes: &[u8], size: usize) -> u16 {
    // Taken from the fnv_hash() function from the FNV crate (https://github.com/servo/rust-fnv/blob/master/lib.rs).
    // fnv_hash() hasn't been released in a version yet.
    const INITIAL_STATE: u64 = 0xcbf29ce484222325;
    const PRIME: u64 = 0x100000001b3;

    let mut hash = INITIAL_STATE;
    let mut i = 0;
    while i < size {
        hash ^= bytes[i] as u64;
        hash = hash.wrapping_mul(PRIME);
        i += 1;
    }
    // Convert the 64-bit hash to a 16-bit hash by XORing everything together
    (hash ^ (hash >> 16) ^ (hash >> 32) ^ (hash >> 48)) as u16
}
