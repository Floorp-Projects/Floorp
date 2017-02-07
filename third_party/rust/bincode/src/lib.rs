//! `bincode` is a crate for encoding and decoding using a tiny binary
//! serialization strategy.
//!
//! There are simple functions for encoding to `Vec<u8>` and decoding from
//! `&[u8]`, but the meat of the library is the `encode_into` and `decode_from`
//! functions which respectively allow encoding into a `std::io::Writer`
//! and decoding from a `std::io::Buffer`.
//!
//! ## Modules
//! There are two ways to encode and decode structs using `bincode`, either using `rustc_serialize`
//! or the `serde` crate.  `rustc_serialize` and `serde` are crates and and also the names of their
//! corresponding modules inside of `bincode`.  Both modules have exactly equivalant functions, and
//! and the only difference is whether or not the library user wants to use `rustc_serialize` or
//! `serde`.
//!
//! ### Using Basic Functions
//!
//! ```rust
//! #![allow(unstable)]
//! extern crate bincode;
//! use bincode::rustc_serialize::{encode, decode};
//! fn main() {
//!     // The object that we will serialize.
//!     let target = Some("hello world".to_string());
//!     // The maximum size of the encoded message.
//!     let limit = bincode::SizeLimit::Bounded(20);
//!
//!     let encoded: Vec<u8>        = encode(&target, limit).unwrap();
//!     let decoded: Option<String> = decode(&encoded[..]).unwrap();
//!     assert_eq!(target, decoded);
//! }
//! ```

#![crate_name = "bincode"]
#![crate_type = "rlib"]
#![crate_type = "dylib"]

#![doc(html_logo_url = "./icon.png")]

#[cfg(feature = "rustc-serialize")]
extern crate rustc_serialize as rustc_serialize_crate;
extern crate byteorder;
extern crate num_traits;
#[cfg(feature = "serde")]
extern crate serde as serde_crate;


pub use refbox::{RefBox, StrBox, SliceBox};

mod refbox;
#[cfg(feature = "rustc-serialize")]
pub mod rustc_serialize;
#[cfg(feature = "serde")]
pub mod serde;

/// A limit on the amount of bytes that can be read or written.
///
/// Size limits are an incredibly important part of both encoding and decoding.
///
/// In order to prevent DOS attacks on a decoder, it is important to limit the
/// amount of bytes that a single encoded message can be; otherwise, if you
/// are decoding bytes right off of a TCP stream for example, it would be
/// possible for an attacker to flood your server with a 3TB vec, causing the
/// decoder to run out of memory and crash your application!
/// Because of this, you can provide a maximum-number-of-bytes that can be read
/// during decoding, and the decoder will explicitly fail if it has to read
/// any more than that.
///
/// On the other side, you want to make sure that you aren't encoding a message
/// that is larger than your decoder expects.  By supplying a size limit to an
/// encoding function, the encoder will verify that the structure can be encoded
/// within that limit.  This verification occurs before any bytes are written to
/// the Writer, so recovering from an error is easy.
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq, Ord, PartialOrd)]
pub enum SizeLimit {
    Infinite,
    Bounded(u64)
}

