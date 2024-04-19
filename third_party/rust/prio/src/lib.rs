// Copyright (c) 2020 Apple Inc.
// SPDX-License-Identifier: MPL-2.0

#![warn(missing_docs)]
#![cfg_attr(docsrs, feature(doc_cfg))]

//! # libprio-rs
//!
//! Implementation of the [Prio](https://crypto.stanford.edu/prio/) private data aggregation
//! protocol.
//!
//! Prio3 is available in the `vdaf` module as part of an implementation of [Verifiable Distributed
//! Aggregation Functions][vdaf], along with an experimental implementation of Poplar1.
//!
//! [vdaf]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/05/

pub mod benchmarked;
pub mod codec;
#[cfg(feature = "experimental")]
pub mod dp;
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
pub mod topology;
pub mod vdaf;
