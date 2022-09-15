// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

#![warn(missing_docs)]
#![cfg_attr(docsrs, feature(doc_cfg))]

//! Libprio-rs
//!
//! Implementation of the [Prio](https://crypto.stanford.edu/prio/) private data aggregation
//! protocol. For now we only support 0 / 1 vectors.

pub mod benchmarked;
#[cfg(feature = "prio2")]
pub mod client;
#[cfg(feature = "prio2")]
pub mod encrypt;
#[cfg(feature = "prio2")]
pub mod server;

pub mod codec;
mod fft;
pub mod field;
pub mod flp;
mod fp;
mod polynomial;
mod prng;
// Module test_vector depends on crate `rand` so we make it an optional feature
// to spare most clients the extra dependency.
#[cfg(all(any(feature = "test-util", test), feature = "prio2"))]
pub mod test_vector;
#[cfg(feature = "prio2")]
pub mod util;
pub mod vdaf;
