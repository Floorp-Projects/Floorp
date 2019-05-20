// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Entropy generator, or wrapper around external generators

use rand_core::{RngCore, CryptoRng, Error, ErrorKind, impls};
#[allow(unused)]
use rngs;

/// An interface returning random data from external source(s), provided
/// specifically for securely seeding algorithmic generators (PRNGs).
///
/// Where possible, `EntropyRng` retrieves random data from the operating
/// system's interface for random numbers ([`OsRng`]); if that fails it will
/// fall back to the [`JitterRng`] entropy collector. In the latter case it will
/// still try to use [`OsRng`] on the next usage.
///
/// If no secure source of entropy is available `EntropyRng` will panic on use;
/// i.e. it should never output predictable data.
///
/// This is either a little slow ([`OsRng`] requires a system call) or extremely
/// slow ([`JitterRng`] must use significant CPU time to generate sufficient
/// jitter); for better performance it is common to seed a local PRNG from
/// external entropy then primarily use the local PRNG ([`thread_rng`] is
/// provided as a convenient, local, automatically-seeded CSPRNG).
///
/// # Panics
///
/// On most systems, like Windows, Linux, macOS and *BSD on common hardware, it
/// is highly unlikely for both [`OsRng`] and [`JitterRng`] to fail. But on
/// combinations like webassembly without Emscripten or stdweb both sources are
/// unavailable. If both sources fail, only [`try_fill_bytes`] is able to
/// report the error, and only the one from `OsRng`. The other [`RngCore`]
/// methods will panic in case of an error.
///
/// [`OsRng`]: rand_os::OsRng
/// [`thread_rng`]: crate::thread_rng
/// [`JitterRng`]: crate::rngs::JitterRng
/// [`try_fill_bytes`]: RngCore::try_fill_bytes
#[derive(Debug)]
pub struct EntropyRng {
    source: Source,
}

#[derive(Debug)]
enum Source {
    Os(Os),
    Custom(Custom),
    Jitter(Jitter),
    None,
}

impl EntropyRng {
    /// Create a new `EntropyRng`.
    ///
    /// This method will do no system calls or other initialization routines,
    /// those are done on first use. This is done to make `new` infallible,
    /// and `try_fill_bytes` the only place to report errors.
    pub fn new() -> Self {
        EntropyRng { source: Source::None }
    }
}

impl Default for EntropyRng {
    fn default() -> Self {
        EntropyRng::new()
    }
}

impl RngCore for EntropyRng {
    fn next_u32(&mut self) -> u32 {
        impls::next_u32_via_fill(self)
    }

    fn next_u64(&mut self) -> u64 {
        impls::next_u64_via_fill(self)
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.try_fill_bytes(dest).unwrap_or_else(|err|
                panic!("all entropy sources failed; first error: {}", err))
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        let mut reported_error = None;

        if let Source::Os(ref mut os_rng) = self.source {
            match os_rng.fill(dest) {
                Ok(()) => return Ok(()),
                Err(err) => {
                    warn!("EntropyRng: OsRng failed \
                          [trying other entropy sources]: {}", err);
                    reported_error = Some(err);
                },
            }
        } else if Os::is_supported() {
            match Os::new_and_fill(dest) {
                Ok(os_rng) => {
                    debug!("EntropyRng: using OsRng");
                    self.source = Source::Os(os_rng);
                    return Ok(());
                },
                Err(err) => { reported_error = reported_error.or(Some(err)) },
            }
        }

        if let Source::Custom(ref mut rng) = self.source {
            match rng.fill(dest) {
                Ok(()) => return Ok(()),
                Err(err) => {
                    warn!("EntropyRng: custom entropy source failed \
                          [trying other entropy sources]: {}", err);
                    reported_error = Some(err);
                },
            }
        } else if Custom::is_supported() {
            match Custom::new_and_fill(dest) {
                Ok(custom) => {
                    debug!("EntropyRng: using custom entropy source");
                    self.source = Source::Custom(custom);
                    return Ok(());
                },
                Err(err) => { reported_error = reported_error.or(Some(err)) },
            }
        }

        if let Source::Jitter(ref mut jitter_rng) = self.source {
            match jitter_rng.fill(dest) {
                Ok(()) => return Ok(()),
                Err(err) => {
                    warn!("EntropyRng: JitterRng failed: {}", err);
                    reported_error = Some(err);
                },
            }
        } else if Jitter::is_supported() {
            match Jitter::new_and_fill(dest) {
                Ok(jitter_rng) => {
                    debug!("EntropyRng: using JitterRng");
                    self.source = Source::Jitter(jitter_rng);
                    return Ok(());
                },
                Err(err) => { reported_error = reported_error.or(Some(err)) },
            }
        }

        if let Some(err) = reported_error {
            Err(Error::with_cause(ErrorKind::Unavailable,
                                  "All entropy sources failed",
                                  err))
        } else {
            Err(Error::new(ErrorKind::Unavailable,
                           "No entropy sources available"))
        }
    }
}

impl CryptoRng for EntropyRng {}



trait EntropySource {
    fn new_and_fill(dest: &mut [u8]) -> Result<Self, Error>
        where Self: Sized;

    fn fill(&mut self, dest: &mut [u8]) -> Result<(), Error>;

    fn is_supported() -> bool { true }
}

#[allow(unused)]
#[derive(Clone, Debug)]
struct NoSource;

#[allow(unused)]
impl EntropySource for NoSource {
    fn new_and_fill(dest: &mut [u8]) -> Result<Self, Error> {
        Err(Error::new(ErrorKind::Unavailable, "Source not supported"))
    }

    fn fill(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        unreachable!()
    }

    fn is_supported() -> bool { false }
}


#[cfg(feature="rand_os")]
#[derive(Clone, Debug)]
pub struct Os(rngs::OsRng);

#[cfg(feature="rand_os")]
impl EntropySource for Os {
    fn new_and_fill(dest: &mut [u8]) -> Result<Self, Error> {
        let mut rng = rngs::OsRng::new()?;
        rng.try_fill_bytes(dest)?;
        Ok(Os(rng))
    }

    fn fill(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

#[cfg(not(feature="std"))]
type Os = NoSource;


type Custom = NoSource;


#[cfg(not(target_arch = "wasm32"))]
#[derive(Clone, Debug)]
pub struct Jitter(rngs::JitterRng);

#[cfg(not(target_arch = "wasm32"))]
impl EntropySource for Jitter {
    fn new_and_fill(dest: &mut [u8]) -> Result<Self, Error> {
        let mut rng = rngs::JitterRng::new()?;
        rng.try_fill_bytes(dest)?;
        Ok(Jitter(rng))
    }

    fn fill(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

#[cfg(target_arch = "wasm32")]
type Jitter = NoSource;


#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_entropy() {
        let mut rng = EntropyRng::new();
        let n = (rng.next_u32() ^ rng.next_u32()).count_ones();
        assert!(n >= 2);    // p(failure) approx 1e-7
    }
}
