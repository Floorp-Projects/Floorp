// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use quickcheck::{Arbitrary, Gen, Testable, QuickCheck, StdGen};

use {
    Expr, ExprBuilder,
    CharClass, ClassRange, ByteClass, ByteRange, Repeater, dec_char,
};

fn qc<T: Testable>(t: T) {
    QuickCheck::new()
        .tests(10_000)
        .max_tests(20_000)
        .quickcheck(t);
}

fn class(ranges: &[(char, char)]) -> CharClass {
    let ranges = ranges.iter().cloned()
                       .map(|(c1, c2)| ClassRange::new(c1, c2)).collect();
    CharClass::new(ranges)
}

// Test invariants for canonicalizing character classes.

#[test]
fn negate() {
    fn prop(ranges: Vec<(char, char)>) -> bool {
        let expected = class(&ranges).canonicalize();
        let got = class(&ranges).negate().negate();
        expected == got
    }
    qc(prop as fn(Vec<(char, char)>) -> bool);
}

#[test]
fn classes_are_sorted_and_nonoverlapping() {
    fn prop(ranges: Vec<(char, char)>) -> bool {
        class(&ranges)
            .canonicalize()
            .windows(2)
            .all(|w| w[0].end < dec_char(w[1].start))
    }
    qc(prop as fn(Vec<(char, char)>) -> bool);
}

#[test]
fn valid_class_ranges() {
    fn prop(ranges: Vec<(char, char)>) -> bool {
        class(&ranges).canonicalize().into_iter().all(|r| r.start <= r.end)
    }
    qc(prop as fn(Vec<(char, char)>) -> bool);
}

/// A wrapper type for generating "regex-like" Unicode strings.
///
/// In particular, this type's `Arbitrary` impl specifically biases toward
/// special regex characters to make test cases more interesting.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
struct RegexLikeString(String);

impl Arbitrary for RegexLikeString {
    fn arbitrary<G: Gen>(g: &mut G) -> RegexLikeString {
        const SPECIAL: &'static [char] = &[
            '\\', '.', '+', '*', '?', '(', ')', '|', '[', ']', '{', '}',
            '^', '$',
        ];
        // Generating random Unicode strings results in mostly uninteresting
        // regexes. Namely, they'll mostly just be literals.
        // To make properties using regex strings more interesting, we bias
        // toward selecting characters of significance to a regex.
        let size = { let s = g.size(); g.gen_range(0, s) };
        RegexLikeString((0..size).map(|_| {
            if g.gen_weighted_bool(3) {
                *g.choose(SPECIAL).unwrap()
            } else {
                g.gen()
            }
        }).collect())
    }

    fn shrink(&self) -> Box<Iterator<Item=RegexLikeString>> {
        // The regular `String` shrinker is good enough.
        Box::new(self.0.shrink().map(RegexLikeString))
    }
}

/// A special type for generating small non-zero sized ASCII strings.
#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
struct SmallAscii(String);

impl Arbitrary for SmallAscii {
    fn arbitrary<G: Gen>(g: &mut G) -> SmallAscii {
        use std::char::from_u32;
        let size = g.gen_range(1, 5);
        SmallAscii((0..size)
                   .map(|_| from_u32(g.gen_range(97, 123)).unwrap())
                   .collect())
    }

    fn shrink(&self) -> Box<Iterator<Item=SmallAscii>> {
        Box::new(self.0.shrink().map(SmallAscii))
    }
}

#[test]
fn parser_never_panics() {
    fn prop(s: RegexLikeString) -> bool {
        let _ = Expr::parse(&s.0); true
    }
    qc(prop as fn(RegexLikeString) -> bool);
}

// Testing entire expressions.
//
// We only have one test at the moment, but the machinery could be useful
// for other things.
//
// In particular, Russ Cox writes about testing regexes by comparing the
// strings they match with other regex implementations. A fuzzer/shrinker
// (which is what's implemented below) would be a great way to drive that
// process. ---AG

