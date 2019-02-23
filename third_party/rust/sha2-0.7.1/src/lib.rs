//! An implementation of the SHA-2 cryptographic hash algorithms.
//!
//! There are 6 standard algorithms specified in the SHA-2 standard:
//!
//! * `Sha224`, which is the 32-bit `Sha256` algorithm with the result truncated
//! to 224 bits.
//! * `Sha256`, which is the 32-bit `Sha256` algorithm.
//! * `Sha384`, which is the 64-bit `Sha512` algorithm with the result truncated
//! to 384 bits.
//! * `Sha512`, which is the 64-bit `Sha512` algorithm.
//! * `Sha512Trunc224`, which is the 64-bit `Sha512` algorithm with the result
//! truncated to 224 bits.
//! * `Sha512Trunc256`, which is the 64-bit `Sha512` algorithm with the result
//! truncated to 256 bits.
//!
//! Algorithmically, there are only 2 core algorithms: `Sha256` and `Sha512`.
//! All other algorithms are just applications of these with different initial
//! hash values, and truncated to different digest bit lengths.
//!
//! # Usage
//!
//! An example of using `Sha256` is:
//!
//! ```rust
//! use sha2::{Sha256, Digest};
//!
//! // create a Sha256 object
//! let mut hasher = Sha256::default();
//!
//! // write input message
//! hasher.input(b"hello world");
//!
//! // read hash digest and consume hasher
//! let output = hasher.result();
//!
//! assert_eq!(output[..], [0xb9, 0x4d, 0x27, 0xb9, 0x93, 0x4d, 0x3e, 0x08,
//!                         0xa5, 0x2e, 0x52, 0xd7, 0xda, 0x7d, 0xab, 0xfa,
//!                         0xc4, 0x84, 0xef, 0xe3, 0x7a, 0x53, 0x80, 0xee,
//!                         0x90, 0x88, 0xf7, 0xac, 0xe2, 0xef, 0xcd, 0xe9]);
//! ```
//!
//! An example of using `Sha512` is:
//!
//! ```rust
//! use sha2::{Sha512, Digest};
//!
//! // create a Sha512 object
//! let mut hasher = Sha512::default();
//!
//! // write input message
//! hasher.input(b"hello world");
//!
//! // read hash digest and consume hasher
//! let output = hasher.result();
//!
//! assert_eq!(output[..], [0x30, 0x9e, 0xcc, 0x48, 0x9c, 0x12, 0xd6, 0xeb,
//!                         0x4c, 0xc4, 0x0f, 0x50, 0xc9, 0x02, 0xf2, 0xb4,
//!                         0xd0, 0xed, 0x77, 0xee, 0x51, 0x1a, 0x7c, 0x7a,
//!                         0x9b, 0xcd, 0x3c, 0xa8, 0x6d, 0x4c, 0xd8, 0x6f,
//!                         0x98, 0x9d, 0xd3, 0x5b, 0xc5, 0xff, 0x49, 0x96,
//!                         0x70, 0xda, 0x34, 0x25, 0x5b, 0x45, 0xb0, 0xcf,
//!                         0xd8, 0x30, 0xe8, 0x1f, 0x60, 0x5d, 0xcf, 0x7d,
//!                         0xc5, 0x54, 0x2e, 0x93, 0xae, 0x9c, 0xd7, 0x6f][..]);
//! ```

#![no_std]
extern crate byte_tools;
#[macro_use]
extern crate digest;
extern crate block_buffer;
extern crate fake_simd as simd;
#[cfg(feature = "asm")]
extern crate sha2_asm;

mod consts;
#[cfg(not(feature = "asm"))]
mod sha256_utils;
#[cfg(not(feature = "asm"))]
mod sha512_utils;
mod sha256;
mod sha512;

pub use digest::Digest;
pub use sha256::{Sha256, Sha224};
pub use sha512::{Sha512, Sha384, Sha512Trunc224, Sha512Trunc256};
