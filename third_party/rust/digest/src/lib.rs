//! This crate provides traits which describe functionality of cryptographic hash
//! functions.
//!
//! Traits in this repository can be separated into two levels:
//! - Low level traits: `Input`, `BlockInput`, `Reset`, `FixedOutput`,
//! `VariableOutput`, `ExtendableOutput`. These traits atomically describe
//! available functionality of hash function implementations.
//! - Convenience trait: `Digest`, `DynDigest`. They are wrappers around
//! low level traits for most common hash-function use-cases.
//!
//! Additionally hash functions implement traits from `std`: `Default`, `Clone`,
//! `Write`. (the latter depends on enabled-by-default `std` crate feature)
//!
//! The `Digest` trait is the most commonly used trait.
#![no_std]
#![doc(html_logo_url =
    "https://raw.githubusercontent.com/RustCrypto/meta/master/logo_small.png")]
pub extern crate generic_array;
#[cfg(feature = "std")]
#[macro_use] extern crate std;
#[cfg(feature = "dev")]
pub extern crate blobby;
use generic_array::{GenericArray, ArrayLength};
#[cfg(feature = "std")]
use std::vec::Vec;

mod digest;
mod dyn_digest;
mod errors;
#[cfg(feature = "dev")]
pub mod dev;

pub use errors::InvalidOutputSize;
pub use digest::Digest;
#[cfg(feature = "std")]
pub use dyn_digest::DynDigest;

/// Trait for processing input data
pub trait Input {
    /// Digest input data.
    ///
    /// This method can be called repeatedly, e.g. for processing streaming
    /// messages.
    fn input<B: AsRef<[u8]>>(&mut self, data: B);

    /// Digest input data in a chained manner.
    fn chain<B: AsRef<[u8]>>(mut self, data: B) -> Self where Self: Sized {
        self.input(data);
        self
    }
}

/// Trait to indicate that digest function processes data in blocks of size
/// `BlockSize`.
///
/// The main usage of this trait is for implementing HMAC generically.
pub trait BlockInput {
    type BlockSize: ArrayLength<u8>;
}

/// Trait for returning digest result with the fixed size
pub trait FixedOutput {
    type OutputSize: ArrayLength<u8>;

    /// Retrieve result and consume hasher instance.
    fn fixed_result(self) -> GenericArray<u8, Self::OutputSize>;
}

/// Trait for returning digest result with the variable size
pub trait VariableOutput: core::marker::Sized {
    /// Create new hasher instance with the given output size.
    ///
    /// It will return `Err(InvalidOutputSize)` in case if hasher can not return
    /// specified output size. It will always return an error if output size
    /// equals to zero.
    fn new(output_size: usize) -> Result<Self, InvalidOutputSize>;

    /// Get output size of the hasher instance provided to the `new` method
    fn output_size(&self) -> usize;

    /// Retrieve result via closure and consume hasher.
    ///
    /// Closure is guaranteed to be called, length of the buffer passed to it
    /// will be equal to `output_size`.
    fn variable_result<F: FnOnce(&[u8])>(self, f: F);

    /// Retrieve result into vector and consume hasher.
    #[cfg(feature = "std")]
    fn vec_result(self) -> Vec<u8> {
        let mut buf = Vec::with_capacity(self.output_size());
        self.variable_result(|res| buf.extend_from_slice(res));
        buf
    }
}

/// Trait for describing readers which are used to extract extendable output
/// from XOF (extendable-output function) result.
pub trait XofReader {
    /// Read output into the `buffer`. Can be called unlimited number of times.
    fn read(&mut self, buffer: &mut [u8]);
}

/// Trait which describes extendable-output functions (XOF).
pub trait ExtendableOutput: core::marker::Sized {
    type Reader: XofReader;

    /// Retrieve XOF reader and consume hasher instance.
    fn xof_result(self) -> Self::Reader;

    /// Retrieve result into vector of specified length.
    #[cfg(feature = "std")]
    fn vec_result(self, n: usize) -> Vec<u8> {
        let mut buf = vec![0u8; n];
        self.xof_result().read(&mut buf);
        buf
    }
}

/// Trait for resetting hash instances
pub trait Reset {
    /// Reset hasher instance to its initial state and return current state.
    fn reset(&mut self);
}

#[macro_export]
/// Implements `std::io::Write` trait for implementer of `Input`
macro_rules! impl_write {
    ($hasher:ident) => {
        #[cfg(feature = "std")]
        impl ::std::io::Write for $hasher {
            fn write(&mut self, buf: &[u8]) -> ::std::io::Result<usize> {
                Input::input(self, buf);
                Ok(buf.len())
            }

            fn flush(&mut self) -> ::std::io::Result<()> {
                Ok(())
            }
        }
    }
}
