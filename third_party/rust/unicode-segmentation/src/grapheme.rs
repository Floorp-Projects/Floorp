// Copyright 2012-2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core::cmp;

use tables::grapheme::GraphemeCat;

/// External iterator for grapheme clusters and byte offsets.
#[derive(Clone)]
pub struct GraphemeIndices<'a> {
    start_offset: usize,
    iter: Graphemes<'a>,
}

impl<'a> GraphemeIndices<'a> {
    #[inline]
    /// View the underlying data (the part yet to be iterated) as a slice of the original string.
    ///
    /// ```rust
    /// # use unicode_segmentation::UnicodeSegmentation;
    /// let mut iter = "abc".grapheme_indices(true);
    /// assert_eq!(iter.as_str(), "abc");
    /// iter.next();
    /// assert_eq!(iter.as_str(), "bc");
    /// iter.next();
    /// iter.next();
    /// assert_eq!(iter.as_str(), "");
    /// ```
    pub fn as_str(&self) -> &'a str {
        self.iter.as_str()
    }
}

impl<'a> Iterator for GraphemeIndices<'a> {
    type Item = (usize, &'a str);

    #[inline]
    fn next(&mut self) -> Option<(usize, &'a str)> {
        self.iter.next().map(|s| (s.as_ptr() as usize - self.start_offset, s))
    }

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl<'a> DoubleEndedIterator for GraphemeIndices<'a> {
    #[inline]
    fn next_back(&mut self) -> Option<(usize, &'a str)> {
        self.iter.next_back().map(|s| (s.as_ptr() as usize - self.start_offset, s))
    }
}

/// External iterator for a string's
/// [grapheme clusters](http://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries).
#[derive(Clone)]
pub struct Graphemes<'a> {
    string: &'a str,
    cursor: GraphemeCursor,
    cursor_back: GraphemeCursor,
}

impl<'a> Graphemes<'a> {
    #[inline]
    /// View the underlying data (the part yet to be iterated) as a slice of the original string.
    ///
    /// ```rust
    /// # use unicode_segmentation::UnicodeSegmentation;
    /// let mut iter = "abc".graphemes(true);
    /// assert_eq!(iter.as_str(), "abc");
    /// iter.next();
    /// assert_eq!(iter.as_str(), "bc");
    /// iter.next();
    /// iter.next();
    /// assert_eq!(iter.as_str(), "");
    /// ```
    pub fn as_str(&self) -> &'a str {
        &self.string[self.cursor.cur_cursor()..self.cursor_back.cur_cursor()]
    }
}

impl<'a> Iterator for Graphemes<'a> {
    type Item = &'a str;

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let slen = self.cursor_back.cur_cursor() - self.cursor.cur_cursor();
        (cmp::min(slen, 1), Some(slen))
    }

    #[inline]
    fn next(&mut self) -> Option<&'a str> {
        let start = self.cursor.cur_cursor();
        if start == self.cursor_back.cur_cursor() {
            return None;
        }
        let next = self.cursor.next_boundary(self.string, 0).unwrap().unwrap();
        Some(&self.string[start..next])
    }
}

impl<'a> DoubleEndedIterator for Graphemes<'a> {
    #[inline]
    fn next_back(&mut self) -> Option<&'a str> {
        let end = self.cursor_back.cur_cursor();
        if end == self.cursor.cur_cursor() {
            return None;
        }
        let prev = self.cursor_back.prev_boundary(self.string, 0).unwrap().unwrap();
        Some(&self.string[prev..end])
    }
}

#[inline]
pub fn new_graphemes<'b>(s: &'b str, is_extended: bool) -> Graphemes<'b> {
    let len = s.len();
    Graphemes {
        string: s,
        cursor: GraphemeCursor::new(0, len, is_extended),
        cursor_back: GraphemeCursor::new(len, len, is_extended),
    }
}

#[inline]
pub fn new_grapheme_indices<'b>(s: &'b str, is_extended: bool) -> GraphemeIndices<'b> {
    GraphemeIndices { start_offset: s.as_ptr() as usize, iter: new_graphemes(s, is_extended) }
}

