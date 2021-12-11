// Copyright 2013-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

extern crate num as num_renamed;
#[macro_use]
extern crate num_derive;

#[derive(Debug, PartialEq, FromPrimitive, ToPrimitive)]
enum Color {}

#[test]
fn test_empty_enum() {
    let v: [Option<Color>; 1] = [num_renamed::FromPrimitive::from_u64(0)];

    assert_eq!(v, [None]);
}
