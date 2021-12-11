// Copyright 2012-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Iterators which split strings on Grapheme Cluster or Word boundaries, according
//! to the [Unicode Standard Annex #29](http://www.unicode.org/reports/tr29/) rules.
//!
//! ```rust
//! extern crate unicode_segmentation;
//!
//! use unicode_segmentation::UnicodeSegmentation;
//!
//! fn main() {
//!     let s = "a̐éö̲\r\n";
//!     let g = UnicodeSegmentation::graphemes(s, true).collect::<Vec<&str>>();
//!     let b: &[_] = &["a̐", "é", "ö̲", "\r\n"];
//!     assert_eq!(g, b);
//!
//!     let s = "The quick (\"brown\") fox can't jump 32.3 feet, right?";
//!     let w = s.unicode_words().collect::<Vec<&str>>();
//!     let b: &[_] = &["The", "quick", "brown", "fox", "can't", "jump", "32.3", "feet", "right"];
//!     assert_eq!(w, b);
//!
//!     let s = "The quick (\"brown\")  fox";
//!     let w = s.split_word_bounds().collect::<Vec<&str>>();
//!     let b: &[_] = &["The", " ", "quick", " ", "(", "\"", "brown", "\"", ")", " ", " ", "fox"];
//!     assert_eq!(w, b);
//! }
//! ```
//!
//! # no_std
//!
//! unicode-segmentation does not depend on libstd, so it can be used in crates
//! with the `#![no_std]` attribute.
//!
//! # crates.io
//!
//! You can use this package in your project by adding the following
//! to your `Cargo.toml`:
//!
//! ```toml
//! [dependencies]
//! unicode-segmentation = "1.1.0"
//! ```

#![deny(missing_docs, unsafe_code)]
#![doc(html_logo_url = "https://unicode-rs.github.io/unicode-rs_sm.png",
       html_favicon_url = "https://unicode-rs.github.io/unicode-rs_sm.png")]

#![no_std]

#[cfg(test)]
#[macro_use]
extern crate std;

#[cfg(test)]
#[macro_use]
extern crate quickcheck;

pub use grapheme::{Graphemes, GraphemeIndices};
pub use grapheme::{GraphemeCursor, GraphemeIncomplete};
pub use tables::UNICODE_VERSION;
pub use word::{UWordBounds, UWordBoundIndices, UnicodeWords};

mod grapheme;
mod tables;
mod word;

#[cfg(test)]
mod test;
#[cfg(test)]
mod testdata;

