// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Random number generators and adapters for common usage:
//!
//! - [`ThreadRng`], a fast, secure, auto-seeded thread-local generator
//! - [`StdRng`] and [`SmallRng`], algorithms to cover typical usage
//! - [`EntropyRng`], [`OsRng`] and [`JitterRng`] as entropy sources
//! - [`mock::StepRng`] as a simple counter for tests
//! - [`adapter::ReadRng`] to read from a file/stream
//! - [`adapter::ReseedingRng`] to reseed a PRNG on clone / process fork etc.
//!
//! # Background — Random number generators (RNGs)
//!
//! Computers are inherently deterministic, so to get *random* numbers one
//! either has to use a hardware generator or collect bits of *entropy* from
//! various sources (e.g. event timestamps, or jitter). This is a relatively
//! slow and complicated operation.
//!
//! Generally the operating system will collect some entropy, remove bias, and
//! use that to seed its own PRNG; [`OsRng`] provides an interface to this.
//! [`JitterRng`] is an entropy collector included with Rand that measures
//! jitter in the CPU execution time, and jitter in memory access time.
//! [`EntropyRng`] is a wrapper that uses the best entropy source that is
//! available.
//!
//! ## Pseudo-random number generators
//!
//! What is commonly used instead of "true" random number renerators, are
//! *pseudo-random number generators* (PRNGs), deterministic algorithms that
//! produce an infinite stream of pseudo-random numbers from a small random
//! seed. PRNGs are faster, and have better provable properties. The numbers
//! produced can be statistically of very high quality and can be impossible to
//! predict. (They can also have obvious correlations and be trivial to predict;
//! quality varies.)
//!
//! There are two different types of PRNGs: those developed for simulations
//! and statistics, and those developed for use in cryptography; the latter are
//! called Cryptographically Secure PRNGs (CSPRNG or CPRNG). Both types can
//! have good statistical quality but the latter also have to be impossible to
//! predict, even after seeing many previous output values. Rand provides a good
//! default algorithm from each class:
//!
//! - [`SmallRng`] is a PRNG chosen for low memory usage, high performance and
//!   good statistical quality.
//! - [`StdRng`] is a CSPRNG chosen for good performance and trust of security
//!   (based on reviews, maturity and usage). The current algorithm is HC-128,
//!   which is one of the recommendations by ECRYPT's eSTREAM project.
//!
//! The above PRNGs do not cover all use-cases; more algorithms can be found in
//! the [`prng`][crate::prng] module, as well as in several other crates. For example, you
//! may wish a CSPRNG with significantly lower memory usage than [`StdRng`]
//! while being less concerned about performance, in which case [`ChaChaRng`]
//! is a good choice.
//!
//! One complexity is that the internal state of a PRNG must change with every
//! generated number. For APIs this generally means a mutable reference to the
//! state of the PRNG has to be passed around.
//!
//! A solution is [`ThreadRng`]. This is a thread-local implementation of
//! [`StdRng`] with automatic seeding on first use. It is the best choice if you
//! "just" want a convenient, secure, fast random number source. Use via the
//! [`thread_rng`] function, which gets a reference to the current thread's
//! local instance.
//!
//! ## Seeding
//!
//! As mentioned above, PRNGs require a random seed in order to produce random
//! output. This is especially important for CSPRNGs, which are still
//! deterministic algorithms, thus can only be secure if their seed value is
//! also secure. To seed a PRNG, use one of:
//!
//! - [`FromEntropy::from_entropy`]; this is the most convenient way to seed
//!   with fresh, secure random data.
//! - [`SeedableRng::from_rng`]; this allows seeding from another PRNG or
//!   from an entropy source such as [`EntropyRng`].
//! - [`SeedableRng::from_seed`]; this is mostly useful if you wish to be able
//!   to reproduce the output sequence by using a fixed seed. (Don't use
//!   [`StdRng`] or [`SmallRng`] in this case since different algorithms may be
//!   used by future versions of Rand; use an algorithm from the
//!   [`prng`] module.)
//!
//! ## Conclusion
//!
//! - [`thread_rng`] is what you often want to use.
//! - If you want more control, flexibility, or better performance, use
//!   [`StdRng`], [`SmallRng`] or an algorithm from the [`prng`] module.
//! - Use [`FromEntropy::from_entropy`] to seed new PRNGs.
//! - If you need reproducibility, use [`SeedableRng::from_seed`] combined with
//!   a named PRNG.
//!
//! More information and notes on cryptographic security can be found
//! in the [`prng`] module.
//!
//! ## Examples
//!
//! Examples of seeding PRNGs:
//!
//! ```
//! use rand::prelude::*;
//! # use rand::Error;
//!
//! // StdRng seeded securely by the OS or local entropy collector:
//! let mut rng = StdRng::from_entropy();
//! # let v: u32 = rng.gen();
//!
//! // SmallRng seeded from thread_rng:
//! # fn try_inner() -> Result<(), Error> {
//! let mut rng = SmallRng::from_rng(thread_rng())?;
//! # let v: u32 = rng.gen();
//! # Ok(())
//! # }
//! # try_inner().unwrap();
//!
//! // SmallRng seeded by a constant, for deterministic results:
//! let seed = [1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16]; // byte array
//! let mut rng = SmallRng::from_seed(seed);
//! # let v: u32 = rng.gen();
//! ```
//!
//!
//! # Implementing custom RNGs
//!
//! If you want to implement custom RNG, see the [`rand_core`] crate. The RNG
//! will have to implement the [`RngCore`] trait, where the [`Rng`] trait is
//! build on top of.
//!
//! If the RNG needs seeding, also implement the [`SeedableRng`] trait.
//!
//! [`CryptoRng`] is a marker trait cryptographically secure PRNGs can
//! implement.
//!
//! [`OsRng`]: rand_os::OsRng
//! [`SmallRng`]: rngs::SmallRng
//! [`StdRng`]: rngs::StdRng
//! [`ThreadRng`]: rngs::ThreadRng
//! [`EntropyRng`]: rngs::EntropyRng
//! [`JitterRng`]: rngs::JitterRng
//! [`mock::StepRng`]: rngs::mock::StepRng
//! [`adapter::ReadRng`]: rngs::adapter::ReadRng
//! [`adapter::ReseedingRng`]: rngs::adapter::ReseedingRng
//! [`ChaChaRng`]: rand_chacha::ChaChaRng

pub mod adapter;

#[cfg(feature="std")] mod entropy;
pub mod mock;   // Public so we don't export `StepRng` directly, making it a bit
                // more clear it is intended for testing.
mod small;
mod std;
#[cfg(feature="std")] pub(crate) mod thread;


pub use rand_jitter::{JitterRng, TimerError};
#[cfg(feature="std")] pub use self::entropy::EntropyRng;

pub use self::small::SmallRng;
pub use self::std::StdRng;
#[cfg(feature="std")] pub use self::thread::ThreadRng;

#[cfg(feature="rand_os")]
pub use rand_os::OsRng;
