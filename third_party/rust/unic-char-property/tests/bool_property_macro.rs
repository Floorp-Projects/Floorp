// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[macro_use]
extern crate unic_char_property;

#[macro_use]
extern crate unic_char_range;

char_property! {
    /// This is a test property.
    pub struct MyProp(bool) {
        abbr => "mp";
        long => "My_Prop";
        human => "My Property";

        data_table_path => "tables/property_table.rsv";
    }

    /// This is the shorthand function.
    pub fn is_my_prop(char) -> bool;
}

#[test]
fn test_basics() {
    assert_eq!(MyProp::of('\u{0000}').as_bool(), false);
    assert_eq!(MyProp::of('\u{0065}').as_bool(), true);

    assert_eq!(is_my_prop('\u{0000}'), false);
    assert_eq!(is_my_prop('\u{0065}'), true);
}

#[cfg_attr(feature = "cargo-clippy", allow(needless_bool))]
#[test]
fn test_into_bool() {
    assert!(if MyProp::of('\u{0065}').into() {
        true
    } else {
        false
    });
}

#[test]
fn test_from_str() {
    assert_eq!("y".parse(), Ok(MyProp(true)));
    assert_eq!("yes".parse(), Ok(MyProp(true)));
    assert_eq!("t".parse(), Ok(MyProp(true)));
    assert_eq!("true".parse(), Ok(MyProp(true)));

    assert_eq!("N".parse(), Ok(MyProp(false)));
    assert_eq!("NO".parse(), Ok(MyProp(false)));
    assert_eq!("F".parse(), Ok(MyProp(false)));
    assert_eq!("FALSE".parse(), Ok(MyProp(false)));
}

#[test]
fn test_display() {
    use unic_char_property::BinaryCharProperty;

    assert_eq!(MyProp::of('\u{0000}').abbr_name(), "N");
    assert_eq!(MyProp::of('\u{0065}').abbr_name(), "Y");
    assert_eq!(MyProp::of('\u{0000}').long_name(), "No");
    assert_eq!(MyProp::of('\u{0065}').long_name(), "Yes");
    assert_eq!(MyProp::of('\u{0000}').human_name(), "No");
    assert_eq!(MyProp::of('\u{0065}').human_name(), "Yes");
}
