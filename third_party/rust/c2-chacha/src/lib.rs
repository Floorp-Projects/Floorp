// copyright 2019 Kaz Wesley

//! Pure Rust ChaCha with SIMD optimizations.
//!
//! Stream-cipher usage:
//! ```
//! extern crate c2_chacha;
//!
//! use c2_chacha::stream_cipher::{NewStreamCipher, SyncStreamCipher, SyncStreamCipherSeek};
//! use c2_chacha::{ChaCha20, ChaCha12};
//!
//! let key = b"very secret key-the most secret.";
//! let iv = b"my nonce";
//! let plaintext = b"The quick brown fox jumps over the lazy dog.";
//!
//! let mut buffer = plaintext.to_vec();
//! // create cipher instance
//! let mut cipher = ChaCha20::new_var(key, iv).unwrap();
//! // apply keystream (encrypt)
//! cipher.apply_keystream(&mut buffer);
//! // and decrypt it back
//! cipher.seek(0);
//! cipher.apply_keystream(&mut buffer);
//! // stream ciphers can be used with streaming messages
//! let mut cipher = ChaCha12::new_var(key, iv).unwrap();
//! for chunk in buffer.chunks_mut(3) {
//!     cipher.apply_keystream(chunk);
//! }
//! ```

#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(test)]
#[macro_use]
extern crate hex_literal;

#[macro_use]
extern crate ppv_lite86;

pub mod guts;

#[cfg(feature = "rustcrypto_api")]
mod rustcrypto_impl;
#[cfg(feature = "rustcrypto_api")]
pub use self::rustcrypto_impl::{stream_cipher, ChaCha12, ChaCha20, ChaCha8, Ietf, XChaCha20};