// maybe unify with PairResult?
// An enum describing information about a potential boundary.
#[derive(PartialEq, Eq, Clone)]
enum GraphemeState {
    // No information is known.
    Unknown,
    // It is known to not be a boundary.
    NotBreak,
    // It is known to be a boundary.
    Break,
    // The codepoint after is a Regional Indicator Symbol, so a boundary iff
    // it is preceded by an even number of RIS codepoints. (GB12, GB13)
    Regional,
    // The codepoint after is in the E_Modifier category, so whether it's a boundary
    // depends on pre-context according to GB10.
    Emoji,
}

/// Cursor-based segmenter for grapheme clusters.
#[derive(Clone)]
pub struct GraphemeCursor {
    // Current cursor position.
    offset: usize,
    // Total length of the string.
    len: usize,
    // A config flag indicating whether this cursor computes legacy or extended
    // grapheme cluster boundaries (enables GB9a and GB9b if set).
    is_extended: bool,
    // Information about the potential boundary at `offset`
    state: GraphemeState,
    // Category of codepoint immediately preceding cursor, if known.
    cat_before: Option<GraphemeCat>,
    // Category of codepoint immediately after cursor, if known.
    cat_after: Option<GraphemeCat>,
    // If set, at least one more codepoint immediately preceding this offset
    // is needed to resolve whether there's a boundary at `offset`.
    pre_context_offset: Option<usize>,
    // The number of RIS codepoints preceding `offset`. If `pre_context_offset`
    // is set, then counts the number of RIS between that and `offset`, otherwise
    // is an accurate count relative to the string.
    ris_count: Option<usize>,
    // Set if a call to `prev_boundary` or `next_boundary` was suspended due
    // to needing more input.
    resuming: bool,
}

/// An error return indicating that not enough content was available in the
/// provided chunk to satisfy the query, and that more content must be provided.
#[derive(PartialEq, Eq, Debug)]
pub enum GraphemeIncomplete {
    /// More pre-context is needed. The caller should call `provide_context`
    /// with a chunk ending at the offset given, then retry the query. This
    /// will only be returned if the `chunk_start` parameter is nonzero.
    PreContext(usize),

    /// When requesting `prev_boundary`, the cursor is moving past the beginning
    /// of the current chunk, so the chunk before that is requested. This will
    /// only be returned if the `chunk_start` parameter is nonzero.
    PrevChunk,

    /// When requesting `next_boundary`, the cursor is moving past the end of the
    /// current chunk, so the chunk after that is requested. This will only be
    /// returned if the chunk ends before the `len` parameter provided on
    /// creation of the cursor.
    NextChunk,  // requesting chunk following the one given

    /// An error returned when the chunk given does not contain the cursor position.
    InvalidOffset,
}

// An enum describing the result from lookup of a pair of categories.
#[derive(PartialEq, Eq)]
enum PairResult {
    NotBreak,  // definitely not a break
    Break,  // definitely a break
    Extended,  // a break iff not in extended mode
    Regional,  // a break if preceded by an even number of RIS
    Emoji,  // a break if preceded by emoji base and (Extend)*
}

fn check_pair(before: GraphemeCat, after: GraphemeCat) -> PairResult {
    use tables::grapheme::GraphemeCat::*;
    use self::PairResult::*;
    match (before, after) {
        (GC_CR, GC_LF) => NotBreak,  // GB3
        (GC_Control, _) => Break,  // GB4
        (GC_CR, _) => Break,  // GB4
        (GC_LF, _) => Break,  // GB4
        (_, GC_Control) => Break,  // GB5
        (_, GC_CR) => Break,  // GB5
        (_, GC_LF) => Break,  // GB5
        (GC_L, GC_L) => NotBreak,  // GB6
        (GC_L, GC_V) => NotBreak,  // GB6
        (GC_L, GC_LV) => NotBreak,  // GB6
        (GC_L, GC_LVT) => NotBreak,  // GB6
        (GC_LV, GC_V) => NotBreak,  // GB7
        (GC_LV, GC_T) => NotBreak,  // GB7
        (GC_V, GC_V) => NotBreak,  // GB7
        (GC_V, GC_T) => NotBreak,  // GB7
        (GC_LVT, GC_T) => NotBreak,  // GB8
        (GC_T, GC_T) => NotBreak,  // GB8
        (_, GC_Extend) => NotBreak, // GB9
        (_, GC_ZWJ) => NotBreak,  // GB9
        (_, GC_SpacingMark) => Extended,  // GB9a
        (GC_Prepend, _) => Extended,  // GB9b
        (GC_E_Base, GC_E_Modifier) => NotBreak,  // GB10
        (GC_E_Base_GAZ, GC_E_Modifier) => NotBreak,  // GB10
        (GC_Extend, GC_E_Modifier) => Emoji,  // GB10
        (GC_ZWJ, GC_Glue_After_Zwj) => NotBreak,  // GB11
        (GC_ZWJ, GC_E_Base_GAZ) => NotBreak,  // GB11
        (GC_Regional_Indicator, GC_Regional_Indicator) => Regional,  // GB12, GB13
        (_, _) => Break,  // GB999
    }
}

