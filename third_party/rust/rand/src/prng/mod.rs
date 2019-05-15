// Copyright 2018 Developers of the Rand project.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Pseudo-random number generators.
//!
//! This module is deprecated:
//! 
//! -   documentation has moved to
//!     [The Book](https://rust-random.github.io/book/guide-rngs.html),
//! -   PRNGs have moved to other `rand_*` crates.

// Deprecations (to be removed in 0.7)
#[doc(hidden)] #[allow(deprecated)]
pub use deprecated::XorShiftRng;
#[doc(hidden)] pub mod isaac {
    // Note: we miss `IsaacCore` here but probably unimportant.
    #[allow(deprecated)] pub use deprecated::IsaacRng;
}
#[doc(hidden)] pub mod isaac64 {
    #[allow(deprecated)] pub use deprecated::Isaac64Rng;
}
#[doc(hidden)] #[allow(deprecated)] pub use deprecated::{IsaacRng, Isaac64Rng};
#[doc(hidden)] pub mod chacha {
    // Note: we miss `ChaChaCore` here but probably unimportant.
    #[allow(deprecated)] pub use deprecated::ChaChaRng;
}
#[doc(hidden)] #[allow(deprecated)] pub use deprecated::ChaChaRng;
#[doc(hidden)] pub mod hc128 {
    // Note: we miss `Hc128Core` here but probably unimportant.
    #[allow(deprecated)] pub use deprecated::Hc128Rng;
}
#[doc(hidden)] #[allow(deprecated)] pub use deprecated::Hc128Rng;
