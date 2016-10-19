// Copyright 2014 The html5ever Project Developers. See the
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
//! use unicode_bidi::{process_text, reorder_line};
//!
//! // This example text is defined using `concat!` because some browsers
//! // and text editors have trouble displaying bidi strings.
//! let text = concat!["א",
//!                    "ב",
//!                    "ג",
//!                    "a",
//!                    "b",
//!                    "c"];
//!
//! // Resolve embedding levels within the text.  Pass `None` to detect the
//! // paragraph level automatically.
//! let info = process_text(&text, None);
//!
//! // This paragraph has embedding level 1 because its first strong character is RTL.
//! assert_eq!(info.paragraphs.len(), 1);
//! let paragraph_info = &info.paragraphs[0];
//! assert_eq!(paragraph_info.level, 1);
//!
//! // Re-ordering is done after wrapping each paragraph into a sequence of
//! // lines. For this example, I'll just use a single line that spans the
//! // entire paragraph.
//! let line = paragraph_info.range.clone();
//!
//! let display = reorder_line(&text, line, &info.levels);
//! assert_eq!(display, concat!["a",
//!                             "b",
//!                             "c",
//!                             "ג",
//!                             "ב",
//!                             "א"]);
//! ```
//!
//! [tr9]: http://www.unicode.org/reports/tr9/

#![forbid(unsafe_code)]

#[macro_use] extern crate matches;

pub mod tables;

pub use tables::{BidiClass, bidi_class, UNICODE_VERSION};
use BidiClass::*;

use std::borrow::Cow;
use std::cmp::{max, min};
use std::iter::repeat;
use std::ops::Range;

/// Output of `process_text`
///
/// The `classes` and `levels` vectors are indexed by byte offsets into the text.  If a character
/// is multiple bytes wide, then its class and level will appear multiple times in these vectors.
#[derive(Debug, PartialEq)]
pub struct BidiInfo {
    /// The BidiClass of the character at each byte in the text.
    pub classes: Vec<BidiClass>,

    /// The directional embedding level of each byte in the text.
    pub levels: Vec<u8>,

    /// The boundaries and paragraph embedding level of each paragraph within the text.
    ///
    /// TODO: Use SmallVec or similar to avoid overhead when there are only one or two paragraphs?
    /// Or just don't include the first paragraph, which always starts at 0?
    pub paragraphs: Vec<ParagraphInfo>,
}

/// Info about a single paragraph 
#[derive(Debug, PartialEq)]
pub struct ParagraphInfo {
    /// The paragraphs boundaries within the text, as byte indices.
    ///
    /// TODO: Shrink this to only include the starting index?
    pub range: Range<usize>,

    /// The paragraph embedding level. http://www.unicode.org/reports/tr9/#BD4
    pub level: u8,
}

/// Determine the bidirectional embedding levels for a single paragraph.
///
/// TODO: In early steps, check for special cases that allow later steps to be skipped. like text
/// that is entirely LTR.  See the `nsBidi` class from Gecko for comparison.
pub fn process_text(text: &str, level: Option<u8>) -> BidiInfo {
    let InitialProperties { initial_classes, paragraphs } = initial_scan(text, level);

    let mut levels = Vec::with_capacity(text.len());
    let mut classes = initial_classes.clone();

    for para in &paragraphs {
        let text = &text[para.range.clone()];
        let classes = &mut classes[para.range.clone()];
        let initial_classes = &initial_classes[para.range.clone()];

        // FIXME: Use `levels.resize(...)` when it becomes stable.
        levels.extend(repeat(para.level).take(para.range.len()));
        let levels = &mut levels[para.range.clone()];

        explicit::compute(text, para.level, &initial_classes, levels, classes);

        let sequences = prepare::isolating_run_sequences(para.level, &initial_classes, levels);
        for sequence in &sequences {
            implicit::resolve_weak(sequence, classes);
            implicit::resolve_neutral(sequence, levels, classes);
        }
        implicit::resolve_levels(classes, levels);
        assign_levels_to_removed_chars(para.level, &initial_classes, levels);
    }

    BidiInfo {
        levels: levels,
        classes: initial_classes,
        paragraphs: paragraphs,
    }
}

#[inline]
/// Even embedding levels are left-to-right.
///
/// http://www.unicode.org/reports/tr9/#BD2
pub fn is_ltr(level: u8) -> bool { level % 2 == 0 }

