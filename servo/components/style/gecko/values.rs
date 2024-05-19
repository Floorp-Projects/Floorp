/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#![allow(unsafe_code)]

//! Different kind of helpers to interact with Gecko values.

use crate::color::{AbsoluteColor, ColorSpace};

/// Convert a color value to `nscolor`.
pub fn convert_absolute_color_to_nscolor(color: &AbsoluteColor) -> u32 {
    let srgb = color.to_color_space(ColorSpace::Srgb);
    u32::from_le_bytes([
        (srgb.components.0 * 255.0).round() as u8,
        (srgb.components.1 * 255.0).round() as u8,
        (srgb.components.2 * 255.0).round() as u8,
        (srgb.alpha * 255.0).round() as u8,
    ])
}

/// Convert a given `nscolor` to a Servo AbsoluteColor value.
pub fn convert_nscolor_to_absolute_color(color: u32) -> AbsoluteColor {
    let [r, g, b, a] = color.to_le_bytes();
    AbsoluteColor::srgb_legacy(r, g, b, a as f32 / 255.0)
}

#[test]
fn convert_ns_color_to_absolute_color_should_be_in_legacy_syntax() {
    use crate::color::ColorFlags;

    let result = convert_nscolor_to_absolute_color(0x336699CC);
    assert!(result.flags.contains(ColorFlags::IS_LEGACY_SRGB));

    assert!(result.is_legacy_syntax());
}
