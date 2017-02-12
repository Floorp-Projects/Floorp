// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/*!
This crate provides a regular expression parser and an abstract syntax for
regular expressions. The abstract syntax is defined by the `Expr` type. The
concrete syntax is enumerated in the
[`regex`](../regex/index.html#syntax)
crate documentation.

Note that since this crate is first and foremost an implementation detail for
the `regex` crate, it may experience more frequent breaking changes. It is
exposed as a separate crate so that others may use it to do analysis on regular
expressions or even build their own matching engine.

# Example: parsing an expression

Parsing a regular expression can be done with the `Expr::parse` function.

```rust
use regex_syntax::Expr;

assert_eq!(Expr::parse(r"ab|yz").unwrap(), Expr::Alternate(vec![
    Expr::Literal { chars: vec!['a', 'b'], casei: false },
    Expr::Literal { chars: vec!['y', 'z'], casei: false },
]));
```

# Example: inspecting an error

The parser in this crate provides very detailed error values. For example,
if an invalid character class range is given:

```rust
use regex_syntax::{Expr, ErrorKind};

let err = Expr::parse(r"[z-a]").unwrap_err();
assert_eq!(err.position(), 4);
assert_eq!(err.kind(), &ErrorKind::InvalidClassRange {
    start: 'z',
    end: 'a',
});
```

Or unbalanced parentheses:

```rust
use regex_syntax::{Expr, ErrorKind};

let err = Expr::parse(r"ab(cd").unwrap_err();
assert_eq!(err.position(), 2);
assert_eq!(err.kind(), &ErrorKind::UnclosedParen);
```
*/

#![deny(missing_docs)]
#![cfg_attr(test, deny(warnings))]

#[cfg(test)] extern crate quickcheck;
#[cfg(test)] extern crate rand;

mod literals;
mod parser;
mod unicode;

use std::ascii;
use std::char;
use std::cmp::{Ordering, max, min};
use std::fmt;
use std::iter::IntoIterator;
use std::ops::Deref;
use std::result;
use std::slice;
use std::u8;
use std::vec;

use unicode::case_folding;

use self::Expr::*;
use self::Repeater::*;

use parser::{Flags, Parser};

pub use literals::{Literals, Lit};

/// A regular expression abstract syntax tree.
///
/// An `Expr` represents the abstract syntax of a regular expression.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Expr {
    /// An empty regex (which never matches any text).
    Empty,
    /// A sequence of one or more literal characters to be matched.
    Literal {
        /// The characters.
        chars: Vec<char>,
        /// Whether to match case insensitively.
        casei: bool,
    },
    /// A sequence of one or more literal bytes to be matched.
    LiteralBytes {
        /// The bytes.
        bytes: Vec<u8>,
        /// Whether to match case insensitively.
        ///
        /// The interpretation of "case insensitive" in this context is
        /// ambiguous since `bytes` can be arbitrary. However, a good heuristic
        /// is to assume that the bytes are ASCII-compatible and do simple
        /// ASCII case folding.
        casei: bool,
    },
    /// Match any character.
    AnyChar,
    /// Match any character, excluding new line (`0xA`).
    AnyCharNoNL,
    /// Match any byte.
    AnyByte,
    /// Match any byte, excluding new line (`0xA`).
    AnyByteNoNL,
    /// A character class.
    Class(CharClass),
    /// A character class with byte ranges only.
    ClassBytes(ByteClass),
    /// Match the start of a line or beginning of input.
    StartLine,
    /// Match the end of a line or end of input.
    EndLine,
    /// Match the beginning of input.
    StartText,
    /// Match the end of input.
    EndText,
    /// Match a word boundary (word character on one side and a non-word
    /// character on the other).
    WordBoundary,
    /// Match a position that is not a word boundary (word or non-word
    /// characters on both sides).
    NotWordBoundary,
    /// Match an ASCII word boundary.
    WordBoundaryAscii,
    /// Match a position that is not an ASCII word boundary.
    NotWordBoundaryAscii,
    /// A group, possibly non-capturing.
    Group {
        /// The expression inside the group.
        e: Box<Expr>,
        /// The capture index (starting at `1`) only for capturing groups.
        i: Option<usize>,
        /// The capture name, only for capturing named groups.
        name: Option<String>,
    },
    /// A repeat operator (`?`, `*`, `+` or `{m,n}`).
    Repeat {
        /// The expression to be repeated. Limited to literals, `.`, classes
        /// or grouped expressions.
        e: Box<Expr>,
        /// The type of repeat operator used.
        r: Repeater,
        /// Whether the repeat is greedy (match the most) or not (match the
        /// least).
        greedy: bool,
    },
    /// A concatenation of expressions. Must be matched one after the other.
    ///
    /// N.B. A concat expression can only appear at the top-level or
    /// immediately inside a group expression.
    Concat(Vec<Expr>),
    /// An alternation of expressions. Only one must match.
    ///
    /// N.B. An alternate expression can only appear at the top-level or
    /// immediately inside a group expression.
    Alternate(Vec<Expr>),
}

type CaptureIndex = Option<usize>;

type CaptureName = Option<String>;

/// The type of a repeat operator expression.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Repeater {
    /// Match zero or one (`?`).
    ZeroOrOne,
    /// Match zero or more (`*`).
    ZeroOrMore,
    /// Match one or more (`+`).
    OneOrMore,
    /// Match for at least `min` and at most `max` (`{m,n}`).
    ///
    /// When `max` is `None`, there is no upper bound on the number of matches.
    Range {
        /// Lower bound on the number of matches.
        min: u32,
        /// Optional upper bound on the number of matches.
        max: Option<u32>,
    },
}

impl Repeater {
    /// Returns true if and only if this repetition can match the empty string.
    fn matches_empty(&self) -> bool {
        use self::Repeater::*;
        match *self {
            ZeroOrOne => true,
            ZeroOrMore => true,
            OneOrMore => false,
            Range { min, .. } => min == 0,
        }
    }
}

/// A character class.
///
/// A character class has a canonical format that the parser guarantees. Its
/// canonical format is defined by the following invariants:
///
/// 1. Given any Unicode scalar value, it is matched by *at most* one character
///    range in a canonical character class.
/// 2. Every adjacent character range is separated by at least one Unicode
///    scalar value.
/// 3. Given any pair of character ranges `r1` and `r2`, if
///    `r1.end < r2.start`, then `r1` comes before `r2` in a canonical
///    character class.
///
/// In sum, any `CharClass` produced by this crate's parser is a sorted
/// sequence of non-overlapping ranges. This makes it possible to test whether
/// a character is matched by a class with a binary search.
///
/// If the case insensitive flag was set when parsing a character class, then
/// simple case folding is done automatically. For example, `(?i)[a-c]` is
/// automatically translated to `[a-cA-C]`.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CharClass {
    ranges: Vec<ClassRange>,
}

/// A single inclusive range in a character class.
///
/// Since range boundaries are defined by Unicode scalar values, the boundaries
/// can never be in the open interval `(0xD7FF, 0xE000)`. However, a range may
/// *cover* codepoints that are not scalar values.
///
/// Note that this has a few convenient impls on `PartialEq` and `PartialOrd`
/// for testing whether a character is contained inside a given range.
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd, Eq, Ord)]
pub struct ClassRange {
    /// The start character of the range.
    ///
    /// This must be less than or equal to `end`.
    pub start: char,

    /// The end character of the range.
    ///
    /// This must be greater than or equal to `start`.
    pub end: char,
}

/// A byte class for byte ranges only.
///
/// A byte class has a canonical format that the parser guarantees. Its
/// canonical format is defined by the following invariants:
///
/// 1. Given any byte, it is matched by *at most* one byte range in a canonical
///    character class.
/// 2. Every adjacent byte range is separated by at least one byte.
/// 3. Given any pair of byte ranges `r1` and `r2`, if
///    `r1.end < r2.start`, then `r1` comes before `r2` in a canonical
///    character class.
///
/// In sum, any `ByteClass` produced by this crate's parser is a sorted
/// sequence of non-overlapping ranges. This makes it possible to test whether
/// a byte is matched by a class with a binary search.
///
/// If the case insensitive flag was set when parsing a character class,
/// then simple ASCII-only case folding is done automatically. For example,
/// `(?i)[a-c]` is automatically translated to `[a-cA-C]`.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ByteClass {
    ranges: Vec<ByteRange>,
}

/// A single inclusive range in a byte class.
///
/// Note that this has a few convenient impls on `PartialEq` and `PartialOrd`
/// for testing whether a byte is contained inside a given range.
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd, Eq, Ord)]
pub struct ByteRange {
    /// The start byte of the range.
    ///
    /// This must be less than or equal to `end`.
    pub start: u8,