/// Odd levels are right-to-left.
///
/// http://www.unicode.org/reports/tr9/#BD2
pub fn is_rtl(level: u8) -> bool { level % 2 == 1 }

/// Generate a character type based on a level (as specified in steps X10 and N2).
fn class_for_level(level: u8) -> BidiClass {
    if is_rtl(level) { R } else { L }
}

/// Re-order a line based on resolved levels.
///
/// `levels` are the embedding levels returned by `process_text`.
/// `line` is a range of bytes indices within `text`.
///
/// Returns the line in display order.
pub fn reorder_line<'a>(text: &'a str, line: Range<usize>, levels: &[u8])
    -> Cow<'a, str>
{
    let runs = visual_runs(line.clone(), &levels);
    if runs.len() == 1 && !is_rtl(levels[runs[0].start]) {
        return text.into()
    }
    let mut result = String::with_capacity(line.len());
    for run in runs {
        if is_rtl(levels[run.start]) {
            result.extend(text[run].chars().rev());
        } else {
            result.push_str(&text[run]);
        }
    }
    result.into()
}

/// A maximal substring of characters with the same embedding level.
///
/// Represented as a range of byte indices.
pub type LevelRun = Range<usize>;

/// Find the level runs within a line and return them in visual order.
///
/// `line` is a range of bytes indices within `levels`.
///
/// http://www.unicode.org/reports/tr9/#Reordering_Resolved_Levels
pub fn visual_runs(line: Range<usize>, levels: &[u8]) -> Vec<LevelRun> {
    assert!(line.start <= levels.len());
    assert!(line.end <= levels.len());

    // TODO: Whitespace handling.
    // http://www.unicode.org/reports/tr9/#L1

    let mut runs = Vec::new();

    // Find consecutive level runs.
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
    min_level |= 1;

    while max_level >= min_level {
        // Look for the start of a sequence of consecutive runs of max_level or higher.
        let mut seq_start = 0;
        while seq_start < run_count {
            if levels[runs[seq_start].start] < max_level {
                seq_start += 1;
                continue
            }

            // Found the start of a sequence. Now find the end.
            let mut seq_end = seq_start + 1;
            while seq_end < run_count {
                if levels[runs[seq_end].start] < max_level {
                    break
                }
                seq_end += 1;
            }

            // Reverse the runs within this sequence.
            runs[seq_start..seq_end].reverse();

            seq_start = seq_end;
        }
        max_level -= 1;
    }

    runs
}

/// Output of `initial_scan`
#[derive(PartialEq, Debug)]
pub struct InitialProperties {
    /// The BidiClass of the character at each byte in the text.
    /// If a character is multiple bytes, its class will appear multiple times in the vector.
    pub initial_classes: Vec<BidiClass>,

    /// The boundaries and level of each paragraph within the text.
    pub paragraphs: Vec<ParagraphInfo>,
}