impl GraphemeCursor {
    /// Create a new cursor. The string and initial offset are given at creation
    /// time, but the contents of the string are not. The `is_extended` parameter
    /// controls whether extended grapheme clusters are selected.
    ///
    /// The `offset` parameter must be on a codepoint boundary.
    ///
    /// ```rust
    /// # use unicode_segmentation::GraphemeCursor;
    /// let s = "हिन्दी";
    /// let mut legacy = GraphemeCursor::new(0, s.len(), false);
    /// assert_eq!(legacy.next_boundary(s, 0), Ok(Some("ह".len())));
    /// let mut extended = GraphemeCursor::new(0, s.len(), true);
    /// assert_eq!(extended.next_boundary(s, 0), Ok(Some("हि".len())));
    /// ```
    pub fn new(offset: usize, len: usize, is_extended: bool) -> GraphemeCursor {
        let state = if offset == 0 || offset == len {
            GraphemeState::Break
        } else {
            GraphemeState::Unknown
        };
        GraphemeCursor {
            offset: offset,
            len: len,
            state: state,
            is_extended: is_extended,
            cat_before: None,
            cat_after: None,
            pre_context_offset: None,
            ris_count: None,
            resuming: false,
        }
    }

    // Not sure I'm gonna keep this, the advantage over new() seems thin.

    /// Set the cursor to a new location in the same string.
    ///
    /// ```rust
    /// # use unicode_segmentation::GraphemeCursor;
    /// let s = "abcd";
    /// let mut cursor = GraphemeCursor::new(0, s.len(), false);
    /// assert_eq!(cursor.cur_cursor(), 0);
    /// cursor.set_cursor(2);
    /// assert_eq!(cursor.cur_cursor(), 2);
    /// ```
    pub fn set_cursor(&mut self, offset: usize) {
        if offset != self.offset {
            self.offset = offset;
            self.state = if offset == 0 || offset == self.len {
                GraphemeState::Break
            } else {
                GraphemeState::Unknown
            };
            // reset state derived from text around cursor
            self.cat_before = None;
            self.cat_after = None;
            self.ris_count = None;
        }
    }

    /// The current offset of the cursor. Equal to the last value provided to
    /// `new()` or `set_cursor()`, or returned from `next_boundary()` or
    /// `prev_boundary()`.
    ///
    /// ```rust
    /// # use unicode_segmentation::GraphemeCursor;
    /// // Two flags (🇷🇸🇮🇴), each flag is two RIS codepoints, each RIS is 4 bytes.
    /// let flags = "\u{1F1F7}\u{1F1F8}\u{1F1EE}\u{1F1F4}";
    /// let mut cursor = GraphemeCursor::new(4, flags.len(), false);
    /// assert_eq!(cursor.cur_cursor(), 4);
    /// assert_eq!(cursor.next_boundary(flags, 0), Ok(Some(8)));
    /// assert_eq!(cursor.cur_cursor(), 8);
    /// ```
    pub fn cur_cursor(&self) -> usize {
        self.offset
    }

