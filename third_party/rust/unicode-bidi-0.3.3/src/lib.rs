// Copyright 2015 The Servo Project Developers. See the
// COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! This crate implements the [Unicode Bidirectional Algorithm][tr9] for display of mixed
//! right-to-left and left-to-right text.  It is written in safe Rust, compatible with the
//! current stable release.
//!
//! ## Example
//!
//! ```rust
//! use unicode_bidi::BidiInfo;
//!
//! // This example text is defined using `concat!` because some browsers
//! // and text editors have trouble displaying bidi strings.
//! let text = concat![
//!   "א",
//!   "ב",
//!   "ג",
//!   "a",
//!   "b",
//!   "c",
//! ];
//!
//! // Resolve embedding levels within the text.  Pass `None` to detect the
//! // paragraph level automatically.
//! let bidi_info = BidiInfo::new(&text, None);
//!
//! // This paragraph has embedding level 1 because its first strong character is RTL.
//! assert_eq!(bidi_info.paragraphs.len(), 1);
//! let para = &bidi_info.paragraphs[0];
//! assert_eq!(para.level.number(), 1);
//! assert_eq!(para.level.is_rtl(), true);
//!
//! // Re-ordering is done after wrapping each paragraph into a sequence of
//! // lines. For this example, I'll just use a single line that spans the
//! // entire paragraph.
//! let line = para.range.clone();
//!
//! let display = bidi_info.reorder_line(para, line);
//! assert_eq!(display, concat![
//!   "a",
//!   "b",
//!   "c",
//!   "ג",
//!   "ב",
//!   "א",
//! ]);
//! ```
//!
//! [tr9]: http://www.unicode.org/reports/tr9/

#![forbid(unsafe_code)]

#[macro_use]
extern crate matches;

#[cfg(feature = "with_serde")]
#[macro_use]
extern crate serde_derive;

#[cfg(all(feature = "with_serde", test))]
extern crate serde_test;

pub mod deprecated;
pub mod format_chars;
pub mod level;

mod char_data;
mod explicit;
mod implicit;
mod prepare;

pub use char_data::{BidiClass, bidi_class, UNICODE_VERSION};
pub use level::Level;
pub use prepare::LevelRun;

use std::borrow::Cow;
use std::cmp::{max, min};
use std::iter::repeat;
use std::ops::Range;

use BidiClass::*;
use format_chars as chars;


/// Bidi information about a single paragraph
#[derive(Debug, PartialEq)]
pub struct ParagraphInfo {
    /// The paragraphs boundaries within the text, as byte indices.
    ///
    /// TODO: Shrink this to only include the starting index?
    pub range: Range<usize>,

    /// The paragraph embedding level.
    ///
    /// http://www.unicode.org/reports/tr9/#BD4
    pub level: Level,
}

/// Initial bidi information of the text
///
/// Contains the paragraphs and BidiClasses in a string of text.
#[derive(PartialEq, Debug)]
pub struct InitialInfo<'text> {
    /// The text
    pub text: &'text str,

    /// The BidiClass of the character at each byte in the text.
    /// If a character is multiple bytes, its class will appear multiple times in the vector.
    pub original_classes: Vec<BidiClass>,

    /// The boundaries and level of each paragraph within the text.
    pub paragraphs: Vec<ParagraphInfo>,
}

