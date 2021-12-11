//! An implementation of the [SHA-1][1] cryptographic hash algorithm.
//!
//! # Usage
//!
//! ```rust
//! # #[macro_use] extern crate hex_literal;
//! # extern crate sha1;
//! # fn main() {
//! use sha1::{Sha1, Digest};
//!
//! // create a Sha1 object
//! let mut hasher = Sha1::new();
//!
//! // process input message
//! hasher.input(b"hello world");
//!
//! // acquire hash digest in the form of GenericArray,
//! // which in this case is equivalent to [u8; 20]
//! let result = hasher.result();
//! assert_eq!(result[..], hex!("2aae6c35c94fcfb415dbe95f408b9ce91ee846ed"));
//! # }
//! ```
//!
//! Also see [RustCrypto/hashes][2] readme.
//!
//! [1]: https://en.wikipedia.org/wiki/SHA-1
//! [2]: https://github.com/RustCrypto/hashes
#![no_std]
#![doc(html_logo_url =
    "https://raw.githubusercontent.com/RustCrypto/meta/master/logo_small.png")]

// Give relevant error messages if the user tries to enable AArch64 asm on unsupported platforms.
#[cfg(all(feature = "asm-aarch64", target_arch = "aarch64", not(target_os = "linux")))]
compile_error!("Your OS isn’t yet supported for runtime-checking of AArch64 features.");
#[cfg(all(feature = "asm-aarch64", target_os = "linux", not(target_arch = "aarch64")))]
compile_error!("Enable the \"asm\" feature instead of \"asm-aarch64\" on non-AArch64 Linux systems.");
#[cfg(all(not(feature = "asm-aarch64"), feature = "asm", target_arch = "aarch64", target_os = "linux"))]
compile_error!("Enable the \"asm-aarch64\" feature on AArch64 if you want to use asm.");

extern crate block_buffer;
#[macro_use] extern crate opaque_debug;
#[macro_use] pub extern crate digest;
#[cfg(feature = "std")]
extern crate std;
#[cfg(any(not(feature = "asm"), feature = "asm-aarch64"))]
extern crate fake_simd as simd;
#[cfg(feature = "asm-aarch64")]
extern crate libc;

#[cfg(feature = "asm")]
extern crate sha1_asm;
#[cfg(all(feature = "asm", not(feature = "asm-aarch64")))]
#[inline(always)]
fn compress(state: &mut [u32; 5], block: &GenericArray<u8, U64>) {
    let block: &[u8; 64] = unsafe { core::mem::transmute(block) };
    sha1_asm::compress(state, block);
}
#[cfg(feature = "asm-aarch64")]
mod aarch64;
#[cfg(feature = "asm-aarch64")]
#[inline(always)]
fn compress(state: &mut [u32; 5], block: &GenericArray<u8, U64>) {
    // TODO: Replace this platform-specific call with is_aarch64_feature_detected!("sha1") once
    // that macro is stabilised and https://github.com/rust-lang/rfcs/pull/2725 is implemented
    // to let us use it on no_std.
    if aarch64::sha1_supported() {
        let block: &[u8; 64] = unsafe { core::mem::transmute(block) };
        sha1_asm::compress(state, block);
    } else {
        utils::compress(state, block);
    }
}

#[cfg(any(not(feature = "asm"), feature = "asm-aarch64"))]
mod utils;
#[cfg(not(feature = "asm"))]
use utils::compress;

pub use digest::Digest;
use digest::{Input, BlockInput, FixedOutput, Reset};
use digest::generic_array::GenericArray;
use digest::generic_array::typenum::{U20, U64};
use block_buffer::BlockBuffer;
use block_buffer::byteorder::{BE, ByteOrder};

mod consts;
use consts::{STATE_LEN, H};

/// Structure representing the state of a SHA-1 computation
#[derive(Clone)]
pub struct Sha1 {
    h: [u32; STATE_LEN],
    len: u64,
    buffer: BlockBuffer<U64>,
}

impl Default for Sha1 {
    fn default() -> Self {
        Sha1{ h: H, len: 0u64, buffer: Default::default() }
    }
}

impl BlockInput for Sha1 {
    type BlockSize = U64;
}

impl Input for Sha1 {
    fn input<B: AsRef<[u8]>>(&mut self, input: B) {
        let input = input.as_ref();
        // Assumes that `length_bits<<3` will not overflow
        self.len += input.len() as u64;
        let state = &mut self.h;
        self.buffer.input(input, |d| compress(state, d));
    }
}

impl FixedOutput for Sha1 {
    type OutputSize = U20;

    fn fixed_result(mut self) -> GenericArray<u8, Self::OutputSize> {
        {
            let state = &mut self.h;
            let l = self.len << 3;
            self.buffer.len64_padding::<BE, _>(l, |d| compress(state, d));
        }
        let mut out = GenericArray::default();
        BE::write_u32_into(&self.h,&mut out);
        out
    }
}

impl Reset for Sha1 {
    fn reset(&mut self) {
        self.h = H;
        self.len = 0;
        self.buffer.reset();
    }
}

impl_opaque_debug!(Sha1);
impl_write!(Sha1);
