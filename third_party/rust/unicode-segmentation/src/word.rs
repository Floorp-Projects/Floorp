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
use core::iter::Filter;

use tables::word::WordCat;

/// An iterator over the substrings of a string which, after splitting the string on
/// [word boundaries](http://www.unicode.org/reports/tr29/#Word_Boundaries),
/// contain any characters with the
/// [Alphabetic](http://unicode.org/reports/tr44/#Alphabetic)
/// property, or with
/// [General_Category=Number](http://unicode.org/reports/tr44/#General_Category_Values).
pub struct UnicodeWords<'a> {
    inner: Filter<UWordBounds<'a>, fn(&&str) -> bool>,
}

impl<'a> Iterator for UnicodeWords<'a> {
    type Item = &'a str;

    #[inline]
    fn next(&mut self) -> Option<&'a str> { self.inner.next() }
}
impl<'a> DoubleEndedIterator for UnicodeWords<'a> {
    #[inline]
    fn next_back(&mut self) -> Option<&'a str> { self.inner.next_back() }
}

/// External iterator for a string's
/// [word boundaries](http://www.unicode.org/reports/tr29/#Word_Boundaries).
#[derive(Clone)]
pub struct UWordBounds<'a> {
    string: &'a str,
    cat: Option<WordCat>,
    catb: Option<WordCat>,
}

/// External iterator for word boundaries and byte offsets.
#[derive(Clone)]
pub struct UWordBoundIndices<'a> {
    start_offset: usize,
    iter: UWordBounds<'a>,
}

impl<'a> UWordBoundIndices<'a> {
    #[inline]
    /// View the underlying data (the part yet to be iterated) as a slice of the original string.
    ///
    /// ```rust
    /// # use unicode_segmentation::UnicodeSegmentation;
    /// let mut iter = "Hello world".split_word_bound_indices();
    /// assert_eq!(iter.as_str(), "Hello world");
    /// iter.next();
    /// assert_eq!(iter.as_str(), " world");
    /// iter.next();
    /// assert_eq!(iter.as_str(), "world");
    /// ```
    pub fn as_str(&self) -> &'a str {
        self.iter.as_str()
    }
}

impl<'a> Iterator for UWordBoundIndices<'a> {
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

impl<'a> DoubleEndedIterator for UWordBoundIndices<'a> {
    #[inline]
    fn next_back(&mut self) -> Option<(usize, &'a str)> {
        self.iter.next_back().map(|s| (s.as_ptr() as usize - self.start_offset, s))
    }
}

// state machine for word boundary rules
#[derive(Clone,Copy,PartialEq,Eq,Debug)]
enum UWordBoundsState {
    Start,
    Letter,
    HLetter,
    Numeric,
    Katakana,
    ExtendNumLet,
    Regional(RegionalState),
    FormatExtend(FormatExtendType),
    Zwj,
    Emoji,
}

// subtypes for FormatExtend state in UWordBoundsState
#[derive(Clone,Copy,PartialEq,Eq,Debug)]
enum FormatExtendType {
    AcceptAny,
    AcceptNone,
    RequireLetter,
    RequireHLetter,
    AcceptQLetter,
    RequireNumeric,
}

#[derive(Clone,Copy,PartialEq,Eq,Debug)]
enum RegionalState {
    Half,
    Full,
    Unknown,
}

impl<'a> Iterator for UWordBounds<'a> {
    type Item = &'a str;

    #[inline]
    fn size_hint(&self) -> (usize, Option<usize>) {
        let slen = self.string.len();
        (cmp::min(slen, 1), Some(slen))
    }

    #[inline]
    fn next(&mut self) -> Option<&'a str> {
        use self::UWordBoundsState::*;
        use self::FormatExtendType::*;
        use tables::word as wd;
        if self.string.len() == 0 {
            return None;
        }

        let mut take_curr = true;
        let mut take_cat = true;
        let mut idx = 0;
        let mut saveidx = 0;
        let mut state = Start;
        let mut cat = wd::WC_Any;
        let mut savecat = wd::WC_Any;

