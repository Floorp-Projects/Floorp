//! This module contains backend-specific code.

use crate::mem::{CompressError, DecompressError, FlushCompress, FlushDecompress, Status};
use crate::Compression;

/// Traits specifying the interface of the backends.
///
/// Sync + Send are added as a condition to ensure they are available
/// for the frontend.
pub trait Backend: Sync + Send {
    fn total_in(&self) -> u64;
    fn total_out(&self) -> u64;
}

pub trait InflateBackend: Backend {
    fn make(zlib_header: bool, window_bits: u8) -> Self;
    fn decompress(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushDecompress,
    ) -> Result<Status, DecompressError>;
    fn reset(&mut self, zlib_header: bool);
}

pub trait DeflateBackend: Backend {
    fn make(level: Compression, zlib_header: bool, window_bits: u8) -> Self;
    fn compress(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushCompress,
    ) -> Result<Status, CompressError>;
    fn reset(&mut self);
}

// hardwire wasm to the Rust implementation so no C compilers need be dealt
// with there, otherwise if miniz-sys/zlib are enabled we use that and fall
// back to the default of Rust
cfg_if::cfg_if! {
    if #[cfg(target_arch = "wasm32")] {
        mod rust;
        pub use self::rust::*;
    } else if #[cfg(any(feature = "miniz-sys", feature = "zlib"))] {
        mod c;
        pub use self::c::*;
    } else {
        mod rust;
        pub use self::rust::*;
    }
}