impl<'text> InitialInfo<'text> {
    /// Find the paragraphs and BidiClasses in a string of text.
    ///
    /// http://www.unicode.org/reports/tr9/#The_Paragraph_Level
    ///
    /// Also sets the class for each First Strong Isolate initiator (FSI) to LRI or RLI if a strong
    /// character is found before the matching PDI.  If no strong character is found, the class will
    /// remain FSI, and it's up to later stages to treat these as LRI when needed.
    pub fn new(text: &str, default_para_level: Option<Level>) -> InitialInfo {
        let mut original_classes = Vec::with_capacity(text.len());

        // The stack contains the starting byte index for each nested isolate we're inside.
        let mut isolate_stack = Vec::new();
        let mut paragraphs = Vec::new();

        let mut para_start = 0;
        let mut para_level = default_para_level;

        for (i, c) in text.char_indices() {
            let class = bidi_class(c);
            original_classes.extend(repeat(class).take(c.len_utf8()));
            match class {
                B => {
                    // P1. Split the text into separate paragraphs. The paragraph separator is kept
                    // with the previous paragraph.
                    let para_end = i + c.len_utf8();
                    paragraphs.push(
                        ParagraphInfo {
                            range: para_start..para_end,
                            // P3. If no character is found in p2, set the paragraph level to zero.
                            level: para_level.unwrap_or(Level::ltr()),
                        }
                    );
                    // Reset state for the start of the next paragraph.
                    para_start = para_end;
                    // TODO: Support defaulting to direction of previous paragraph
                    //
                    // http://www.unicode.org/reports/tr9/#HL1
                    para_level = default_para_level;
                    isolate_stack.clear();
                }
                L | R | AL => {
                    match isolate_stack.last() {
                        Some(&start) => {
                            if original_classes[start] == FSI {
                                // X5c. If the first strong character between FSI and its matching
                                // PDI is R or AL, treat it as RLI. Otherwise, treat it as LRI.
                                for j in 0..chars::FSI.len_utf8() {
                                    original_classes[start + j] =
                                        if class == L { LRI } else { RLI };
                                }
                            }
                        }
                        None => {
                            if para_level.is_none() {
                                // P2. Find the first character of type L, AL, or R, while skipping
                                // any characters between an isolate initiator and its matching
                                // PDI.
                                para_level = Some(
                                    if class != L {
                                        Level::rtl()
                                    } else {
                                        Level::ltr()
                                    }
                                );
                            }
                        }
                    }
                }
                RLI | LRI | FSI => {
                    isolate_stack.push(i);
                }
                PDI => {
                    isolate_stack.pop();
                }
                _ => {}
            }
        }
        if para_start < text.len() {
            paragraphs.push(
                ParagraphInfo {
                    range: para_start..text.len(),
                    level: para_level.unwrap_or(Level::ltr()),
                }
            );
        }
        assert!(original_classes.len() == text.len());

        InitialInfo {
            text: text,
            original_classes: original_classes,
            paragraphs: paragraphs,
        }
    }
}

/// Bidi information of the text
///
/// The `original_classes` and `levels` vectors are indexed by byte offsets into the text.  If a
/// character is multiple bytes wide, then its class and level will appear multiple times in these
/// vectors.
// TODO: Impl `struct StringProperty<T> { values: Vec<T> }` and use instead of Vec<T>
#[derive(Debug, PartialEq)]
pub struct BidiInfo<'text> {
    /// The text
    pub text: &'text str,

    /// The BidiClass of the character at each byte in the text.
    pub original_classes: Vec<BidiClass>,

    /// The directional embedding level of each byte in the text.
    pub levels: Vec<Level>,

    /// The boundaries and paragraph embedding level of each paragraph within the text.
    ///
    /// TODO: Use SmallVec or similar to avoid overhead when there are only one or two paragraphs?
    /// Or just don't include the first paragraph, which always starts at 0?
    pub paragraphs: Vec<ParagraphInfo>,
}

impl<'text> BidiInfo<'text> {
    /// Split the text into paragraphs and determine the bidi embedding levels for each paragraph.
    ///
    /// TODO: In early steps, check for special cases that allow later steps to be skipped. like
    /// text that is entirely LTR.  See the `nsBidi` class from Gecko for comparison.
    ///
    /// TODO: Support auto-RTL base direction
    pub fn new(text: &str, default_para_level: Option<Level>) -> BidiInfo {
        let InitialInfo {
            text: _,
            original_classes,
            paragraphs,
        } = InitialInfo::new(text, default_para_level);

        let mut levels = Vec::<Level>::with_capacity(text.len());
        let mut processing_classes = original_classes.clone();

        for para in &paragraphs {
            let text = &text[para.range.clone()];
            let original_classes = &original_classes[para.range.clone()];
            let processing_classes = &mut processing_classes[para.range.clone()];

            let new_len = levels.len() + para.range.len();
            levels.resize(new_len, para.level);
            let levels = &mut levels[para.range.clone()];

            explicit::compute(
                text,
                para.level,
                &original_classes,
                levels,
                processing_classes,
            );

            let sequences = prepare::isolating_run_sequences(para.level, &original_classes, levels);
            for sequence in &sequences {
                implicit::resolve_weak(sequence, processing_classes);
                implicit::resolve_neutral(sequence, levels, processing_classes);
            }
            implicit::resolve_levels(processing_classes, levels);

            assign_levels_to_removed_chars(para.level, &original_classes, levels);
        }

        BidiInfo {
            text: text,
            original_classes: original_classes,
            paragraphs: paragraphs,
            levels: levels,
        }
    }

