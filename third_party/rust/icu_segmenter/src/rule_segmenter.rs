// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::complex::ComplexPayloads;
use crate::indices::{Latin1Indices, Utf16Indices};
use crate::provider::RuleBreakDataV1;
use crate::symbols::*;
use core::str::CharIndices;
use utf8_iter::Utf8CharIndices;

/// The category tag that is returned by
/// [`WordBreakIterator::word_type()`][crate::WordBreakIterator::word_type()].
#[non_exhaustive]
#[derive(Copy, Clone, PartialEq, Debug)]
#[repr(u8)]
pub enum RuleStatusType {
    /// No category tag
    None = 0,
    /// Number category tag
    Number = 1,
    /// Letter category tag, including CJK.
    Letter = 2,
}

/// A trait allowing for RuleBreakIterator to be generalized to multiple string
/// encoding methods and granularity such as grapheme cluster, word, etc.
pub trait RuleBreakType<'l, 's> {
    /// The iterator over characters.
    type IterAttr: Iterator<Item = (usize, Self::CharType)> + Clone + core::fmt::Debug;

    /// The character type.
    type CharType: Copy + Into<u32> + core::fmt::Debug;

    fn get_current_position_character_len(iter: &RuleBreakIterator<'l, 's, Self>) -> usize;

    fn handle_complex_language(
        iter: &mut RuleBreakIterator<'l, 's, Self>,
        left_codepoint: Self::CharType,
    ) -> Option<usize>;
}

/// Implements the [`Iterator`] trait over the segmenter boundaries of the given string.
///
/// Lifetimes:
///
/// - `'l` = lifetime of the segmenter object from which this iterator was created
/// - `'s` = lifetime of the string being segmented
///
/// The [`Iterator::Item`] is an [`usize`] representing index of a code unit
/// _after_ the boundary (for a boundary at the end of text, this index is the length
/// of the [`str`] or array of code units).
#[derive(Debug)]
pub struct RuleBreakIterator<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized> {
    pub(crate) iter: Y::IterAttr,
    pub(crate) len: usize,
    pub(crate) current_pos_data: Option<(usize, Y::CharType)>,
    pub(crate) result_cache: alloc::vec::Vec<usize>,
    pub(crate) data: &'l RuleBreakDataV1<'l>,
    pub(crate) complex: Option<&'l ComplexPayloads>,
    pub(crate) boundary_property: u8,
}

impl<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized> Iterator for RuleBreakIterator<'l, 's, Y> {
    type Item = usize;

