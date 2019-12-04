// Copyright 2018 Developers of the Rand project.
// Copyright 2013 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A wrapper around any Read to treat it as an RNG.

use std::io::Read;

use rand_core::{RngCore, Error, ErrorKind, impls};


/// An RNG that reads random bytes straight from any type supporting
/// [`std::io::Read`], for example files.
///
/// This will work best with an infinite reader, but that is not required.
///
/// This can be used with `/dev/urandom` on Unix but it is recommended to use
/// [`OsRng`] instead.
///
/// # Panics
///
/// `ReadRng` uses [`std::io::Read::read_exact`], which retries on interrupts.
/// All other errors from the underlying reader, including when it does not
/// have enough data, will only be reported through [`try_fill_bytes`].
/// The other [`RngCore`] methods will panic in case of an error.
///
/// # Example
///
/// ```
/// use rand::Rng;
/// use rand::rngs::adapter::ReadRng;
///
/// let data = vec![1, 2, 3, 4, 5, 6, 7, 8];
/// let mut rng = ReadRng::new(&data[..]);
/// println!("{:x}", rng.gen::<u32>());
/// ```
///
/// [`OsRng`]: rand_os::OsRng
/// [`try_fill_bytes`]: RngCore::try_fill_bytes
#[derive(Debug)]
pub struct ReadRng<R> {
    reader: R
}

impl<R: Read> ReadRng<R> {
    /// Create a new `ReadRng` from a `Read`.
    pub fn new(r: R) -> ReadRng<R> {
        ReadRng {
            reader: r
        }
    }
}

impl<R: Read> RngCore for ReadRng<R> {
    fn next_u32(&mut self) -> u32 {
        impls::next_u32_via_fill(self)
    }

    fn next_u64(&mut self) -> u64 {
        impls::next_u64_via_fill(self)
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.try_fill_bytes(dest).unwrap_or_else(|err|
                panic!("reading random bytes from Read implementation failed; error: {}", err));
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        if dest.len() == 0 { return Ok(()); }
        // Use `std::io::read_exact`, which retries on `ErrorKind::Interrupted`.
        self.reader.read_exact(dest).map_err(|err| {
            match err.kind() {
                ::std::io::ErrorKind::UnexpectedEof => Error::with_cause(
                    ErrorKind::Unavailable,
                    "not enough bytes available, reached end of source", err),
                _ => Error::with_cause(ErrorKind::Unavailable,
                    "error reading from Read source", err)
            }
        })
    }
}

#[cfg(test)]
mod test {
    use super::ReadRng;
    use {RngCore, ErrorKind};

    #[test]
    fn test_reader_rng_u64() {
        // transmute from the target to avoid endianness concerns.
        let v = vec![0u8, 0, 0, 0, 0, 0, 0, 1,
                     0  , 0, 0, 0, 0, 0, 0, 2,
                     0,   0, 0, 0, 0, 0, 0, 3];
        let mut rng = ReadRng::new(&v[..]);

        assert_eq!(rng.next_u64(), 1_u64.to_be());
        assert_eq!(rng.next_u64(), 2_u64.to_be());
        assert_eq!(rng.next_u64(), 3_u64.to_be());
    }

    #[test]
    fn test_reader_rng_u32() {
        let v = vec![0u8, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3];
        let mut rng = ReadRng::new(&v[..]);

        assert_eq!(rng.next_u32(), 1_u32.to_be());
        assert_eq!(rng.next_u32(), 2_u32.to_be());
        assert_eq!(rng.next_u32(), 3_u32.to_be());
    }

    #[test]
    fn test_reader_rng_fill_bytes() {
        let v = [1u8, 2, 3, 4, 5, 6, 7, 8];
        let mut w = [0u8; 8];

        let mut rng = ReadRng::new(&v[..]);
        rng.fill_bytes(&mut w);

        assert!(v == w);
    }

    #[test]
    fn test_reader_rng_insufficient_bytes() {
        let v = [1u8, 2, 3, 4, 5, 6, 7, 8];
        let mut w = [0u8; 9];

        let mut rng = ReadRng::new(&v[..]);

        assert!(rng.try_fill_bytes(&mut w).err().unwrap().kind == ErrorKind::Unavailable);
    }
}