    /// Re-order a line based on resolved levels and return only the embedding levels, one `Level`
    /// per *byte*.
    pub fn reordered_levels(&self, para: &ParagraphInfo, line: Range<usize>) -> Vec<Level> {
        let (levels, _) = self.visual_runs(para, line.clone());
        levels
    }

    /// Re-order a line based on resolved levels and return only the embedding levels, one `Level`
    /// per *character*.
    pub fn reordered_levels_per_char(
        &self,
        para: &ParagraphInfo,
        line: Range<usize>,
    ) -> Vec<Level> {
        let levels = self.reordered_levels(para, line);
        self.text.char_indices().map(|(i, _)| levels[i]).collect()
    }


    /// Re-order a line based on resolved levels and return the line in display order.
    pub fn reorder_line(&self, para: &ParagraphInfo, line: Range<usize>) -> Cow<'text, str> {
        let (levels, runs) = self.visual_runs(para, line.clone());

        // If all isolating run sequences are LTR, no reordering is needed
        if runs.iter().all(|run| levels[run.start].is_ltr()) {
            return self.text[line.clone()].into();
        }

        let mut result = String::with_capacity(line.len());
        for run in runs {
            if levels[run.start].is_rtl() {
                result.extend(self.text[run].chars().rev());
            } else {
                result.push_str(&self.text[run]);
            }
        }
        result.into()
    }

    /// Find the level runs within a line and return them in visual order.
    ///
    /// `line` is a range of bytes indices within `levels`.
    ///
    /// http://www.unicode.org/reports/tr9/#Reordering_Resolved_Levels
    pub fn visual_runs(
        &self,
        para: &ParagraphInfo,
        line: Range<usize>,
    ) -> (Vec<Level>, Vec<LevelRun>) {
        assert!(line.start <= self.levels.len());
        assert!(line.end <= self.levels.len());

        let mut levels = self.levels.clone();

        // Reset some whitespace chars to paragraph level.
        // http://www.unicode.org/reports/tr9/#L1
        let line_str: &str = &self.text[line.clone()];
        let mut reset_from: Option<usize> = Some(0);
        let mut reset_to: Option<usize> = None;
        for (i, c) in line_str.char_indices() {
            match self.original_classes[i] {
                // Ignored by X9
                RLE | LRE | RLO | LRO | PDF | BN => {}
                // Segment separator, Paragraph separator
                B | S => {
                    assert!(reset_to == None);
                    reset_to = Some(i + c.len_utf8());
                    if reset_from == None {
                        reset_from = Some(i);
                    }
                }
                // Whitespace, isolate formatting
                WS | FSI | LRI | RLI | PDI => {
                    if reset_from == None {
                        reset_from = Some(i);
                    }
                }
                _ => {
                    reset_from = None;
                }
            }
            if let (Some(from), Some(to)) = (reset_from, reset_to) {
                for j in from..to {
                    levels[j] = para.level;
                }
                reset_from = None;
                reset_to = None;
            }
        }
        if let Some(from) = reset_from {
            for j in from..line_str.len() {
                levels[j] = para.level;
            }
        }

        // Find consecutive level runs.
        let mut runs = Vec::new();
        let mut start = line.start;
        let mut level = levels[start];
        let mut min_level = level;
        let mut max_level = level;

        for i in (start + 1)..line.end {
            let new_level = levels[i];
            if new_level != level {
                // End of the previous run, start of a new one.
                runs.push(start..i);
                start = i;
                level = new_level;

                min_level = min(level, min_level);
                max_level = max(level, max_level);
            }
        }
        runs.push(start..line.end);

        let run_count = runs.len();

        // Re-order the odd runs.
        // http://www.unicode.org/reports/tr9/#L2

        // Stop at the lowest *odd* level.
        min_level = min_level.new_lowest_ge_rtl().expect("Level error");

        while max_level >= min_level {
            // Look for the start of a sequence of consecutive runs of max_level or higher.
            let mut seq_start = 0;
            while seq_start < run_count {
                if self.levels[runs[seq_start].start] < max_level {
                    seq_start += 1;
                    continue;
                }

                // Found the start of a sequence. Now find the end.
                let mut seq_end = seq_start + 1;
                while seq_end < run_count {
                    if self.levels[runs[seq_end].start] < max_level {
                        break;
                    }
                    seq_end += 1;
                }

                // Reverse the runs within this sequence.
                runs[seq_start..seq_end].reverse();

                seq_start = seq_end;
            }
            max_level
                .lower(1)
                .expect("Lowering embedding level below zero");
        }

        (levels, runs)
    }

    /// If processed text has any computed RTL levels
    ///
    /// This information is usually used to skip re-ordering of text when no RTL level is present
    #[inline]
    pub fn has_rtl(&self) -> bool {
        level::has_rtl(&self.levels)
    }
}