    /// The end byte of the range.
    ///
    /// This must be greater than or equal to `end`.
    pub end: u8,
}

/// A builder for configuring regular expression parsing.
///
/// This allows setting the default values of flags and other options, such
/// as the maximum nesting depth.
#[derive(Clone, Debug)]
pub struct ExprBuilder {
    flags: Flags,
    nest_limit: usize,
}

impl ExprBuilder {
    /// Create a new builder for configuring expression parsing.
    ///
    /// Note that all flags are disabled by default.
    pub fn new() -> ExprBuilder {
        ExprBuilder {
            flags: Flags::default(),
            nest_limit: 200,
        }
    }

    /// Set the default value for the case insensitive (`i`) flag.
    pub fn case_insensitive(mut self, yes: bool) -> ExprBuilder {
        self.flags.casei = yes;
        self
    }

    /// Set the default value for the multi-line matching (`m`) flag.
    pub fn multi_line(mut self, yes: bool) -> ExprBuilder {
        self.flags.multi = yes;
        self
    }

    /// Set the default value for the any character (`s`) flag.
    pub fn dot_matches_new_line(mut self, yes: bool) -> ExprBuilder {
        self.flags.dotnl = yes;
        self
    }

    /// Set the default value for the greedy swap (`U`) flag.
    pub fn swap_greed(mut self, yes: bool) -> ExprBuilder {
        self.flags.swap_greed = yes;
        self
    }

    /// Set the default value for the ignore whitespace (`x`) flag.
    pub fn ignore_whitespace(mut self, yes: bool) -> ExprBuilder {
        self.flags.ignore_space = yes;
        self
    }

    /// Set the default value for the Unicode (`u`) flag.
    ///
    /// If `yes` is false, then `allow_bytes` is set to true.
    pub fn unicode(mut self, yes: bool) -> ExprBuilder {
        self.flags.unicode = yes;
        if !yes {
            self.allow_bytes(true)
        } else {
            self
        }
    }

    /// Whether the parser allows matching arbitrary bytes or not.
    ///
    /// When the `u` flag is disabled (either with this builder or in the
    /// expression itself), the parser switches to interpreting the expression
    /// as matching arbitrary bytes instead of Unicode codepoints. For example,
    /// the expression `(?u:\xFF)` matches the *codepoint* `\xFF`, which
    /// corresponds to the UTF-8 byte sequence `\xCE\xBF`. Conversely,
    /// `(?-u:\xFF)` matches the *byte* `\xFF`, which is not valid UTF-8.
    ///
    /// When `allow_bytes` is disabled (the default), an expression like
    /// `(?-u:\xFF)` will cause the parser to return an error, since it would
    /// otherwise match invalid UTF-8. When enabled, it will be allowed.
    pub fn allow_bytes(mut self, yes: bool) -> ExprBuilder {
        self.flags.allow_bytes = yes;
        self
    }

    /// Set the nesting limit for regular expression parsing.
    ///
    /// Regular expressions that nest more than this limit will result in a
    /// `StackExhausted` error.
    pub fn nest_limit(mut self, limit: usize) -> ExprBuilder {
        self.nest_limit = limit;
        self
    }

    /// Parse a string as a regular expression using the current configuraiton.
    pub fn parse(self, s: &str) -> Result<Expr> {
        Parser::parse(s, self.flags).and_then(|e| e.simplify(self.nest_limit))
    }
}

impl Expr {
    /// Parses a string in a regular expression syntax tree.
    ///
    /// This is a convenience method for parsing an expression using the
    /// default configuration. To tweak parsing options (such as which flags
    /// are enabled by default), use the `ExprBuilder` type.
    pub fn parse(s: &str) -> Result<Expr> {
        ExprBuilder::new().parse(s)
    }

    /// Returns true iff the expression can be repeated by a quantifier.
    fn can_repeat(&self) -> bool {
        match *self {
            Literal{..} | LiteralBytes{..}
            | AnyChar | AnyCharNoNL | AnyByte | AnyByteNoNL
            | Class(_) | ClassBytes(_)
            | StartLine | EndLine | StartText | EndText
            | WordBoundary | NotWordBoundary
            | WordBoundaryAscii | NotWordBoundaryAscii
            | Group{..}
            => true,
            _ => false,
        }
    }

    fn simplify(self, nest_limit: usize) -> Result<Expr> {
        fn combine_literals(es: &mut Vec<Expr>, e: Expr) {
            match (es.pop(), e) {
                (None, e) => es.push(e),
                (Some(Literal { chars: mut chars1, casei: casei1 }),
                      Literal { chars: chars2, casei: casei2 }) => {
                    if casei1 == casei2 {
                        chars1.extend(chars2);
                        es.push(Literal { chars: chars1, casei: casei1 });
                    } else {
                        es.push(Literal { chars: chars1, casei: casei1 });
                        es.push(Literal { chars: chars2, casei: casei2 });
                    }
                }
                (Some(LiteralBytes { bytes: mut bytes1, casei: casei1 }),
                      LiteralBytes { bytes: bytes2, casei: casei2 }) => {
                    if casei1 == casei2 {
                        bytes1.extend(bytes2);
                        es.push(LiteralBytes { bytes: bytes1, casei: casei1 });
                    } else {
                        es.push(LiteralBytes { bytes: bytes1, casei: casei1 });
                        es.push(LiteralBytes { bytes: bytes2, casei: casei2 });
                    }
                }
                (Some(e1), e2) => {
                    es.push(e1);
                    es.push(e2);
                }
            }
        }
        fn simp(expr: Expr, recurse: usize, limit: usize) -> Result<Expr> {
            if recurse > limit {
                return Err(Error {
                    pos: 0,
                    surround: "".to_owned(),
                    kind: ErrorKind::StackExhausted,
                });
            }
            let simplify = |e| simp(e, recurse + 1, limit);
            Ok(match expr {
                Repeat { e, r, greedy } => Repeat {
                    e: Box::new(try!(simplify(*e))),
                    r: r,
                    greedy: greedy,
                },
                Group { e, i, name } => {
                    let e = try!(simplify(*e));
                    if i.is_none() && name.is_none() && e.can_repeat() {
                        e
                    } else {
                        Group { e: Box::new(e), i: i, name: name }
                    }
                }
                Concat(es) => {
                    let mut new_es = Vec::with_capacity(es.len());
                    for e in es {
                        combine_literals(&mut new_es, try!(simplify(e)));
                    }
                    if new_es.len() == 1 {
                        new_es.pop().unwrap()
                    } else {
                        Concat(new_es)
                    }
                }
                Alternate(es) => {
                    let mut new_es = Vec::with_capacity(es.len());
                    for e in es {
                        new_es.push(try!(simplify(e)));
                    }
                    Alternate(new_es)
                }
                e => e,
            })
        }
        simp(self, 0, nest_limit)
    }

    /// Returns a set of literal prefixes extracted from this expression.
    pub fn prefixes(&self) -> Literals {
        let mut lits = Literals::empty();
        lits.union_prefixes(self);
        lits
    }

    /// Returns a set of literal suffixes extracted from this expression.
    pub fn suffixes(&self) -> Literals {
        let mut lits = Literals::empty();
        lits.union_suffixes(self);
        lits
    }

    /// Returns true if and only if the expression is required to match from
    /// the beginning of text.
    pub fn is_anchored_start(&self) -> bool {
        match *self {
            Repeat { ref e, r, .. } => {
                !r.matches_empty() && e.is_anchored_start()
            }
            Group { ref e, .. } => e.is_anchored_start(),
            Concat(ref es) => es[0].is_anchored_start(),
            Alternate(ref es) => es.iter().all(|e| e.is_anchored_start()),
            StartText => true,
            _ => false,
        }
    }

    /// Returns true if and only if the expression has at least one matchable
    /// sub-expression that must match the beginning of text.
    pub fn has_anchored_start(&self) -> bool {
        match *self {
            Repeat { ref e, r, .. } => {
                !r.matches_empty() && e.has_anchored_start()
            }
            Group { ref e, .. } => e.has_anchored_start(),
            Concat(ref es) => es[0].has_anchored_start(),
            Alternate(ref es) => es.iter().any(|e| e.has_anchored_start()),
            StartText => true,
            _ => false,
        }
    }