/// Find the paragraphs and BidiClasses in a string of text.
///
/// http://www.unicode.org/reports/tr9/#The_Paragraph_Level
///
/// Also sets the class for each First Strong Isolate initiator (FSI) to LRI or RLI if a strong
/// character is found before the matching PDI.  If no strong character is found, the class will
/// remain FSI, and it's up to later stages to treat these as LRI when needed.
pub fn initial_scan(text: &str, default_para_level: Option<u8>) -> InitialProperties {
    let mut classes = Vec::with_capacity(text.len());

    // The stack contains the starting byte index for each nested isolate we're inside.
    let mut isolate_stack = Vec::new();
    let mut paragraphs = Vec::new();

    let mut para_start = 0;
    let mut para_level = default_para_level;

    const FSI_CHAR: char = '\u{2069}';

    for (i, c) in text.char_indices() {
        let class = bidi_class(c);
        classes.extend(repeat(class).take(c.len_utf8()));
        match class {
            B => {
                // P1. Split the text into separate paragraphs. The paragraph separator is kept
                // with the previous paragraph.
                let para_end = i + c.len_utf8();
                paragraphs.push(ParagraphInfo {
                    range: para_start..para_end,
                    // P3. If no character is found in p2, set the paragraph level to zero.
                    level: para_level.unwrap_or(0)
                });
                // Reset state for the start of the next paragraph.
                para_start = para_end;
                para_level = default_para_level;
                isolate_stack.clear();
            }
            L | R | AL => match isolate_stack.last() {
                Some(&start) => if classes[start] == FSI {
                    // X5c. If the first strong character between FSI and its matching PDI is R
                    // or AL, treat it as RLI. Otherwise, treat it as LRI.
                    for j in 0..FSI_CHAR.len_utf8() {
                        classes[start+j] = if class == L { LRI } else { RLI };
                    }
                },
                None => if para_level.is_none() {
                    // P2. Find the first character of type L, AL, or R, while skipping any
                    // characters between an isolate initiator and its matching PDI.
                    para_level = Some(if class == L { 0 } else { 1 });
                }
            },
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
        paragraphs.push(ParagraphInfo {
            range: para_start..text.len(),
            level: para_level.unwrap_or(0)
        });
    }
    assert!(classes.len() == text.len());

    InitialProperties {
        initial_classes: classes,
        paragraphs: paragraphs,
    }
}

/// Assign levels to characters removed by rule X9.
///
/// The levels assigned to these characters are not specified by the algorithm.  This function
/// assigns each one the level of the previous character, to avoid breaking level runs.
fn assign_levels_to_removed_chars(para_level: u8, classes: &[BidiClass], levels: &mut [u8]) {
    for i in 0..levels.len() {
        if prepare::removed_by_x9(classes[i]) {
            levels[i] = if i > 0 { levels[i-1] } else { para_level };
        }
    }
}

/// 3.3.2 Explicit Levels and Directions
///
/// http://www.unicode.org/reports/tr9/#Explicit_Levels_and_Directions
mod explicit {
    use super::{BidiClass, is_rtl};
    use super::BidiClass::*;

    /// Compute explicit embedding levels for one paragraph of text (X1-X8).
    ///
    /// `classes[i]` must contain the BidiClass of the char at byte index `i`,
    /// for each char in `text`.
    pub fn compute(text: &str, para_level: u8, initial_classes: &[BidiClass],
                   levels: &mut [u8], classes: &mut [BidiClass]) {
        assert!(text.len() == initial_classes.len());

        // http://www.unicode.org/reports/tr9/#X1
        let mut stack = DirectionalStatusStack::new();
        stack.push(para_level, OverrideStatus::Neutral);

        let mut overflow_isolate_count = 0u32;
        let mut overflow_embedding_count = 0u32;
        let mut valid_isolate_count = 0u32;

        for (i, c) in text.char_indices() {
            match initial_classes[i] {
                // Rules X2-X5c
                RLE | LRE | RLO | LRO | RLI | LRI | FSI => {
                    let is_rtl = match initial_classes[i] {
                        RLE | RLO | RLI => true,
                        _ => false
                    };

                    let last_level = stack.last().level;
                    let new_level = match is_rtl {
                        true  => next_rtl_level(last_level),
                        false => next_ltr_level(last_level)
                    };

                    // X5a-X5c: Isolate initiators get the level of the last entry on the stack.
                    let is_isolate = matches!(initial_classes[i], RLI | LRI | FSI);
                    if is_isolate {
                        levels[i] = last_level;
                        match stack.last().status {
                            OverrideStatus::RTL => classes[i] = R,
                            OverrideStatus::LTR => classes[i] = L,
                            _ => {}
                        }
                    }

                    if valid(new_level) && overflow_isolate_count == 0 && overflow_embedding_count == 0 {
                        stack.push(new_level, match initial_classes[i] {
                            RLO => OverrideStatus::RTL,
                            LRO => OverrideStatus::LTR,
                            RLI | LRI | FSI => OverrideStatus::Isolate,
                            _ => OverrideStatus::Neutral
                        });
                        if is_isolate {
                            valid_isolate_count += 1;
                        } else {
                            // The spec doesn't explicitly mention this step, but it is necessary.
                            // See the reference implementations for comparison.
                            levels[i] = new_level;
                        }
                    } else if is_isolate {
                        overflow_isolate_count += 1;
                    } else if overflow_isolate_count == 0 {
                        overflow_embedding_count += 1;
                    }
                }
                // http://www.unicode.org/reports/tr9/#X6a
                PDI => {
                    if overflow_isolate_count > 0 {
                        overflow_isolate_count -= 1;
                    } else if valid_isolate_count > 0 {
                        overflow_embedding_count = 0;
                        loop {
                            // Pop everything up to and including the last Isolate status.
                            match stack.vec.pop() {
                                Some(Status { status: OverrideStatus::Isolate, .. }) => break,
                                None => break,
                                _ => continue
                            }
                        }
                        valid_isolate_count -= 1;
                    }
                    let last = stack.last();
                    levels[i] = last.level;
                    match last.status {
                        OverrideStatus::RTL => classes[i] = R,
                        OverrideStatus::LTR => classes[i] = L,
                        _ => {}
                    }
                }
                // http://www.unicode.org/reports/tr9/#X7
                PDF => {
                    if overflow_isolate_count > 0 {
                        continue
                    }
                    if overflow_embedding_count > 0 {
                        overflow_embedding_count -= 1;
                        continue
                    }
                    if stack.last().status != OverrideStatus::Isolate && stack.vec.len() >= 2 {
                        stack.vec.pop();
                    }
                    // The spec doesn't explicitly mention this step, but it is necessary.
                    // See the reference implementations for comparison.
                    levels[i] = stack.last().level;
                }
                // http://www.unicode.org/reports/tr9/#X6
                B | BN => {}
                _ => {
                    let last = stack.last();
                    levels[i] = last.level;
                    match last.status {
                        OverrideStatus::RTL => classes[i] = R,
                        OverrideStatus::LTR => classes[i] = L,
                        _ => {}
                    }
                }
            }
            // Handle multi-byte characters.
            for j in 1..c.len_utf8() {
                levels[i+j] = levels[i];
                classes[i+j] = classes[i];
            }
        }
    }

    /// Maximum depth of the directional status stack.
    pub const MAX_DEPTH: u8 = 125;

    /// Levels from 0 through max_depth are valid at this stage.
    /// http://www.unicode.org/reports/tr9/#X1
    fn valid(level: u8) -> bool { level <= MAX_DEPTH }

    /// The next odd level greater than `level`.
    fn next_rtl_level(level: u8) -> u8 { (level + 1) |  1 }

    /// The next even level greater than `level`.
    fn next_ltr_level(level: u8) -> u8 { (level + 2) & !1 }

    /// Entries in the directional status stack:
    struct Status {
        level: u8,
        status: OverrideStatus,
    }

    #[derive(PartialEq)]
    enum OverrideStatus { Neutral, RTL, LTR, Isolate }

    struct DirectionalStatusStack {
        vec: Vec<Status>,
    }

    impl DirectionalStatusStack {
        fn new() -> Self {
            DirectionalStatusStack {
                vec: Vec::with_capacity(MAX_DEPTH as usize + 2)
            }
        }
        fn push(&mut self, level: u8, status: OverrideStatus) {
            self.vec.push(Status { level: level, status: status });
        }
        fn last(&self) -> &Status {
            self.vec.last().unwrap()
        }
    }
}

/// 3.3.3 Preparations for Implicit Processing
///
/// http://www.unicode.org/reports/tr9/#Preparations_for_Implicit_Processing
mod prepare {
    use super::{BidiClass, class_for_level, LevelRun};
    use super::BidiClass::*;
    use std::cmp::max;

    /// Output of `isolating_run_sequences` (steps X9-X10)
    pub struct IsolatingRunSequence {
        pub runs: Vec<LevelRun>,
        pub sos: BidiClass, // Start-of-sequence type.
        pub eos: BidiClass, // End-of-sequence type.
    }

    /// Compute the set of isolating run sequences.
    ///
    /// An isolating run sequence is a maximal sequence of level runs such that for all level runs
    /// except the last one in the sequence, the last character of the run is an isolate initiator
    /// whose matching PDI is the first character of the next level run in the sequence.
    ///
    /// Note: This function does *not* return the sequences in order by their first characters.
    pub fn isolating_run_sequences(para_level: u8, initial_classes: &[BidiClass], levels: &[u8])
        -> Vec<IsolatingRunSequence>
    {
        let runs = level_runs(levels, initial_classes);

        // Compute the set of isolating run sequences.
        // http://www.unicode.org/reports/tr9/#BD13

        let mut sequences = Vec::with_capacity(runs.len());

        // When we encounter an isolate initiator, we push the current sequence onto the
        // stack so we can resume it after the matching PDI.
        let mut stack = vec![Vec::new()];

        for run in runs {
            assert!(run.len() > 0);
            assert!(stack.len() > 0);

            let start_class = initial_classes[run.start];
            let end_class = initial_classes[run.end - 1];

            let mut sequence = if start_class == PDI && stack.len() > 1 {
                // Continue a previous sequence interrupted by an isolate.
                stack.pop().unwrap()
            } else {
                // Start a new sequence.
                Vec::new()
            };

            sequence.push(run);

            if matches!(end_class, RLI | LRI | FSI) {
                // Resume this sequence after the isolate.
                stack.push(sequence);
            } else {
                // This sequence is finished.
                sequences.push(sequence);
            }
        }
        // Pop any remaning sequences off the stack.
        sequences.extend(stack.into_iter().rev().filter(|seq| seq.len() > 0));

        // Determine the `sos` and `eos` class for each sequence.
        // http://www.unicode.org/reports/tr9/#X10
        return sequences.into_iter().map(|sequence| {
            assert!(!sequence.len() > 0);
            let start = sequence[0].start;
            let end = sequence[sequence.len() - 1].end;

            // Get the level inside these level runs.
            let level = levels[start];

            // Get the level of the last non-removed char before the runs.
            let pred_level = match initial_classes[..start].iter().rposition(not_removed_by_x9) {
                Some(idx) => levels[idx],
                None => para_level
            };

            // Get the level of the next non-removed char after the runs.
            let succ_level = if matches!(initial_classes[end - 1], RLI|LRI|FSI) {
                para_level
            } else {
                match initial_classes[end..].iter().position(not_removed_by_x9) {
                    Some(idx) => levels[idx],
                    None => para_level
                }
            };

            IsolatingRunSequence {
                runs: sequence,
                sos: class_for_level(max(level, pred_level)),
                eos: class_for_level(max(level, succ_level)),
            }
        }).collect()
    }

    /// Finds the level runs in a paragraph.
    ///
    /// http://www.unicode.org/reports/tr9/#BD7
    fn level_runs(levels: &[u8], classes: &[BidiClass]) -> Vec<LevelRun> {
        assert!(levels.len() == classes.len());

        let mut runs = Vec::new();
        if levels.len() == 0 {
            return runs
        }

        let mut current_run_level = levels[0];
        let mut current_run_start = 0;

        for i in 1..levels.len() {
            if !removed_by_x9(classes[i]) {
                if levels[i] != current_run_level {
                    // End the last run and start a new one.
                    runs.push(current_run_start..i);
                    current_run_level = levels[i];
                    current_run_start = i;
                }
            }
        }
        runs.push(current_run_start..levels.len());
        runs
    }

    /// Should this character be ignored in steps after X9?
    ///
    /// http://www.unicode.org/reports/tr9/#X9
    pub fn removed_by_x9(class: BidiClass) -> bool {
        matches!(class, RLE | LRE | RLO | LRO | PDF | BN)
    }

    // For use as a predicate for `position` / `rposition`
    pub fn not_removed_by_x9(class: &BidiClass) -> bool {
        !removed_by_x9(*class)
    }

    #[cfg(test)] #[test]
    fn test_level_runs() {
        assert_eq!(level_runs(&[0,0,0,1,1,2,0,0], &[L; 8]), &[0..3, 3..5, 5..6, 6..8]);
    }

    #[cfg(test)] #[test]
    fn test_isolating_run_sequences() {
        // Example 3 from http://www.unicode.org/reports/tr9/#BD13:

        //              0  1    2   3    4  5  6  7    8   9   10
        let classes = &[L, RLI, AL, LRI, L, R, L, PDI, AL, PDI, L];
        let levels =  &[0, 0,   1,  1,   2, 3, 2, 1,   1,  0,   0];
        let para_level = 0;

        let sequences = isolating_run_sequences(para_level, classes, levels);
        let runs: Vec<Vec<LevelRun>> = sequences.iter().map(|s| s.runs.clone()).collect();
        assert_eq!(runs, vec![vec![4..5], vec![5..6], vec![6..7], vec![2..4, 7..9], vec![0..2, 9..11]]);
    }
}

/// 3.3.4 - 3.3.6. Resolve implicit levels and types.
mod implicit {
    use super::{BidiClass, class_for_level, is_rtl, LevelRun};
    use super::BidiClass::*;
    use super::prepare::{IsolatingRunSequence, not_removed_by_x9, removed_by_x9};
    use std::cmp::max;

    /// 3.3.4 Resolving Weak Types
    ///
    /// http://www.unicode.org/reports/tr9/#Resolving_Weak_Types
    pub fn resolve_weak(sequence: &IsolatingRunSequence, classes: &mut [BidiClass]) {
        // FIXME (#8): This function applies steps W1-W6 in a single pass.  This can produce
        // incorrect results in cases where a "later" rule changes the value of `prev_class` seen
        // by an "earlier" rule.  We should either split this into separate passes, or preserve
        // extra state so each rule can see the correct previous class.

        let mut prev_class = sequence.sos;
        let mut last_strong_is_al = false;
        let mut et_run_indices = Vec::new(); // for W5

        // Like sequence.runs.iter().flat_map(Clone::clone), but make indices itself clonable.
        fn id(x: LevelRun) -> LevelRun { x }
        let mut indices = sequence.runs.iter().cloned().flat_map(id as fn(LevelRun) -> LevelRun);

        while let Some(i) = indices.next() {
            match classes[i] {
                // http://www.unicode.org/reports/tr9/#W1
                NSM => {
                    classes[i] = match prev_class {
                        RLI | LRI | FSI | PDI => ON,
                        _ => prev_class
                    };
                }
                EN => {
                    if last_strong_is_al {
                        // W2. If previous strong char was AL, change EN to AN.
                        classes[i] = AN;
                    } else {
                        // W5. If a run of ETs is adjacent to an EN, change the ETs to EN.
                        for j in &et_run_indices {
                            classes[*j] = EN;
                        }
                        et_run_indices.clear();
                    }
                }
                // http://www.unicode.org/reports/tr9/#W3
                AL => classes[i] = R,

                // http://www.unicode.org/reports/tr9/#W4
                ES | CS => {
                    let next_class = indices.clone().map(|j| classes[j]).filter(not_removed_by_x9)
                        .next().unwrap_or(sequence.eos);
                    classes[i] = match (prev_class, classes[i], next_class) {
                        (EN, ES, EN) |
                        (EN, CS, EN) => EN,
                        (AN, CS, AN) => AN,
                        (_,  _,  _ ) => ON,
                    }
                }
                // http://www.unicode.org/reports/tr9/#W5
                ET => {
                    match prev_class {
                        EN => classes[i] = EN,
                        _ => et_run_indices.push(i) // In case this is followed by an EN.
                    }
                }
                class => if removed_by_x9(class) {
                    continue
                }
            }

            prev_class = classes[i];
            match prev_class {
                L | R => { last_strong_is_al = false; }
                AL => { last_strong_is_al = true;  }
                _ => {}
            }
            if prev_class != ET {
                // W6. If we didn't find an adjacent EN, turn any ETs into ON instead.
                for j in &et_run_indices {
                    classes[*j] = ON;
                }
                et_run_indices.clear();
            }
        }

        // W7. If the previous strong char was L, change EN to L.
        let mut last_strong_is_l = sequence.sos == L;
        for run in &sequence.runs {
            for i in run.clone() {
                match classes[i] {
                    EN if last_strong_is_l => { classes[i] = L; }
                    L => { last_strong_is_l = true; }
                    R | AL => { last_strong_is_l = false; }
                    _ => {}
                }
            }
        }
    }

    /// 3.3.5 Resolving Neutral Types
    ///
    /// http://www.unicode.org/reports/tr9/#Resolving_Neutral_Types
    pub fn resolve_neutral(sequence: &IsolatingRunSequence, levels: &[u8],
                           classes: &mut [BidiClass])
    {
        let mut indices = sequence.runs.iter().flat_map(Clone::clone);
        let mut prev_class = sequence.sos;

        // Neutral or Isolate formatting characters (NI).
        // http://www.unicode.org/reports/tr9/#NI
        fn ni(class: BidiClass) -> bool {
            matches!(class, B | S | WS | ON | FSI | LRI | RLI | PDI)
        }

        while let Some(mut i) = indices.next() {
            // N0. Process bracket pairs.
            // TODO

            // Process sequences of NI characters.
            let mut ni_run = Vec::new();
            if ni(classes[i]) {
                // Consume a run of consecutive NI characters.
                ni_run.push(i);
                let mut next_class;
                loop {
                    match indices.next() {
                        Some(j) => {
                            i = j;
                            if removed_by_x9(classes[i]) {
                                continue
                            }
                            next_class = classes[j];
                            if ni(next_class) {
                                ni_run.push(i);
                            } else {
                                break
                            }
                        }
                        None => {
                            next_class = sequence.eos;
                            break
                        }
                    };
                }

                // N1-N2.
                let new_class = match (prev_class, next_class) {
                    (L,  L ) => L,
                    (R,  R ) |
                    (R,  AN) |
                    (R,  EN) |
                    (AN, R ) |
                    (AN, AN) |
                    (AN, EN) |
                    (EN, R ) |
                    (EN, AN) |
                    (EN, EN) => R,
                    (_,  _ ) => class_for_level(levels[i]),
                };
                for j in &ni_run {
                    classes[*j] = new_class;
                }
                ni_run.clear();
            }
            prev_class = classes[i];
        }
    }

    /// 3.3.6 Resolving Implicit Levels
    ///
    /// Returns the maximum embedding level in the paragraph.
    ///
    /// http://www.unicode.org/reports/tr9/#Resolving_Implicit_Levels
    pub fn resolve_levels(classes: &[BidiClass], levels: &mut [u8]) -> u8 {
        let mut max_level = 0;

        assert!(classes.len() == levels.len());
        for i in 0..levels.len() {
            match (is_rtl(levels[i]), classes[i]) {
                // http://www.unicode.org/reports/tr9/#I1
                (false, R)  => levels[i] += 1,
                (false, AN) |
                (false, EN) => levels[i] += 2,
                // http://www.unicode.org/reports/tr9/#I2
                (true, L)  |
                (true, EN) |
                (true, AN) => levels[i] += 1,
                (_, _) => {}
            }
            max_level = max(max_level, levels[i]);
        }
        max_level
    }
}

#[cfg(test)]
mod test {
    use super::BidiClass::*;

    #[test]
    fn test_initial_scan() {
        use super::{InitialProperties, initial_scan, ParagraphInfo};

        assert_eq!(initial_scan("a1", None), InitialProperties {
            initial_classes: vec![L, EN],
            paragraphs: vec![ParagraphInfo { range: 0..2, level: 0 }],
        });
        assert_eq!(initial_scan("غ א", None), InitialProperties {
            initial_classes: vec![AL, AL, WS, R, R],
            paragraphs: vec![ParagraphInfo { range: 0..5, level: 1 }],
        });
        {
            let para1 = ParagraphInfo { range: 0..4, level: 0 };
            let para2 = ParagraphInfo { range: 4..5, level: 0 };
            assert_eq!(initial_scan("a\u{2029}b", None), InitialProperties {
                initial_classes: vec![L, B, B, B, L],
                paragraphs: vec![para1, para2],
            });
        }

        let fsi = '\u{2068}';
        let pdi = '\u{2069}';

        let s = format!("{}א{}a", fsi, pdi);
        assert_eq!(initial_scan(&s, None), InitialProperties {
            initial_classes: vec![RLI, RLI, RLI, R, R, PDI, PDI, PDI, L],
            paragraphs: vec![ParagraphInfo { range: 0..9, level: 0 }],
        });
    }

    #[test]
    fn test_bidi_class() {
        use super::bidi_class;

        assert_eq!(bidi_class('c'), L);
        assert_eq!(bidi_class('\u{05D1}'), R);
        assert_eq!(bidi_class('\u{0627}'), AL);
    }

    #[test]
    fn test_process_text() {
        use super::{BidiInfo, ParagraphInfo, process_text};

        assert_eq!(process_text("abc123", Some(0)), BidiInfo {
            levels:  vec![0, 0, 0, 0,  0,  0],
            classes: vec![L, L, L, EN, EN, EN],
            paragraphs: vec![ParagraphInfo { range: 0..6, level: 0 }],
        });
        assert_eq!(process_text("abc אבג", Some(0)), BidiInfo {
            levels:  vec![0, 0, 0, 0,  1,1, 1,1, 1,1],
            classes: vec![L, L, L, WS, R,R, R,R, R,R],
            paragraphs: vec![ParagraphInfo { range: 0..10, level: 0 }],
        });
        assert_eq!(process_text("abc אבג", Some(1)), BidiInfo {
            levels:  vec![2, 2, 2, 1,  1,1, 1,1, 1,1],
            classes: vec![L, L, L, WS, R,R, R,R, R,R],
            paragraphs: vec![ParagraphInfo { range: 0..10, level: 1 }],
        });
        assert_eq!(process_text("אבג abc", Some(0)), BidiInfo {
            levels:  vec![1,1, 1,1, 1,1, 0,  0, 0, 0],
            classes: vec![R,R, R,R, R,R, WS, L, L, L],
            paragraphs: vec![ParagraphInfo { range: 0..10, level: 0 }],
        });
        assert_eq!(process_text("אבג abc", None), BidiInfo {
            levels:  vec![1,1, 1,1, 1,1, 1,  2, 2, 2],
            classes: vec![R,R, R,R, R,R, WS, L, L, L],
            paragraphs: vec![ParagraphInfo { range: 0..10, level: 1 }],
        });
        assert_eq!(process_text("غ2ظ א2ג", Some(0)), BidiInfo {
            levels:  vec![1, 1,  2,  1, 1,  1,  1,1, 2,  1,1],
            classes: vec![AL,AL, EN, AL,AL, WS, R,R, EN, R,R],
            paragraphs: vec![ParagraphInfo { range: 0..11, level: 0 }],
        });
        assert_eq!(process_text("a א.\nג", None), BidiInfo {
            classes: vec![L, WS, R,R, CS, B, R,R],
            levels:  vec![0, 0,  1,1, 0,  0, 1,1],
            paragraphs: vec![ParagraphInfo { range: 0..6, level: 0 },
                             ParagraphInfo { range: 6..8, level: 1 }],
        });
    }

    #[test]
    fn test_reorder_line() {
        use super::{process_text, reorder_line};
        use std::borrow::Cow;
        fn reorder(s: &str) -> Cow<str> {
            let info = process_text(s, None);
            let para = &info.paragraphs[0];
            reorder_line(s, para.range.clone(), &info.levels)
        }
        assert_eq!(reorder("abc123"), "abc123");
        assert_eq!(reorder("1.-2"), "1.-2");
        assert_eq!(reorder("1-.2"), "1-.2");
        assert_eq!(reorder("abc אבג"), "abc גבא");
        //Numbers being weak LTR characters, cannot reorder strong RTL
        assert_eq!(reorder("123 אבג"), "גבא 123");
        //Testing for RLE Character
        assert_eq!(reorder("\u{202B}abc אבג\u{202C}"), "\u{202B}\u{202C}גבא abc");
        //Testing neutral characters
        assert_eq!(reorder("אבג? אבג"), "גבא ?גבא");
        //Testing neutral characters with special case
        assert_eq!(reorder("A אבג?"), "A גבא?");
        //Testing neutral characters with Implicit RTL Marker
        //The given test highlights a possible non-conformance issue that will perhaps be fixed in the subsequent steps.
        //assert_eq!(reorder("A אבג?\u{202f}"), "A \u{202f}?גבא");
        assert_eq!(reorder("אבג abc"), "abc גבא");
        assert_eq!(reorder("abc\u{2067}.-\u{2069}ghi"),
                           "abc\u{2067}-.\u{2069}ghi");
        assert_eq!(reorder("Hello, \u{2068}\u{202E}world\u{202C}\u{2069}!"),
                           "Hello, \u{2068}\u{202E}\u{202C}dlrow\u{2069}!");
    }

    #[test]
    fn test_is_ltr() {
        use super::is_ltr;
        assert_eq!(is_ltr(10), true);
        assert_eq!(is_ltr(11), false);
        assert_eq!(is_ltr(20), true);
    }

    #[test]
    fn test_is_rtl() {
        use super::is_rtl;
        assert_eq!(is_rtl(13), true);
        assert_eq!(is_rtl(11), true);
        assert_eq!(is_rtl(20), false);
    }

    #[test]
    fn test_removed_by_x9() {
        use prepare::removed_by_x9;
        let rem_classes = &[RLE, LRE, RLO, LRO, PDF, BN];
        let not_classes = &[L, RLI, AL, LRI, PDI];
        for x in rem_classes {
            assert_eq!(removed_by_x9(*x), true);
        }
        for x in not_classes {
            assert_eq!(removed_by_x9(*x), false);
        }
    }

    #[test]
    fn test_not_removed_by_x9() {
        use prepare::not_removed_by_x9;
        let non_x9_classes = &[L, R, AL, EN, ES, ET, AN, CS, NSM, B, S, WS, ON, LRI, RLI, FSI, PDI];
        for x in non_x9_classes {
            assert_eq!(not_removed_by_x9(&x), true);
        }
    }
}