        // Whether or not the previous category was ZWJ
        // ZWJs get collapsed, so this handles precedence of WB3c over WB4
        let mut prev_zwj;
        for (curr, ch) in self.string.char_indices() {
            idx = curr;
            prev_zwj = cat == wd::WC_ZWJ;
            // if there's a category cached, grab it
            cat = match self.cat {
                None => wd::word_category(ch),
                _ => self.cat.take().unwrap()
            };
            take_cat = true;

            // handle rule WB4
            // just skip all format, extend, and zwj chars
            // note that Start is a special case: if there's a bunch of Format | Extend
            // characters at the beginning of a block of text, dump them out as one unit.
            //
            // (This is not obvious from the wording of UAX#29, but if you look at the
            // test cases http://www.unicode.org/Public/UNIDATA/auxiliary/WordBreakTest.txt
            // then the "correct" interpretation of WB4 becomes apparent.)
            if state != Start {
                match cat {
                    wd::WC_Extend | wd::WC_Format | wd::WC_ZWJ => {
                        continue
                    }
                    _ => {}
                }
            }

            // rule WB3c
            // WB4 makes all ZWJs collapse into the previous state
            // but you can still be in a Zwj state if you started with Zwj
            //
            // This means that Zwj + Extend will collapse into Zwj, which is wrong,
            // since Extend has a boundary with following EBG/GAZ chars but ZWJ doesn't,
            // and that rule (WB3c) has higher priority
            //
            // Additionally, Emoji_Base+ZWJ+(EBG/GAZ) will collapse into Emoji_Base+EBG/GAZ
            // which won't have a boundary even though EB+ZWJ+GAZ should have a boundary.
            //
            // Thus, we separately keep track of whether or not the last character
            // was a ZWJ. This is an additional bit of state tracked outside of the
            // state enum; the state enum represents the last non-zwj state encountered.
            // When prev_zwj is true, for the purposes of WB3c, we are in the Zwj state,
            // however we are in the previous state for the purposes of all other rules.
            if prev_zwj {
                match cat { 
                    wd::WC_Glue_After_Zwj => continue,
                    wd::WC_E_Base_GAZ => {
                        state = Emoji;
                        continue;
                    },
                    _ => ()
                }
            }
            // Don't use `continue` in this match without updating `cat`
            state = match state {
                Start if cat == wd::WC_CR => {
                    idx += match self.get_next_cat(idx) {
                        Some(ncat) if ncat == wd::WC_LF => 1,       // rule WB3
                        _ => 0
                    };
                    break;                                          // rule WB3a
                },
                Start => match cat {
                    wd::WC_ALetter => Letter,           // rule WB5, WB6, WB9, WB13a
                    wd::WC_Hebrew_Letter => HLetter,    // rule WB5, WB6, WB7a, WB7b, WB9, WB13a
                    wd::WC_Numeric => Numeric,          // rule WB8, WB10, WB12, WB13a
                    wd::WC_Katakana => Katakana,        // rule WB13, WB13a
                    wd::WC_ExtendNumLet => ExtendNumLet,    // rule WB13a, WB13b
                    wd::WC_Regional_Indicator => Regional(RegionalState::Half),  // rule WB13c
                    wd::WC_LF | wd::WC_Newline => break,    // rule WB3a
                    wd::WC_ZWJ => Zwj,                      // rule WB3c
                    wd::WC_E_Base | wd::WC_E_Base_GAZ => Emoji, // rule WB14
                    _ => {
                        if let Some(ncat) = self.get_next_cat(idx) {                // rule WB4
                            if ncat == wd::WC_Format || ncat == wd::WC_Extend || ncat == wd::WC_ZWJ {
                                state = FormatExtend(AcceptNone);
                                self.cat = Some(ncat);
                                continue;
                            }
                        }
                        break;                                                      // rule WB999
                    }
                },
                Zwj => {
                    // We already handle WB3c above. At this point,
                    // the current category is not GAZ or EBG,
                    // or the previous character was not actually a ZWJ
                    take_curr = false;
                    break;
                }
                Letter | HLetter => match cat {
                    wd::WC_ALetter => Letter,                   // rule WB5
                    wd::WC_Hebrew_Letter => HLetter,            // rule WB5
                    wd::WC_Numeric => Numeric,                  // rule WB9
                    wd::WC_ExtendNumLet => ExtendNumLet,        // rule WB13a
                    wd::WC_Double_Quote if state == HLetter => {
                        savecat = cat;
                        saveidx = idx;
                        FormatExtend(RequireHLetter)                        // rule WB7b
                    },
                    wd::WC_Single_Quote if state == HLetter => {
                        FormatExtend(AcceptQLetter)                         // rule WB7a
                    },
                    wd::WC_MidLetter | wd::WC_MidNumLet | wd::WC_Single_Quote => {
                        savecat = cat;
                        saveidx = idx;
                        FormatExtend(RequireLetter)                         // rule WB6
                    },
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Numeric => match cat {
                    wd::WC_Numeric => Numeric,                  // rule WB8
                    wd::WC_ALetter => Letter,                   // rule WB10
                    wd::WC_Hebrew_Letter => HLetter,            // rule WB10
                    wd::WC_ExtendNumLet => ExtendNumLet,        // rule WB13a
                    wd::WC_MidNum | wd::WC_MidNumLet | wd::WC_Single_Quote => {
                        savecat = cat;
                        saveidx = idx;
                        FormatExtend(RequireNumeric)            // rule WB12
                    },
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Katakana => match cat {
                    wd::WC_Katakana => Katakana,                // rule WB13
                    wd::WC_ExtendNumLet => ExtendNumLet,        // rule WB13a
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                ExtendNumLet => match cat {
                    wd::WC_ExtendNumLet => ExtendNumLet,        // rule WB13a
                    wd::WC_ALetter => Letter,                   // rule WB13b
                    wd::WC_Hebrew_Letter => HLetter,            // rule WB13b
                    wd::WC_Numeric => Numeric,                  // rule WB13b
                    wd::WC_Katakana => Katakana,                // rule WB13b
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Regional(RegionalState::Full) => {
                    // if it reaches here we've gone too far,
                    // a full flag can only compose with ZWJ/Extend/Format
                    // proceeding it.
                    take_curr = false;
                    break;
                }
                Regional(RegionalState::Half) => match cat {
                    wd::WC_Regional_Indicator => Regional(RegionalState::Full),      // rule WB13c
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Regional(_) => unreachable!("RegionalState::Unknown should not occur on forward iteration"),
                Emoji => match cat {                            // rule WB14
                    wd::WC_E_Modifier => state,
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                FormatExtend(t) => match t {    // handle FormatExtends depending on what type
                    RequireNumeric if cat == wd::WC_Numeric => Numeric,     // rule WB11
                    RequireLetter | AcceptQLetter if cat == wd::WC_ALetter => Letter,   // rule WB7
                    RequireLetter | AcceptQLetter if cat == wd::WC_Hebrew_Letter => HLetter, // WB7a
                    RequireHLetter if cat == wd::WC_Hebrew_Letter => HLetter,   // rule WB7b
                    AcceptNone | AcceptQLetter => {
                        take_curr = false;  // emit all the Format|Extend characters
                        take_cat = false;
                        break;
                    },
                    _ => break      // rewind (in if statement below)
                }
            }
        }

        if let FormatExtend(t) = state {
            // we were looking for something and didn't find it; we have to back up
            if t == RequireLetter || t == RequireHLetter || t == RequireNumeric {
                idx = saveidx;
                cat = savecat;
                take_curr = false;
            }
        }

        self.cat = if take_curr {
            idx = idx + self.string[idx..].chars().next().unwrap().len_utf8();
            None
        } else if take_cat {
            Some(cat)
        } else {
            None
        };

        let retstr = &self.string[..idx];
        self.string = &self.string[idx..];
        Some(retstr)
    }
}

impl<'a> DoubleEndedIterator for UWordBounds<'a> {
    #[inline]
    fn next_back(&mut self) -> Option<&'a str> {
        use self::UWordBoundsState::*;
        use self::FormatExtendType::*;
        use tables::word as wd;
        if self.string.len() == 0 {
            return None;
        }

        let mut take_curr = true;
        let mut take_cat = true;
        let mut idx = self.string.len();
        idx -= self.string.chars().next_back().unwrap().len_utf8();
        let mut previdx = idx;
        let mut saveidx = idx;
        let mut state = Start;
        let mut savestate = Start;
        let mut cat = wd::WC_Any;

        for (curr, ch) in self.string.char_indices().rev() {
            previdx = idx;
            idx = curr;

            // if there's a category cached, grab it
            cat = match self.catb {
                None => wd::word_category(ch),
                _ => self.catb.take().unwrap()
            };
            take_cat = true;

            // backward iterator over word boundaries. Mostly the same as the forward
            // iterator, with two weirdnesses:
            // (1) If we encounter a single quote in the Start state, we have to check for a
            //     Hebrew Letter immediately before it.
            // (2) Format and Extend char handling takes some gymnastics.

            if cat == wd::WC_Extend
                || cat == wd::WC_Format
                || (cat == wd::WC_ZWJ && state != Zwj) { // WB3c has more priority so we should not
                                                         // fold in that case
                if match state {
                    FormatExtend(_) | Start => false,
                    _ => true
                } {
                    saveidx = previdx;
                    savestate = state;
                    state = FormatExtend(AcceptNone);
                }

                if state != Start {
                    continue;
                }
            } else if state == FormatExtend(AcceptNone) {
                // finished a scan of some Format|Extend chars, restore previous state
                state = savestate;
                previdx = saveidx;
                take_cat = false;
            }

            // Don't use `continue` in this match without updating `catb`
            state = match state {
                Start | FormatExtend(AcceptAny) => match cat {
                    wd::WC_ALetter => Letter,           // rule WB5, WB7, WB10, WB13b
                    wd::WC_Hebrew_Letter => HLetter,    // rule WB5, WB7, WB7c, WB10, WB13b
                    wd::WC_Numeric => Numeric,          // rule WB8, WB9, WB11, WB13b
                    wd::WC_Katakana => Katakana,                    // rule WB13, WB13b
                    wd::WC_ExtendNumLet => ExtendNumLet,                    // rule WB13a
                    wd::WC_Regional_Indicator => Regional(RegionalState::Unknown), // rule WB13c
                    wd::WC_Glue_After_Zwj | wd::WC_E_Base_GAZ => Zwj,       // rule WB3c
                    // rule WB4:
                    wd::WC_Extend | wd::WC_Format | wd::WC_ZWJ => FormatExtend(AcceptAny),
                    wd::WC_Single_Quote => {
                        saveidx = idx;
                        FormatExtend(AcceptQLetter)                         // rule WB7a
                    },
                    wd::WC_E_Modifier => Emoji,                             // rule WB14
                    wd::WC_CR | wd::WC_LF | wd::WC_Newline => {
                        if state == Start {
                            if cat == wd::WC_LF {
                                idx -= match self.get_prev_cat(idx) {
                                    Some(pcat) if pcat == wd::WC_CR => 1,   // rule WB3
                                    _ => 0
                                };
                            }
                        } else {
                            take_curr = false;
                        }
                        break;                                              // rule WB3a
                    },
                    _ => break                              // rule WB999
                },
                Zwj => match cat {                          // rule WB3c
                    wd::WC_ZWJ => {
                        FormatExtend(AcceptAny)
                    }
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Letter | HLetter => match cat {
                    wd::WC_ALetter => Letter,               // rule WB5
                    wd::WC_Hebrew_Letter => HLetter,        // rule WB5
                    wd::WC_Numeric => Numeric,              // rule WB10
                    wd::WC_ExtendNumLet => ExtendNumLet,    // rule WB13b
                    wd::WC_Double_Quote if state == HLetter => {
                        saveidx = previdx;
                        FormatExtend(RequireHLetter)         // rule WB7c
                    },
                    wd::WC_MidLetter | wd::WC_MidNumLet | wd::WC_Single_Quote => {
                        saveidx = previdx;
                        FormatExtend(RequireLetter)          // rule WB7
                    },
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Numeric => match cat {
                    wd::WC_Numeric => Numeric,              // rule WB8
                    wd::WC_ALetter => Letter,               // rule WB9
                    wd::WC_Hebrew_Letter => HLetter,        // rule WB9
                    wd::WC_ExtendNumLet => ExtendNumLet,    // rule WB13b
                    wd::WC_MidNum | wd::WC_MidNumLet | wd::WC_Single_Quote => {
                        saveidx = previdx;
                        FormatExtend(RequireNumeric)         // rule WB11
                    },
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Katakana => match cat {
                    wd::WC_Katakana => Katakana,            // rule WB13
                    wd::WC_ExtendNumLet => ExtendNumLet,    // rule WB13b
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                ExtendNumLet => match cat {
                    wd::WC_ExtendNumLet => ExtendNumLet,    // rule WB13a
                    wd::WC_ALetter => Letter,               // rule WB13a
                    wd::WC_Hebrew_Letter => HLetter,        // rule WB13a
                    wd::WC_Numeric => Numeric,              // rule WB13a
                    wd::WC_Katakana => Katakana,            // rule WB13a
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Regional(mut regional_state) => match cat {
                    // rule WB13c
                    wd::WC_Regional_Indicator => {
                        if regional_state == RegionalState::Unknown {
                            let count = self.string[..previdx]
                                            .chars().rev()
                                            .map(|c| wd::word_category(c))
                                            .filter(|&c| ! (c == wd::WC_ZWJ || c == wd::WC_Extend || c == wd::WC_Format))
                                            .take_while(|&c| c == wd::WC_Regional_Indicator)
                                            .count();
                            regional_state = if count % 2 == 0 {
                                RegionalState::Full
                            } else {
                                RegionalState::Half
                            };
                        }
                        if regional_state == RegionalState::Full {
                            take_curr = false;
                            break;
                        } else {
                            Regional(RegionalState::Full)
                        }
                    }
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                Emoji => match cat {                            // rule WB14
                    wd::WC_E_Base | wd::WC_E_Base_GAZ => {
                        Zwj
                    },
                    _ => {
                        take_curr = false;
                        break;
                    }
                },
                FormatExtend(t) => match t {
                    RequireNumeric if cat == wd::WC_Numeric => Numeric,          // rule WB12
                    RequireLetter if cat == wd::WC_ALetter => Letter,            // rule WB6
                    RequireLetter if cat == wd::WC_Hebrew_Letter => HLetter,     // rule WB6
                    AcceptQLetter if cat == wd::WC_Hebrew_Letter => HLetter,     // rule WB7a
                    RequireHLetter if cat == wd::WC_Hebrew_Letter => HLetter,    // rule WB7b
                    _ => break  // backtrack will happens
                }
            }
        }

        if let FormatExtend(t) = state {
            // if we required something but didn't find it, backtrack
            if t == RequireLetter || t == RequireHLetter ||
                t == RequireNumeric || t == AcceptNone || t == AcceptQLetter {
                previdx = saveidx;
                take_cat = false;
                take_curr = false;
            }
        }

        self.catb = if take_curr {
            None
        } else {
            idx = previdx;
            if take_cat {
                Some(cat)
            } else {
                None
            }
        };

        let retstr = &self.string[idx..];
        self.string = &self.string[..idx];
        Some(retstr)
    }
}

impl<'a> UWordBounds<'a> {
    #[inline]
    /// View the underlying data (the part yet to be iterated) as a slice of the original string.
    ///
    /// ```rust
    /// # use unicode_segmentation::UnicodeSegmentation;
    /// let mut iter = "Hello world".split_word_bounds();
    /// assert_eq!(iter.as_str(), "Hello world");
    /// iter.next();
    /// assert_eq!(iter.as_str(), " world");
    /// iter.next();
    /// assert_eq!(iter.as_str(), "world");
    /// ```
    pub fn as_str(&self) -> &'a str {
        self.string
    }

    #[inline]
    fn get_next_cat(&self, idx: usize) -> Option<WordCat> {
        use tables::word as wd;
        let nidx = idx + self.string[idx..].chars().next().unwrap().len_utf8();
        if nidx < self.string.len() {
            let nch = self.string[nidx..].chars().next().unwrap();
            Some(wd::word_category(nch))
        } else {
            None
        }
    }

    #[inline]
    fn get_prev_cat(&self, idx: usize) -> Option<WordCat> {
        use tables::word as wd;
        if idx > 0 {
            let nch = self.string[..idx].chars().next_back().unwrap();
            Some(wd::word_category(nch))
        } else {
            None
        }
    }
}

#[inline]
pub fn new_word_bounds<'b>(s: &'b str) -> UWordBounds<'b> {
    UWordBounds { string: s, cat: None, catb: None }
}

#[inline]
pub fn new_word_bound_indices<'b>(s: &'b str) -> UWordBoundIndices<'b> {
    UWordBoundIndices { start_offset: s.as_ptr() as usize, iter: new_word_bounds(s) }
}

#[inline]
pub fn new_unicode_words<'b>(s: &'b str) -> UnicodeWords<'b> {
    use super::UnicodeSegmentation;
    use tables::util::is_alphanumeric;

    fn has_alphanumeric(s: &&str) -> bool { s.chars().any(|c| is_alphanumeric(c)) }
    let has_alphanumeric: fn(&&str) -> bool = has_alphanumeric; // coerce to fn pointer

    UnicodeWords { inner: s.split_word_bounds().filter(has_alphanumeric) }
}
