// Copyright 2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

mat!(t01, r".*abcd", r"abcd", Some((0, 4)));
mat!(t02, r".*(?:abcd)+", r"abcd", Some((0, 4)));
mat!(t03, r".*(?:abcd)+", r"abcdabcd", Some((0, 8)));
mat!(t04, r".*(?:abcd)+", r"abcdxabcd", Some((0, 9)));
mat!(t05, r".*x(?:abcd)+", r"abcdxabcd", Some((0, 9)));
mat!(t06, r"[^abcd]*x(?:abcd)+", r"abcdxabcd", Some((4, 9)));