    /// Returns true if and only if the expression is required to match at the
    /// end of the text.
    pub fn is_anchored_end(&self) -> bool {
        match *self {
            Repeat { ref e, r, .. } => {
                !r.matches_empty() && e.is_anchored_end()
            }
            Group { ref e, .. } => e.is_anchored_end(),
            Concat(ref es) => es[es.len() - 1].is_anchored_end(),
            Alternate(ref es) => es.iter().all(|e| e.is_anchored_end()),
            EndText => true,
            _ => false,
        }
    }

    /// Returns true if and only if the expression has at least one matchable
    /// sub-expression that must match the beginning of text.
    pub fn has_anchored_end(&self) -> bool {
        match *self {
            Repeat { ref e, r, .. } => {
                !r.matches_empty() && e.has_anchored_end()
            }
            Group { ref e, .. } => e.has_anchored_end(),
            Concat(ref es) => es[es.len() - 1].has_anchored_end(),
            Alternate(ref es) => es.iter().any(|e| e.has_anchored_end()),
            EndText => true,
            _ => false,
        }
    }

    /// Returns true if and only if the expression contains sub-expressions
    /// that can match arbitrary bytes.
    pub fn has_bytes(&self) -> bool {
        match *self {
            Repeat { ref e, .. } => e.has_bytes(),
            Group { ref e, .. } => e.has_bytes(),
            Concat(ref es) => es.iter().any(|e| e.has_bytes()),
            Alternate(ref es) => es.iter().any(|e| e.has_bytes()),
            LiteralBytes{..} => true,
            AnyByte | AnyByteNoNL => true,
            ClassBytes(_) => true,
            WordBoundaryAscii | NotWordBoundaryAscii => true,
            _ => false,
        }
    }
}

impl Deref for CharClass {
    type Target = Vec<ClassRange>;
    fn deref(&self) -> &Vec<ClassRange> { &self.ranges }
}

impl IntoIterator for CharClass {
    type Item = ClassRange;
    type IntoIter = vec::IntoIter<ClassRange>;
    fn into_iter(self) -> vec::IntoIter<ClassRange> { self.ranges.into_iter() }
}

impl<'a> IntoIterator for &'a CharClass {
    type Item = &'a ClassRange;
    type IntoIter = slice::Iter<'a, ClassRange>;
    fn into_iter(self) -> slice::Iter<'a, ClassRange> { self.iter() }
}

impl CharClass {
    /// Create a new class from an existing set of ranges.
    pub fn new(ranges: Vec<ClassRange>) -> CharClass {
        CharClass { ranges: ranges }
    }

    /// Create an empty class.
    fn empty() -> CharClass {
        CharClass::new(Vec::new())
    }

    /// Returns true if `c` is matched by this character class.
    pub fn matches(&self, c: char) -> bool {
        self.binary_search_by(|range| c.partial_cmp(range).unwrap()).is_ok()
    }

    /// Removes the given character from the class if it exists.
    ///
    /// Note that this takes `O(n)` time in the number of ranges.
    pub fn remove(&mut self, c: char) {
        let mut i = match self.binary_search_by(|r| c.partial_cmp(r).unwrap()) {
            Ok(i) => i,
            Err(_) => return,
        };
        let mut r = self.ranges.remove(i);
        if r.start == c {
            r.start = inc_char(c);
            if r.start > r.end || c == char::MAX {
                return;
            }
            self.ranges.insert(i, r);
        } else if r.end == c {
            r.end = dec_char(c);
            if r.end < r.start || c == '\x00' {
                return;
            }
            self.ranges.insert(0, r);
        } else {
            let (mut r1, mut r2) = (r.clone(), r.clone());
            r1.end = dec_char(c);
            if r1.start <= r1.end {
                self.ranges.insert(i, r1);
                i += 1;
            }
            r2.start = inc_char(c);
            if r2.start <= r2.end {
                self.ranges.insert(i, r2);
            }
        }
    }

    /// Create a new empty class from this one.
    fn to_empty(&self) -> CharClass {
        CharClass { ranges: Vec::with_capacity(self.len()) }
    }

    /// Create a byte class from this character class.
    ///
    /// Codepoints above 0xFF are removed.
    fn to_byte_class(self) -> ByteClass {
        ByteClass::new(
            self.ranges.into_iter()
                       .filter_map(|r| r.to_byte_range())
                       .collect()).canonicalize()
    }

    /// Merge two classes and canonicalize them.
    #[cfg(test)]
    fn merge(mut self, other: CharClass) -> CharClass {
        self.ranges.extend(other);
        self.canonicalize()
    }

    /// Canonicalze any sequence of ranges.
    ///
    /// This is responsible for enforcing the canonical format invariants
    /// as described on the docs for the `CharClass` type.
    fn canonicalize(mut self) -> CharClass {
        // TODO: Save some cycles here by checking if already canonicalized.
        self.ranges.sort();
        let mut ordered = self.to_empty(); // TODO: Do this in place?
        for candidate in self {
            // If the candidate overlaps with an existing range, then it must
            // be the most recent range added because we process the candidates
            // in order.
            if let Some(or) = ordered.ranges.last_mut() {
                if or.overlapping(candidate) {
                    *or = or.merge(candidate);
                    continue;
                }
            }
            ordered.ranges.push(candidate);
        }
        ordered
    }

    /// Negates the character class.
    ///
    /// For all `c` where `c` is a Unicode scalar value, `c` matches `self`
    /// if and only if `c` does not match `self.negate()`.
    pub fn negate(mut self) -> CharClass {
        fn range(s: char, e: char) -> ClassRange { ClassRange::new(s, e) }

        if self.is_empty() {
            // Inverting an empty range yields all of Unicode.
            return CharClass {
                ranges: vec![ClassRange { start: '\x00', end: '\u{10ffff}' }],
            };
        }
        self = self.canonicalize();
        let mut inv = self.to_empty();
        if self[0].start > '\x00' {
            inv.ranges.push(range('\x00', dec_char(self[0].start)));
        }
        for win in self.windows(2) {
            inv.ranges.push(range(inc_char(win[0].end),
                                  dec_char(win[1].start)));
        }
        if self[self.len() - 1].end < char::MAX {
            inv.ranges.push(range(inc_char(self[self.len() - 1].end),
                                  char::MAX));
        }
        inv
    }

    /// Apply case folding to this character class.
    ///
    /// N.B. Applying case folding to a negated character class probably
    /// won't produce the expected result. e.g., `(?i)[^x]` really should
    /// match any character sans `x` and `X`, but if `[^x]` is negated
    /// before being case folded, you'll end up matching any character.
    pub fn case_fold(self) -> CharClass {
        let mut folded = self.to_empty();
        for r in self {
            // Applying case folding to a range is expensive because *every*
            // character needs to be examined. Thus, we avoid that drudgery
            // if no character in the current range is in our case folding
            // table.
            if r.needs_case_folding() {
                folded.ranges.extend(r.case_fold());
            }
            folded.ranges.push(r);
        }
        folded.canonicalize()
    }

    /// Returns the number of characters that match this class.
    fn num_chars(&self) -> usize {
        self.ranges.iter()
            .map(|&r| 1 + (r.end as u32) - (r.start as u32))
            .fold(0, |acc, len| acc + len)
            as usize
    }
}

impl ClassRange {
    /// Create a new class range.
    ///
    /// If `end < start`, then the two values are swapped so that
    /// the invariant `start <= end` is preserved.
    fn new(start: char, end: char) -> ClassRange {
        if start <= end {
            ClassRange { start: start, end: end }
        } else {
            ClassRange { start: end, end: start }
        }
    }

    /// Translate this to a byte class.
    ///
    /// If the start codepoint exceeds 0xFF, then this returns `None`.
    ///
    /// If the end codepoint exceeds 0xFF, then it is set to 0xFF.
    fn to_byte_range(self) -> Option<ByteRange> {
        if self.start > '\u{FF}' {
            None
        } else {
            let s = self.start as u8;
            let e = min('\u{FF}', self.end) as u8;
            Some(ByteRange::new(s, e))
        }
    }

    /// Create a range of one character.
    fn one(c: char) -> ClassRange {
        ClassRange { start: c, end: c }
    }

    /// Returns true if and only if the two ranges are overlapping. Note that
    /// since ranges are inclusive, `a-c` and `d-f` are overlapping!
    fn overlapping(self, other: ClassRange) -> bool {
        max(self.start, other.start) <= inc_char(min(self.end, other.end))
    }

    /// Creates a new range representing the union of `self` and `other.
    fn merge(self, other: ClassRange) -> ClassRange {
        ClassRange {
            start: min(self.start, other.start),
            end: max(self.end, other.end),
        }
    }

