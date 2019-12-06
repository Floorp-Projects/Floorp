// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! [`Uuid`] parsing constructs and utilities.
//!
//! [`Uuid`]: ../struct.Uuid.html

mod core_support;
#[cfg(feature = "std")]
mod std_support;

/// The expected value.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Expected {
    /// Expected any one of the given values.
    Any(&'static [usize]),
    /// Expected the given value.
    Exact(usize),
    /// Expected any values in the given range.
    Range {
        /// The minimum expected value.
        min: usize,
        /// The maximum expected value.
        max: usize,
    },
}

/// An error that can occur while parsing a [`Uuid`] string.
///
/// [`Uuid`]: ../struct.Uuid.html
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum ParseError {
    /// Invalid character in the [`Uuid`] string.
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    InvalidCharacter {
        /// The expected characters.
        expected: &'static str,
        /// The invalid character found.
        found: char,
        /// The invalid character position.
        index: usize,
    },
    /// Invalid number of segments in the [`Uuid`] string.
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    InvalidGroupCount {
        /// The expected number of segments.
        // TODO: explain multiple segment count.
        // BODY: Parsers can expect a range of Uuid segment count.
        //       This needs to be expanded on.
        expected: Expected,
        /// The number of segments found.
        found: usize,
    },
    /// Invalid length of a segment in a [`Uuid`] string.
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    InvalidGroupLength {
        /// The expected length of the segment.
        expected: Expected,
        /// The length of segment found.
        found: usize,
        /// The segment with invalid length.
        group: usize,
    },
    /// Invalid length of the [`Uuid`] string.
    ///
    /// [`Uuid`]: ../struct.Uuid.html
    InvalidLength {
        /// The expected length(s).
        // TODO: explain multiple lengths.
        // BODY: Parsers can expect a range of Uuid lenghts.
        //       This needs to be expanded on.
        expected: Expected,
        /// The invalid length found.
        found: usize,
    },
}

impl ParseError {
    fn _description(&self) -> &str {
        match *self {
            ParseError::InvalidCharacter { .. } => "invalid character",
            ParseError::InvalidGroupCount { .. } => "invalid number of groups",
            ParseError::InvalidGroupLength { .. } => "invalid group length",
            ParseError::InvalidLength { .. } => "invalid length",
        }
    }
}

/// Check if the length matches any of the given criteria lengths.
pub(crate) fn len_matches_any(len: usize, crits: &[usize]) -> bool {
    for crit in crits {
        if len == *crit {
            return true;
        }
    }

    false
}

/// Check if the length matches any criteria lengths in the given range
/// (inclusive).
#[allow(dead_code)]
pub(crate) fn len_matches_range(len: usize, min: usize, max: usize) -> bool {
    for crit in min..(max + 1) {
        if len == crit {
            return true;
        }
    }

    false
}

// Accumulated length of each hyphenated group in hex digits.
pub(crate) const ACC_GROUP_LENS: [usize; 5] = [8, 12, 16, 20, 32];

// Length of each hyphenated group in hex digits.
pub(crate) const GROUP_LENS: [usize; 5] = [8, 4, 4, 4, 12];