impl Arbitrary for Expr {
    fn arbitrary<G: Gen>(g: &mut G) -> Expr {
        let e = fix_capture_indices(gen_expr(g, 0, ExprType::Anything));
        e.simplify(200).unwrap()
    }

    fn shrink(&self) -> Box<Iterator<Item=Expr>> {
        use Expr::*;

        let nada = || Box::new(None.into_iter());
        let es: Box<Iterator<Item=Expr>> = match *self {
            Empty | AnyChar | AnyCharNoNL | AnyByte | AnyByteNoNL
            | StartLine | EndLine | StartText | EndText
            | WordBoundary | NotWordBoundary
            | WordBoundaryAscii | NotWordBoundaryAscii => nada(),
            Literal { ref chars, .. } if chars.len() == 1 => nada(),
            Literal { ref chars, casei } => {
                Box::new((chars.clone(), casei)
                         .shrink()
                         .filter(|&(ref chars, _)| chars.len() > 0)
                         .map(|(chars, casei)| {
                             Literal { chars: chars, casei: casei }
                         }))
            }
            LiteralBytes { ref bytes, .. } if bytes.len() == 1 => nada(),
            LiteralBytes { ref bytes, casei } => {
                Box::new((bytes.clone(), casei)
                         .shrink()
                         .filter(|&(ref bytes, _)| bytes.len() > 0)
                         .map(|(bytes, casei)| {
                             LiteralBytes { bytes: bytes, casei: casei }
                         }))
            }
            Class(ref cls) => Box::new(cls.shrink().map(Class)),
            ClassBytes(ref cls) => Box::new(cls.shrink().map(ClassBytes)),
            Group { ref e, ref i, ref name } => {
                let (i, name) = (i.clone(), name.clone());
                Box::new(e.clone().shrink()
                          .chain(e.clone().shrink()
                                  .map(move |e| Group {
                                      e: Box::new(e),
                                      i: i.clone(),
                                      name: name.clone(),
                                  })))
            }
            Repeat { ref e, ref r, greedy } => {
                Box::new((*e.clone(), r.clone())
                         .shrink()
                         .filter(|&(ref e, _)| e.can_repeat())
                         .map(move |(e, r)| Repeat {
                             e: Box::new(e),
                             r: r,
                             greedy: greedy,
                         }))
            }
            // Concat(ref es) if es.len() <= 2 => nada(),
            Concat(ref es) => {
                Box::new(es.clone()
                           .shrink()
                           .filter(|es| es.len() > 0)
                           .map(|mut es| if es.len() == 1 {
                               es.pop().unwrap()
                           } else {
                               Concat(es)
                           }))
            }
            // Alternate(ref es) if es.len() <= 2 => nada(),
            Alternate(ref es) => {
                Box::new(es.clone()
                           .shrink()
                           .filter(|es| es.len() > 0)
                           .map(|mut es| if es.len() == 1 {
                               es.pop().unwrap()
                           } else {
                               Alternate(es)
                           }))
            }
        };
        Box::new(es.map(|e| fix_capture_indices(e).simplify(200).unwrap()))
    }
}

enum ExprType {
    NoSequences, // disallow concat/alternate
    Anything,
}