    /// Provide additional pre-context when it is needed to decide a boundary.
    /// The end of the chunk must coincide with the value given in the
    /// `GraphemeIncomplete::PreContext` request.
    ///
    /// ```rust
    /// # use unicode_segmentation::{GraphemeCursor, GraphemeIncomplete};
    /// let flags = "\u{1F1F7}\u{1F1F8}\u{1F1EE}\u{1F1F4}";
    /// let mut cursor = GraphemeCursor::new(8, flags.len(), false);
    /// // Not enough pre-context to decide if there's a boundary between the two flags.
    /// assert_eq!(cursor.is_boundary(&flags[8..], 8), Err(GraphemeIncomplete::PreContext(8)));
    /// // Provide one more Regional Indicator Symbol of pre-context
    /// cursor.provide_context(&flags[4..8], 4);
    /// // Still not enough context to decide.
    /// assert_eq!(cursor.is_boundary(&flags[8..], 8), Err(GraphemeIncomplete::PreContext(4)));
    /// // Provide additional requested context.
    /// cursor.provide_context(&flags[0..4], 0);
    /// // That's enough to decide (it always is when context goes to the start of the string)
    /// assert_eq!(cursor.is_boundary(&flags[8..], 8), Ok(true));
    /// ```
    pub fn provide_context(&mut self, chunk: &str, chunk_start: usize) {
        use tables::grapheme as gr;
        assert!(chunk_start + chunk.len() == self.pre_context_offset.unwrap());
        self.pre_context_offset = None;
        if self.is_extended && chunk_start + chunk.len() == self.offset {
            let ch = chunk.chars().rev().next().unwrap();
            if gr::grapheme_category(ch) == gr::GC_Prepend {
                self.decide(false);  // GB9b
                return;
            }
        }
        match self.state {
            GraphemeState::Regional => self.handle_regional(chunk, chunk_start),
            GraphemeState::Emoji => self.handle_emoji(chunk, chunk_start),
            _ => if self.cat_before.is_none() && self.offset == chunk.len() + chunk_start {
                let ch = chunk.chars().rev().next().unwrap();
                self.cat_before = Some(gr::grapheme_category(ch));
            },
        }
    }

    fn decide(&mut self, is_break: bool) {
        self.state = if is_break {
            GraphemeState::Break
        } else {
            GraphemeState::NotBreak
        };
    }

    fn decision(&mut self, is_break: bool) -> Result<bool, GraphemeIncomplete> {
        self.decide(is_break);
        Ok(is_break)
    }

    fn is_boundary_result(&self) -> Result<bool, GraphemeIncomplete> {
        if self.state == GraphemeState::Break {
            Ok(true)
        } else if self.state == GraphemeState::NotBreak {
            Ok(false)
        } else if let Some(pre_context_offset) = self.pre_context_offset {
            Err(GraphemeIncomplete::PreContext(pre_context_offset))
        } else {
            unreachable!("inconsistent state");
        }
    }

    fn handle_regional(&mut self, chunk: &str, chunk_start: usize) {
        use tables::grapheme as gr;
        let mut ris_count = self.ris_count.unwrap_or(0);
        for ch in chunk.chars().rev() {
            if gr::grapheme_category(ch) != gr::GC_Regional_Indicator {
                self.ris_count = Some(ris_count);
                self.decide((ris_count % 2) == 0);
                return;
            }
            ris_count += 1;
        }
        self.ris_count = Some(ris_count);
        if chunk_start == 0 {
            self.decide((ris_count % 2) == 0);
            return;
        }
        self.pre_context_offset = Some(chunk_start);
        self.state = GraphemeState::Regional;
    }

    fn handle_emoji(&mut self, chunk: &str, chunk_start: usize) {
        use tables::grapheme as gr;
        for ch in chunk.chars().rev() {
            match gr::grapheme_category(ch) {
                gr::GC_Extend => (),
                gr::GC_E_Base | gr::GC_E_Base_GAZ => {
                    self.decide(false);
                    return;
                }
                _ => {
                    self.decide(true);
                    return;
                }
            }
        }
        if chunk_start == 0 {
            self.decide(true);
            return;
        }
        self.pre_context_offset = Some(chunk_start);
        self.state = GraphemeState::Emoji;
    }

