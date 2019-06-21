//! `x-www-form-urlencoded` meets Serde

#![warn(unused_extern_crates)]

extern crate dtoa;
extern crate itoa;
#[macro_use]
extern crate serde;
extern crate url;

pub mod de;
pub mod ser;

#[doc(inline)]
pub use de::{from_bytes, from_reader, from_str, Deserializer};
#[doc(inline)]
pub use ser::{to_string, Serializer};
