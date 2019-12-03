//! Bit-level packing and unpacking for Rust
//! ===========================================
//!
//! [![Build Status](https://travis-ci.org/hashmismatch/packed_struct.rs.svg?branch=master)](https://travis-ci.org/hashmismatch/packed_struct.rs)
//!
//! [![Documentation](https://docs.rs/packed_struct/badge.svg)](https://docs.rs/packed_struct)
//!
//! # Introduction
//!
//! Packing and unpacking bit-level structures is usually a programming tasks that needlessly reinvents the wheel. This library provides
//! a meta-programming approach, using attributes to define fields and how they should be packed. The resulting trait implementations
//! provide safe packing, unpacking and runtime debugging formatters with per-field documentation generated for each structure.
//!
//! # Features
//!
//!  * Plain Rust structures, decorated with attributes
//!  * MSB or LSB integers of user-defined bit widths
//!  * Primitive enum code generation helper
//!  * MSB0 or LSB0 bit positioning
//!  * Documents the field's packing table
//!  * Runtime packing visualization
//!  * Nested packed types
//!  * Arrays of packed structures as fields
//!  * Reserved fields, their bits are always 0 or 1
//!
//! # Sample usage
//!
//! ## Cargo.toml
//!
//! ```toml
//! [dependencies]
//! packed_struct = "0.3"
//! packed_struct_codegen = "0.3"
//! ```
//! ## Including the library and the code generator
//!
//! ```rust
//! extern crate packed_struct;
//! #[macro_use]
//! extern crate packed_struct_codegen;
//! # fn main() {
//! # }
//! ```
//!
//! ## Example of a single-byte structure, with a 3 bit integer, primitive enum and a bool field.
//!
//! ```rust
//! extern crate packed_struct;
//! #[macro_use] extern crate packed_struct_codegen;
//!
//! use packed_struct::prelude::*;
//!
//! #[derive(PackedStruct)]
//! #[packed_struct(bit_numbering="msb0")]
//! pub struct TestPack {
//!     #[packed_field(bits="0..=2")]
//!     tiny_int: Integer<u8, packed_bits::Bits3>,
//!     #[packed_field(bits="3..=4", ty="enum")]
//!     mode: SelfTestMode,
//!     #[packed_field(bits="7")]
//!     enabled: bool
//! }
//!
//! #[derive(PrimitiveEnum_u8, Clone, Copy, Debug, PartialEq)]
//! pub enum SelfTestMode {
//!     NormalMode = 0,
//!     PositiveSignSelfTest = 1,
//!     NegativeSignSelfTest = 2,
//!     DebugMode = 3,
//! }
//!
//! fn main() {
//!     let test = TestPack {
//!         tiny_int: 5.into(),
//!         mode: SelfTestMode::DebugMode,
//!         enabled: true
//!     };
//!
//!     let packed = test.pack();
//!     assert_eq!([0b10111001], packed);
//!
//!     let unpacked = TestPack::unpack(&packed).unwrap();
//!     assert_eq!(*unpacked.tiny_int, 5);
//!     assert_eq!(unpacked.mode, SelfTestMode::DebugMode);
//!     assert_eq!(unpacked.enabled, true);
//! }
//! ```
//!
//! # Packing attributes
//!
//! ## Syntax
//!
//! ```rust
//! extern crate packed_struct;
//! #[macro_use] extern crate packed_struct_codegen;
//!
//! #[derive(PackedStruct)]
//! #[packed_struct(attr1="val", attr2="val")]
//! pub struct Structure {
//!     #[packed_field(attr1="val", attr2="val")]
//!     field: u8
//! }
//! # fn main() {
//! # }
//! ```
//!
//! ## Per-structure attributes
//!
//! Attribute | Values | Comment
//! :--|:--|:--
//! ```size_bytes``` | ```1``` ... n | Size of the packed byte stream
//! ```bit_numbering``` | ```msb0``` or ```lsb0``` | Bit numbering for bit positioning of fields. Required if the bits attribute field is used.
//! ```endian``` | ```msb``` or ```lsb``` | Default integer endianness
//!
//! ## Per-field attributes
//!
//! Attribute | Values | Comment
//! :--|:--|:--
//! ```bits``` | ```0```, ```0..1```, ... | Position of the field in the packed structure. Three modes are supported: a single bit, the starting bit, or a range of bits. See details below.
//! ```bytes``` | ```0```, ```0..1```, ... | Same as above, multiplied by 8.
//! ```size_bits``` | ```1```, ... | Specifies the size of the packed structure. Mandatory for certain types. Specifying a range of bits like ```bits="0..2"``` can substite the required usage of ```size_bits```.
//! ```size_bytes``` | ```1```, ... | Same as above, multiplied by 8.
//! ```element_size_bits``` | ```1```, ... | For packed arrays, specifies the size of a single element of the array. Explicitly stating the size of the entire array can substite the usage of this attribute.
//! ```element_size_bytes``` | ```1```, ... | Same as above, multiplied by 8.
//! ```ty``` | ```enum``` | Packing helper for primitive enums.
//! ```endian``` | ```msb``` or ```lsb``` | Integer endianness. Applies to u16/i16 and larger types.
//! 
//! ## Bit and byte positioning
//! 
//! Used for either ```bits``` or ```bytes``` on fields. The examples are for MSB0 positioning.
//! 
//! Value | Comment
//! :--|:--
//! ```0``` | A single bit or byte
//! ```0..```, ```0:``` | The field starts at bit zero
//! ```0..2``` | Exclusive range, bits zero and one
//! ```0:1```, ```0..=1``` | Inclusive range, bits zero and one
//!
//! # More examples
//!
//! ## Mixed endian integers
//!
//! ```rust
//! extern crate packed_struct;
//! #[macro_use] extern crate packed_struct_codegen;
//!
//! use packed_struct::prelude::*;
//!
//! #[derive(PackedStruct)]
//! pub struct EndianExample {
//!     #[packed_field(endian="lsb")]
//!     int1: u16,
//!     #[packed_field(endian="msb")]
//!     int2: i32
//! }
//!
//! fn main() {
//!     let example = EndianExample {
//!         int1: 0xBBAA,
//!         int2: 0x11223344
//!     };
//!
//!     let packed = example.pack();
//!     assert_eq!([0xAA, 0xBB, 0x11, 0x22, 0x33, 0x44], packed);
//! }
//! ```
//!
//! ## 24 bit LSB integers
//!
//! ```rust
//! extern crate packed_struct;
//! #[macro_use] extern crate packed_struct_codegen;
//!
//! use packed_struct::prelude::*;
//!
//! #[derive(PackedStruct)]
//! #[packed_struct(endian="lsb")]
//! pub struct LsbIntExample {
//!     int1: Integer<u32, packed_bits::Bits24>,
//! }
//!
//! fn main() {
//!     let example = LsbIntExample {
//!         int1: 0xCCBBAA.into()
//!     };
//!
//!     let packed = example.pack();
//!     assert_eq!([0xAA, 0xBB, 0xCC], packed);
//! }
//! ```
//!
//! ## Nested packed types within arrays
//!
//! ```rust
//! extern crate packed_struct;
//! #[macro_use] extern crate packed_struct_codegen;
//!
//! use packed_struct::prelude::*;
//!
//! #[derive(PackedStruct, Default, Debug, PartialEq)]
//! #[packed_struct(bit_numbering="msb0")]
//! pub struct TinyFlags {
//!     _reserved: ReservedZero<packed_bits::Bits4>,
//!     flag1: bool,
//!     val1: Integer<u8, packed_bits::Bits2>,
//!     flag2: bool
//! }
//!
//! #[derive(PackedStruct, Debug, PartialEq)]
//! pub struct Settings {
//!     #[packed_field(element_size_bits="4")]
//!     values: [TinyFlags; 4]
//! }
//!
//! fn main() {
//!     let example = Settings {
//!         values: [
//!             TinyFlags { flag1: true,  val1: 1.into(), flag2: false, .. TinyFlags::default() },
//!             TinyFlags { flag1: true,  val1: 2.into(), flag2: true,  .. TinyFlags::default() },
//!             TinyFlags { flag1: false, val1: 3.into(), flag2: false, .. TinyFlags::default() },
//!             TinyFlags { flag1: true,  val1: 0.into(), flag2: false, .. TinyFlags::default() },
//!         ]
//!     };
//!
//!     let packed = example.pack();
//!     let unpacked = Settings::unpack(&packed).unwrap();
//!
//!     assert_eq!(example, unpacked);
//! }
//! ```
//! 
//! # Primitive enums with simple discriminants
//! 
//! Supported backing integer types: ```u8```, ```u16```, ```u32```, ```u64```, ```i8```, ```i16```, ```i32```, ```i64```.
//! 
//! Explicit or implicit backing type:
//! 
//! ```rust
//! extern crate packed_struct;
//! #[macro_use] extern crate packed_struct_codegen;
//!
//! #[derive(PrimitiveEnum, Clone, Copy)]
//! pub enum ImplicitType {
//!     VariantMin = 0,
//!     VariantMax = 255
//! }
//! 
//! #[derive(PrimitiveEnum_i16, Clone, Copy)]
//! pub enum ExplicitType {
//!     VariantMin = -32768,
//!     VariantMax = 32767
//! }
//! 
//! # fn main() {}
//! ```
//! 
//! # Primitive enum packing with support for catch-all unknown values
//! 
//! ```rust
//! # use packed_struct::prelude::*;
//! extern crate packed_struct;
//! #[macro_use] extern crate packed_struct_codegen;
//!
//! #[derive(PrimitiveEnum_u8, Debug, Clone, Copy)]
//! pub enum Field {
//!     A = 1,
//!     B = 2,
//!     C = 3
//! }
//! 
//! #[derive(PackedStruct, Debug, PartialEq)]
//! #[packed_struct(bit_numbering="msb0")]
//! pub struct Register {
//!     #[packed_field(bits="0..4", ty="enum")]
//!     field: EnumCatchAll<Field>
//! }
//! 
//! # fn main() {}
//! ```

