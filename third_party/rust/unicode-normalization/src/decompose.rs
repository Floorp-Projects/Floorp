// Copyright 2012-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
use std::fmt::{self, Write};

#[derive(Clone)]
enum DecompositionType {
    Canonical,
    Compatible
}

/// External iterator for a string decomposition's characters.
#[derive(Clone)]
pub struct Decompositions<I> {
    kind: DecompositionType,
    iter: I,
    done: bool,

    // This buffer stores pairs of (canonical combining class, character),
    // pushed onto the end in text order.
    //
    // It's split into two contiguous regions by the `ready` offset.  The first
    // `ready` pairs are sorted and ready to emit on demand.  The "pending"
    // suffix afterwards still needs more characters for us to be able to sort
    // in canonical order and is not safe to emit.
    buffer: Vec<(u8, char)>,
    ready: usize,
}

#[inline]
pub fn new_canonical<I: Iterator<Item=char>>(iter: I) -> Decompositions<I> {
    Decompositions {
        kind: self::DecompositionType::Canonical,
        iter: iter,
        done: false,
        buffer: Vec::new(),
        ready: 0,
    }
}

#[inline]
pub fn new_compatible<I: Iterator<Item=char>>(iter: I) -> Decompositions<I> {
    Decompositions {
        kind: self::DecompositionType::Compatible,
        iter: iter,
        done: false,
        buffer: Vec::new(),
        ready: 0,
    }
}

impl<I> Decompositions<I> {
    #[inline]
    fn push_back(&mut self, ch: char) {
        let class = super::char::canonical_combining_class(ch);
        if class == 0 {
            self.sort_pending();
        }
        self.buffer.push((class, ch));
    }

    #[inline]
    fn sort_pending(&mut self) {
        if self.ready == 0 && self.buffer.is_empty() {
            return;
        }

        // NB: `sort_by_key` is stable, so it will preserve the original text's
        // order within a combining class.
        self.buffer[self.ready..].sort_by_key(|k| k.0);
        self.ready = self.buffer.len();
    }

    #[inline]
    fn pop_front(&mut self) -> Option<char> {
        if self.ready == 0 {
            None
        } else {
            self.ready -= 1;
            Some(self.buffer.remove(0).1)
        }
    }
}

impl<I: Iterator<Item=char>> Iterator for Decompositions<I> {
    type Item = char;

    #[inline]
    fn next(&mut self) -> Option<char> {
        while self.ready == 0 && !self.done {
            match (self.iter.next(), &self.kind) {
                (Some(ch), &DecompositionType::Canonical) => {
                    super::char::decompose_canonical(ch, |d| self.push_back(d));
                },
                (Some(ch), &DecompositionType::Compatible) => {
                    super::char::decompose_compatible(ch, |d| self.push_back(d));
                },
                (None, _) => {
                    self.sort_pending();
                    self.done = true;
                },
            }
        }
        self.pop_front()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let (lower, _) = self.iter.size_hint();
        (lower, None)
    }
}

impl<I: Iterator<Item=char> + Clone> fmt::Display for Decompositions<I> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for c in self.clone() {
            f.write_char(c)?;
        }
        Ok(())
    }
}
