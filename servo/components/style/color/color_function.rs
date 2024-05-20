/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Output of parsing a color function, e.g. rgb(..), hsl(..), color(..)

use crate::{color::ColorFlags, values::normalize};
use cssparser::color::{PredefinedColorSpace, OPAQUE};

use super::{
    component::ColorComponent,
    convert::normalize_hue,
    parsing::{NumberOrAngle, NumberOrPercentage},
    AbsoluteColor, ColorSpace,
};

/// Represents a specified color function.
#[derive(Debug)]
pub enum ColorFunction {
    /// <https://drafts.csswg.org/css-color-4/#rgb-functions>
    Rgb(
        ColorComponent<u8>,                 // red
        ColorComponent<u8>,                 // green
        ColorComponent<u8>,                 // blue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#the-hsl-notation>
    Hsl(
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // saturation
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // alpha
        bool,                               // is_legacy_syntax
    ),
    /// <https://drafts.csswg.org/css-color-4/#the-hwb-notation>
    Hwb(
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // whiteness
        ColorComponent<NumberOrPercentage>, // blackness
        ColorComponent<NumberOrPercentage>, // alpha
        bool,                               // is_legacy_syntax
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-lab-lch>
    Lab(
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // a
        ColorComponent<NumberOrPercentage>, // b
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-lab-lch>
    Lch(
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // chroma
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-oklab-oklch>
    Oklab(
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // a
        ColorComponent<NumberOrPercentage>, // b
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#specifying-oklab-oklch>
    Oklch(
        ColorComponent<NumberOrPercentage>, // lightness
        ColorComponent<NumberOrPercentage>, // chroma
        ColorComponent<NumberOrAngle>,      // hue
        ColorComponent<NumberOrPercentage>, // alpha
    ),
    /// <https://drafts.csswg.org/css-color-4/#color-function>
    Color(
        PredefinedColorSpace,
        ColorComponent<NumberOrPercentage>, // red / x
        ColorComponent<NumberOrPercentage>, // green / y
        ColorComponent<NumberOrPercentage>, // blue / z
        ColorComponent<NumberOrPercentage>, // alpha
    ),
}

impl ColorFunction {
    /// Try to resolve the color function to an [`AbsoluteColor`] that does not
    /// contain any variables (currentcolor, color components, etc.).
    pub fn resolve_to_absolute(&self) -> AbsoluteColor {
        macro_rules! value {
            ($v:expr) => {{
                match $v {
                    ColorComponent::None => None,
                    // value should be Copy.
                    ColorComponent::Value(value) => Some(*value),
                }
            }};
        }

        macro_rules! alpha {
            ($alpha:expr) => {{
                value!($alpha).map(|value| normalize(value.to_number(1.0)).clamp(0.0, OPAQUE))
            }};
        }

        match self {
            ColorFunction::Rgb(r, g, b, alpha) => {
                let r = value!(r).unwrap_or(0);
                let g = value!(g).unwrap_or(0);
                let b = value!(b).unwrap_or(0);

                AbsoluteColor::srgb_legacy(r, g, b, alpha!(alpha).unwrap_or(0.0))
            },
            ColorFunction::Hsl(h, s, l, alpha, is_legacy_syntax) => {
                // Percent reference range for S and L: 0% = 0.0, 100% = 100.0
                const LIGHTNESS_RANGE: f32 = 100.0;
                const SATURATION_RANGE: f32 = 100.0;

                let mut result = AbsoluteColor::new(
                    ColorSpace::Hsl,
                    value!(h).map(|angle| normalize_hue(angle.degrees())),
                    value!(s).map(|s| {
                        if *is_legacy_syntax {
                            s.to_number(SATURATION_RANGE).clamp(0.0, SATURATION_RANGE)
                        } else {
                            s.to_number(SATURATION_RANGE)
                        }
                    }),
                    value!(l).map(|l| {
                        if *is_legacy_syntax {
                            l.to_number(LIGHTNESS_RANGE).clamp(0.0, LIGHTNESS_RANGE)
                        } else {
                            l.to_number(LIGHTNESS_RANGE)
                        }
                    }),
                    alpha!(alpha),
                );

                if *is_legacy_syntax {
                    result.flags.insert(ColorFlags::IS_LEGACY_SRGB);
                }

                result
            },
            ColorFunction::Hwb(h, w, b, alpha, is_legacy_syntax) => {
                // Percent reference range for W and B: 0% = 0.0, 100% = 100.0
                const WHITENESS_RANGE: f32 = 100.0;
                const BLACKNESS_RANGE: f32 = 100.0;

                let mut result = AbsoluteColor::new(
                    ColorSpace::Hwb,
                    value!(h).map(|angle| normalize_hue(angle.degrees())),
                    value!(w).map(|w| {
                        if *is_legacy_syntax {
                            w.to_number(WHITENESS_RANGE).clamp(0.0, WHITENESS_RANGE)
                        } else {
                            w.to_number(WHITENESS_RANGE)
                        }
                    }),
                    value!(b).map(|b| {
                        if *is_legacy_syntax {
                            b.to_number(BLACKNESS_RANGE).clamp(0.0, BLACKNESS_RANGE)
                        } else {
                            b.to_number(BLACKNESS_RANGE)
                        }
                    }),
                    alpha!(alpha),
                );

                if *is_legacy_syntax {
                    result.flags.insert(ColorFlags::IS_LEGACY_SRGB);
                }

                result
            },
            ColorFunction::Lab(l, a, b, alpha) => {
                // for L: 0% = 0.0, 100% = 100.0
                // for a and b: -100% = -125, 100% = 125
                const LIGHTNESS_RANGE: f32 = 100.0;
                const A_B_RANGE: f32 = 125.0;

                AbsoluteColor::new(
                    ColorSpace::Lab,
                    value!(l).map(|l| l.to_number(LIGHTNESS_RANGE)),
                    value!(a).map(|a| a.to_number(A_B_RANGE)),
                    value!(b).map(|b| b.to_number(A_B_RANGE)),
                    alpha!(alpha),
                )
            },
            ColorFunction::Lch(l, c, h, alpha) => {
                // for L: 0% = 0.0, 100% = 100.0
                // for C: 0% = 0, 100% = 150
                const LIGHTNESS_RANGE: f32 = 100.0;
                const CHROMA_RANGE: f32 = 150.0;

                AbsoluteColor::new(
                    ColorSpace::Lch,
                    value!(l).map(|l| l.to_number(LIGHTNESS_RANGE)),
                    value!(c).map(|c| c.to_number(CHROMA_RANGE)),
                    value!(h).map(|angle| normalize_hue(angle.degrees())),
                    alpha!(alpha),
                )
            },
            ColorFunction::Oklab(l, a, b, alpha) => {
                // for L: 0% = 0.0, 100% = 1.0
                // for a and b: -100% = -0.4, 100% = 0.4
                const LIGHTNESS_RANGE: f32 = 1.0;
                const A_B_RANGE: f32 = 0.4;

                AbsoluteColor::new(
                    ColorSpace::Oklab,
                    value!(l).map(|l| l.to_number(LIGHTNESS_RANGE)),
                    value!(a).map(|a| a.to_number(A_B_RANGE)),
                    value!(b).map(|b| b.to_number(A_B_RANGE)),
                    alpha!(alpha),
                )
            },
            ColorFunction::Oklch(l, c, h, alpha) => {
                // for L: 0% = 0.0, 100% = 1.0
                // for C: 0% = 0.0 100% = 0.4
                const LIGHTNESS_RANGE: f32 = 1.0;
                const CHROMA_RANGE: f32 = 0.4;

                AbsoluteColor::new(
                    ColorSpace::Oklch,
                    value!(l).map(|l| l.to_number(LIGHTNESS_RANGE)),
                    value!(c).map(|c| c.to_number(CHROMA_RANGE)),
                    value!(h).map(|angle| normalize_hue(angle.degrees())),
                    alpha!(alpha),
                )
            },
            ColorFunction::Color(color_space, r, g, b, alpha) => AbsoluteColor::new(
                (*color_space).into(),
                value!(r).map(|c| c.to_number(1.0)),
                value!(g).map(|c| c.to_number(1.0)),
                value!(b).map(|c| c.to_number(1.0)),
                alpha!(alpha),
            ),
        }
    }
}