    /// Determine whether the current cursor location is a grapheme cluster boundary.
    /// Only a part of the string need be supplied. If `chunk_start` is nonzero or
    /// the length of `chunk` is not equal to `len` on creation, then this method
    /// may return `GraphemeIncomplete::PreContext`. The caller should then
    /// call `provide_context` with the requested chunk, then retry calling this
    /// method.
    ///
    /// For partial chunks, if the cursor is not at the beginning or end of the
    /// string, the chunk should contain at least the codepoint following the cursor.
    /// If the string is nonempty, the chunk must be nonempty.
    ///
    /// All calls should have consistent chunk contents (ie, if a chunk provides
    /// content for a given slice, all further chunks covering that slice must have
    /// the same content for it).
    ///
    /// ```rust
    /// # use unicode_segmentation::GraphemeCursor;
    /// let flags = "\u{1F1F7}\u{1F1F8}\u{1F1EE}\u{1F1F4}";
    /// let mut cursor = GraphemeCursor::new(8, flags.len(), false);
    /// assert_eq!(cursor.is_boundary(flags, 0), Ok(true));
    /// cursor.set_cursor(12);
    /// assert_eq!(cursor.is_boundary(flags, 0), Ok(false));
    /// ```
    pub fn is_boundary(&mut self, chunk: &str, chunk_start: usize) -> Result<bool, GraphemeIncomplete> {
        use tables::grapheme as gr;
        if self.state == GraphemeState::Break {
            return Ok(true)
        }
        if self.state == GraphemeState::NotBreak {
            return Ok(false)
        }
        if self.offset < chunk_start || self.offset >= chunk_start + chunk.len() {
            if self.offset > chunk_start + chunk.len() || self.cat_after.is_none() {
                return Err(GraphemeIncomplete::InvalidOffset)
            }
        }
        if let Some(pre_context_offset) = self.pre_context_offset {
            return Err(GraphemeIncomplete::PreContext(pre_context_offset));
        }
        let offset_in_chunk = self.offset - chunk_start;
        if self.cat_after.is_none() {
            let ch = chunk[offset_in_chunk..].chars().next().unwrap();
            self.cat_after = Some(gr::grapheme_category(ch));
        }
        if self.offset == chunk_start {
            let mut need_pre_context = true;
            match self.cat_after.unwrap() {
                gr::GC_Regional_Indicator => self.state = GraphemeState::Regional,
                gr::GC_E_Modifier => self.state = GraphemeState::Emoji,
                _ => need_pre_context = self.cat_before.is_none(),
            }
            if need_pre_context {
                self.pre_context_offset = Some(chunk_start);
                return Err(GraphemeIncomplete::PreContext(chunk_start));
            }
        }
        if self.cat_before.is_none() {
            let ch = chunk[..offset_in_chunk].chars().rev().next().unwrap();
            self.cat_before = Some(gr::grapheme_category(ch));
        }
        match check_pair(self.cat_before.unwrap(), self.cat_after.unwrap()) {
            PairResult::NotBreak => return self.decision(false),
            PairResult::Break => return self.decision(true),
            PairResult::Extended => {
                let is_extended = self.is_extended;
                return self.decision(!is_extended);
            }
            PairResult::Regional => {
                if let Some(ris_count) = self.ris_count {
                    return self.decision((ris_count % 2) == 0);
                }
                self.handle_regional(&chunk[..offset_in_chunk], chunk_start);
                self.is_boundary_result()
            }
            PairResult::Emoji => {
                self.handle_emoji(&chunk[..offset_in_chunk], chunk_start);
                self.is_boundary_result()
            }
        }
    }

