/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Write colors into CSS strings.

use super::{parsing, AbsoluteColor, ColorFlags, ColorSpace};
use crate::values::normalize;
use cssparser::color::{serialize_color_alpha, PredefinedColorSpace};
use std::fmt::{self, Write};
use style_traits::{CssWriter, ToCss};

impl ToCss for AbsoluteColor {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        match self.color_space {
            ColorSpace::Srgb if self.flags.contains(ColorFlags::IS_LEGACY_SRGB) => {
                // The "none" keyword is not supported in the rgb/rgba legacy syntax.
                cssparser::ToCss::to_css(
                    &parsing::RgbaLegacy::from_floats(
                        self.components.0,
                        self.components.1,
                        self.components.2,
                        self.alpha,
                    ),
                    dest,
                )
            },
            ColorSpace::Hsl | ColorSpace::Hwb => self.into_srgb_legacy().to_css(dest),
            ColorSpace::Lab => cssparser::ToCss::to_css(
                &parsing::Lab::new(self.c0(), self.c1(), self.c2(), self.alpha()),
                dest,
            ),
            ColorSpace::Lch => cssparser::ToCss::to_css(
                &parsing::Lch::new(self.c0(), self.c1(), self.c2(), self.alpha()),
                dest,
            ),
            ColorSpace::Oklab => cssparser::ToCss::to_css(
                &parsing::Oklab::new(self.c0(), self.c1(), self.c2(), self.alpha()),
                dest,
            ),
            ColorSpace::Oklch => cssparser::ToCss::to_css(
                &parsing::Oklch::new(self.c0(), self.c1(), self.c2(), self.alpha()),
                dest,
            ),
            _ => {
                let color_space = match self.color_space {
                    ColorSpace::Srgb => {
                        debug_assert!(
                            !self.flags.contains(ColorFlags::IS_LEGACY_SRGB),
                            "legacy srgb is not a color function"
                        );
                        PredefinedColorSpace::Srgb
                    },
                    ColorSpace::SrgbLinear => PredefinedColorSpace::SrgbLinear,
                    ColorSpace::DisplayP3 => PredefinedColorSpace::DisplayP3,
                    ColorSpace::A98Rgb => PredefinedColorSpace::A98Rgb,
                    ColorSpace::ProphotoRgb => PredefinedColorSpace::ProphotoRgb,
                    ColorSpace::Rec2020 => PredefinedColorSpace::Rec2020,
                    ColorSpace::XyzD50 => PredefinedColorSpace::XyzD50,
                    ColorSpace::XyzD65 => PredefinedColorSpace::XyzD65,

                    _ => {
                        unreachable!("other color spaces do not support color() syntax")
                    },
                };

                let color_function = parsing::ColorFunction {
                    color_space,
                    c1: self.c0(),
                    c2: self.c1(),
                    c3: self.c2(),
                    alpha: self.alpha(),
                };
                let color = parsing::Color::ColorFunction(color_function);
                cssparser::ToCss::to_css(&color, dest)
            },
        }
    }
}

impl AbsoluteColor {
    /// Write a string to `dest` that represents a color as an author would
    /// enter it.
    /// NOTE: The format of the output is NOT according to any specification,
    /// but makes assumptions about the best ways that authors would want to
    /// enter color values in style sheets, devtools, etc.
    pub fn write_author_preferred_value<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        macro_rules! precision {
            ($v:expr) => {{
                ($v * 100.0).round() / 100.0
            }};
        }
        macro_rules! number {
            ($c:expr) => {{
                if let Some(v) = $c.map(normalize) {
                    precision!(v).to_css(dest)?;
                } else {
                    write!(dest, "none")?;
                }
            }};
        }
        macro_rules! percentage {
            ($c:expr) => {{
                if let Some(v) = $c.map(normalize) {
                    precision!(v).to_css(dest)?;
                    dest.write_char('%')?;
                } else {
                    write!(dest, "none")?;
                }
            }};
        }
        macro_rules! unit_percentage {
            ($c:expr) => {{
                if let Some(v) = $c.map(normalize) {
                    precision!(v * 100.0).to_css(dest)?;
                    dest.write_char('%')?;
                } else {
                    write!(dest, "none")?;
                }
            }};
        }
        macro_rules! angle {
            ($c:expr) => {{
                if let Some(v) = $c.map(normalize) {
                    precision!(v).to_css(dest)?;
                    dest.write_str("deg")?;
                } else {
                    write!(dest, "none")?;
                }
            }};
        }

        match self.color_space {
            ColorSpace::Srgb => {
                write!(dest, "rgb(")?;
                unit_percentage!(self.c0());
                dest.write_char(' ')?;
                unit_percentage!(self.c1());
                dest.write_char(' ')?;
                unit_percentage!(self.c2());
                serialize_color_alpha(dest, self.alpha(), false)?;
                dest.write_char(')')
            },
            ColorSpace::Hsl | ColorSpace::Hwb => {
                dest.write_str(if self.color_space == ColorSpace::Hsl {
                    "hsl("
                } else {
                    "hwb("
                })?;
                angle!(self.c0());
                dest.write_char(' ')?;
                percentage!(self.c1());
                dest.write_char(' ')?;
                percentage!(self.c2());
                serialize_color_alpha(dest, self.alpha(), false)?;
                dest.write_char(')')
            },
            ColorSpace::Lab | ColorSpace::Oklab => {
                if self.color_space == ColorSpace::Oklab {
                    dest.write_str("ok")?;
                }
                dest.write_str("lab(")?;
                if self.color_space == ColorSpace::Lab {
                    percentage!(self.c0())
                } else {
                    unit_percentage!(self.c0())
                }
                dest.write_char(' ')?;
                number!(self.c1());
                dest.write_char(' ')?;
                number!(self.c2());
                serialize_color_alpha(dest, self.alpha(), false)?;
                dest.write_char(')')
            },
            ColorSpace::Lch | ColorSpace::Oklch => {
                if self.color_space == ColorSpace::Oklch {
                    dest.write_str("ok")?;
                }
                dest.write_str("lch(")?;
                number!(self.c0());
                dest.write_char(' ')?;
                number!(self.c1());
                dest.write_char(' ')?;
                angle!(self.c2());
                serialize_color_alpha(dest, self.alpha(), false)?;
                dest.write_char(')')
            },
            ColorSpace::SrgbLinear |
            ColorSpace::DisplayP3 |
            ColorSpace::A98Rgb |
            ColorSpace::ProphotoRgb |
            ColorSpace::Rec2020 |
            ColorSpace::XyzD50 |
            ColorSpace::XyzD65 => {
                dest.write_str("color(")?;
                self.color_space.to_css(dest)?;
                dest.write_char(' ')?;
                number!(self.c0());
                dest.write_char(' ')?;
                number!(self.c1());
                dest.write_char(' ')?;
                number!(self.c2());
                serialize_color_alpha(dest, self.alpha(), false)?;
                dest.write_char(')')
            },
        }
    }
}
