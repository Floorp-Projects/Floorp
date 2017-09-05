// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use re_trait::{RegularExpression, Slot, Locations, as_slots};

/// Plugin is the compiler plugin's data structure. It declare some static
/// data (like capture groups and the original regex string), but defines its
/// matching engine as a simple function.
#[doc(hidden)]
pub struct Plugin {
    #[doc(hidden)]
    pub original: &'static str,
    #[doc(hidden)]
    pub names: &'static &'static [Option<&'static str>],
    #[doc(hidden)]
    pub groups: &'static &'static [(&'static str, usize)],
    #[doc(hidden)]
    pub prog: fn(&mut [Slot], &str, usize) -> bool,
}

impl Copy for Plugin {}

impl Clone for Plugin {
    fn clone(&self) -> Plugin {
        *self
    }
}

impl RegularExpression for Plugin {
    type Text = str;

    fn slots_len(&self) -> usize {
        self.names.len() * 2
    }

    fn next_after_empty(&self, text: &str, i: usize) -> usize {
        let b = match text.as_bytes().get(i) {
            None => return text.len() + 1,
            Some(&b) => b,
        };
        let inc = if b <= 0x7F {
            1
        } else if b <= 0b110_11111 {
            2
        } else if b <= 0b1110_1111 {
            3
        } else {
            4
        };
        i + inc
    }

    fn shortest_match_at(&self, text: &str, start: usize) -> Option<usize> {
        self.find_at(text, start).map(|(_, e)| e)
    }

    fn is_match_at(&self, text: &str, start: usize) -> bool {
        (self.prog)(&mut [], text, start)
    }

    fn find_at(&self, text: &str, start: usize) -> Option<(usize, usize)> {
        let mut slots = [None, None];
        (self.prog)(&mut slots, text, start);
        match (slots[0], slots[1]) {
            (Some(s), Some(e)) => Some((s, e)),
            _ => None,
        }
    }

    fn read_captures_at<'t>(
        &self,
        locs: &mut Locations,
        text: &'t str,
        start: usize,
    ) -> Option<(usize, usize)> {
        let slots = as_slots(locs);
        for slot in slots.iter_mut() {
            *slot = None;
        }
        (self.prog)(slots, text, start);
        match (slots[0], slots[1]) {
            (Some(s), Some(e)) => Some((s, e)),
            _ => None,
        }
    }
}