/// Methods for segmenting strings according to
/// [Unicode Standard Annex #29](http://www.unicode.org/reports/tr29/).
pub trait UnicodeSegmentation {
    /// Returns an iterator over the [grapheme clusters][graphemes] of `self`.
    ///
    /// [graphemes]: http://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
    ///
    /// If `is_extended` is true, the iterator is over the
    /// *extended grapheme clusters*;
    /// otherwise, the iterator is over the *legacy grapheme clusters*.
    /// [UAX#29](http://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries)
    /// recommends extended grapheme cluster boundaries for general processing.
    ///
    /// # Examples
    ///
    /// ```
    /// # use self::unicode_segmentation::UnicodeSegmentation;
    /// let gr1 = UnicodeSegmentation::graphemes("a\u{310}e\u{301}o\u{308}\u{332}", true)
    ///           .collect::<Vec<&str>>();
    /// let b: &[_] = &["a\u{310}", "e\u{301}", "o\u{308}\u{332}"];
    ///
    /// assert_eq!(&gr1[..], b);
    ///
    /// let gr2 = UnicodeSegmentation::graphemes("a\r\nb🇷🇺🇸🇹", true).collect::<Vec<&str>>();
    /// let b: &[_] = &["a", "\r\n", "b", "🇷🇺", "🇸🇹"];
    ///
    /// assert_eq!(&gr2[..], b);
    /// ```
    fn graphemes<'a>(&'a self, is_extended: bool) -> Graphemes<'a>;

    /// Returns an iterator over the grapheme clusters of `self` and their
    /// byte offsets. See `graphemes()` for more information.
    ///
    /// # Examples
    ///
    /// ```
    /// # use self::unicode_segmentation::UnicodeSegmentation;
    /// let gr_inds = UnicodeSegmentation::grapheme_indices("a̐éö̲\r\n", true)
    ///               .collect::<Vec<(usize, &str)>>();
    /// let b: &[_] = &[(0, "a̐"), (3, "é"), (6, "ö̲"), (11, "\r\n")];
    ///
    /// assert_eq!(&gr_inds[..], b);
    /// ```
    fn grapheme_indices<'a>(&'a self, is_extended: bool) -> GraphemeIndices<'a>;

    /// Returns an iterator over the words of `self`, separated on
    /// [UAX#29 word boundaries](http://www.unicode.org/reports/tr29/#Word_Boundaries).
    ///
    /// Here, "words" are just those substrings which, after splitting on
    /// UAX#29 word boundaries, contain any alphanumeric characters. That is, the
    /// substring must contain at least one character with the
    /// [Alphabetic](http://unicode.org/reports/tr44/#Alphabetic)
    /// property, or with
    /// [General_Category=Number](http://unicode.org/reports/tr44/#General_Category_Values).
    ///
    /// # Example
    ///
    /// ```
    /// # use self::unicode_segmentation::UnicodeSegmentation;
    /// let uws = "The quick (\"brown\") fox can't jump 32.3 feet, right?";
    /// let uw1 = uws.unicode_words().collect::<Vec<&str>>();
    /// let b: &[_] = &["The", "quick", "brown", "fox", "can't", "jump", "32.3", "feet", "right"];
    ///
    /// assert_eq!(&uw1[..], b);
    /// ```
    fn unicode_words<'a>(&'a self) -> UnicodeWords<'a>;

    /// Returns an iterator over substrings of `self` separated on
    /// [UAX#29 word boundaries](http://www.unicode.org/reports/tr29/#Word_Boundaries).
    ///
    /// The concatenation of the substrings returned by this function is just the original string.
    ///
    /// # Example
    ///
    /// ```
    /// # use self::unicode_segmentation::UnicodeSegmentation;
    /// let swu1 = "The quick (\"brown\")  fox".split_word_bounds().collect::<Vec<&str>>();
    /// let b: &[_] = &["The", " ", "quick", " ", "(", "\"", "brown", "\"", ")", " ", " ", "fox"];
    ///
    /// assert_eq!(&swu1[..], b);
    /// ```
    fn split_word_bounds<'a>(&'a self) -> UWordBounds<'a>;

    /// Returns an iterator over substrings of `self`, split on UAX#29 word boundaries,
    /// and their offsets. See `split_word_bounds()` for more information.
    ///
    /// # Example
    ///
    /// ```
    /// # use self::unicode_segmentation::UnicodeSegmentation;
    /// let swi1 = "Brr, it's 29.3°F!".split_word_bound_indices().collect::<Vec<(usize, &str)>>();
    /// let b: &[_] = &[(0, "Brr"), (3, ","), (4, " "), (5, "it's"), (9, " "), (10, "29.3"),
    ///                 (14, "°"), (16, "F"), (17, "!")];
    ///
    /// assert_eq!(&swi1[..], b);
    /// ```
    fn split_word_bound_indices<'a>(&'a self) -> UWordBoundIndices<'a>;
}

impl UnicodeSegmentation for str {
    #[inline]
    fn graphemes(&self, is_extended: bool) -> Graphemes {
        grapheme::new_graphemes(self, is_extended)
    }

    #[inline]
    fn grapheme_indices(&self, is_extended: bool) -> GraphemeIndices {
        grapheme::new_grapheme_indices(self, is_extended)
    }

    #[inline]
    fn unicode_words(&self) -> UnicodeWords {
        word::new_unicode_words(self)
    }

    #[inline]
    fn split_word_bounds(&self) -> UWordBounds {
        word::new_word_bounds(self)
    }

    #[inline]
    fn split_word_bound_indices(&self) -> UWordBoundIndices {
        word::new_word_bound_indices(self)
    }
}