fn gen_expr<G: Gen>(g: &mut G, depth: u32, ty: ExprType) -> Expr {
    use Expr::*;
    let ub = match (depth as usize >= g.size(), ty) {
        (true, _) => 16,
        (false, ExprType::NoSequences) => 18,
        (false, ExprType::Anything) => 20,
    };
    match g.gen_range(1, ub) {
        0 => Empty,
        1 => Literal {
            chars: SmallAscii::arbitrary(g).0.chars().collect(),
            casei: g.gen(),
        },
        2 => LiteralBytes {
            bytes: SmallAscii::arbitrary(g).0.as_bytes().to_owned(),
            casei: g.gen(),
        },
        3 => AnyChar,
        4 => AnyCharNoNL,
        5 => AnyByte,
        6 => AnyByteNoNL,
        7 => Class(CharClass::arbitrary(g)),
        8 => StartLine,
        9 => EndLine,
        10 => StartText,
        11 => EndText,
        12 => WordBoundary,
        13 => NotWordBoundary,
        14 => WordBoundaryAscii,
        15 => NotWordBoundaryAscii,
        16 => gen_group_expr(g, depth + 1),
        17 => Repeat {
            e: Box::new(gen_repeatable_expr(g, depth + 1)),
            r: Repeater::arbitrary(g),
            greedy: bool::arbitrary(g),
        },
        18 => {
            let size = { let s = g.size(); g.gen_range(2, s) };
            Concat((0..size)
                   .map(|_| {
                       gen_expr(g, depth + 1, ExprType::NoSequences)
                    })
                   .collect())
        }
        19 => {
            let size = { let s = g.size(); g.gen_range(2, s) };
            Alternate((0..size)
                      .map(|_| {
                          gen_expr(g, depth + 1, ExprType::NoSequences)
                      })
                      .collect())
        }
        _ => unreachable!()
    }
}

fn gen_repeatable_expr<G: Gen>(g: &mut G, depth: u32) -> Expr {
    use Expr::*;
    match g.gen_range(1, 10) {
        0 => Empty,
        1 => Literal {
            chars: vec![Arbitrary::arbitrary(g)],
            casei: g.gen(),
        },
        2 => LiteralBytes {
            bytes: vec![Arbitrary::arbitrary(g)],
            casei: g.gen(),
        },
        3 => AnyChar,
        4 => AnyCharNoNL,
        5 => AnyByte,
        6 => AnyByteNoNL,
        7 => Class(CharClass::arbitrary(g)),
        8 => ClassBytes(ByteClass::arbitrary(g)),
        9 => gen_group_expr(g, depth + 1),
        _ => unreachable!(),
    }
}

fn gen_group_expr<G: Gen>(g: &mut G, depth: u32) -> Expr {
    let (i, name) = if g.gen() {
        (None, None)
    } else {
        (Some(0), if g.gen() {
            Some(SmallAscii::arbitrary(g).0)
        } else {
            None
        })
    };
    Expr::Group {
        e: Box::new(gen_expr(g, depth + 1, ExprType::Anything)),
        i: i,
        name: name,
    }
}

fn fix_capture_indices(e: Expr) -> Expr {
    fn bx(e: Expr) -> Box<Expr> { Box::new(e) }
    fn fix(e: Expr, capi: &mut usize, names: &mut Vec<String>) -> Expr {
        use Expr::*;
        match e {
            Group { e, i: Some(_), mut name } => {
                *capi += 1;
                let i = *capi;
                let mut dupe_name = false;
                if let Some(ref n1) = name {
                    if names.iter().any(|n2| n1 == n2) {
                        dupe_name = true;
                    } else {
                        names.push(n1.clone());
                    }
                }
                if dupe_name { name = None; }
                Group { e: bx(fix(*e, capi, names)), i: Some(i), name: name }
            }
            Group { e, i, name } => {
                Group { e: bx(fix(*e, capi, names)), i: i, name: name }
            }
            Repeat { e, r, greedy } => {
                Repeat { e: bx(fix(*e, capi, names)), r: r, greedy: greedy }
            }
            Concat(es) =>
                Concat(es.into_iter().map(|e| fix(e, capi, names)).collect()),
            Alternate(es) =>
                Alternate(es.into_iter().map(|e| fix(e, capi, names)).collect()),
            e => e,
        }
    }
    fix(e, &mut 0, &mut vec![])
}

