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

impl ToCss for ColorFunction {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_str("color(")?;
        self.color_space.to_css(dest)?;
        dest.write_char(' ')?;
        ModernComponent(&self.c1).to_css(dest)?;
        dest.write_char(' ')?;
        ModernComponent(&self.c2).to_css(dest)?;
        dest.write_char(' ')?;
        ModernComponent(&self.c3).to_css(dest)?;

        serialize_color_alpha(dest, self.alpha, false)?;

        dest.write_char(')')
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
                RgbaLegacy::from_floats(
                    self.components.0,
                    self.components.1,
                    self.components.2,
                    self.alpha,
                )
                .to_css(dest)
            },
            ColorSpace::Hsl | ColorSpace::Hwb => self.into_srgb_legacy().to_css(dest),
            ColorSpace::Lab => Lab::new(self.c0(), self.c1(), self.c2(), self.alpha()).to_css(dest),
            ColorSpace::Lch => Lch::new(self.c0(), self.c1(), self.c2(), self.alpha()).to_css(dest),
            ColorSpace::Oklab => {
                Oklab::new(self.c0(), self.c1(), self.c2(), self.alpha()).to_css(dest)
            },
            ColorSpace::Oklch => {
                Oklch::new(self.c0(), self.c1(), self.c2(), self.alpha()).to_css(dest)
            },
            _ => {
                #[cfg(debug_assertions)]
                match self.color_space {
                    ColorSpace::Srgb => {
                        debug_assert!(
                            !self.flags.contains(ColorFlags::IS_LEGACY_SRGB),
                            "legacy srgb is not a color function"
                        );
                        PredefinedColorSpace::Srgb
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

                let color_function = ColorFunction::new(
                    self.color_space,
                    self.c0(),
                    self.c1(),
                    self.c2(),
                    self.alpha(),
                );
                color_function.to_css(dest)
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

/// A color with red, green, blue, and alpha components, in a byte each.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct RgbaLegacy {
    /// The red component.
    pub red: u8,
    /// The green component.
    pub green: u8,
    /// The blue component.
    pub blue: u8,
    /// The alpha component.
    pub alpha: f32,
}

impl RgbaLegacy {
    /// Constructs a new RGBA value from float components. It expects the red,
    /// green, blue and alpha channels in that order, and all values will be
    /// clamped to the 0.0 ... 1.0 range.
    #[inline]
    pub fn from_floats(red: f32, green: f32, blue: f32, alpha: f32) -> Self {
        Self::new(
            clamp_unit_f32(red),
            clamp_unit_f32(green),
            clamp_unit_f32(blue),
            alpha.clamp(0.0, OPAQUE),
        )
    }

    /// Same thing, but with `u8` values instead of floats in the 0 to 1 range.
    #[inline]
    pub const fn new(red: u8, green: u8, blue: u8, alpha: f32) -> Self {
        Self {
            red,
            green,
            blue,
            alpha,
        }
    }
}

impl ToCss for RgbaLegacy {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: fmt::Write,
    {
        let has_alpha = self.alpha != OPAQUE;

        dest.write_str(if has_alpha { "rgba(" } else { "rgb(" })?;
        self.red.to_css(dest)?;
        dest.write_str(", ")?;
        self.green.to_css(dest)?;
        dest.write_str(", ")?;
        self.blue.to_css(dest)?;

        // Legacy syntax does not allow none components.
        serialize_color_alpha(dest, Some(self.alpha), true)?;

        dest.write_char(')')
    }
}

// NOTE: LAB and OKLAB is not declared inside the [impl_lab_like] macro,
// because it causes cbindgen to ignore them.

/// Color specified by lightness, a- and b-axis components.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Lab {
    /// The lightness component.
    pub lightness: Option<f32>,
    /// The a-axis component.
    pub a: Option<f32>,
    /// The b-axis component.
    pub b: Option<f32>,
    /// The alpha component.
    pub alpha: Option<f32>,
}

/// Color specified by lightness, a- and b-axis components.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Oklab {
    /// The lightness component.
    pub lightness: Option<f32>,
    /// The a-axis component.
    pub a: Option<f32>,
    /// The b-axis component.
    pub b: Option<f32>,
    /// The alpha component.
    pub alpha: Option<f32>,
}

macro_rules! impl_lab_like {
    ($cls:ident, $fname:literal) => {
        impl $cls {
            /// Construct a new Lab color format with lightness, a, b and alpha components.
            pub fn new(
                lightness: Option<f32>,
                a: Option<f32>,
                b: Option<f32>,
                alpha: Option<f32>,
            ) -> Self {
                Self {
                    lightness,
                    a,
                    b,
                    alpha,
                }
            }
        }

        impl ToCss for $cls {
            fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
            where
                W: fmt::Write,
            {
                dest.write_str($fname)?;
                dest.write_str("(")?;
                ModernComponent(&self.lightness).to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.a).to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.b).to_css(dest)?;
                serialize_color_alpha(dest, self.alpha, false)?;
                dest.write_char(')')
            }
        }
    };
}

impl_lab_like!(Lab, "lab");
impl_lab_like!(Oklab, "oklab");

// NOTE: LCH and OKLCH is not declared inside the [impl_lch_like] macro,
// because it causes cbindgen to ignore them.

/// Color specified by lightness, chroma and hue components.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Lch {
    /// The lightness component.
    pub lightness: Option<f32>,
    /// The chroma component.
    pub chroma: Option<f32>,
    /// The hue component.
    pub hue: Option<f32>,
    /// The alpha component.
    pub alpha: Option<f32>,
}

/// Color specified by lightness, chroma and hue components.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Oklch {
    /// The lightness component.
    pub lightness: Option<f32>,
    /// The chroma component.
    pub chroma: Option<f32>,
    /// The hue component.
    pub hue: Option<f32>,
    /// The alpha component.
    pub alpha: Option<f32>,
}

macro_rules! impl_lch_like {
    ($cls:ident, $fname:literal) => {
        impl $cls {
            /// Construct a new color with lightness, chroma and hue components.
            pub fn new(
                lightness: Option<f32>,
                chroma: Option<f32>,
                hue: Option<f32>,
                alpha: Option<f32>,
            ) -> Self {
                Self {
                    lightness,
                    chroma,
                    hue,
                    alpha,
                }
            }
        }

        impl ToCss for $cls {
            fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
            where
                W: fmt::Write,
            {
                dest.write_str($fname)?;
                dest.write_str("(")?;
                ModernComponent(&self.lightness).to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.chroma).to_css(dest)?;
                dest.write_char(' ')?;
                ModernComponent(&self.hue).to_css(dest)?;
                serialize_color_alpha(dest, self.alpha, false)?;
                dest.write_char(')')
            }
        }
    };
}

impl_lch_like!(Lch, "lch");
impl_lch_like!(Oklch, "oklch");

/// A color specified by the color() function.
/// <https://drafts.csswg.org/css-color-4/#color-function>
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct ColorFunction {
    /// The color space for this color.
    pub color_space: ColorSpace,
    /// The first component of the color.  Either red or x.
    pub c1: Option<f32>,
    /// The second component of the color.  Either green or y.
    pub c2: Option<f32>,
    /// The third component of the color.  Either blue or z.
    pub c3: Option<f32>,
    /// The alpha component of the color.
    pub alpha: Option<f32>,
}

impl ColorFunction {
    /// Construct a new color function definition with the given color space and
    /// color components.
    pub fn new(
        color_space: ColorSpace,
        c1: Option<f32>,
        c2: Option<f32>,
        c3: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Self {
            color_space,
            c1,
            c2,
            c3,
            alpha,
        }
    }
}
