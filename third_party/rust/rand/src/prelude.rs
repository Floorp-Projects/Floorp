// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Convenience re-export of common members
//!
//! Like the standard library's prelude, this module simplifies importing of
//! common items. Unlike the standard prelude, the contents of this module must
//! be imported manually:
//!
//! ```
//! use rand::prelude::*;
//! # let _ = StdRng::from_entropy();
//! # let mut r = SmallRng::from_rng(thread_rng()).unwrap();
//! # let _: f32 = r.gen();
//! ```

#[doc(no_inline)] pub use distributions::Distribution;
#[doc(no_inline)] pub use rngs::{SmallRng, StdRng};
#[doc(no_inline)] #[cfg(feature="std")] pub use rngs::ThreadRng;
#[doc(no_inline)] pub use {Rng, RngCore, CryptoRng, SeedableRng};
#[doc(no_inline)] #[cfg(feature="std")] pub use {FromEntropy, random, thread_rng};
#[doc(no_inline)] pub use seq::{SliceRandom, IteratorRandom};