    /// Returns true if and only if this range contains a character that is
    /// in the case folding table.
    fn needs_case_folding(self) -> bool {
        case_folding::C_plus_S_both_table
        .binary_search_by(|&(c, _)| self.partial_cmp(&c).unwrap()).is_ok()
    }

    /// Apply case folding to this range.
    ///
    /// Since case folding might add characters such that the range is no
    /// longer contiguous, this returns multiple class ranges. They are in
    /// canonical order.
    fn case_fold(self) -> Vec<ClassRange> {
        let table = &case_folding::C_plus_S_both_table;
        let (s, e) = (self.start as u32, self.end as u32 + 1);
        let mut start = self.start;
        let mut end = start;
        let mut next_case_fold = '\x00';
        let mut ranges = Vec::with_capacity(10);
        for mut c in (s..e).filter_map(char::from_u32) {
            if c >= next_case_fold {
                c = match simple_case_fold_both_result(c) {
                    Ok(i) => {
                        for &(c1, c2) in &table[i..] {
                            if c1 != c {
                                break;
                            }
                            if c2 != inc_char(end) {
                                ranges.push(ClassRange::new(start, end));
                                start = c2;
                            }
                            end = c2;
                        }
                        continue;
                    }
                    Err(i) => {
                        if i < table.len() {
                            next_case_fold = table[i].0;
                        } else {
                            next_case_fold = '\u{10FFFF}';
                        }
                        c
                    }
                };
            }
            // The fast path. We know this character doesn't have an entry
            // in the case folding table.
            if c != inc_char(end) {
                ranges.push(ClassRange::new(start, end));
                start = c;
            }
            end = c;
        }
        ranges.push(ClassRange::new(start, end));
        ranges
    }
}

impl PartialEq<char> for ClassRange {
    #[inline]
    fn eq(&self, other: &char) -> bool {
        self.start <= *other && *other <= self.end
    }
}

impl PartialEq<ClassRange> for char {
    #[inline]
    fn eq(&self, other: &ClassRange) -> bool {
        other.eq(self)
    }
}

impl PartialOrd<char> for ClassRange {
    #[inline]
    fn partial_cmp(&self, other: &char) -> Option<Ordering> {
        Some(if self == other {
            Ordering::Equal
        } else if *other > self.end {
            Ordering::Greater
        } else {
            Ordering::Less
        })
    }
}

impl PartialOrd<ClassRange> for char {
    #[inline]
    fn partial_cmp(&self, other: &ClassRange) -> Option<Ordering> {
        other.partial_cmp(self).map(|o| o.reverse())
    }
}

impl ByteClass {
    /// Create a new class from an existing set of ranges.
    pub fn new(ranges: Vec<ByteRange>) -> ByteClass {
        ByteClass { ranges: ranges }
    }

    /// Returns true if `b` is matched by this byte class.
    pub fn matches(&self, b: u8) -> bool {
        self.binary_search_by(|range| b.partial_cmp(range).unwrap()).is_ok()
    }

    /// Removes the given byte from the class if it exists.
    ///
    /// Note that this takes `O(n)` time in the number of ranges.
    pub fn remove(&mut self, b: u8) {
        let mut i = match self.binary_search_by(|r| b.partial_cmp(r).unwrap()) {
            Ok(i) => i,
            Err(_) => return,
        };
        let mut r = self.ranges.remove(i);
        if r.start == b {
            r.start = b.saturating_add(1);
            if r.start > r.end || b == u8::MAX {
                return;
            }
            self.ranges.insert(i, r);
        } else if r.end == b {
            r.end = b.saturating_sub(1);
            if r.end < r.start || b == b'\x00' {
                return;
            }
            self.ranges.insert(0, r);
        } else {
            let (mut r1, mut r2) = (r.clone(), r.clone());
            r1.end = b.saturating_sub(1);
            if r1.start <= r1.end {
                self.ranges.insert(i, r1);
                i += 1;
            }
            r2.start = b.saturating_add(1);
            if r2.start <= r2.end {
                self.ranges.insert(i, r2);
            }
        }
    }

    /// Create a new empty class from this one.
    fn to_empty(&self) -> ByteClass {
        ByteClass { ranges: Vec::with_capacity(self.len()) }
    }

    /// Canonicalze any sequence of ranges.
    ///
    /// This is responsible for enforcing the canonical format invariants
    /// as described on the docs for the `ByteClass` type.
    fn canonicalize(mut self) -> ByteClass {
        // TODO: Save some cycles here by checking if already canonicalized.
        self.ranges.sort();
        let mut ordered = self.to_empty(); // TODO: Do this in place?
        for candidate in self {
            // If the candidate overlaps with an existing range, then it must
            // be the most recent range added because we process the candidates
            // in order.
            if let Some(or) = ordered.ranges.last_mut() {
                if or.overlapping(candidate) {
                    *or = or.merge(candidate);
                    continue;
                }
            }
            ordered.ranges.push(candidate);
        }
        ordered
    }

    /// Negates the byte class.
    ///
    /// For all `b` where `b` is a byte, `b` matches `self` if and only if `b`
    /// does not match `self.negate()`.
    pub fn negate(mut self) -> ByteClass {
        fn range(s: u8, e: u8) -> ByteRange { ByteRange::new(s, e) }

        if self.is_empty() {
            // Inverting an empty range yields all bytes.
            return ByteClass {
                ranges: vec![ByteRange { start: b'\x00', end: b'\xff' }],
            };
        }
        self = self.canonicalize();
        let mut inv = self.to_empty();
        if self[0].start > b'\x00' {
            inv.ranges.push(range(b'\x00', self[0].start.saturating_sub(1)));
        }
        for win in self.windows(2) {
            inv.ranges.push(range(win[0].end.saturating_add(1),
                                  win[1].start.saturating_sub(1)));
        }
        if self[self.len() - 1].end < u8::MAX {
            inv.ranges.push(range(self[self.len() - 1].end.saturating_add(1),
                                  u8::MAX));
        }
        inv
    }

    /// Apply case folding to this byte class.
    ///
    /// This assumes that the bytes in the ranges are ASCII compatible.
    ///
    /// N.B. Applying case folding to a negated character class probably
    /// won't produce the expected result. e.g., `(?i)[^x]` really should
    /// match any character sans `x` and `X`, but if `[^x]` is negated
    /// before being case folded, you'll end up matching any character.
    pub fn case_fold(self) -> ByteClass {
        let mut folded = self.to_empty();
        for r in self {
            folded.ranges.extend(r.case_fold());
        }
        folded.canonicalize()
    }

    /// Returns the number of bytes that match this class.
    fn num_bytes(&self) -> usize {
        self.ranges.iter()
            .map(|&r| 1 + (r.end as u32) - (r.start as u32))
            .fold(0, |acc, len| acc + len)
            as usize
    }
}

impl ByteRange {
    /// Create a new class range.
    ///
    /// If `end < start`, then the two values are swapped so that
    /// the invariant `start <= end` is preserved.
    fn new(start: u8, end: u8) -> ByteRange {
        if start <= end {
            ByteRange { start: start, end: end }
        } else {
            ByteRange { start: end, end: start }
        }
    }

    /// Returns true if and only if the two ranges are overlapping. Note that
    /// since ranges are inclusive, `a-c` and `d-f` are overlapping!
    fn overlapping(self, other: ByteRange) -> bool {
        max(self.start, other.start)
        <= min(self.end, other.end).saturating_add(1)
    }

    /// Returns true if and only if the intersection of self and other is non
    /// empty.
    fn is_intersect_empty(self, other: ByteRange) -> bool {
        max(self.start, other.start) > min(self.end, other.end)
    }

    /// Creates a new range representing the union of `self` and `other.
    fn merge(self, other: ByteRange) -> ByteRange {
        ByteRange {
            start: min(self.start, other.start),
            end: max(self.end, other.end),
        }
    }

    /// Apply case folding to this range.
    ///
    /// Since case folding might add bytes such that the range is no
    /// longer contiguous, this returns multiple byte ranges.
    ///
    /// This assumes that the bytes in this range are ASCII compatible.
    fn case_fold(self) -> Vec<ByteRange> {
        // So much easier than Unicode case folding!
        let mut ranges = vec![self];
        if !ByteRange::new(b'a', b'z').is_intersect_empty(self) {
            let lower = max(self.start, b'a');
            let upper = min(self.end, b'z');
            ranges.push(ByteRange::new(lower - 32, upper - 32));
        }
        if !ByteRange::new(b'A', b'Z').is_intersect_empty(self) {
            let lower = max(self.start, b'A');
            let upper = min(self.end, b'Z');
            ranges.push(ByteRange::new(lower + 32, upper + 32));
        }
        ranges
    }
}