    /// Find the next boundary after the current cursor position. Only a part of
    /// the string need be supplied. If the chunk is incomplete, then this
    /// method might return `GraphemeIncomplete::PreContext` or
    /// `GraphemeIncomplete::NextChunk`. In the former case, the caller should
    /// call `provide_context` with the requested chunk, then retry. In the
    /// latter case, the caller should provide the chunk following the one
    /// given, then retry.
    ///
    /// See `is_boundary` for expectations on the provided chunk.
    ///
    /// ```rust
    /// # use unicode_segmentation::GraphemeCursor;
    /// let flags = "\u{1F1F7}\u{1F1F8}\u{1F1EE}\u{1F1F4}";
    /// let mut cursor = GraphemeCursor::new(4, flags.len(), false);
    /// assert_eq!(cursor.next_boundary(flags, 0), Ok(Some(8)));
    /// assert_eq!(cursor.next_boundary(flags, 0), Ok(Some(16)));
    /// assert_eq!(cursor.next_boundary(flags, 0), Ok(None));
    /// ```
    ///
    /// And an example that uses partial strings:
    ///
    /// ```rust
    /// # use unicode_segmentation::{GraphemeCursor, GraphemeIncomplete};
    /// let s = "abcd";
    /// let mut cursor = GraphemeCursor::new(0, s.len(), false);
    /// assert_eq!(cursor.next_boundary(&s[..2], 0), Ok(Some(1)));
    /// assert_eq!(cursor.next_boundary(&s[..2], 0), Err(GraphemeIncomplete::NextChunk));
    /// assert_eq!(cursor.next_boundary(&s[2..4], 2), Ok(Some(2)));
    /// assert_eq!(cursor.next_boundary(&s[2..4], 2), Ok(Some(3)));
    /// assert_eq!(cursor.next_boundary(&s[2..4], 2), Ok(Some(4)));
    /// assert_eq!(cursor.next_boundary(&s[2..4], 2), Ok(None));
    /// ```
    pub fn next_boundary(&mut self, chunk: &str, chunk_start: usize) -> Result<Option<usize>, GraphemeIncomplete> {
        use tables::grapheme as gr;
        if self.offset == self.len {
            return Ok(None);
        }
        let mut iter = chunk[self.offset - chunk_start..].chars();
        let mut ch = iter.next().unwrap();
        loop {
            if self.resuming {
                if self.cat_after.is_none() {
                    self.cat_after = Some(gr::grapheme_category(ch));
                }
            } else {
                self.offset += ch.len_utf8();
                self.state = GraphemeState::Unknown;
                self.cat_before = self.cat_after.take();
                if self.cat_before.is_none() {
                    self.cat_before = Some(gr::grapheme_category(ch));
                }
                if self.cat_before.unwrap() == GraphemeCat::GC_Regional_Indicator {
                    self.ris_count = self.ris_count.map(|c| c + 1);
                } else {
                    self.ris_count = Some(0);
                }
                if let Some(next_ch) = iter.next() {
                    ch = next_ch;
                    self.cat_after = Some(gr::grapheme_category(ch));
                } else if self.offset == self.len {
                    self.decide(true);
                } else {
                    self.resuming = true;
                    return Err(GraphemeIncomplete::NextChunk);
                }
            }
            self.resuming = true;
            if self.is_boundary(chunk, chunk_start)? {
                self.resuming = false;
                return Ok(Some(self.offset));
            }
            self.resuming = false;
        }
    }

