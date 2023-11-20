/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use parsing::parse;
use servo_atoms::Atom;
use style::parser::Parse;
use style::properties::longhands::animation_name;
use style::values::specified::AnimationIterationCount;
use style::values::{CustomIdent, KeyframesName};
use style_traits::ToCss;

#[test]
fn test_animation_iteration() {
    assert_roundtrip_with_context!(AnimationIterationCount::parse, "0", "0");
    assert_roundtrip_with_context!(AnimationIterationCount::parse, "0.1", "0.1");
    assert_roundtrip_with_context!(AnimationIterationCount::parse, "infinite", "infinite");

    // Negative numbers are invalid
    assert!(parse(AnimationIterationCount::parse, "-1").is_err());
}