impl Deref for ByteClass {
    type Target = Vec<ByteRange>;
    fn deref(&self) -> &Vec<ByteRange> { &self.ranges }
}

impl IntoIterator for ByteClass {
    type Item = ByteRange;
    type IntoIter = vec::IntoIter<ByteRange>;
    fn into_iter(self) -> vec::IntoIter<ByteRange> { self.ranges.into_iter() }
}

impl<'a> IntoIterator for &'a ByteClass {
    type Item = &'a ByteRange;
    type IntoIter = slice::Iter<'a, ByteRange>;
    fn into_iter(self) -> slice::Iter<'a, ByteRange> { self.iter() }
}

impl PartialEq<u8> for ByteRange {
    #[inline]
    fn eq(&self, other: &u8) -> bool {
        self.start <= *other && *other <= self.end
    }
}

impl PartialEq<ByteRange> for u8 {
    #[inline]
    fn eq(&self, other: &ByteRange) -> bool {
        other.eq(self)
    }
}

impl PartialOrd<u8> for ByteRange {
    #[inline]
    fn partial_cmp(&self, other: &u8) -> Option<Ordering> {
        Some(if self == other {
            Ordering::Equal
        } else if *other > self.end {
            Ordering::Greater
        } else {
            Ordering::Less
        })
    }
}

impl PartialOrd<ByteRange> for u8 {
    #[inline]
    fn partial_cmp(&self, other: &ByteRange) -> Option<Ordering> {
        other.partial_cmp(self).map(|o| o.reverse())
    }
}

/// This implementation of `Display` will write a regular expression from the
/// syntax tree. It does not write the original string parsed.
impl fmt::Display for Expr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Empty => write!(f, ""),
            Literal { ref chars, casei } => {
                if casei {
                    try!(write!(f, "(?iu:"));
                } else {
                    try!(write!(f, "(?u:"));
                }
                for &c in chars {
                    try!(write!(f, "{}", quote_char(c)));
                }
                try!(write!(f, ")"));
                Ok(())
            }
            LiteralBytes { ref bytes, casei } => {
                if casei {
                    try!(write!(f, "(?i-u:"));
                } else {
                    try!(write!(f, "(?-u:"));
                }
                for &b in bytes {
                    try!(write!(f, "{}", quote_byte(b)));
                }
                try!(write!(f, ")"));
                Ok(())
            }
            AnyChar => write!(f, "(?su:.)"),
            AnyCharNoNL => write!(f, "(?u:.)"),
            AnyByte => write!(f, "(?s-u:.)"),
            AnyByteNoNL => write!(f, "(?-u:.)"),
            Class(ref cls) => write!(f, "{}", cls),
            ClassBytes(ref cls) => write!(f, "{}", cls),
            StartLine => write!(f, "(?m:^)"),
            EndLine => write!(f, "(?m:$)"),
            StartText => write!(f, r"^"),
            EndText => write!(f, r"$"),
            WordBoundary => write!(f, r"(?u:\b)"),
            NotWordBoundary => write!(f, r"(?u:\B)"),
            WordBoundaryAscii => write!(f, r"(?-u:\b)"),
            NotWordBoundaryAscii => write!(f, r"(?-u:\B)"),
            Group { ref e, i: None, name: None } => write!(f, "(?:{})", e),
            Group { ref e, name: None, .. } => write!(f, "({})", e),
            Group { ref e, name: Some(ref n), .. } => {
                write!(f, "(?P<{}>{})", n, e)
            }
            Repeat { ref e, r, greedy } => {
                match &**e {
                    &Literal { ref chars, .. } if chars.len() > 1 => {
                        try!(write!(f, "(?:{}){}", e, r))
                    }
                    _ => try!(write!(f, "{}{}", e, r)),
                }
                if !greedy { try!(write!(f, "?")); }
                Ok(())
            }
            Concat(ref es) => {
                for e in es {
                    try!(write!(f, "{}", e));
                }
                Ok(())
            }
            Alternate(ref es) => {
                for (i, e) in es.iter().enumerate() {
                    if i > 0 { try!(write!(f, "|")); }
                    try!(write!(f, "{}", e));
                }
                Ok(())
            }
        }
    }
}

impl fmt::Display for Repeater {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ZeroOrOne => write!(f, "?"),
            ZeroOrMore => write!(f, "*"),
            OneOrMore => write!(f, "+"),
            Range { min: s, max: None } => write!(f, "{{{},}}", s),
            Range { min: s, max: Some(e) } if s == e => write!(f, "{{{}}}", s),
            Range { min: s, max: Some(e) } => write!(f, "{{{}, {}}}", s, e),
        }
    }
}

impl fmt::Display for CharClass {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        try!(write!(f, "(?u:["));
        for range in self.iter() {
            if range.start == '-' || range.end == '-' {
                try!(write!(f, "-"));
                break;
            }
        }
        for range in self.iter() {
            let mut range = *range;
            if range.start == '-' {
                range.start = ((range.start as u8) + 1) as char;
            }
            if range.end == '-' {
                range.end = ((range.end as u8) - 1) as char;
            }
            if range.start > range.end {
                continue;
            }
            try!(write!(f, "{}", range));
        }
        try!(write!(f, "])"));
        Ok(())
    }
}

impl fmt::Display for ClassRange {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}-{}", quote_char(self.start), quote_char(self.end))
    }
}

impl fmt::Display for ByteClass {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        try!(write!(f, "(?-u:["));
        for range in self.iter() {
            if range.start == b'-' || range.end == b'-' {
                try!(write!(f, "-"));
                break;
            }
        }
        for range in self.iter() {
            let mut range = *range;
            if range.start == b'-' {
                range.start += 1;
            }
            if range.end == b'-' {
                range.start -= 1;
            }
            if range.start > range.end {
                continue;
            }
            try!(write!(f, "{}", range));
        }
        try!(write!(f, "])"));
        Ok(())
    }
}

impl fmt::Display for ByteRange {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}-{}", quote_byte(self.start), quote_byte(self.end))
    }
}

/// An alias for computations that can return a `Error`.
pub type Result<T> = ::std::result::Result<T, Error>;

/// A parse error.
///
/// This includes details about the specific type of error and a rough
/// approximation of where it occurred.
#[derive(Clone, Debug, PartialEq)]
pub struct Error {
    pos: usize,
    surround: String,
    kind: ErrorKind,
}

