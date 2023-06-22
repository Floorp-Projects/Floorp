// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

#![warn(missing_docs)]
#![cfg_attr(docsrs, feature(doc_cfg))]

//! # libprio-rs
//!
//! Implementation of the [Prio](https://crypto.stanford.edu/prio/) private data aggregation
//! protocol.
//!
//! Prio v2, used in the [Exposure Notifications Private Analytics][enpa] system, is available in
//! the `client` and `server` modules.
//!
//! Prio3 is available in the `vdaf` module as part of an implementation of [Verifiable Distributed
//! Aggregation Functions][vdaf], along with an experimental implementation of Poplar1.
//!
//! [enpa]: https://www.abetterinternet.org/post/prio-services-for-covid-en/
//! [vdaf]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/05/

pub mod benchmarked;
#[cfg(feature = "prio2")]
#[cfg_attr(docsrs, doc(cfg(feature = "prio2")))]
pub mod client;
#[cfg(feature = "prio2")]
#[cfg_attr(docsrs, doc(cfg(feature = "prio2")))]
pub mod encrypt;
#[cfg(feature = "prio2")]
#[cfg_attr(docsrs, doc(cfg(feature = "prio2")))]
pub mod server;

pub mod codec;
mod fft;
pub mod field;
pub mod flp;
mod fp;
#[cfg(all(feature = "crypto-dependencies", feature = "experimental"))]
#[cfg_attr(
    docsrs,
    doc(cfg(all(feature = "crypto-dependencies", feature = "experimental")))
)]
pub mod idpf;
mod polynomial;
mod prng;
// Module test_vector depends on crate `rand` so we make it an optional feature
// to spare most clients the extra dependency.
#[cfg(all(any(feature = "test-util", test), feature = "prio2"))]
#[doc(hidden)]
pub mod test_vector;
#[cfg(feature = "prio2")]
#[cfg_attr(docsrs, doc(cfg(feature = "prio2")))]
pub mod util;
pub mod vdaf;
