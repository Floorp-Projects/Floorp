//! CBOR and serialization.
//!
//! # Usage
//!
//! Serde CBOR supports Rust 1.40 and up. Add this to your `Cargo.toml`:
//! ```toml
//! [dependencies]
//! serde_cbor = "0.10"
//! ```
//!
//! Storing and loading Rust types is easy and requires only
//! minimal modifications to the program code.
//!
//! ```rust
//! use serde_derive::{Deserialize, Serialize};
//! use std::error::Error;
//! use std::fs::File;
//!
//! // Types annotated with `Serialize` can be stored as CBOR.
//! // To be able to load them again add `Deserialize`.
//! #[derive(Debug, Serialize, Deserialize)]
//! struct Mascot {
//!     name: String,
//!     species: String,
//!     year_of_birth: u32,
//! }
//!
//! fn main() -> Result<(), Box<dyn Error>> {
//!     let ferris = Mascot {
//!         name: "Ferris".to_owned(),
//!         species: "crab".to_owned(),
//!         year_of_birth: 2015,
//!     };
//!
//!     let ferris_file = File::create("examples/ferris.cbor")?;
//!     // Write Ferris to the given file.
//!     // Instead of a file you can use any type that implements `io::Write`
//!     // like a HTTP body, database connection etc.
//!     serde_cbor::to_writer(ferris_file, &ferris)?;
//!
//!     let tux_file = File::open("examples/tux.cbor")?;
//!     // Load Tux from a file.
//!     // Serde CBOR performs roundtrip serialization meaning that
//!     // the data will not change in any way.
//!     let tux: Mascot = serde_cbor::from_reader(tux_file)?;
//!
//!     println!("{:?}", tux);
//!     // prints: Mascot { name: "Tux", species: "penguin", year_of_birth: 1996 }
//!
//!     Ok(())
//! }
//! ```
//!
//! There are a lot of options available to customize the format.
//! To operate on untyped CBOR values have a look at the `Value` type.
//!
//! # Type-based Serialization and Deserialization
//! Serde provides a mechanism for low boilerplate serialization & deserialization of values to and
//! from CBOR via the serialization API. To be able to serialize a piece of data, it must implement
//! the `serde::Serialize` trait. To be able to deserialize a piece of data, it must implement the
//! `serde::Deserialize` trait. Serde provides an annotation to automatically generate the
//! code for these traits: `#[derive(Serialize, Deserialize)]`.
//!
//! The CBOR API also provides an enum `serde_cbor::Value`.
//!
//! # Packed Encoding
//! When serializing structs or enums in CBOR the keys or enum variant names will be serialized
//! as string keys to a map. Especially in embedded environments this can increase the file
//! size too much. In packed encoding all struct keys, as well as any enum variant that has no data,
//! will be serialized as variable sized integers. The first 24 entries in any struct consume only a
//! single byte!  Packed encoding uses serde's preferred [externally tagged enum
//! format](https://serde.rs/enum-representations.html) and therefore serializes enum variant names
//! as string keys when that variant contains data.  So, in the packed encoding example, `FirstVariant`
//! encodes to a single byte, but encoding `SecondVariant` requires 16 bytes.
//!
//! To serialize a document in this format use `Serializer::new(writer).packed_format()` or
//! the shorthand `ser::to_vec_packed`. The deserialization works without any changes.
//!
//! If you would like to omit the enum variant encoding for all variants, including ones that
//! contain data, you can add `legacy_enums()` in addition to `packed_format()`, as can seen
//! in the Serialize using minimal encoding example.
//!
//! # Self describing documents
//! In some contexts different formats are used but there is no way to declare the format used
//! out of band. For this reason CBOR has a magic number that may be added before any document.
//! Self describing documents are created with `serializer.self_describe()`.
//!
//! # Examples
//! Read a CBOR value that is known to be a map of string keys to string values and print it.
//!
//! ```rust
//! use std::collections::BTreeMap;
//! use serde_cbor::from_slice;
//!
//! let slice = b"\xa5aaaAabaBacaCadaDaeaE";
//! let value: BTreeMap<String, String> = from_slice(slice).unwrap();
//! println!("{:?}", value); // {"e": "E", "d": "D", "a": "A", "c": "C", "b": "B"}
//! ```
//!
//! Read a general CBOR value with an unknown content.
//!
//! ```rust
//! use serde_cbor::from_slice;
//! use serde_cbor::value::Value;
//!
//! let slice = b"\x82\x01\xa1aaab";
//! let value: Value = from_slice(slice).unwrap();
//! println!("{:?}", value); // Array([U64(1), Object({String("a"): String("b")})])
//! ```
//!
//! Serialize an object.
//!
//! ```rust
//! use std::collections::BTreeMap;
//! use serde_cbor::to_vec;
//!
//! let mut programming_languages = BTreeMap::new();
//! programming_languages.insert("rust", vec!["safe", "concurrent", "fast"]);
//! programming_languages.insert("python", vec!["powerful", "friendly", "open"]);
//! programming_languages.insert("js", vec!["lightweight", "interpreted", "object-oriented"]);
//! let encoded = to_vec(&programming_languages);
//! assert_eq!(encoded.unwrap().len(), 103);
//! ```
//!
//! Deserializing data in the middle of a slice
//! ```
//! # extern crate serde_cbor;
//! use serde_cbor::Deserializer;
//!
//! # fn main() {
//! let data: Vec<u8> = vec![
//!     0x66, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72, 0x66, 0x66, 0x6f, 0x6f, 0x62,
//!     0x61, 0x72,
//! ];
//! let mut deserializer = Deserializer::from_slice(&data);
//! let value: &str = serde::de::Deserialize::deserialize(&mut deserializer)
//!     .unwrap();
//! let rest = &data[deserializer.byte_offset()..];
//! assert_eq!(value, "foobar");
//! assert_eq!(rest, &[0x66, 0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72]);
//! # }
//! ```
//!
//! Serialize using packed encoding
//!
//! ```rust
//! use serde_derive::{Deserialize, Serialize};
//! use serde_cbor::ser::to_vec_packed;
//! use WithTwoVariants::*;
//!
//! #[derive(Debug, Serialize, Deserialize)]
//! enum WithTwoVariants {
//!     FirstVariant,
//!     SecondVariant(u8),
//! }
//!
//! let cbor = to_vec_packed(&FirstVariant).unwrap();
//! assert_eq!(cbor.len(), 1);
//!
//! let cbor = to_vec_packed(&SecondVariant(0)).unwrap();
//! assert_eq!(cbor.len(), 16); // Includes 13 bytes of "SecondVariant"
//! ```
//!
//! Serialize using minimal encoding
//!
//! ```rust
//! use serde_derive::{Deserialize, Serialize};
//! use serde_cbor::{Result, Serializer, ser::{self, IoWrite}};
//! use WithTwoVariants::*;
//!
//! fn to_vec_minimal<T>(value: &T) -> Result<Vec<u8>>
//! where
//!     T: serde::Serialize,
//! {
//!     let mut vec = Vec::new();
//!     value.serialize(&mut Serializer::new(&mut IoWrite::new(&mut vec)).packed_format().legacy_enums())?;
//!     Ok(vec)
//! }
//!
//! #[derive(Debug, Serialize, Deserialize)]
//! enum WithTwoVariants {
//!     FirstVariant,
//!     SecondVariant(u8),
//! }
//!
//! let cbor = to_vec_minimal(&FirstVariant).unwrap();
//! assert_eq!(cbor.len(), 1);
//!
//! let cbor = to_vec_minimal(&SecondVariant(0)).unwrap();
//! assert_eq!(cbor.len(), 3);
//! ```
//!
//! # `no-std` support
//!
//! Serde CBOR supports building in a `no_std` context, use the following lines
//! in your `Cargo.toml` dependencies:
//! ``` toml
//! [dependencies]
//! serde = { version = "1.0", default-features = false }
//! serde_cbor = { version = "0.10", default-features = false }
//! ```
//!
//! Without the `std` feature the functions [from_reader], [from_slice], [to_vec], and [to_writer]
//! are not exported. To export [from_slice] and [to_vec] enable the `alloc` feature. The `alloc`
//! feature uses the [`alloc` library][alloc-lib] and requires at least version 1.36.0 of Rust.
//!
//! [alloc-lib]: https://doc.rust-lang.org/alloc/
//!
//! *Note*: to use derive macros in serde you will need to declare `serde`
//! dependency like so:
//! ``` toml
//! serde = { version = "1.0", default-features = false, features = ["derive"] }
//! ```
//!
//! Serialize an object with `no_std` and without `alloc`.
//! ``` rust
//! # #[macro_use] extern crate serde_derive;
//! # fn main() -> Result<(), serde_cbor::Error> {
//! use serde::Serialize;
//! use serde_cbor::Serializer;
//! use serde_cbor::ser::SliceWrite;
//!
//! #[derive(Serialize)]
//! struct User {
//!     user_id: u32,
//!     password_hash: [u8; 4],
//! }
//!
//! let mut buf = [0u8; 100];
//! let writer = SliceWrite::new(&mut buf[..]);
//! let mut ser = Serializer::new(writer);
//! let user = User {
//!     user_id: 42,
//!     password_hash: [1, 2, 3, 4],
//! };
//! user.serialize(&mut ser)?;
//! let writer = ser.into_inner();
//! let size = writer.bytes_written();
//! let expected = [
//!     0xa2, 0x67, 0x75, 0x73, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x2a, 0x6d,
//!     0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x5f, 0x68, 0x61, 0x73,
//!     0x68, 0x84, 0x1, 0x2, 0x3, 0x4
//! ];
//! assert_eq!(&buf[..size], expected);
//! # Ok(())
//! # }
//! ```
//!
//! Deserialize an object.
//! ``` rust
//! # #[macro_use] extern crate serde_derive;
//! # fn main() -> Result<(), serde_cbor::Error> {
//! #[derive(Debug, PartialEq, Deserialize)]
//! struct User {
//!     user_id: u32,
//!     password_hash: [u8; 4],
//! }
//!
//! let value = [
//!     0xa2, 0x67, 0x75, 0x73, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x2a, 0x6d,
//!     0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x5f, 0x68, 0x61, 0x73,
//!     0x68, 0x84, 0x1, 0x2, 0x3, 0x4
//! ];
//!
//! // from_slice_with_scratch will not alter input data, use it whenever you
//! // borrow from somewhere else.
//! // You will have to size your scratch according to the input data you
//! // expect.
//! use serde_cbor::de::from_slice_with_scratch;
//! let mut scratch = [0u8; 32];
//! let user: User = from_slice_with_scratch(&value[..], &mut scratch)?;
//! assert_eq!(user, User {
//!     user_id: 42,
//!     password_hash: [1, 2, 3, 4],
//! });
//!
//! let mut value = [
//!     0xa2, 0x67, 0x75, 0x73, 0x65, 0x72, 0x5f, 0x69, 0x64, 0x18, 0x2a, 0x6d,
//!     0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x5f, 0x68, 0x61, 0x73,
//!     0x68, 0x84, 0x1, 0x2, 0x3, 0x4
//! ];
//!
//! // from_mut_slice will move data around the input slice, you may only use it
//! // on data you may own or can modify.
//! use serde_cbor::de::from_mut_slice;
//! let user: User = from_mut_slice(&mut value[..])?;
//! assert_eq!(user, User {
//!     user_id: 42,
//!     password_hash: [1, 2, 3, 4],
//! });
//! # Ok(())
//! # }
//! ```
//!
//! # Limitations
//!
//! While Serde CBOR strives to support all features of Serde and CBOR
//! there are a few limitations.
//!
//! * [Tags] are ignored during deserialization and can't be emitted during
//!     serialization. This is because Serde has no concept of tagged
//!     values. See:&nbsp;[#3]
//! * Unknown [simple values] cause an `UnassignedCode` error.
//!     The simple values *False* and *True* are recognized and parsed as bool.
//!     *Null* and *Undefined* are both deserialized as *unit*.
//!     The *unit* type is serialized as *Null*. See:&nbsp;[#86]
//! * [128-bit integers] can't be directly encoded in CBOR. If you need them
//!     store them as a byte string. See:&nbsp;[#77]
//!
//! [Tags]: https://tools.ietf.org/html/rfc7049#section-2.4.4
//! [#3]: https://github.com/pyfisch/cbor/issues/3
//! [simple values]: https://tools.ietf.org/html/rfc7049#section-3.5
//! [#86]: https://github.com/pyfisch/cbor/issues/86
//! [128-bit integers]: https://doc.rust-lang.org/std/primitive.u128.html
//! [#77]: https://github.com/pyfisch/cbor/issues/77

