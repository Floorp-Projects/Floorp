// This is a part of rust-encoding.
//
// Any copyright is dedicated to the Public Domain.
// https://creativecommons.org/publicdomain/zero/1.0/

//! Single-byte index tables for
//! [rust-encoding](https://github.com/lifthrasiir/rust-encoding).

#![cfg_attr(test, feature(test))]

#[cfg(test)]
#[macro_use]
extern crate encoding_index_tests;

/// IBM code page 866.
pub mod ibm866;

/// ISO 8859-2.
pub mod iso_8859_2;

/// ISO 8859-3.
pub mod iso_8859_3;

/// ISO 8859-4.
pub mod iso_8859_4;

/// ISO 8859-5.
pub mod iso_8859_5;

/// ISO 8859-6.
pub mod iso_8859_6;

/// ISO 8859-7.
pub mod iso_8859_7;

/// ISO 8859-8 (either visual or logical).
pub mod iso_8859_8;

/// ISO 8859-10.
pub mod iso_8859_10;

/// ISO 8859-13.
pub mod iso_8859_13;

/// ISO 8859-14.
pub mod iso_8859_14;

/// ISO 8859-15.
pub mod iso_8859_15;

/// ISO 8859-16.
pub mod iso_8859_16;

/// KOI8-R.
pub mod koi8_r;

/// KOI8-U.
pub mod koi8_u;

/// MacRoman.
pub mod macintosh;

/// Windows code page 874.
pub mod windows_874;

/// Windows code page 1250.
pub mod windows_1250;

/// Windows code page 1251.
pub mod windows_1251;

/// Windows code page 1252.
pub mod windows_1252;

/// Windows code page 1253.
pub mod windows_1253;

/// Windows code page 1254.
pub mod windows_1254;

/// Windows code page 1254.
pub mod windows_1255;

/// Windows code page 1256.
pub mod windows_1256;

/// Windows code page 1257.
pub mod windows_1257;

/// Windows code page 1258.
pub mod windows_1258;

/// MacCyrillic.
pub mod x_mac_cyrillic;