    fn next(&mut self) -> Option<Self::Item> {
        // If we have break point cache by previous run, return this result
        if let Some(&first_result) = self.result_cache.first() {
            let mut i = 0;
            loop {
                if i == first_result {
                    self.result_cache = self.result_cache.iter().skip(1).map(|r| r - i).collect();
                    return self.get_current_position();
                }
                i += Y::get_current_position_character_len(self);
                self.advance_iter();
                if self.is_eof() {
                    self.result_cache.clear();
                    return Some(self.len);
                }
            }
        }

        if self.is_eof() {
            self.advance_iter();
            if self.is_eof() && self.len == 0 {
                // Empty string. Since `self.current_pos_data` is always going to be empty,
                // we never read `self.len` except for here, so we can use it to mark that
                // we have already returned the single empty-string breakpoint.
                self.len = 1;
                return Some(0);
            }
            // SOT x anything
            let right_prop = self.get_current_break_property()?;
            if self.is_break_from_table(self.data.sot_property, right_prop) {
                self.boundary_property = 0; // SOT is special type
                return self.get_current_position();
            }
        }

        loop {
            debug_assert!(!self.is_eof());
            let left_codepoint = self.get_current_codepoint()?;
            let left_prop = self.get_break_property(left_codepoint);
            self.advance_iter();

            let Some(right_prop) = self.get_current_break_property() else {
                self.boundary_property = left_prop;
                return Some(self.len);
            };

            // Some segmenter rules doesn't have language-specific rules, we have to use LSTM (or dictionary) segmenter.
            // If property is marked as SA, use it
            if right_prop == self.data.complex_property {
                if left_prop != self.data.complex_property {
                    // break before SA
                    self.boundary_property = left_prop;
                    return self.get_current_position();
                }
                let break_offset = Y::handle_complex_language(self, left_codepoint);
                if break_offset.is_some() {
                    return break_offset;
                }
            }

            // If break_state is equals or grater than 0, it is alias of property.
            let mut break_state = self.get_break_state_from_table(left_prop, right_prop);

            if break_state >= 0 {
                // This isn't simple rule set. We need marker to restore iterator to previous position.
                let mut previous_iter = self.iter.clone();
                let mut previous_pos_data = self.current_pos_data;
                let mut previous_left_prop = left_prop;

                break_state &= !INTERMEDIATE_MATCH_RULE;
                loop {
                    self.advance_iter();

                    let Some(prop) = self.get_current_break_property() else {
                        // Reached EOF. But we are analyzing multiple characters now, so next break may be previous point.
                        self.boundary_property = break_state as u8;
                        if self
                            .get_break_state_from_table(break_state as u8, self.data.eot_property)
                            == NOT_MATCH_RULE
                        {
                            self.boundary_property = previous_left_prop;
                            self.iter = previous_iter;
                            self.current_pos_data = previous_pos_data;
                            return self.get_current_position();
                        }
                        // EOF
                        return Some(self.len);
                    };

                    let previous_break_state = break_state;
                    break_state = self.get_break_state_from_table(break_state as u8, prop);
                    if break_state < 0 {
                        break;
                    }
                    if previous_break_state >= 0
                        && previous_break_state <= self.data.last_codepoint_property
                    {
                        // Move marker
                        previous_iter = self.iter.clone();
                        previous_pos_data = self.current_pos_data;
                        previous_left_prop = break_state as u8;
                    }
                    if (break_state & INTERMEDIATE_MATCH_RULE) != 0 {
                        break_state -= INTERMEDIATE_MATCH_RULE;
                        previous_iter = self.iter.clone();
                        previous_pos_data = self.current_pos_data;
                        previous_left_prop = break_state as u8;
                    }
                }
                if break_state == KEEP_RULE {
                    continue;
                }
                if break_state == NOT_MATCH_RULE {
                    self.boundary_property = previous_left_prop;
                    self.iter = previous_iter;
                    self.current_pos_data = previous_pos_data;
                    return self.get_current_position();
                }
                return self.get_current_position();
            }

            if self.is_break_from_table(left_prop, right_prop) {
                self.boundary_property = left_prop;
                return self.get_current_position();
            }
        }
    }
}

impl<'l, 's, Y: RuleBreakType<'l, 's> + ?Sized> RuleBreakIterator<'l, 's, Y> {
    pub(crate) fn advance_iter(&mut self) {
        self.current_pos_data = self.iter.next();
    }

    pub(crate) fn is_eof(&self) -> bool {
        self.current_pos_data.is_none()
    }

    pub(crate) fn get_current_break_property(&self) -> Option<u8> {
        self.get_current_codepoint()
            .map(|c| self.get_break_property(c))
    }

    pub(crate) fn get_current_position(&self) -> Option<usize> {
        self.current_pos_data.map(|(pos, _)| pos)
    }

    pub(crate) fn get_current_codepoint(&self) -> Option<Y::CharType> {
        self.current_pos_data.map(|(_, codepoint)| codepoint)
    }

    fn get_break_property(&self, codepoint: Y::CharType) -> u8 {
        // Note: Default value is 0 == UNKNOWN
        self.data.property_table.0.get32(codepoint.into())
    }

