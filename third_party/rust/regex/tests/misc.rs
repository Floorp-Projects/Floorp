// Copyright 2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

mat!(prefix_literal_match, r"^abc", r"abc", Some((0, 3)));
mat!(prefix_literal_nomatch, r"^abc", r"zabc", None);
mat!(one_literal_edge, r"abc", r"xxxxxab", None);
matiter!(terminates, r"a$", r"a", (0, 1));
