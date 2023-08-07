// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use alloc::vec::Vec;
use icu_provider::prelude::*;

use crate::indices::{Latin1Indices, Utf16Indices};
use crate::iterator_helpers::derive_usize_iterator_with_type;
use crate::rule_segmenter::*;
use crate::{provider::*, SegmenterError};
use utf8_iter::Utf8CharIndices;

/// Implements the [`Iterator`] trait over the sentence boundaries of the given string.
///
/// Lifetimes:
///
/// - `'l` = lifetime of the segmenter object from which this iterator was created
/// - `'s` = lifetime of the string being segmented
///
/// The [`Iterator::Item`] is an [`usize`] representing index of a code unit
/// _after_ the boundary (for a boundary at the end of text, this index is the length
/// of the [`str`] or array of code units).
///
/// For examples of use, see [`SentenceSegmenter`].
#[derive(Debug)]
pub struct SentenceBreakIterator<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized>(
    RuleBreakIterator<'l, 's, Y>,
);

derive_usize_iterator_with_type!(SentenceBreakIterator);

/// Sentence break iterator for an `str` (a UTF-8 string).
///
/// For examples of use, see [`SentenceSegmenter`].
pub type SentenceBreakIteratorUtf8<'l, 's> = SentenceBreakIterator<'l, 's, RuleBreakTypeUtf8>;

/// Sentence break iterator for a potentially invalid UTF-8 string.
///
/// For examples of use, see [`SentenceSegmenter`].
pub type SentenceBreakIteratorPotentiallyIllFormedUtf8<'l, 's> =
    SentenceBreakIterator<'l, 's, RuleBreakTypePotentiallyIllFormedUtf8>;

/// Sentence break iterator for a Latin-1 (8-bit) string.
///
/// For examples of use, see [`SentenceSegmenter`].
pub type SentenceBreakIteratorLatin1<'l, 's> = SentenceBreakIterator<'l, 's, RuleBreakTypeLatin1>;

/// Sentence break iterator for a UTF-16 string.
///
/// For examples of use, see [`SentenceSegmenter`].
pub type SentenceBreakIteratorUtf16<'l, 's> = SentenceBreakIterator<'l, 's, RuleBreakTypeUtf16>;

/// Supports loading sentence break data, and creating sentence break iterators for different string
/// encodings.
///
/// # Examples
///
/// Segment a string:
///
/// ```rust
/// use icu_segmenter::SentenceSegmenter;
/// let segmenter =
///     SentenceSegmenter::try_new_unstable(&icu_testdata::unstable())
///         .expect("Data exists");
///
/// let breakpoints: Vec<usize> =
///     segmenter.segment_str("Hello World").collect();
/// assert_eq!(&breakpoints, &[0, 11]);
/// ```
///
/// Segment a Latin1 byte string:
///
/// ```rust
/// use icu_segmenter::SentenceSegmenter;
/// let segmenter =
///     SentenceSegmenter::try_new_unstable(&icu_testdata::unstable())
///         .expect("Data exists");
///
/// let breakpoints: Vec<usize> =
///     segmenter.segment_latin1(b"Hello World").collect();
/// assert_eq!(&breakpoints, &[0, 11]);
/// ```
///
/// Successive boundaries can be used to retrieve the sentences.
/// In particular, the first boundary is always 0, and the last one is the
/// length of the segmented text in code units.
///
/// ```rust
/// # use icu_segmenter::SentenceSegmenter;
/// # let segmenter =
/// #     SentenceSegmenter::try_new_unstable(&icu_testdata::unstable())
/// #         .expect("Data exists");
/// use itertools::Itertools;
/// let text = "Ceci tuera cela. Le livre tuera l’édifice.";
/// let sentences: Vec<&str> = segmenter
///     .segment_str(text)
///     .tuple_windows()
///     .map(|(i, j)| &text[i..j])
///     .collect();
/// assert_eq!(
///     &sentences,
///     &["Ceci tuera cela. ", "Le livre tuera l’édifice."]
/// );
/// ```
#[derive(Debug)]
pub struct SentenceSegmenter {
    payload: DataPayload<SentenceBreakDataV1Marker>,
}

impl SentenceSegmenter {
    /// Constructs a [`SentenceSegmenter`] with an invariant locale.
    pub fn try_new_unstable<D>(provider: &D) -> Result<Self, SegmenterError>
    where
        D: DataProvider<SentenceBreakDataV1Marker> + ?Sized,
    {
        let payload = provider.load(Default::default())?.take_payload()?;
        Ok(Self { payload })
    }

    icu_provider::gen_any_buffer_constructors!(locale: skip, options: skip, error: SegmenterError);

    /// Creates a sentence break iterator for an `str` (a UTF-8 string).
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_str<'l, 's>(&'l self, input: &'s str) -> SentenceBreakIteratorUtf8<'l, 's> {
        SentenceBreakIterator(RuleBreakIterator {
            iter: input.char_indices(),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.payload.get(),
            complex: None,
            boundary_property: 0,
        })
    }
    /// Creates a sentence break iterator for a potentially ill-formed UTF8 string
    ///
    /// Invalid characters are treated as REPLACEMENT CHARACTER
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_utf8<'l, 's>(
        &'l self,
        input: &'s [u8],
    ) -> SentenceBreakIteratorPotentiallyIllFormedUtf8<'l, 's> {
        SentenceBreakIterator(RuleBreakIterator {
            iter: Utf8CharIndices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.payload.get(),
            complex: None,
            boundary_property: 0,
        })
    }
    /// Creates a sentence break iterator for a Latin-1 (8-bit) string.
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_latin1<'l, 's>(
        &'l self,
        input: &'s [u8],
    ) -> SentenceBreakIteratorLatin1<'l, 's> {
        SentenceBreakIterator(RuleBreakIterator {
            iter: Latin1Indices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.payload.get(),
            complex: None,
            boundary_property: 0,
        })
    }

    /// Creates a sentence break iterator for a UTF-16 string.
    ///
    /// There are always breakpoints at 0 and the string length, or only at 0 for the empty string.
    pub fn segment_utf16<'l, 's>(&'l self, input: &'s [u16]) -> SentenceBreakIteratorUtf16<'l, 's> {
        SentenceBreakIterator(RuleBreakIterator {
            iter: Utf16Indices::new(input),
            len: input.len(),
            current_pos_data: None,
            result_cache: Vec::new(),
            data: self.payload.get(),
            complex: None,
            boundary_property: 0,
        })
    }
}

#[cfg(all(test, feature = "serde"))]
#[test]
fn empty_string() {
    let segmenter =
        SentenceSegmenter::try_new_with_buffer_provider(&icu_testdata::buffer()).unwrap();
    let breaks: Vec<usize> = segmenter.segment_str("").collect();
    assert_eq!(breaks, [0]);
}
