/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Write colors into CSS strings.

use super::{AbsoluteColor, ColorFlags, ColorSpace};
use crate::values::normalize;
use cssparser::color::{clamp_unit_f32, serialize_color_alpha, OPAQUE};
use std::fmt::{self, Write};
use style_traits::{CssWriter, ToCss};

/// A [`ModernComponent`] can serialize to `none`, `nan`, `infinity` and
/// floating point values.
struct ModernComponent<'a>(&'a Option<f32>);

impl<'a> ToCss for ModernComponent<'a> {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: fmt::Write,
    {
        if let Some(value) = self.0 {
            if value.is_finite() {
                value.to_css(dest)
            } else if value.is_nan() {
                dest.write_str("calc(NaN)")
            } else {
                debug_assert!(value.is_infinite());
                if value.is_sign_negative() {
                    dest.write_str("calc(-infinity)")
                } else {
                    dest.write_str("calc(infinity)")
                }
            }
        } else {
            dest.write_str("none")
        }
    }
}

impl ToCss for AbsoluteColor {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        match self.color_space {
            ColorSpace::Srgb if self.flags.contains(ColorFlags::IS_LEGACY_SRGB) => {
                // The "none" keyword is not supported in the rgb/rgba legacy syntax.
                let has_alpha = self.alpha != OPAQUE;

                dest.write_str(if has_alpha { "rgba(" } else { "rgb(" })?;
                clamp_unit_f32(self.components.0).to_css(dest)?;
                dest.write_str(", ")?;
                clamp_unit_f32(self.components.1).to_css(dest)?;
                dest.write_str(", ")?;
                clamp_unit_f32(self.components.2).to_css(dest)?;

                // Legacy syntax does not allow none components.
                serialize_color_alpha(dest, Some(self.alpha), true)?;

                dest.write_char(')')
            },
            ColorSpace::Hsl | ColorSpace::Hwb => self.into_srgb_legacy().to_css(dest),
            ColorSpace::Oklab | ColorSpace::Lab | ColorSpace::Oklch | ColorSpace::Lch => {
                if let ColorSpace::Oklab | ColorSpace::Oklch = self.color_space {
                    dest.write_str("ok")?;
                }
                if let ColorSpace::Oklab | ColorSpace::Lab = self.color_space {
                    dest.write_str("lab(")?;
                } else {
                    dest.write_str("lch(")?;
                }
                ModernComponent(&self.c0()).to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.c1()).to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.c2()).to_css(dest)?;
                serialize_color_alpha(dest, self.alpha(), false)?;
                dest.write_char(')')
            },
            _ => {
                #[cfg(debug_assertions)]
                match self.color_space {
                    ColorSpace::Srgb => {
                        debug_assert!(
                            !self.flags.contains(ColorFlags::IS_LEGACY_SRGB),
                            "legacy srgb is not a color function"
                        );
                    },
                    ColorSpace::SrgbLinear |
                    ColorSpace::DisplayP3 |
                    ColorSpace::A98Rgb |
                    ColorSpace::ProphotoRgb |
                    ColorSpace::Rec2020 |
                    ColorSpace::XyzD50 |
                    ColorSpace::XyzD65 => {
                        // These color spaces are allowed.
                    },
                    _ => {
                        unreachable!("other color spaces do not support color() syntax")
                    },
                };

                dest.write_str("color(")?;
                self.color_space.to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.c0()).to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.c1()).to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.c2()).to_css(dest)?;

                serialize_color_alpha(dest, self.alpha(), false)?;

                dest.write_char(')')
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