impl Arbitrary for Repeater {
    fn arbitrary<G: Gen>(g: &mut G) -> Repeater {
        use Repeater::*;
        match g.gen_range(0, 4) {
            0 => ZeroOrOne,
            1 => ZeroOrMore,
            2 => OneOrMore,
            3 => {
                use std::cmp::{max, min};
                let n1 = Arbitrary::arbitrary(g);
                let n2 = Arbitrary::arbitrary(g);
                Range {
                    min: min(n1, n2),
                    max: if g.gen() { None } else { Some(max(n1, n2)) },
                }
            },
            _ => unreachable!(),
        }
    }

    fn shrink(&self) -> Box<Iterator<Item=Repeater>> {
        use Repeater::*;
        match *self {
            ZeroOrOne | ZeroOrMore | OneOrMore => Box::new(None.into_iter()),
            Range { min, max } => {
                Box::new((min, max)
                         .shrink()
                         .map(|(min, max)| Range { min: min, max: max }))
            }
        }
    }
}

impl Arbitrary for CharClass {
    fn arbitrary<G: Gen>(g: &mut G) -> CharClass {
        let mut ranges: Vec<ClassRange> = Arbitrary::arbitrary(g);
        if ranges.is_empty() {
            ranges.push(Arbitrary::arbitrary(g));
        }
        let cls = CharClass { ranges: ranges }.canonicalize();
        if g.gen() { cls.case_fold() } else { cls }
    }

    fn shrink(&self) -> Box<Iterator<Item=CharClass>> {
        Box::new(self.ranges.clone()
                 .shrink()
                 .filter(|ranges| ranges.len() > 0)
                 .map(|ranges| CharClass { ranges: ranges }.canonicalize()))
    }
}

impl Arbitrary for ClassRange {
    fn arbitrary<G: Gen>(g: &mut G) -> ClassRange {
        use std::char::from_u32;
        ClassRange::new(
            from_u32(g.gen_range(97, 123)).unwrap(),
            from_u32(g.gen_range(97, 123)).unwrap(),
        )
    }

    fn shrink(&self) -> Box<Iterator<Item=ClassRange>> {
        Box::new((self.start, self.end)
                 .shrink().map(|(s, e)| ClassRange::new(s, e)))
    }
}

impl Arbitrary for ByteClass {
    fn arbitrary<G: Gen>(g: &mut G) -> ByteClass {
        let mut ranges: Vec<ByteRange> = Arbitrary::arbitrary(g);
        if ranges.is_empty() {
            ranges.push(Arbitrary::arbitrary(g));
        }
        let cls = ByteClass { ranges: ranges }.canonicalize();
        if g.gen() { cls.case_fold() } else { cls }
    }

    fn shrink(&self) -> Box<Iterator<Item=ByteClass>> {
        Box::new(self.ranges.clone()
                 .shrink()
                 .filter(|ranges| ranges.len() > 0)
                 .map(|ranges| ByteClass { ranges: ranges }.canonicalize()))
    }
}

impl Arbitrary for ByteRange {
    fn arbitrary<G: Gen>(g: &mut G) -> ByteRange {
        ByteRange::new(g.gen_range(97, 123), g.gen_range(97, 123))
    }

    fn shrink(&self) -> Box<Iterator<Item=ByteRange>> {
        Box::new((self.start, self.end)
                 .shrink().map(|(s, e)| ByteRange::new(s, e)))
    }
}

#[test]
fn display_regex_roundtrips() {
    // Given an AST, if we print it as a regex and then re-parse it, do we
    // get back the same AST?
    // A lot of this relies crucially on regex simplification. So this is
    // testing `Expr::simplify` as much as it is testing the `Display` impl.
    fn prop(e: Expr) -> bool {
        let parser = ExprBuilder::new().allow_bytes(true);
        e == parser.parse(&e.to_string()).unwrap()
    }
    QuickCheck::new()
        .tests(10_000)
        .max_tests(20_000)
        .gen(StdGen::new(::rand::thread_rng(), 50))
        .quickcheck(prop as fn(Expr) -> bool);
}