    /// Find the previous boundary after the current cursor position. Only a part
    /// of the string need be supplied. If the chunk is incomplete, then this
    /// method might return `GraphemeIncomplete::PreContext` or
    /// `GraphemeIncomplete::PrevChunk`. In the former case, the caller should
    /// call `provide_context` with the requested chunk, then retry. In the
    /// latter case, the caller should provide the chunk preceding the one
    /// given, then retry.
    ///
    /// See `is_boundary` for expectations on the provided chunk.
    ///
    /// ```rust
    /// # use unicode_segmentation::GraphemeCursor;
    /// let flags = "\u{1F1F7}\u{1F1F8}\u{1F1EE}\u{1F1F4}";
    /// let mut cursor = GraphemeCursor::new(12, flags.len(), false);
    /// assert_eq!(cursor.prev_boundary(flags, 0), Ok(Some(8)));
    /// assert_eq!(cursor.prev_boundary(flags, 0), Ok(Some(0)));
    /// assert_eq!(cursor.prev_boundary(flags, 0), Ok(None));
    /// ```
    ///
    /// And an example that uses partial strings (note the exact return is not
    /// guaranteed, and may be `PrevChunk` or `PreContext` arbitrarily):
    ///
    /// ```rust
    /// # use unicode_segmentation::{GraphemeCursor, GraphemeIncomplete};
    /// let s = "abcd";
    /// let mut cursor = GraphemeCursor::new(4, s.len(), false);
    /// assert_eq!(cursor.prev_boundary(&s[2..4], 2), Ok(Some(3)));
    /// assert_eq!(cursor.prev_boundary(&s[2..4], 2), Err(GraphemeIncomplete::PrevChunk));
    /// assert_eq!(cursor.prev_boundary(&s[0..2], 0), Ok(Some(2)));
    /// assert_eq!(cursor.prev_boundary(&s[0..2], 0), Ok(Some(1)));
    /// assert_eq!(cursor.prev_boundary(&s[0..2], 0), Ok(Some(0)));
    /// assert_eq!(cursor.prev_boundary(&s[0..2], 0), Ok(None));
    /// ```
    pub fn prev_boundary(&mut self, chunk: &str, chunk_start: usize) -> Result<Option<usize>, GraphemeIncomplete> {
        use tables::grapheme as gr;
        if self.offset == 0 {
            return Ok(None);
        }
        if self.offset == chunk_start {
            return Err(GraphemeIncomplete::PrevChunk);
        }
        let mut iter = chunk[..self.offset - chunk_start].chars().rev();
        let mut ch = iter.next().unwrap();
        loop {
            if self.offset == chunk_start {
                self.resuming = true;
                return Err(GraphemeIncomplete::PrevChunk);
            }
            if self.resuming {
                self.cat_before = Some(gr::grapheme_category(ch));
            } else {
                self.offset -= ch.len_utf8();
                self.cat_after = self.cat_before.take();
                self.state = GraphemeState::Unknown;
                if let Some(ris_count) = self.ris_count {
                    self.ris_count = if ris_count > 0 { Some(ris_count - 1) } else { None };
                }
                if let Some(prev_ch) = iter.next() {
                    ch = prev_ch;
                    self.cat_before = Some(gr::grapheme_category(ch));
                } else if self.offset == 0 {
                    self.decide(true);
                } else {
                    self.resuming = true;
                    self.cat_after = Some(gr::grapheme_category(ch));
                    return Err(GraphemeIncomplete::PrevChunk);
                }
            }
            self.resuming = true;
            if self.is_boundary(chunk, chunk_start)? {
                self.resuming = false;
                return Ok(Some(self.offset));
            }
            self.resuming = false;
        }
    }
}

#[test]
fn test_grapheme_cursor_ris_precontext() {
    let s = "\u{1f1fa}\u{1f1f8}\u{1f1fa}\u{1f1f8}\u{1f1fa}\u{1f1f8}";
    let mut c = GraphemeCursor::new(8, s.len(), true);
    assert_eq!(c.is_boundary(&s[4..], 4), Err(GraphemeIncomplete::PreContext(4)));
    c.provide_context(&s[..4], 0);
    assert_eq!(c.is_boundary(&s[4..], 4), Ok(true));
}

#[test]
fn test_grapheme_cursor_chunk_start_require_precontext() {
    let s = "\r\n";
    let mut c = GraphemeCursor::new(1, s.len(), true);
    assert_eq!(c.is_boundary(&s[1..], 1), Err(GraphemeIncomplete::PreContext(1)));
    c.provide_context(&s[..1], 0);
    assert_eq!(c.is_boundary(&s[1..], 1), Ok(false));
}

#[test]
fn test_grapheme_cursor_prev_boundary() {
    let s = "abcd";
    let mut c = GraphemeCursor::new(3, s.len(), true);
    assert_eq!(c.prev_boundary(&s[2..], 2), Err(GraphemeIncomplete::PrevChunk));
    assert_eq!(c.prev_boundary(&s[..2], 0), Ok(Some(2)));
}

#[test]
fn test_grapheme_cursor_prev_boundary_chunk_start() {
    let s = "abcd";
    let mut c = GraphemeCursor::new(2, s.len(), true);
    assert_eq!(c.prev_boundary(&s[2..], 2), Err(GraphemeIncomplete::PrevChunk));
    assert_eq!(c.prev_boundary(&s[..2], 0), Ok(Some(1)));
}