/// Assign levels to characters removed by rule X9.
///
/// The levels assigned to these characters are not specified by the algorithm.  This function
/// assigns each one the level of the previous character, to avoid breaking level runs.
fn assign_levels_to_removed_chars(para_level: Level, classes: &[BidiClass], levels: &mut [Level]) {
    for i in 0..levels.len() {
        if prepare::removed_by_x9(classes[i]) {
            levels[i] = if i > 0 { levels[i - 1] } else { para_level };
        }
    }
}


#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_initial_text_info() {
        let text = "a1";
        assert_eq!(
            InitialInfo::new(text, None),
            InitialInfo {
                text: &text,
                original_classes: vec![L, EN],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..2,
                        level: Level::ltr(),
                    },
                ],
            }
        );

        let text = "غ א";
        assert_eq!(
            InitialInfo::new(text, None),
            InitialInfo {
                text: &text,
                original_classes: vec![AL, AL, WS, R, R],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..5,
                        level: Level::rtl(),
                    },
                ],
            }
        );

        let text = "a\u{2029}b";
        assert_eq!(
            InitialInfo::new(text, None),
            InitialInfo {
                text: &text,
                original_classes: vec![L, B, B, B, L],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..4,
                        level: Level::ltr(),
                    },
                    ParagraphInfo {
                        range: 4..5,
                        level: Level::ltr(),
                    },
                ],
            }
        );

        let text = format!("{}א{}a", chars::FSI, chars::PDI);
        assert_eq!(
            InitialInfo::new(&text, None),
            InitialInfo {
                text: &text,
                original_classes: vec![RLI, RLI, RLI, R, R, PDI, PDI, PDI, L],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..9,
                        level: Level::ltr(),
                    },
                ],
            }
        );
    }

    #[test]
    fn test_process_text() {
        let text = "abc123";
        assert_eq!(
            BidiInfo::new(text, Some(Level::ltr())),
            BidiInfo {
                text: &text,
                levels: Level::vec(&[0, 0, 0, 0, 0, 0]),
                original_classes: vec![L, L, L, EN, EN, EN],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..6,
                        level: Level::ltr(),
                    },
                ],
            }
        );

        let text = "abc אבג";
        assert_eq!(
            BidiInfo::new(text, Some(Level::ltr())),
            BidiInfo {
                text: &text,
                levels: Level::vec(&[0, 0, 0, 0, 1, 1, 1, 1, 1, 1]),
                original_classes: vec![L, L, L, WS, R, R, R, R, R, R],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..10,
                        level: Level::ltr(),
                    },
                ],
            }
        );
        assert_eq!(
            BidiInfo::new(text, Some(Level::rtl())),
            BidiInfo {
                text: &text,
                levels: Level::vec(&[2, 2, 2, 1, 1, 1, 1, 1, 1, 1]),
                original_classes: vec![L, L, L, WS, R, R, R, R, R, R],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..10,
                        level: Level::rtl(),
                    },
                ],
            }
        );

        let text = "אבג abc";
        assert_eq!(
            BidiInfo::new(text, Some(Level::ltr())),
            BidiInfo {
                text: &text,
                levels: Level::vec(&[1, 1, 1, 1, 1, 1, 0, 0, 0, 0]),
                original_classes: vec![R, R, R, R, R, R, WS, L, L, L],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..10,
                        level: Level::ltr(),
                    },
                ],
            }
        );
        assert_eq!(
            BidiInfo::new(text, None),
            BidiInfo {
                text: &text,
                levels: Level::vec(&[1, 1, 1, 1, 1, 1, 1, 2, 2, 2]),
                original_classes: vec![R, R, R, R, R, R, WS, L, L, L],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..10,
                        level: Level::rtl(),
                    },
                ],
            }
        );

        let text = "غ2ظ א2ג";
        assert_eq!(
            BidiInfo::new(text, Some(Level::ltr())),
            BidiInfo {
                text: &text,
                levels: Level::vec(&[1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1]),
                original_classes: vec![AL, AL, EN, AL, AL, WS, R, R, EN, R, R],
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..11,
                        level: Level::ltr(),
                    },
                ],
            }
        );

        let text = "a א.\nג";
        assert_eq!(
            BidiInfo::new(text, None),
            BidiInfo {
                text: &text,
                original_classes: vec![L, WS, R, R, CS, B, R, R],
                levels: Level::vec(&[0, 0, 1, 1, 0, 0, 1, 1]),
                paragraphs: vec![
                    ParagraphInfo {
                        range: 0..6,
                        level: Level::ltr(),
                    },
                    ParagraphInfo {
                        range: 6..8,
                        level: Level::rtl(),
                    },
                ],
            }
        );

        /// BidiTest:69635 (AL ET EN)
        let bidi_info = BidiInfo::new("\u{060B}\u{20CF}\u{06F9}", None);
        assert_eq!(bidi_info.original_classes, vec![AL, AL, ET, ET, ET, EN, EN]);
    }

    #[test]
    fn test_bidi_info_has_rtl() {
        // ASCII only
        assert_eq!(BidiInfo::new("123", None).has_rtl(), false);
        assert_eq!(BidiInfo::new("123", Some(Level::ltr())).has_rtl(), false);
        assert_eq!(BidiInfo::new("123", Some(Level::rtl())).has_rtl(), false);
        assert_eq!(BidiInfo::new("abc", None).has_rtl(), false);
        assert_eq!(BidiInfo::new("abc", Some(Level::ltr())).has_rtl(), false);
        assert_eq!(BidiInfo::new("abc", Some(Level::rtl())).has_rtl(), false);
        assert_eq!(BidiInfo::new("abc 123", None).has_rtl(), false);
        assert_eq!(BidiInfo::new("abc\n123", None).has_rtl(), false);

        // With Hebrew
        assert_eq!(BidiInfo::new("אבּג", None).has_rtl(), true);
        assert_eq!(
            BidiInfo::new("אבּג", Some(Level::ltr())).has_rtl(),
            true
        );
        assert_eq!(
            BidiInfo::new("אבּג", Some(Level::rtl())).has_rtl(),
            true
        );
        assert_eq!(BidiInfo::new("abc אבּג", None).has_rtl(), true);
        assert_eq!(BidiInfo::new("abc\nאבּג", None).has_rtl(), true);
        assert_eq!(BidiInfo::new("אבּג abc", None).has_rtl(), true);
        assert_eq!(BidiInfo::new("אבּג\nabc", None).has_rtl(), true);
        assert_eq!(BidiInfo::new("אבּג 123", None).has_rtl(), true);
        assert_eq!(BidiInfo::new("אבּג\n123", None).has_rtl(), true);
    }

    fn reorder_paras(text: &str) -> Vec<Cow<str>> {
        let bidi_info = BidiInfo::new(text, None);
        bidi_info
            .paragraphs
            .iter()
            .map(|para| bidi_info.reorder_line(para, para.range.clone()))
            .collect()
    }

    #[test]
    fn test_reorder_line() {
        /// Bidi_Class: L L L B L L L B L L L
        assert_eq!(
            reorder_paras("abc\ndef\nghi"),
            vec!["abc\n", "def\n", "ghi"]
        );

        /// Bidi_Class: L L EN B L L EN B L L EN
        assert_eq!(
            reorder_paras("ab1\nde2\ngh3"),
            vec!["ab1\n", "de2\n", "gh3"]
        );

        /// Bidi_Class: L L L B AL AL AL
        assert_eq!(reorder_paras("abc\nابج"), vec!["abc\n", "جبا"]);

        /// Bidi_Class: AL AL AL B L L L
        assert_eq!(reorder_paras("ابج\nabc"), vec!["\nجبا", "abc"]);

        assert_eq!(reorder_paras("1.-2"), vec!["1.-2"]);
        assert_eq!(reorder_paras("1-.2"), vec!["1-.2"]);
        assert_eq!(reorder_paras("abc אבג"), vec!["abc גבא"]);

        // Numbers being weak LTR characters, cannot reorder strong RTL
        assert_eq!(reorder_paras("123 אבג"), vec!["גבא 123"]);

        assert_eq!(reorder_paras("abc\u{202A}def"), vec!["abc\u{202A}def"]);

        assert_eq!(
            reorder_paras("abc\u{202A}def\u{202C}ghi"),
            vec!["abc\u{202A}def\u{202C}ghi"]
        );

        assert_eq!(
            reorder_paras("abc\u{2066}def\u{2069}ghi"),
            vec!["abc\u{2066}def\u{2069}ghi"]
        );

        // Testing for RLE Character
        assert_eq!(
            reorder_paras("\u{202B}abc אבג\u{202C}"),
            vec!["\u{202B}\u{202C}גבא abc"]
        );

        // Testing neutral characters
        assert_eq!(reorder_paras("אבג? אבג"), vec!["גבא ?גבא"]);

        // Testing neutral characters with special case
        assert_eq!(reorder_paras("A אבג?"), vec!["A גבא?"]);

        // Testing neutral characters with Implicit RTL Marker
        assert_eq!(
            reorder_paras("A אבג?\u{200F}"),
            vec!["A \u{200F}?גבא"]
        );
        assert_eq!(reorder_paras("אבג abc"), vec!["abc גבא"]);
        assert_eq!(
            reorder_paras("abc\u{2067}.-\u{2069}ghi"),
            vec!["abc\u{2067}-.\u{2069}ghi"]
        );

        assert_eq!(
            reorder_paras("Hello, \u{2068}\u{202E}world\u{202C}\u{2069}!"),
            vec!["Hello, \u{2068}\u{202E}\u{202C}dlrow\u{2069}!"]
        );

        // With mirrorable characters in RTL run
        assert_eq!(reorder_paras("א(ב)ג."), vec![".ג)ב(א"]);

        // With mirrorable characters on level boundry
        assert_eq!(
            reorder_paras("אב(גד[&ef].)gh"),
            vec!["ef].)gh&[דג(בא"]
        );
    }

    fn reordered_levels_for_paras(text: &str) -> Vec<Vec<Level>> {
        let bidi_info = BidiInfo::new(text, None);
        bidi_info
            .paragraphs
            .iter()
            .map(|para| bidi_info.reordered_levels(para, para.range.clone()))
            .collect()
    }

    fn reordered_levels_per_char_for_paras(text: &str) -> Vec<Vec<Level>> {
        let bidi_info = BidiInfo::new(text, None);
        bidi_info
            .paragraphs
            .iter()
            .map(|para| bidi_info.reordered_levels_per_char(para, para.range.clone()))
            .collect()
    }

    #[test]
    fn test_reordered_levels() {

        /// BidiTest:946 (LRI PDI)
        let text = "\u{2067}\u{2069}";
        assert_eq!(
            reordered_levels_for_paras(text),
            vec![Level::vec(&[0, 0, 0, 0, 0, 0])]
        );
        assert_eq!(
            reordered_levels_per_char_for_paras(text),
            vec![Level::vec(&[0, 0])]
        );

        /* TODO
        /// BidiTest:69635 (AL ET EN)
        let text = "\u{060B}\u{20CF}\u{06F9}";
        assert_eq!(
            reordered_levels_for_paras(text),
            vec![Level::vec(&[1, 1, 1, 1, 1, 2, 2])]
        );
        assert_eq!(
            reordered_levels_per_char_for_paras(text),
            vec![Level::vec(&[1, 1, 2])]
        );
         */

        /* TODO
        // BidiTest:291284 (AN RLI PDF R)
        assert_eq!(
            reordered_levels_per_char_for_paras("\u{0605}\u{2067}\u{202C}\u{0590}"),
            vec![&["2", "0", "x", "1"]]
        );
         */
    }
}


#[cfg(all(feature = "with_serde", test))]
mod serde_tests {
    use serde_test::{Token, assert_tokens};
    use super::*;

    #[test]
    fn test_levels() {
        let text = "abc אבג";
        let bidi_info = BidiInfo::new(text, None);
        let levels = bidi_info.levels;
        assert_eq!(text.as_bytes().len(), 10);
        assert_eq!(levels.len(), 10);
        assert_tokens(
            &levels,
            &[
                Token::Seq { len: Some(10) },
                Token::NewtypeStruct { name: "Level" },
                Token::U8(0),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(0),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(0),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(0),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(1),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(1),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(1),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(1),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(1),
                Token::NewtypeStruct { name: "Level" },
                Token::U8(1),
                Token::SeqEnd,
            ],
        );
    }
}
