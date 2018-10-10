use super::{Input, BlockInput, FixedOutput};
use generic_array::GenericArray;
#[cfg(feature = "std")]
use std::io;

type Output<N> = GenericArray<u8, N>;

/// The `Digest` trait specifies an interface common for digest functions.
///
/// It's a convinience wrapper around `Input`, `FixedOutput`, `BlockInput` and
/// `Default` traits. It also provides additional convenience methods.
pub trait Digest: Input + BlockInput + FixedOutput + Default {
    /// Create new hasher instance
    fn new() -> Self {
        Self::default()
    }

    /// Digest input data. This method can be called repeatedly
    /// for use with streaming messages.
    fn input(&mut self, input: &[u8]) {
        self.process(input);
    }

    /// Retrieve the digest result. This method consumes digest instance.
    fn result(self) -> Output<Self::OutputSize> {
        self.fixed_result()
    }

    /// Convenience function to compute hash of the `data`. It will handle
    /// hasher creation, data feeding and finalization.
    ///
    /// Example:
    ///
    /// ```rust,ignore
    /// println!("{:x}", sha2::Sha256::digest(b"Hello world"));
    /// ```
    #[inline]
    fn digest(data: &[u8]) -> Output<Self::OutputSize> {
        let mut hasher = Self::default();
        hasher.process(data);
        hasher.fixed_result()
    }

    /// Convenience function to compute hash of the string. It's equivalent to
    /// `digest(input_string.as_bytes())`.
    #[inline]
    fn digest_str(str: &str) -> Output<Self::OutputSize> {
        Self::digest(str.as_bytes())
    }

    /// Convenience function which takes `std::io::Read` as a source and computes
    /// value of digest function `D`, e.g. SHA-2, SHA-3, BLAKE2, etc. using 1 KB
    /// blocks.
    ///
    /// Usage example:
    ///
    /// ```rust,ignore
    /// use std::fs;
    /// use sha2::{Sha256, Digest};
    ///
    /// let mut file = fs::File::open("Cargo.toml")?;
    /// let result = Sha256::digest_reader(&mut file)?;
    /// println!("{:x}", result);
    /// ```
    #[cfg(feature = "std")]
    #[inline]
    fn digest_reader(source: &mut io::Read)
        -> io::Result<Output<Self::OutputSize>>
    {
        let mut hasher = Self::default();

        let mut buf = [0u8; 8 * 1024];

        loop {
            let len = match source.read(&mut buf) {
                Ok(0) => return Ok(hasher.result()),
                Ok(len) => len,
                Err(ref e) if e.kind() == io::ErrorKind::Interrupted => continue,
                Err(e) => Err(e)?,
            };
            hasher.process(&buf[..len]);
        }
    }
}

impl<D: Input + FixedOutput + BlockInput + Default> Digest for D {}