#![deny(missing_docs)]
#![cfg_attr(not(feature = "std"), no_std)]

// When we are running tests in no_std mode we need to explicitly link std, because `cargo test`
// will not work without it.
#[cfg(all(not(feature = "std"), test))]
extern crate std;

#[cfg(feature = "alloc")]
extern crate alloc;

pub mod de;
pub mod error;
mod read;
pub mod ser;
pub mod tags;
mod write;

#[cfg(feature = "std")]
pub mod value;

// Re-export the [items recommended by serde](https://serde.rs/conventions.html).
#[doc(inline)]
pub use crate::de::{Deserializer, StreamDeserializer};

#[doc(inline)]
pub use crate::error::{Error, Result};

#[doc(inline)]
pub use crate::ser::Serializer;

// Convenience functions for serialization and deserialization.
// These functions are only available in `std` mode.
#[cfg(feature = "std")]
#[doc(inline)]
pub use crate::de::from_reader;

#[cfg(any(feature = "std", feature = "alloc"))]
#[doc(inline)]
pub use crate::de::from_slice;

#[cfg(any(feature = "std", feature = "alloc"))]
#[doc(inline)]
pub use crate::ser::to_vec;

#[cfg(feature = "std")]
#[doc(inline)]
pub use crate::ser::to_writer;

// Re-export the value type like serde_json
#[cfg(feature = "std")]
#[doc(inline)]
pub use crate::value::Value;
