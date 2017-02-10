// This is a part of rust-encoding.
//
// Any copyright is dedicated to the Public Domain.
// https://creativecommons.org/publicdomain/zero/1.0/

//! Korean index tables for [rust-encoding](https://github.com/lifthrasiir/rust-encoding).

#![cfg_attr(test, feature(test))]

#[cfg(test)]
#[macro_use]
extern crate encoding_index_tests;

/// KS X 1001 plus Unified Hangul Code.
///
/// From the Encoding Standard:
///
/// > This matches the KS X 1001 standard and the Unified Hangul Code,
/// > more commonly known together as Windows Codepage 949.
pub mod euc_kr;