/// The specific type of parse error that can occur.
#[derive(Clone, Debug, PartialEq)]
pub enum ErrorKind {
    /// A negation symbol is used twice in flag settings.
    /// e.g., `(?-i-s)`.
    DoubleFlagNegation,
    /// The same capture name was used more than once.
    /// e.g., `(?P<a>.)(?P<a>.)`.
    DuplicateCaptureName(String),
    /// An alternate is empty. e.g., `(|a)`.
    EmptyAlternate,
    /// A capture group name is empty. e.g., `(?P<>a)`.
    EmptyCaptureName,
    /// A negation symbol was not proceded by any flags. e.g., `(?i-)`.
    EmptyFlagNegation,
    /// A group is empty. e.g., `()`.
    EmptyGroup,
    /// An invalid number was used in a counted repetition. e.g., `a{b}`.
    InvalidBase10(String),
    /// An invalid hexadecimal number was used in an escape sequence.
    /// e.g., `\xAG`.
    InvalidBase16(String),
    /// An invalid capture name was used. e.g., `(?P<0a>b)`.
    InvalidCaptureName(String),
    /// An invalid class range was givien. Specifically, when the start of the
    /// range is greater than the end. e.g., `[z-a]`.
    InvalidClassRange {
        /// The first character specified in the range.
        start: char,
        /// The second character specified in the range.
        end: char,
    },
    /// An escape sequence was used in a character class where it is not
    /// allowed. e.g., `[a-\pN]` or `[\A]`.
    InvalidClassEscape(Expr),
    /// An invalid counted repetition min/max was given. e.g., `a{2,1}`.
    InvalidRepeatRange {
        /// The first number specified in the repetition.
        min: u32,
        /// The second number specified in the repetition.
        max: u32,
    },
    /// An invalid Unicode scalar value was used in a long hexadecimal
    /// sequence. e.g., `\x{D800}`.
    InvalidScalarValue(u32),
    /// An empty counted repetition operator. e.g., `a{}`.
    MissingBase10,
    /// A repetition operator was not applied to an expression. e.g., `*`.
    RepeaterExpectsExpr,
    /// A repetition operator was applied to an expression that cannot be
    /// repeated. e.g., `a+*` or `a|*`.
    RepeaterUnexpectedExpr(Expr),
    /// A capture group name that is never closed. e.g., `(?P<a`.
    UnclosedCaptureName(String),
    /// An unclosed hexadecimal literal. e.g., `\x{a`.
    UnclosedHex,
    /// An unclosed parenthesis. e.g., `(a`.
    UnclosedParen,
    /// An unclosed counted repetition operator. e.g., `a{2`.
    UnclosedRepeat,
    /// An unclosed named Unicode class. e.g., `\p{Yi`.
    UnclosedUnicodeName,
    /// Saw end of regex before class was closed. e.g., `[a`.
    UnexpectedClassEof,
    /// Saw end of regex before escape sequence was closed. e.g., `\`.
    UnexpectedEscapeEof,
    /// Saw end of regex before flags were closed. e.g., `(?i`.
    UnexpectedFlagEof,
    /// Saw end of regex before two hexadecimal digits were seen. e.g., `\xA`.
    UnexpectedTwoDigitHexEof,
    /// Unopened parenthesis. e.g., `)`.
    UnopenedParen,
    /// Unrecognized escape sequence. e.g., `\q`.
    UnrecognizedEscape(char),
    /// Unrecognized flag. e.g., `(?a)`.
    UnrecognizedFlag(char),
    /// Unrecognized named Unicode class. e.g., `\p{Foo}`.
    UnrecognizedUnicodeClass(String),
    /// Indicates that the regex uses too much nesting.
    ///
    /// (N.B. This error exists because traversing the Expr is recursive and
    /// an explicit heap allocated stack is not (yet?) used. Regardless, some
    /// sort of limit must be applied to avoid unbounded memory growth.
    StackExhausted,
    /// A disallowed flag was found (e.g., `u`).
    FlagNotAllowed(char),
    /// A Unicode class was used when the Unicode (`u`) flag was disabled.
    UnicodeNotAllowed,
    /// InvalidUtf8 indicates that the expression may match non-UTF-8 bytes.
    /// This never returned if the parser is permitted to allow expressions
    /// that match arbitrary bytes.
    InvalidUtf8,
    /// A character class was constructed such that it is empty.
    /// e.g., `[^\d\D]`.
    EmptyClass,
    /// Indicates that unsupported notation was used in a character class.
    ///
    /// The char in this error corresponds to the illegal character.
    ///
    /// The intent of this error is to carve a path to support set notation
    /// as described in UTS#18 RL1.3. We do this by rejecting regexes that
    /// would use the notation.
    ///
    /// The work around for end users is to escape the character included in
    /// this error message.
    UnsupportedClassChar(char),
    /// Hints that destructuring should not be exhaustive.
    ///
    /// This enum may grow additional variants, so this makes sure clients
    /// don't count on exhaustive matching. (Otherwise, adding a new variant
    /// could break existing code.)
    #[doc(hidden)]
    __Nonexhaustive,
}

impl Error {
    /// Returns an approximate *character* offset at which the error occurred.
    ///
    /// The character offset may be equal to the number of characters in the
    /// string, in which case it should be interpreted as pointing to the end
    /// of the regex.
    pub fn position(&self) -> usize {
        self.pos
    }

    /// Returns the type of the regex parse error.
    pub fn kind(&self) -> &ErrorKind {
        &self.kind
    }
}

impl ErrorKind {
    fn description(&self) -> &str {
        use ErrorKind::*;
        match *self {
            DoubleFlagNegation => "double flag negation",
            DuplicateCaptureName(_) => "duplicate capture name",
            EmptyAlternate => "empty alternate",
            EmptyCaptureName => "empty capture name",
            EmptyFlagNegation => "flag negation without any flags",
            EmptyGroup => "empty group (e.g., '()')",
            InvalidBase10(_) => "invalid base 10 number",
            InvalidBase16(_) => "invalid base 16 number",
            InvalidCaptureName(_) => "invalid capture name",
            InvalidClassRange{..} => "invalid character class range",
            InvalidClassEscape(_) => "invalid escape sequence in class",
            InvalidRepeatRange{..} => "invalid counted repetition range",
            InvalidScalarValue(_) => "invalid Unicode scalar value",
            MissingBase10 => "missing count in repetition operator",
            RepeaterExpectsExpr => "repetition operator missing expression",
            RepeaterUnexpectedExpr(_) => "expression cannot be repeated",
            UnclosedCaptureName(_) => "unclosed capture group name",
            UnclosedHex => "unclosed hexadecimal literal",
            UnclosedParen => "unclosed parenthesis",
            UnclosedRepeat => "unclosed counted repetition operator",
            UnclosedUnicodeName => "unclosed Unicode class literal",
            UnexpectedClassEof => "unexpected EOF in character class",
            UnexpectedEscapeEof => "unexpected EOF in escape sequence",
            UnexpectedFlagEof => "unexpected EOF in flags",
            UnexpectedTwoDigitHexEof => "unexpected EOF in hex literal",
            UnopenedParen => "unopened parenthesis",
            UnrecognizedEscape(_) => "unrecognized escape sequence",
            UnrecognizedFlag(_) => "unrecognized flag",
            UnrecognizedUnicodeClass(_) => "unrecognized Unicode class name",
            StackExhausted => "stack exhausted, too much nesting",
            FlagNotAllowed(_) => "flag not allowed",
            UnicodeNotAllowed => "Unicode features not allowed",
            InvalidUtf8 => "matching arbitrary bytes is not allowed",
            EmptyClass => "empty character class",
            UnsupportedClassChar(_) => "unsupported class notation",
            __Nonexhaustive => unreachable!(),
        }
    }
}

impl ::std::error::Error for Error {
    fn description(&self) -> &str {
        self.kind.description()
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if let ErrorKind::StackExhausted = self.kind {
            write!(f, "Error parsing regex: {}", self.kind)
        } else {
            write!(
                f, "Error parsing regex near '{}' at character offset {}: {}",
                self.surround, self.pos, self.kind)
        }
    }
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use ErrorKind::*;
        match *self {
            DoubleFlagNegation =>
                write!(f, "Only one negation symbol is allowed in flags."),
            DuplicateCaptureName(ref s) =>
                write!(f, "Capture name '{}' is used more than once.", s),
            EmptyAlternate =>
                write!(f, "Alternations cannot be empty."),
            EmptyCaptureName =>
                write!(f, "Capture names cannot be empty."),
            EmptyFlagNegation =>
                write!(f, "Flag negation requires setting at least one flag."),
            EmptyGroup =>
                write!(f, "Empty regex groups (e.g., '()') are not allowed."),
            InvalidBase10(ref s) =>
                write!(f, "Not a valid base 10 number: '{}'", s),
            InvalidBase16(ref s) =>
                write!(f, "Not a valid base 16 number: '{}'", s),
            InvalidCaptureName(ref s) =>
                write!(f, "Invalid capture name: '{}'. Capture names must \
                           consist of [_a-zA-Z0-9] and are not allowed to \
                           start with with a number.", s),
            InvalidClassRange { start, end } =>
                write!(f, "Invalid character class range '{}-{}'. \
                           Character class ranges must start with the smaller \
                           character, but {} > {}", start, end, start, end),
            InvalidClassEscape(ref e) =>
                write!(f, "Invalid escape sequence in character \
                           class: '{}'.", e),
            InvalidRepeatRange { min, max } =>
                write!(f, "Invalid counted repetition range: {{{}, {}}}. \
                           Counted repetition ranges must start with the \
                           minimum, but {} > {}", min, max, min, max),
            InvalidScalarValue(c) =>
                write!(f, "Number does not correspond to a Unicode scalar \
                           value: '{}'.", c),
            MissingBase10 =>
                write!(f, "Missing maximum in counted
repetition operator."),
            RepeaterExpectsExpr =>
                write!(f, "Missing expression for repetition operator."),
            RepeaterUnexpectedExpr(ref e) =>
                write!(f, "Invalid application of repetition operator to: \
                          '{}'.", e),
            UnclosedCaptureName(ref s) =>
                write!(f, "Capture name group for '{}' is not closed. \
                           (Missing a '>'.)", s),
            UnclosedHex =>
                write!(f, "Unclosed hexadecimal literal (missing a '}}')."),
            UnclosedParen =>
                write!(f, "Unclosed parenthesis."),
            UnclosedRepeat =>
                write!(f, "Unclosed counted repetition (missing a '}}')."),
            UnclosedUnicodeName =>
                write!(f, "Unclosed Unicode literal (missing a '}}')."),
            UnexpectedClassEof =>
                write!(f, "Character class was not closed before the end of \
                           the regex (missing a ']')."),
            UnexpectedEscapeEof =>
                write!(f, "Started an escape sequence that didn't finish \
                           before the end of the regex."),
            UnexpectedFlagEof =>
                write!(f, "Inline flag settings was not closed before the end \
                           of the regex (missing a ')' or ':')."),
            UnexpectedTwoDigitHexEof =>
                write!(f, "Unexpected end of two digit hexadecimal literal."),
            UnopenedParen =>
                write!(f, "Unopened parenthesis."),
            UnrecognizedEscape(c) =>
                write!(f, "Unrecognized escape sequence: '\\{}'.", c),
            UnrecognizedFlag(c) =>
                write!(f, "Unrecognized flag: '{}'. \
                           (Allowed flags: i, m, s, U, u, x.)", c),
            UnrecognizedUnicodeClass(ref s) =>
                write!(f, "Unrecognized Unicode class name: '{}'.", s),
            StackExhausted =>
                write!(f, "Exhausted space required to parse regex with too \
                           much nesting."),
            FlagNotAllowed(flag) =>
                write!(f, "Use of the flag '{}' is not allowed.", flag),
            UnicodeNotAllowed =>
                write!(f, "Unicode features are not allowed when the Unicode \
                           (u) flag is not set."),
            InvalidUtf8 =>
                write!(f, "Matching arbitrary bytes is not allowed."),
            EmptyClass =>
                write!(f, "Empty character classes are not allowed."),
            UnsupportedClassChar(c) =>
                write!(f, "Use of unescaped '{}' in character class is \
                           not allowed.", c),
            __Nonexhaustive => unreachable!(),
        }
    }
}