#![cfg_attr(not(feature = "std"), no_std)]

#![cfg_attr(feature="alloc", feature(alloc))]

#[cfg(feature="alloc")]
#[macro_use]
extern crate alloc;


extern crate serde;
#[macro_use] extern crate serde_derive;

mod internal_prelude;

#[macro_use]
mod packing;

mod primitive_enum;
pub use primitive_enum::*;


#[cfg(any(feature="alloc", feature="std"))]
pub mod debug_fmt;

mod types_array;
mod types_basic;
mod types_bits;
mod types_num;
mod types_reserved;

/// Implementations and wrappers for various packing types.
pub mod types {
    pub use super::types_basic::*;

    /// Types that specify the exact number of bits a packed integer should occupy.
    pub mod bits {
        pub use super::super::types_bits::*;
    }

    pub use super::types_num::*;
    pub use super::types_array::*;
    pub use super::types_reserved::*;
}

pub use self::packing::*;


pub mod prelude {
    //! Re-exports the most useful traits and types. Meant to be glob imported.

    pub use PackedStruct;
    pub use PackedStructSlice;
    pub use PackingError;

    pub use PrimitiveEnum;
    #[cfg(any(feature="alloc", feature="std"))]
    pub use PrimitiveEnumDynamicStr;

    #[cfg(not(any(feature="alloc", feature="std")))]
    pub use PrimitiveEnumStaticStr;


    pub use EnumCatchAll;

    pub use types::*;
    pub use types::bits as packed_bits;
}
