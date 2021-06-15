//! An implementation of the [SHA-2][1] cryptographic hash algorithms.
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
//! ```rust
//! # #[macro_use] extern crate hex_literal;
//! # extern crate sha2;
//! # fn main() {
//! use sha2::{Sha256, Sha512, Digest};
//!
//! // create a Sha256 object
//! let mut hasher = Sha256::new();
//!
//! // write input message
//! hasher.input(b"hello world");
//!
//! // read hash digest and consume hasher
//! let result = hasher.result();
//!
//! assert_eq!(result[..], hex!("
//!     b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9
//! ")[..]);
//!
//! // same for Sha512
//! let mut hasher = Sha512::new();
//! hasher.input(b"hello world");
//! let result = hasher.result();
//!
//! assert_eq!(result[..], hex!("
//!     309ecc489c12d6eb4cc40f50c902f2b4d0ed77ee511a7c7a9bcd3ca86d4cd86f
//!     989dd35bc5ff499670da34255b45b0cfd830e81f605dcf7dc5542e93ae9cd76f
//! ")[..]);
//! # }
//! ```
//!
//! Also see [RustCrypto/hashes][2] readme.
//!
//! [1]: https://en.wikipedia.org/wiki/SHA-2
//! [2]: https://github.com/RustCrypto/hashes

#![no_std]
#![doc(html_logo_url = "https://raw.githubusercontent.com/RustCrypto/meta/master/logo_small.png")]

// Give relevant error messages if the user tries to enable AArch64 asm on unsupported platforms.
#[cfg(all(
    feature = "asm-aarch64",
    target_arch = "aarch64",
    not(target_os = "linux")
))]
compile_error!("Your OS isn’t yet supported for runtime-checking of AArch64 features.");
#[cfg(all(feature = "asm-aarch64", not(target_arch = "aarch64")))]
compile_error!("Enable the \"asm\" feature instead of \"asm-aarch64\" on non-AArch64 systems.");
#[cfg(all(
    feature = "asm-aarch64",
    target_arch = "aarch64",
    target_feature = "crypto"
))]
compile_error!("Enable the \"asm\" feature instead of \"asm-aarch64\" when building for AArch64 systems with crypto extensions.");
#[cfg(all(
    not(feature = "asm-aarch64"),
    feature = "asm",
    target_arch = "aarch64",
    not(target_feature = "crypto"),
    target_os = "linux"
))]
compile_error!("Enable the \"asm-aarch64\" feature on AArch64 if you want to use asm detected at runtime, or build with the crypto extensions support, for instance with RUSTFLAGS='-C target-cpu=native' on a compatible CPU.");

extern crate block_buffer;
extern crate fake_simd as simd;
#[macro_use]
extern crate opaque_debug;
#[macro_use]
pub extern crate digest;
#[cfg(feature = "asm-aarch64")]
extern crate libc;
#[cfg(feature = "asm")]
extern crate sha2_asm;
#[cfg(feature = "std")]
extern crate std;

#[cfg(feature = "asm-aarch64")]
mod aarch64;
mod consts;
mod sha256;
#[cfg(any(not(feature = "asm"), feature = "asm-aarch64", feature = "compress"))]
mod sha256_utils;
mod sha512;
#[cfg(any(not(feature = "asm"), target_arch = "aarch64", feature = "compress"))]
mod sha512_utils;

pub use digest::Digest;
pub use sha256::{Sha224, Sha256};
#[cfg(feature = "compress")]
pub use sha256_utils::compress256;
pub use sha512::{Sha384, Sha512, Sha512Trunc224, Sha512Trunc256};
#[cfg(feature = "compress")]
pub use sha512_utils::compress512;