/// The result of binary search on the simple case folding table.
///
/// Note that this binary search is done on the "both" table, such that
/// the index returned corresponds to the *first* location of `c1` in the
/// table. The table can then be scanned linearly starting from the position
/// returned to find other case mappings for `c1`.
fn simple_case_fold_both_result(c1: char) -> result::Result<usize, usize> {
    let table = &case_folding::C_plus_S_both_table;
    let i = binary_search(table, |&(c2, _)| c1 <= c2);
    if i >= table.len() || table[i].0 != c1 {
        Err(i)
    } else {
        Ok(i)
    }
}

/// Binary search to find first element such that `pred(T) == true`.
///
/// Assumes that if `pred(xs[i]) == true` then `pred(xs[i+1]) == true`.
///
/// If all elements yield `pred(T) == false`, then `xs.len()` is returned.
fn binary_search<T, F>(xs: &[T], mut pred: F) -> usize
        where F: FnMut(&T) -> bool {
    let (mut left, mut right) = (0, xs.len());
    while left < right {
        let mid = (left + right) / 2;
        if pred(&xs[mid]) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    left
}

/// Escapes all regular expression meta characters in `text`.
///
/// The string returned may be safely used as a literal in a regular
/// expression.
pub fn escape(text: &str) -> String {
    let mut quoted = String::with_capacity(text.len());
    for c in text.chars() {
        if parser::is_punct(c) {
            quoted.push('\\');
        }
        quoted.push(c);
    }
    quoted
}

fn quote_char(c: char) -> String {
    let mut s = String::new();
    if parser::is_punct(c) {
        s.push('\\');
    }
    s.push(c);
    s
}

fn quote_byte(b: u8) -> String {
    if parser::is_punct(b as char) || b == b'\'' || b == b'"' {
        quote_char(b as char)
    } else {
        let escaped: Vec<u8> = ascii::escape_default(b).collect();
        String::from_utf8(escaped).unwrap()
    }
}

fn inc_char(c: char) -> char {
    match c {
        char::MAX => char::MAX,
        '\u{D7FF}' => '\u{E000}',
        c => char::from_u32(c as u32 + 1).unwrap(),
    }
}

fn dec_char(c: char) -> char {
    match c {
        '\x00' => '\x00',
        '\u{E000}' => '\u{D7FF}',
        c => char::from_u32(c as u32 - 1).unwrap(),
    }
}

/// Returns true if and only if `c` is a word character.
#[doc(hidden)]
pub fn is_word_char(c: char) -> bool {
    match c {
        '_' | '0' ... '9' | 'a' ... 'z' | 'A' ... 'Z'  => true,
        _ => ::unicode::regex::PERLW.binary_search_by(|&(start, end)| {
            if c >= start && c <= end {
                Ordering::Equal
            } else if start > c {
                Ordering::Greater
            } else {
                Ordering::Less
            }
        }).is_ok(),
    }
}

/// Returns true if and only if `c` is an ASCII word byte.
#[doc(hidden)]
pub fn is_word_byte(b: u8) -> bool {
    match b {
        b'_' | b'0' ... b'9' | b'a' ... b'z' | b'A' ... b'Z'  => true,
        _ => false,
    }
}

#[cfg(test)]
mod properties;

#[cfg(test)]
mod tests {
    use {CharClass, ClassRange, ByteClass, ByteRange, Expr};

    fn class(ranges: &[(char, char)]) -> CharClass {
        let ranges = ranges.iter().cloned()
                           .map(|(c1, c2)| ClassRange::new(c1, c2)).collect();
        CharClass::new(ranges)
    }

    fn bclass(ranges: &[(u8, u8)]) -> ByteClass {
        let ranges = ranges.iter().cloned()
                           .map(|(c1, c2)| ByteRange::new(c1, c2)).collect();
        ByteClass::new(ranges)
    }

    fn e(re: &str) -> Expr { Expr::parse(re).unwrap() }

    #[test]
    fn stack_exhaustion() {
        use std::iter::repeat;

        let open: String = repeat('(').take(200).collect();
        let close: String = repeat(')').take(200).collect();
        assert!(Expr::parse(&format!("{}a{}", open, close)).is_ok());

        let open: String = repeat('(').take(200 + 1).collect();
        let close: String = repeat(')').take(200 + 1).collect();
        assert!(Expr::parse(&format!("{}a{}", open, close)).is_err());
    }

    #[test]
    fn anchored_start() {
        assert!(e("^a").is_anchored_start());
        assert!(e("(^a)").is_anchored_start());
        assert!(e("^a|^b").is_anchored_start());
        assert!(e("(^a)|(^b)").is_anchored_start());
        assert!(e("(^(a|b))").is_anchored_start());

        assert!(!e("^a|b").is_anchored_start());
        assert!(!e("a|^b").is_anchored_start());
    }

    #[test]
    fn anchored_end() {
        assert!(e("a$").is_anchored_end());
        assert!(e("(a$)").is_anchored_end());
        assert!(e("a$|b$").is_anchored_end());
        assert!(e("(a$)|(b$)").is_anchored_end());
        assert!(e("((a|b)$)").is_anchored_end());

        assert!(!e("a$|b").is_anchored_end());
        assert!(!e("a|b$").is_anchored_end());
    }

    #[test]
    fn class_canon_no_change() {
        let cls = class(&[('a', 'c'), ('x', 'z')]);
        assert_eq!(cls.clone().canonicalize(), cls);
    }

    #[test]
    fn class_canon_unordered() {
        let cls = class(&[('x', 'z'), ('a', 'c')]);
        assert_eq!(cls.canonicalize(), class(&[
            ('a', 'c'), ('x', 'z'),
        ]));
    }

    #[test]
    fn class_canon_overlap() {
        let cls = class(&[('x', 'z'), ('w', 'y')]);
        assert_eq!(cls.canonicalize(), class(&[
            ('w', 'z'),
        ]));
    }

    #[test]
    fn class_canon_overlap_many() {
        let cls = class(&[
            ('c', 'f'), ('a', 'g'), ('d', 'j'), ('a', 'c'),
            ('m', 'p'), ('l', 's'),
        ]);
        assert_eq!(cls.clone().canonicalize(), class(&[
            ('a', 'j'), ('l', 's'),
        ]));
    }

    #[test]
    fn class_canon_overlap_boundary() {
        let cls = class(&[('x', 'z'), ('u', 'w')]);
        assert_eq!(cls.canonicalize(), class(&[
            ('u', 'z'),
        ]));
    }

    #[test]
    fn class_canon_extreme_edge_case() {
        let cls = class(&[('\x00', '\u{10FFFF}'), ('\x00', '\u{10FFFF}')]);
        assert_eq!(cls.canonicalize(), class(&[
            ('\x00', '\u{10FFFF}'),
        ]));
    }

    #[test]
    fn class_canon_singles() {
        let cls = class(&[('a', 'a'), ('b', 'b')]);
        assert_eq!(cls.canonicalize(), class(&[('a', 'b')]));
    }

    #[test]
    fn class_negate_single() {
        let cls = class(&[('a', 'a')]);
        assert_eq!(cls.negate(), class(&[
            ('\x00', '\x60'), ('\x62', '\u{10FFFF}'),
        ]));
    }

    #[test]
    fn class_negate_singles() {
        let cls = class(&[('a', 'a'), ('b', 'b')]);
        assert_eq!(cls.negate(), class(&[
            ('\x00', '\x60'), ('\x63', '\u{10FFFF}'),
        ]));
    }

    #[test]
    fn class_negate_multiples() {
        let cls = class(&[('a', 'c'), ('x', 'z')]);
        assert_eq!(cls.negate(), class(&[
            ('\x00', '\x60'), ('\x64', '\x77'), ('\x7b', '\u{10FFFF}'),
        ]));
    }

    #[test]
    fn class_negate_min_scalar() {
        let cls = class(&[('\x00', 'a')]);
        assert_eq!(cls.negate(), class(&[
            ('\x62', '\u{10FFFF}'),
        ]));
    }

    #[test]
    fn class_negate_max_scalar() {
        let cls = class(&[('a', '\u{10FFFF}')]);
        assert_eq!(cls.negate(), class(&[
            ('\x00', '\x60'),
        ]));
    }

    #[test]
    fn class_negate_everything() {
        let cls = class(&[('\x00', '\u{10FFFF}')]);
        assert_eq!(cls.negate(), class(&[]));
    }

    #[test]
    fn class_negate_everything_sans_one() {
        let cls = class(&[
            ('\x00', '\u{10FFFD}'), ('\u{10FFFF}', '\u{10FFFF}')
        ]);
        assert_eq!(cls.negate(), class(&[
            ('\u{10FFFE}', '\u{10FFFE}'),
        ]));
    }

    #[test]
    fn class_negate_surrogates_min() {
        let cls = class(&[('\x00', '\u{D7FF}')]);
        assert_eq!(cls.negate(), class(&[
            ('\u{E000}', '\u{10FFFF}'),
        ]));
    }

    #[test]
    fn class_negate_surrogates_min_edge() {
        let cls = class(&[('\x00', '\u{D7FE}')]);
        assert_eq!(cls.negate(), class(&[
            ('\u{D7FF}', '\u{10FFFF}'),
        ]));
    }

    #[test]
    fn class_negate_surrogates_max() {
        let cls = class(&[('\u{E000}', '\u{10FFFF}')]);
        assert_eq!(cls.negate(), class(&[
            ('\x00', '\u{D7FF}'),
        ]));
    }

    #[test]
    fn class_negate_surrogates_max_edge() {
        let cls = class(&[('\u{E001}', '\u{10FFFF}')]);
        assert_eq!(cls.negate(), class(&[
            ('\x00', '\u{E000}'),
        ]));
    }

    #[test]
    fn class_canon_overlap_many_case_fold() {
        let cls = class(&[
            ('C', 'F'), ('A', 'G'), ('D', 'J'), ('A', 'C'),
            ('M', 'P'), ('L', 'S'), ('c', 'f'),
        ]);
        assert_eq!(cls.case_fold(), class(&[
            ('A', 'J'), ('L', 'S'),
            ('a', 'j'), ('l', 's'),
            ('\u{17F}', '\u{17F}'),
        ]));

        let cls = bclass(&[
            (b'C', b'F'), (b'A', b'G'), (b'D', b'J'), (b'A', b'C'),
            (b'M', b'P'), (b'L', b'S'), (b'c', b'f'),
        ]);
        assert_eq!(cls.case_fold(), bclass(&[
            (b'A', b'J'), (b'L', b'S'),
            (b'a', b'j'), (b'l', b's'),
        ]));
    }

    #[test]
    fn class_fold_az() {
        let cls = class(&[('A', 'Z')]);
        assert_eq!(cls.case_fold(), class(&[
            ('A', 'Z'), ('a', 'z'),
            ('\u{17F}', '\u{17F}'),
            ('\u{212A}', '\u{212A}'),
        ]));
        let cls = class(&[('a', 'z')]);
        assert_eq!(cls.case_fold(), class(&[
            ('A', 'Z'), ('a', 'z'),
            ('\u{17F}', '\u{17F}'),
            ('\u{212A}', '\u{212A}'),
        ]));

        let cls = bclass(&[(b'A', b'Z')]);
        assert_eq!(cls.case_fold(), bclass(&[
            (b'A', b'Z'), (b'a', b'z'),
        ]));
        let cls = bclass(&[(b'a', b'z')]);
        assert_eq!(cls.case_fold(), bclass(&[
            (b'A', b'Z'), (b'a', b'z'),
        ]));
    }

    #[test]
    fn class_fold_a_underscore() {
        let cls = class(&[('A', 'A'), ('_', '_')]);
        assert_eq!(cls.clone().canonicalize(), class(&[
            ('A', 'A'), ('_', '_'),
        ]));
        assert_eq!(cls.case_fold(), class(&[
            ('A', 'A'), ('_', '_'), ('a', 'a'),
        ]));

        let cls = bclass(&[(b'A', b'A'), (b'_', b'_')]);
        assert_eq!(cls.clone().canonicalize(), bclass(&[
            (b'A', b'A'), (b'_', b'_'),
        ]));
        assert_eq!(cls.case_fold(), bclass(&[
            (b'A', b'A'), (b'_', b'_'), (b'a', b'a'),
        ]));
    }

    #[test]
    fn class_fold_a_equals() {
        let cls = class(&[('A', 'A'), ('=', '=')]);
        assert_eq!(cls.clone().canonicalize(), class(&[
            ('=', '='), ('A', 'A'),
        ]));
        assert_eq!(cls.case_fold(), class(&[
            ('=', '='), ('A', 'A'), ('a', 'a'),
        ]));

        let cls = bclass(&[(b'A', b'A'), (b'=', b'=')]);
        assert_eq!(cls.clone().canonicalize(), bclass(&[
            (b'=', b'='), (b'A', b'A'),
        ]));
        assert_eq!(cls.case_fold(), bclass(&[
            (b'=', b'='), (b'A', b'A'), (b'a', b'a'),
        ]));
    }

    #[test]
    fn class_fold_no_folding_needed() {
        let cls = class(&[('\x00', '\x10')]);
        assert_eq!(cls.case_fold(), class(&[
            ('\x00', '\x10'),
        ]));

        let cls = bclass(&[(b'\x00', b'\x10')]);
        assert_eq!(cls.case_fold(), bclass(&[
            (b'\x00', b'\x10'),
        ]));
    }

    #[test]
    fn class_fold_negated() {
        let cls = class(&[('x', 'x')]);
        assert_eq!(cls.clone().case_fold(), class(&[
            ('X', 'X'), ('x', 'x'),
        ]));
        assert_eq!(cls.case_fold().negate(), class(&[
            ('\x00', 'W'), ('Y', 'w'), ('y', '\u{10FFFF}'),
        ]));

        let cls = bclass(&[(b'x', b'x')]);
        assert_eq!(cls.clone().case_fold(), bclass(&[
            (b'X', b'X'), (b'x', b'x'),
        ]));
        assert_eq!(cls.case_fold().negate(), bclass(&[
            (b'\x00', b'W'), (b'Y', b'w'), (b'y', b'\xff'),
        ]));
    }

    #[test]
    fn class_fold_single_to_multiple() {
        let cls = class(&[('k', 'k')]);
        assert_eq!(cls.case_fold(), class(&[
            ('K', 'K'), ('k', 'k'), ('\u{212A}', '\u{212A}'),
        ]));

        let cls = bclass(&[(b'k', b'k')]);
        assert_eq!(cls.case_fold(), bclass(&[
            (b'K', b'K'), (b'k', b'k'),
        ]));
    }

    #[test]
    fn class_fold_at() {
        let cls = class(&[('@', '@')]);
        assert_eq!(cls.clone().canonicalize(), class(&[('@', '@')]));
        assert_eq!(cls.case_fold(), class(&[('@', '@')]));

        let cls = bclass(&[(b'@', b'@')]);
        assert_eq!(cls.clone().canonicalize(), bclass(&[(b'@', b'@')]));
        assert_eq!(cls.case_fold(), bclass(&[(b'@', b'@')]));
    }

    #[test]
    fn roundtrip_class_hypen() {
        let expr = e("[-./]");
        assert_eq!("(?u:[-\\.-/])", expr.to_string());

        let expr = e("(?-u)[-./]");
        assert_eq!("(?-u:[-\\.-/])", expr.to_string());
    }
}