    fn get_break_state_from_table(&self, left: u8, right: u8) -> i8 {
        let idx = left as usize * self.data.property_count as usize + right as usize;
        // We use unwrap_or to fall back to the base case and prevent panics on bad data.
        self.data.break_state_table.0.get(idx).unwrap_or(KEEP_RULE)
    }

    fn is_break_from_table(&self, left: u8, right: u8) -> bool {
        let rule = self.get_break_state_from_table(left, right);
        if rule == KEEP_RULE {
            return false;
        }
        if rule >= 0 {
            // need additional next characters to get break rule.
            return false;
        }
        true
    }

    /// Return the status value of break boundary.
    /// If segmenter isn't word, always return RuleStatusType::None
    pub fn rule_status(&self) -> RuleStatusType {
        if self.result_cache.first().is_some() {
            // Dictionary type (CJ and East Asian) is letter.
            return RuleStatusType::Letter;
        }
        if self.boundary_property == 0 {
            // break position is SOT / Any
            return RuleStatusType::None;
        }
        match self
            .data
            .rule_status_table
            .0
            .get((self.boundary_property - 1) as usize)
        {
            Some(1) => RuleStatusType::Number,
            Some(2) => RuleStatusType::Letter,
            _ => RuleStatusType::None,
        }
    }

    /// Return true when break boundary is word-like such as letter/number/CJK
    /// If segmenter isn't word, return false
    pub fn is_word_like(&self) -> bool {
        self.rule_status() != RuleStatusType::None
    }
}

#[derive(Debug)]
pub struct RuleBreakTypeUtf8;

impl<'l, 's> RuleBreakType<'l, 's> for RuleBreakTypeUtf8 {
    type IterAttr = CharIndices<'s>;
    type CharType = char;

    fn get_current_position_character_len(iter: &RuleBreakIterator<Self>) -> usize {
        iter.get_current_codepoint().map_or(0, |c| c.len_utf8())
    }

    fn handle_complex_language(
        _: &mut RuleBreakIterator<Self>,
        _: Self::CharType,
    ) -> Option<usize> {
        unreachable!()
    }
}

#[derive(Debug)]
pub struct RuleBreakTypePotentiallyIllFormedUtf8;

impl<'l, 's> RuleBreakType<'l, 's> for RuleBreakTypePotentiallyIllFormedUtf8 {
    type IterAttr = Utf8CharIndices<'s>;
    type CharType = char;

    fn get_current_position_character_len(iter: &RuleBreakIterator<Self>) -> usize {
        iter.get_current_codepoint().map_or(0, |c| c.len_utf8())
    }

    fn handle_complex_language(
        _: &mut RuleBreakIterator<Self>,
        _: Self::CharType,
    ) -> Option<usize> {
        unreachable!()
    }
}

#[derive(Debug)]
pub struct RuleBreakTypeLatin1;

impl<'l, 's> RuleBreakType<'l, 's> for RuleBreakTypeLatin1 {
    type IterAttr = Latin1Indices<'s>;
    type CharType = u8;

    fn get_current_position_character_len(_: &RuleBreakIterator<Self>) -> usize {
        unreachable!()
    }

    fn handle_complex_language(
        _: &mut RuleBreakIterator<Self>,
        _: Self::CharType,
    ) -> Option<usize> {
        unreachable!()
    }
}

#[derive(Debug)]
pub struct RuleBreakTypeUtf16;

impl<'l, 's> RuleBreakType<'l, 's> for RuleBreakTypeUtf16 {
    type IterAttr = Utf16Indices<'s>;
    type CharType = u32;

    fn get_current_position_character_len(iter: &RuleBreakIterator<Self>) -> usize {
        match iter.get_current_codepoint() {
            None => 0,
            Some(ch) if ch >= 0x10000 => 2,
            _ => 1,
        }
    }

    fn handle_complex_language(
        _: &mut RuleBreakIterator<Self>,
        _: Self::CharType,
    ) -> Option<usize> {
        unreachable!()
    }
}
