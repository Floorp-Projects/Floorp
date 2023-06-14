/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Allow text like <color> in docs.
#![allow(rustdoc::invalid_html_tags)]

use std::f32::consts::PI;
use std::fmt;
use std::str::FromStr;

use super::{CowRcStr, ParseError, Parser, ToCss, Token};

#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};

const OPAQUE: f32 = 1.0;

fn serialize_none_or<T>(dest: &mut impl fmt::Write, value: &Option<T>) -> fmt::Result
where
    T: ToCss,
{
    match value {
        Some(v) => v.to_css(dest),
        None => dest.write_str("none"),
    }
}

/// Serialize the alpha copmonent of a color according to the specification.
/// <https://drafts.csswg.org/css-color-4/#serializing-alpha-values>
#[inline]
pub fn serialize_color_alpha(
    dest: &mut impl fmt::Write,
    alpha: Option<f32>,
    legacy_syntax: bool,
) -> fmt::Result {
    let alpha = match alpha {
        None => return dest.write_str(" / none"),
        Some(a) => a,
    };

    // If the alpha component is full opaque, don't emit the alpha value in CSS.
    if alpha == OPAQUE {
        return Ok(());
    }

    dest.write_str(if legacy_syntax { ", " } else { " / " })?;

    // Try first with two decimal places, then with three.
    let mut rounded_alpha = (alpha * 100.).round() / 100.;
    if clamp_unit_f32(rounded_alpha) != clamp_unit_f32(alpha) {
        rounded_alpha = (alpha * 1000.).round() / 1000.;
    }

    rounded_alpha.to_css(dest)
}

// Guaratees hue in [0..360)
fn normalize_hue(hue: f32) -> f32 {
    // <https://drafts.csswg.org/css-values/#angles>
    // Subtract an integer before rounding, to avoid some rounding errors:
    hue - 360.0 * (hue / 360.0).floor()
}

/// A color with red, green, blue, and alpha components, in a byte each.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct RGBA {
    /// The red component.
    pub red: Option<u8>,
    /// The green component.
    pub green: Option<u8>,
    /// The blue component.
    pub blue: Option<u8>,
    /// The alpha component.
    pub alpha: Option<f32>,
}

impl RGBA {
    /// Constructs a new RGBA value from float components. It expects the red,
    /// green, blue and alpha channels in that order, and all values will be
    /// clamped to the 0.0 ... 1.0 range.
    #[inline]
    pub fn from_floats(
        red: Option<f32>,
        green: Option<f32>,
        blue: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Self::new(
            red.map(clamp_unit_f32),
            green.map(clamp_unit_f32),
            blue.map(clamp_unit_f32),
            alpha.map(|a| a.clamp(0.0, OPAQUE)),
        )
    }

    /// Same thing, but with `u8` values instead of floats in the 0 to 1 range.
    #[inline]
    pub const fn new(
        red: Option<u8>,
        green: Option<u8>,
        blue: Option<u8>,
        alpha: Option<f32>,
    ) -> Self {
        Self {
            red,
            green,
            blue,
            alpha,
        }
    }
}

#[cfg(feature = "serde")]
impl Serialize for RGBA {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        (self.red, self.green, self.blue, self.alpha).serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for RGBA {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let (r, g, b, a) = Deserialize::deserialize(deserializer)?;
        Ok(RGBA::new(r, g, b, a))
    }
}

impl ToCss for RGBA {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result
    where
        W: fmt::Write,
    {
        let has_alpha = self.alpha.unwrap_or(0.0) != OPAQUE;

        dest.write_str(if has_alpha { "rgba(" } else { "rgb(" })?;
        self.red.unwrap_or(0).to_css(dest)?;
        dest.write_str(", ")?;
        self.green.unwrap_or(0).to_css(dest)?;
        dest.write_str(", ")?;
        self.blue.unwrap_or(0).to_css(dest)?;

        // Legacy syntax does not allow none components.
        serialize_color_alpha(dest, Some(self.alpha.unwrap_or(0.0)), true)?;

        dest.write_char(')')
    }
}

/// Color specified by hue, saturation and lightness components.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Hsl {
    /// The hue component.
    pub hue: Option<f32>,
    /// The saturation component.
    pub saturation: Option<f32>,
    /// The lightness component.
    pub lightness: Option<f32>,
    /// The alpha component.
    pub alpha: Option<f32>,
}

impl Hsl {
    /// Construct a new HSL color from it's components.
    pub fn new(
        hue: Option<f32>,
        saturation: Option<f32>,
        lightness: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Self {
            hue,
            saturation,
            lightness,
            alpha,
        }
    }
}

impl ToCss for Hsl {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result
    where
        W: fmt::Write,
    {
        // HSL serializes to RGB, so we have to convert it.
        let (red, green, blue) = hsl_to_rgb(
            self.hue.unwrap_or(0.0) / 360.0,
            self.saturation.unwrap_or(0.0),
            self.lightness.unwrap_or(0.0),
        );

        RGBA::from_floats(Some(red), Some(green), Some(blue), self.alpha).to_css(dest)
    }
}

#[cfg(feature = "serde")]
impl Serialize for Hsl {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        (self.hue, self.saturation, self.lightness, self.alpha).serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for Hsl {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let (lightness, a, b, alpha) = Deserialize::deserialize(deserializer)?;
        Ok(Self::new(lightness, a, b, alpha))
    }
}

/// Color specified by hue, whiteness and blackness components.
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Hwb {
    /// The hue component.
    pub hue: Option<f32>,
    /// The whiteness component.
    pub whiteness: Option<f32>,
    /// The blackness component.
    pub blackness: Option<f32>,
    /// The alpha component.
    pub alpha: Option<f32>,
}

impl Hwb {
    /// Construct a new HWB color from it's components.
    pub fn new(
        hue: Option<f32>,
        whiteness: Option<f32>,
        blackness: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Self {
            hue,
            whiteness,
            blackness,
            alpha,
        }
    }
}

impl ToCss for Hwb {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result
    where
        W: fmt::Write,
    {
        // HWB serializes to RGB, so we have to convert it.
        let (red, green, blue) = hwb_to_rgb(
            self.hue.unwrap_or(0.0) / 360.0,
            self.whiteness.unwrap_or(0.0),
            self.blackness.unwrap_or(0.0),
        );

        RGBA::from_floats(Some(red), Some(green), Some(blue), self.alpha).to_css(dest)
    }
}

#[cfg(feature = "serde")]
impl Serialize for Hwb {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        (self.hue, self.whiteness, self.blackness, self.alpha).serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for Hwb {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let (lightness, whiteness, blackness, alpha) = Deserialize::deserialize(deserializer)?;
        Ok(Self::new(lightness, whiteness, blackness, alpha))
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

        #[cfg(feature = "serde")]
        impl Serialize for $cls {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                (self.lightness, self.a, self.b, self.alpha).serialize(serializer)
            }
        }

        #[cfg(feature = "serde")]
        impl<'de> Deserialize<'de> for $cls {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: Deserializer<'de>,
            {
                let (lightness, a, b, alpha) = Deserialize::deserialize(deserializer)?;
                Ok(Self::new(lightness, a, b, alpha))
            }
        }

        impl ToCss for $cls {
            fn to_css<W>(&self, dest: &mut W) -> fmt::Result
            where
                W: fmt::Write,
            {
                dest.write_str($fname)?;
                dest.write_str("(")?;
                serialize_none_or(dest, &self.lightness)?;
                dest.write_char(' ')?;
                serialize_none_or(dest, &self.a)?;
                dest.write_char(' ')?;
                serialize_none_or(dest, &self.b)?;
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

        #[cfg(feature = "serde")]
        impl Serialize for $cls {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
            where
                S: Serializer,
            {
                (self.lightness, self.chroma, self.hue, self.alpha).serialize(serializer)
            }
        }

        #[cfg(feature = "serde")]
        impl<'de> Deserialize<'de> for $cls {
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
            where
                D: Deserializer<'de>,
            {
                let (lightness, chroma, hue, alpha) = Deserialize::deserialize(deserializer)?;
                Ok(Self::new(lightness, chroma, hue, alpha))
            }
        }

        impl ToCss for $cls {
            fn to_css<W>(&self, dest: &mut W) -> fmt::Result
            where
                W: fmt::Write,
            {
                dest.write_str($fname)?;
                dest.write_str("(")?;
                serialize_none_or(dest, &self.lightness)?;
                dest.write_char(' ')?;
                serialize_none_or(dest, &self.chroma)?;
                dest.write_char(' ')?;
                serialize_none_or(dest, &self.hue)?;
                serialize_color_alpha(dest, self.alpha, false)?;
                dest.write_char(')')
            }
        }
    };
}

impl_lch_like!(Lch, "lch");
impl_lch_like!(Oklch, "oklch");

/// A Predefined color space specified in:
/// <https://drafts.csswg.org/css-color-4/#predefined>
#[derive(Clone, Copy, PartialEq, Debug)]
pub enum PredefinedColorSpace {
    /// <https://drafts.csswg.org/css-color-4/#predefined-sRGB>
    Srgb,
    /// <https://drafts.csswg.org/css-color-4/#predefined-sRGB-linear>
    SrgbLinear,
    /// <https://drafts.csswg.org/css-color-4/#predefined-display-p3>
    DisplayP3,
    /// <https://drafts.csswg.org/css-color-4/#predefined-a98-rgb>
    A98Rgb,
    /// <https://drafts.csswg.org/css-color-4/#predefined-prophoto-rgb>
    ProphotoRgb,
    /// <https://drafts.csswg.org/css-color-4/#predefined-rec2020>
    Rec2020,
    /// <https://drafts.csswg.org/css-color-4/#predefined-xyz>
    XyzD50,
    /// <https://drafts.csswg.org/css-color-4/#predefined-xyz>
    XyzD65,
}

impl PredefinedColorSpace {
    /// Returns the string value of the predefined color space.
    pub fn as_str(&self) -> &str {
        match self {
            PredefinedColorSpace::Srgb => "srgb",
            PredefinedColorSpace::SrgbLinear => "srgb-linear",
            PredefinedColorSpace::DisplayP3 => "display-p3",
            PredefinedColorSpace::A98Rgb => "a98-rgb",
            PredefinedColorSpace::ProphotoRgb => "prophoto-rgb",
            PredefinedColorSpace::Rec2020 => "rec2020",
            PredefinedColorSpace::XyzD50 => "xyz-d50",
            PredefinedColorSpace::XyzD65 => "xyz-d65",
        }
    }
}

impl FromStr for PredefinedColorSpace {
    type Err = ();

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(match_ignore_ascii_case! { s,
            "srgb" => PredefinedColorSpace::Srgb,
            "srgb-linear" => PredefinedColorSpace::SrgbLinear,
            "display-p3" => PredefinedColorSpace::DisplayP3,
            "a98-rgb" => PredefinedColorSpace::A98Rgb,
            "prophoto-rgb" => PredefinedColorSpace::ProphotoRgb,
            "rec2020" => PredefinedColorSpace::Rec2020,
            "xyz-d50" => PredefinedColorSpace::XyzD50,
            "xyz" | "xyz-d65" => PredefinedColorSpace::XyzD65,

            _ => return Err(()),
        })
    }
}

impl ToCss for PredefinedColorSpace {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result
    where
        W: fmt::Write,
    {
        dest.write_str(self.as_str())
    }
}

/// A color specified by the color() function.
/// <https://drafts.csswg.org/css-color-4/#color-function>
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct ColorFunction {
    /// The color space for this color.
    pub color_space: PredefinedColorSpace,
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
        color_space: PredefinedColorSpace,
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

impl ToCss for ColorFunction {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result
    where
        W: fmt::Write,
    {
        dest.write_str("color(")?;
        self.color_space.to_css(dest)?;
        dest.write_char(' ')?;
        serialize_none_or(dest, &self.c1)?;
        dest.write_char(' ')?;
        serialize_none_or(dest, &self.c2)?;
        dest.write_char(' ')?;
        serialize_none_or(dest, &self.c3)?;

        serialize_color_alpha(dest, self.alpha, false)?;

        dest.write_char(')')
    }
}

/// Describes one of the value <color> values according to the CSS
/// specification.
///
/// Most components are `Option<_>`, so when the value is `None`, that component
/// serializes to the "none" keyword.
///
/// <https://drafts.csswg.org/css-color-4/#color-type>
#[derive(Clone, Copy, PartialEq, Debug)]
pub enum Color {
    /// The 'currentcolor' keyword.
    CurrentColor,
    /// Specify sRGB colors directly by their red/green/blue/alpha chanels.
    Rgba(RGBA),
    /// Specifies a color in sRGB using hue, saturation and lightness components.
    Hsl(Hsl),
    /// Specifies a color in sRGB using hue, whiteness and blackness components.
    Hwb(Hwb),
    /// Specifies a CIELAB color by CIE Lightness and its a- and b-axis hue
    /// coordinates (red/green-ness, and yellow/blue-ness) using the CIE LAB
    /// rectangular coordinate model.
    Lab(Lab),
    /// Specifies a CIELAB color by CIE Lightness, Chroma, and hue using the
    /// CIE LCH cylindrical coordinate model.
    Lch(Lch),
    /// Specifies an Oklab color by Oklab Lightness and its a- and b-axis hue
    /// coordinates (red/green-ness, and yellow/blue-ness) using the Oklab
    /// rectangular coordinate model.
    Oklab(Oklab),
    /// Specifies an Oklab color by Oklab Lightness, Chroma, and hue using
    /// the OKLCH cylindrical coordinate model.
    Oklch(Oklch),
    /// Specifies a color in a predefined color space.
    ColorFunction(ColorFunction),
}

impl ToCss for Color {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result
    where
        W: fmt::Write,
    {
        match *self {
            Color::CurrentColor => dest.write_str("currentcolor"),
            Color::Rgba(rgba) => rgba.to_css(dest),
            Color::Hsl(hsl) => hsl.to_css(dest),
            Color::Hwb(hwb) => hwb.to_css(dest),
            Color::Lab(lab) => lab.to_css(dest),
            Color::Lch(lch) => lch.to_css(dest),
            Color::Oklab(lab) => lab.to_css(dest),
            Color::Oklch(lch) => lch.to_css(dest),
            Color::ColorFunction(color_function) => color_function.to_css(dest),
        }
    }
}

/// Either a number or a percentage.
pub enum NumberOrPercentage {
    /// `<number>`.
    Number {
        /// The numeric value parsed, as a float.
        value: f32,
    },
    /// `<percentage>`
    Percentage {
        /// The value as a float, divided by 100 so that the nominal range is
        /// 0.0 to 1.0.
        unit_value: f32,
    },
}

impl NumberOrPercentage {
    fn unit_value(&self) -> f32 {
        match *self {
            NumberOrPercentage::Number { value } => value,
            NumberOrPercentage::Percentage { unit_value } => unit_value,
        }
    }

    fn value(&self, percentage_basis: f32) -> f32 {
        match *self {
            Self::Number { value } => value,
            Self::Percentage { unit_value } => unit_value * percentage_basis,
        }
    }
}

/// Either an angle or a number.
pub enum AngleOrNumber {
    /// `<number>`.
    Number {
        /// The numeric value parsed, as a float.
        value: f32,
    },
    /// `<angle>`
    Angle {
        /// The value as a number of degrees.
        degrees: f32,
    },
}

impl AngleOrNumber {
    fn degrees(&self) -> f32 {
        match *self {
            AngleOrNumber::Number { value } => value,
            AngleOrNumber::Angle { degrees } => degrees,
        }
    }
}

/// A trait that can be used to hook into how `cssparser` parses color
/// components, with the intention of implementing more complicated behavior.
///
/// For example, this is used by Servo to support calc() in color.
pub trait ColorParser<'i> {
    /// The type that the parser will construct on a successful parse.
    type Output: FromParsedColor;

    /// A custom error type that can be returned from the parsing functions.
    type Error: 'i;

    /// Parse an `<angle>` or `<number>`.
    ///
    /// Returns the result in degrees.
    fn parse_angle_or_number<'t>(
        &self,
        input: &mut Parser<'i, 't>,
    ) -> Result<AngleOrNumber, ParseError<'i, Self::Error>> {
        let location = input.current_source_location();
        Ok(match *input.next()? {
            Token::Number { value, .. } => AngleOrNumber::Number { value },
            Token::Dimension {
                value: v, ref unit, ..
            } => {
                let degrees = match_ignore_ascii_case! { unit,
                    "deg" => v,
                    "grad" => v * 360. / 400.,
                    "rad" => v * 360. / (2. * PI),
                    "turn" => v * 360.,
                    _ => {
                        return Err(location.new_unexpected_token_error(Token::Ident(unit.clone())))
                    }
                };

                AngleOrNumber::Angle { degrees }
            }
            ref t => return Err(location.new_unexpected_token_error(t.clone())),
        })
    }

    /// Parse a `<percentage>` value.
    ///
    /// Returns the result in a number from 0.0 to 1.0.
    fn parse_percentage<'t>(
        &self,
        input: &mut Parser<'i, 't>,
    ) -> Result<f32, ParseError<'i, Self::Error>> {
        input.expect_percentage().map_err(From::from)
    }

    /// Parse a `<number>` value.
    fn parse_number<'t>(
        &self,
        input: &mut Parser<'i, 't>,
    ) -> Result<f32, ParseError<'i, Self::Error>> {
        input.expect_number().map_err(From::from)
    }

    /// Parse a `<number>` value or a `<percentage>` value.
    fn parse_number_or_percentage<'t>(
        &self,
        input: &mut Parser<'i, 't>,
    ) -> Result<NumberOrPercentage, ParseError<'i, Self::Error>> {
        let location = input.current_source_location();
        Ok(match *input.next()? {
            Token::Number { value, .. } => NumberOrPercentage::Number { value },
            Token::Percentage { unit_value, .. } => NumberOrPercentage::Percentage { unit_value },
            ref t => return Err(location.new_unexpected_token_error(t.clone())),
        })
    }
}

/// Default implementation of a [`ColorParser`]
pub struct DefaultColorParser;

impl<'i> ColorParser<'i> for DefaultColorParser {
    type Output = Color;
    type Error = ();
}

impl Color {
    /// Parse a <color> value, per CSS Color Module Level 3.
    ///
    /// FIXME(#2) Deprecated CSS2 System Colors are not supported yet.
    pub fn parse<'i>(input: &mut Parser<'i, '_>) -> Result<Color, ParseError<'i, ()>> {
        parse_color_with(&DefaultColorParser, input)
    }
}

/// This trait is used by the [`ColorParser`] to construct colors of any type.
pub trait FromParsedColor {
    /// Construct a new color from the CSS `currentcolor` keyword.
    fn from_current_color() -> Self;

    /// Construct a new color from red, green, blue and alpha components.
    fn from_rgba(red: Option<u8>, green: Option<u8>, blue: Option<u8>, alpha: Option<f32>) -> Self;

    /// Construct a new color from hue, saturation, lightness and alpha components.
    fn from_hsl(
        hue: Option<f32>,
        saturation: Option<f32>,
        lightness: Option<f32>,
        alpha: Option<f32>,
    ) -> Self;

    /// Construct a new color from hue, blackness, whiteness and alpha components.
    fn from_hwb(
        hue: Option<f32>,
        whiteness: Option<f32>,
        blackness: Option<f32>,
        alpha: Option<f32>,
    ) -> Self;

    /// Construct a new color from the `lab` notation.
    fn from_lab(lightness: Option<f32>, a: Option<f32>, b: Option<f32>, alpha: Option<f32>)
        -> Self;

    /// Construct a new color from the `lch` notation.
    fn from_lch(
        lightness: Option<f32>,
        chroma: Option<f32>,
        hue: Option<f32>,
        alpha: Option<f32>,
    ) -> Self;

    /// Construct a new color from the `oklab` notation.
    fn from_oklab(
        lightness: Option<f32>,
        a: Option<f32>,
        b: Option<f32>,
        alpha: Option<f32>,
    ) -> Self;

    /// Construct a new color from the `oklch` notation.
    fn from_oklch(
        lightness: Option<f32>,
        chroma: Option<f32>,
        hue: Option<f32>,
        alpha: Option<f32>,
    ) -> Self;

    /// Construct a new color with a predefined color space.
    fn from_color_function(
        color_space: PredefinedColorSpace,
        c1: Option<f32>,
        c2: Option<f32>,
        c3: Option<f32>,
        alpha: Option<f32>,
    ) -> Self;
}

/// Parse a color hash, without the leading '#' character.
#[inline]
pub fn parse_hash_color<'i, O>(value: &[u8]) -> Result<O, ()>
where
    O: FromParsedColor,
{
    Ok(match value.len() {
        8 => O::from_rgba(
            Some(from_hex(value[0])? * 16 + from_hex(value[1])?),
            Some(from_hex(value[2])? * 16 + from_hex(value[3])?),
            Some(from_hex(value[4])? * 16 + from_hex(value[5])?),
            Some((from_hex(value[6])? * 16 + from_hex(value[7])?) as f32 / 255.0),
        ),
        6 => O::from_rgba(
            Some(from_hex(value[0])? * 16 + from_hex(value[1])?),
            Some(from_hex(value[2])? * 16 + from_hex(value[3])?),
            Some(from_hex(value[4])? * 16 + from_hex(value[5])?),
            Some(OPAQUE),
        ),
        4 => O::from_rgba(
            Some(from_hex(value[0])? * 17),
            Some(from_hex(value[1])? * 17),
            Some(from_hex(value[2])? * 17),
            Some((from_hex(value[3])? * 17) as f32 / 255.0),
        ),
        3 => O::from_rgba(
            Some(from_hex(value[0])? * 17),
            Some(from_hex(value[1])? * 17),
            Some(from_hex(value[2])? * 17),
            Some(OPAQUE),
        ),
        _ => return Err(()),
    })
}

/// Parse a CSS color using the specified [`ColorParser`] and return a new color
/// value on success.
pub fn parse_color_with<'i, 't, P>(
    color_parser: &P,
    input: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let location = input.current_source_location();
    let token = input.next()?;
    match *token {
        Token::Hash(ref value) | Token::IDHash(ref value) => parse_hash_color(value.as_bytes()),
        Token::Ident(ref value) => parse_color_keyword(value),
        Token::Function(ref name) => {
            let name = name.clone();
            return input.parse_nested_block(|arguments| {
                parse_color_function(color_parser, name, arguments)
            });
        }
        _ => Err(()),
    }
    .map_err(|()| location.new_unexpected_token_error(token.clone()))
}

impl FromParsedColor for Color {
    #[inline]
    fn from_current_color() -> Self {
        Color::CurrentColor
    }

    #[inline]
    fn from_rgba(red: Option<u8>, green: Option<u8>, blue: Option<u8>, alpha: Option<f32>) -> Self {
        Color::Rgba(RGBA::new(red, green, blue, alpha))
    }

    fn from_hsl(
        hue: Option<f32>,
        saturation: Option<f32>,
        lightness: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Color::Hsl(Hsl::new(hue, saturation, lightness, alpha))
    }

    fn from_hwb(
        hue: Option<f32>,
        blackness: Option<f32>,
        whiteness: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Color::Hwb(Hwb::new(hue, blackness, whiteness, alpha))
    }

    #[inline]
    fn from_lab(
        lightness: Option<f32>,
        a: Option<f32>,
        b: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Color::Lab(Lab::new(lightness, a, b, alpha))
    }

    #[inline]
    fn from_lch(
        lightness: Option<f32>,
        chroma: Option<f32>,
        hue: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Color::Lch(Lch::new(lightness, chroma, hue, alpha))
    }

    #[inline]
    fn from_oklab(
        lightness: Option<f32>,
        a: Option<f32>,
        b: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Color::Oklab(Oklab::new(lightness, a, b, alpha))
    }

    #[inline]
    fn from_oklch(
        lightness: Option<f32>,
        chroma: Option<f32>,
        hue: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Color::Oklch(Oklch::new(lightness, chroma, hue, alpha))
    }

    #[inline]
    fn from_color_function(
        color_space: PredefinedColorSpace,
        c1: Option<f32>,
        c2: Option<f32>,
        c3: Option<f32>,
        alpha: Option<f32>,
    ) -> Self {
        Color::ColorFunction(ColorFunction::new(color_space, c1, c2, c3, alpha))
    }
}

/// Return the named color with the given name.
///
/// Matching is case-insensitive in the ASCII range.
/// CSS escaping (if relevant) should be resolved before calling this function.
/// (For example, the value of an `Ident` token is fine.)
#[inline]
pub fn parse_color_keyword<Output>(ident: &str) -> Result<Output, ()>
where
    Output: FromParsedColor,
{
    ascii_case_insensitive_phf_map! {
        keyword -> (u8, u8, u8) = {
            "black" => (0, 0, 0),
            "silver" => (192, 192, 192),
            "gray" => (128, 128, 128),
            "white" => (255, 255, 255),
            "maroon" => (128, 0, 0),
            "red" => (255, 0, 0),
            "purple" => (128, 0, 128),
            "fuchsia" => (255, 0, 255),
            "green" => (0, 128, 0),
            "lime" => (0, 255, 0),
            "olive" => (128, 128, 0),
            "yellow" => (255, 255, 0),
            "navy" => (0, 0, 128),
            "blue" => (0, 0, 255),
            "teal" => (0, 128, 128),
            "aqua" => (0, 255, 255),

            "aliceblue" => (240, 248, 255),
            "antiquewhite" => (250, 235, 215),
            "aquamarine" => (127, 255, 212),
            "azure" => (240, 255, 255),
            "beige" => (245, 245, 220),
            "bisque" => (255, 228, 196),
            "blanchedalmond" => (255, 235, 205),
            "blueviolet" => (138, 43, 226),
            "brown" => (165, 42, 42),
            "burlywood" => (222, 184, 135),
            "cadetblue" => (95, 158, 160),
            "chartreuse" => (127, 255, 0),
            "chocolate" => (210, 105, 30),
            "coral" => (255, 127, 80),
            "cornflowerblue" => (100, 149, 237),
            "cornsilk" => (255, 248, 220),
            "crimson" => (220, 20, 60),
            "cyan" => (0, 255, 255),
            "darkblue" => (0, 0, 139),
            "darkcyan" => (0, 139, 139),
            "darkgoldenrod" => (184, 134, 11),
            "darkgray" => (169, 169, 169),
            "darkgreen" => (0, 100, 0),
            "darkgrey" => (169, 169, 169),
            "darkkhaki" => (189, 183, 107),
            "darkmagenta" => (139, 0, 139),
            "darkolivegreen" => (85, 107, 47),
            "darkorange" => (255, 140, 0),
            "darkorchid" => (153, 50, 204),
            "darkred" => (139, 0, 0),
            "darksalmon" => (233, 150, 122),
            "darkseagreen" => (143, 188, 143),
            "darkslateblue" => (72, 61, 139),
            "darkslategray" => (47, 79, 79),
            "darkslategrey" => (47, 79, 79),
            "darkturquoise" => (0, 206, 209),
            "darkviolet" => (148, 0, 211),
            "deeppink" => (255, 20, 147),
            "deepskyblue" => (0, 191, 255),
            "dimgray" => (105, 105, 105),
            "dimgrey" => (105, 105, 105),
            "dodgerblue" => (30, 144, 255),
            "firebrick" => (178, 34, 34),
            "floralwhite" => (255, 250, 240),
            "forestgreen" => (34, 139, 34),
            "gainsboro" => (220, 220, 220),
            "ghostwhite" => (248, 248, 255),
            "gold" => (255, 215, 0),
            "goldenrod" => (218, 165, 32),
            "greenyellow" => (173, 255, 47),
            "grey" => (128, 128, 128),
            "honeydew" => (240, 255, 240),
            "hotpink" => (255, 105, 180),
            "indianred" => (205, 92, 92),
            "indigo" => (75, 0, 130),
            "ivory" => (255, 255, 240),
            "khaki" => (240, 230, 140),
            "lavender" => (230, 230, 250),
            "lavenderblush" => (255, 240, 245),
            "lawngreen" => (124, 252, 0),
            "lemonchiffon" => (255, 250, 205),
            "lightblue" => (173, 216, 230),
            "lightcoral" => (240, 128, 128),
            "lightcyan" => (224, 255, 255),
            "lightgoldenrodyellow" => (250, 250, 210),
            "lightgray" => (211, 211, 211),
            "lightgreen" => (144, 238, 144),
            "lightgrey" => (211, 211, 211),
            "lightpink" => (255, 182, 193),
            "lightsalmon" => (255, 160, 122),
            "lightseagreen" => (32, 178, 170),
            "lightskyblue" => (135, 206, 250),
            "lightslategray" => (119, 136, 153),
            "lightslategrey" => (119, 136, 153),
            "lightsteelblue" => (176, 196, 222),
            "lightyellow" => (255, 255, 224),
            "limegreen" => (50, 205, 50),
            "linen" => (250, 240, 230),
            "magenta" => (255, 0, 255),
            "mediumaquamarine" => (102, 205, 170),
            "mediumblue" => (0, 0, 205),
            "mediumorchid" => (186, 85, 211),
            "mediumpurple" => (147, 112, 219),
            "mediumseagreen" => (60, 179, 113),
            "mediumslateblue" => (123, 104, 238),
            "mediumspringgreen" => (0, 250, 154),
            "mediumturquoise" => (72, 209, 204),
            "mediumvioletred" => (199, 21, 133),
            "midnightblue" => (25, 25, 112),
            "mintcream" => (245, 255, 250),
            "mistyrose" => (255, 228, 225),
            "moccasin" => (255, 228, 181),
            "navajowhite" => (255, 222, 173),
            "oldlace" => (253, 245, 230),
            "olivedrab" => (107, 142, 35),
            "orange" => (255, 165, 0),
            "orangered" => (255, 69, 0),
            "orchid" => (218, 112, 214),
            "palegoldenrod" => (238, 232, 170),
            "palegreen" => (152, 251, 152),
            "paleturquoise" => (175, 238, 238),
            "palevioletred" => (219, 112, 147),
            "papayawhip" => (255, 239, 213),
            "peachpuff" => (255, 218, 185),
            "peru" => (205, 133, 63),
            "pink" => (255, 192, 203),
            "plum" => (221, 160, 221),
            "powderblue" => (176, 224, 230),
            "rebeccapurple" => (102, 51, 153),
            "rosybrown" => (188, 143, 143),
            "royalblue" => (65, 105, 225),
            "saddlebrown" => (139, 69, 19),
            "salmon" => (250, 128, 114),
            "sandybrown" => (244, 164, 96),
            "seagreen" => (46, 139, 87),
            "seashell" => (255, 245, 238),
            "sienna" => (160, 82, 45),
            "skyblue" => (135, 206, 235),
            "slateblue" => (106, 90, 205),
            "slategray" => (112, 128, 144),
            "slategrey" => (112, 128, 144),
            "snow" => (255, 250, 250),
            "springgreen" => (0, 255, 127),
            "steelblue" => (70, 130, 180),
            "tan" => (210, 180, 140),
            "thistle" => (216, 191, 216),
            "tomato" => (255, 99, 71),
            "turquoise" => (64, 224, 208),
            "violet" => (238, 130, 238),
            "wheat" => (245, 222, 179),
            "whitesmoke" => (245, 245, 245),
            "yellowgreen" => (154, 205, 50),
        }
    }

    match_ignore_ascii_case! { ident ,
        "transparent" => Ok(Output::from_rgba(Some(0), Some(0), Some(0), Some(0.0))),
        "currentcolor" => Ok(Output::from_current_color()),
        _ => keyword(ident)
            .map(|(r, g, b)| Output::from_rgba(Some(*r), Some(*g), Some(*b), Some(1.0)))
            .ok_or(()),
    }
}

#[inline]
fn from_hex(c: u8) -> Result<u8, ()> {
    match c {
        b'0'..=b'9' => Ok(c - b'0'),
        b'a'..=b'f' => Ok(c - b'a' + 10),
        b'A'..=b'F' => Ok(c - b'A' + 10),
        _ => Err(()),
    }
}

fn clamp_unit_f32(val: f32) -> u8 {
    // Whilst scaling by 256 and flooring would provide
    // an equal distribution of integers to percentage inputs,
    // this is not what Gecko does so we instead multiply by 255
    // and round (adding 0.5 and flooring is equivalent to rounding)
    //
    // Chrome does something similar for the alpha value, but not
    // the rgb values.
    //
    // See <https://bugzilla.mozilla.org/show_bug.cgi?id=1340484>
    //
    // Clamping to 256 and rounding after would let 1.0 map to 256, and
    // `256.0_f32 as u8` is undefined behavior:
    //
    // <https://github.com/rust-lang/rust/issues/10184>
    clamp_floor_256_f32(val * 255.)
}

fn clamp_floor_256_f32(val: f32) -> u8 {
    val.round().clamp(0., 255.) as u8
}

fn parse_none_or<'i, 't, F, T, E>(input: &mut Parser<'i, 't>, thing: F) -> Result<Option<T>, E>
where
    F: FnOnce(&mut Parser<'i, 't>) -> Result<T, E>,
{
    match input.try_parse(|p| p.expect_ident_matching("none")) {
        Ok(_) => Ok(None),
        Err(_) => Ok(Some(thing(input)?)),
    }
}

/// Parse one of the color functions: rgba(), lab(), color(), etc.
#[inline]
fn parse_color_function<'i, 't, P>(
    color_parser: &P,
    name: CowRcStr<'i>,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let color = match_ignore_ascii_case! { &name,
        "rgb" | "rgba" => parse_rgb(color_parser, arguments),

        "hsl" | "hsla" => parse_hsl(color_parser, arguments),

        "hwb" => parse_hwb(color_parser, arguments),

        // for L: 0% = 0.0, 100% = 100.0
        // for a and b: -100% = -125, 100% = 125
        "lab" => parse_lab_like(color_parser, arguments, 100.0, 125.0, P::Output::from_lab),

        // for L: 0% = 0.0, 100% = 100.0
        // for C: 0% = 0, 100% = 150
        "lch" => parse_lch_like(color_parser, arguments, 100.0, 150.0, P::Output::from_lch),

        // for L: 0% = 0.0, 100% = 1.0
        // for a and b: -100% = -0.4, 100% = 0.4
        "oklab" => parse_lab_like(color_parser, arguments, 1.0, 0.4, P::Output::from_oklab),

        // for L: 0% = 0.0, 100% = 1.0
        // for C: 0% = 0.0 100% = 0.4
        "oklch" => parse_lch_like(color_parser, arguments, 1.0, 0.4, P::Output::from_oklch),

        "color" => parse_color_with_color_space(color_parser, arguments),

        _ => return Err(arguments.new_unexpected_token_error(Token::Ident(name))),
    }?;

    arguments.expect_exhausted()?;

    Ok(color)
}

/// Parse the alpha component by itself from either number or percentage,
/// clipping the result to [0.0..1.0].
#[inline]
fn parse_alpha_component<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<f32, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    Ok(color_parser
        .parse_number_or_percentage(arguments)?
        .unit_value()
        .clamp(0.0, OPAQUE))
}

fn parse_legacy_alpha<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<f32, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    Ok(if !arguments.is_exhausted() {
        arguments.expect_comma()?;
        parse_alpha_component(color_parser, arguments)?
    } else {
        OPAQUE
    })
}

fn parse_modern_alpha<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<Option<f32>, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    if !arguments.is_exhausted() {
        arguments.expect_delim('/')?;
        parse_none_or(arguments, |p| parse_alpha_component(color_parser, p))
    } else {
        Ok(Some(OPAQUE))
    }
}

#[inline]
fn parse_rgb<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let maybe_red = parse_none_or(arguments, |p| color_parser.parse_number_or_percentage(p))?;

    // If the first component is not "none" and is followed by a comma, then we
    // are parsing the legacy syntax.
    let is_legacy_syntax = maybe_red.is_some() && arguments.try_parse(|p| p.expect_comma()).is_ok();

    let red: Option<u8>;
    let green: Option<u8>;
    let blue: Option<u8>;

    let alpha = if is_legacy_syntax {
        match maybe_red.unwrap() {
            NumberOrPercentage::Number { value } => {
                red = Some(clamp_floor_256_f32(value));
                green = Some(clamp_floor_256_f32(color_parser.parse_number(arguments)?));
                arguments.expect_comma()?;
                blue = Some(clamp_floor_256_f32(color_parser.parse_number(arguments)?));
            }
            NumberOrPercentage::Percentage { unit_value } => {
                red = Some(clamp_unit_f32(unit_value));
                green = Some(clamp_unit_f32(color_parser.parse_percentage(arguments)?));
                arguments.expect_comma()?;
                blue = Some(clamp_unit_f32(color_parser.parse_percentage(arguments)?));
            }
        }

        Some(parse_legacy_alpha(color_parser, arguments)?)
    } else {
        #[inline]
        fn get_component_value(c: Option<NumberOrPercentage>) -> Option<u8> {
            c.map(|c| match c {
                NumberOrPercentage::Number { value } => clamp_floor_256_f32(value),
                NumberOrPercentage::Percentage { unit_value } => clamp_unit_f32(unit_value),
            })
        }

        red = get_component_value(maybe_red);

        green = get_component_value(parse_none_or(arguments, |p| {
            color_parser.parse_number_or_percentage(p)
        })?);

        blue = get_component_value(parse_none_or(arguments, |p| {
            color_parser.parse_number_or_percentage(p)
        })?);

        parse_modern_alpha(color_parser, arguments)?
    };

    Ok(P::Output::from_rgba(red, green, blue, alpha))
}

/// Parses hsl syntax.
///
/// <https://drafts.csswg.org/css-color/#the-hsl-notation>
#[inline]
fn parse_hsl<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let maybe_hue = parse_none_or(arguments, |p| color_parser.parse_angle_or_number(p))?;

    // If the hue is not "none" and is followed by a comma, then we are parsing
    // the legacy syntax.
    let is_legacy_syntax = maybe_hue.is_some() && arguments.try_parse(|p| p.expect_comma()).is_ok();

    let saturation: Option<f32>;
    let lightness: Option<f32>;

    let alpha = if is_legacy_syntax {
        saturation = Some(color_parser.parse_percentage(arguments)?);
        arguments.expect_comma()?;
        lightness = Some(color_parser.parse_percentage(arguments)?);
        Some(parse_legacy_alpha(color_parser, arguments)?)
    } else {
        saturation = parse_none_or(arguments, |p| color_parser.parse_percentage(p))?;
        lightness = parse_none_or(arguments, |p| color_parser.parse_percentage(p))?;

        parse_modern_alpha(color_parser, arguments)?
    };

    let hue = maybe_hue.map(|h| normalize_hue(h.degrees()));
    let saturation = saturation.map(|s| s.clamp(0.0, 1.0));
    let lightness = lightness.map(|l| l.clamp(0.0, 1.0));

    Ok(P::Output::from_hsl(hue, saturation, lightness, alpha))
}

/// Parses hwb syntax.
///
/// <https://drafts.csswg.org/css-color/#the-hbw-notation>
#[inline]
fn parse_hwb<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let (hue, whiteness, blackness, alpha) = parse_components(
        color_parser,
        arguments,
        P::parse_angle_or_number,
        P::parse_percentage,
        P::parse_percentage,
    )?;

    let hue = hue.map(|h| normalize_hue(h.degrees()));
    let whiteness = whiteness.map(|w| w.clamp(0.0, 1.0));
    let blackness = blackness.map(|b| b.clamp(0.0, 1.0));

    Ok(P::Output::from_hwb(hue, whiteness, blackness, alpha))
}

/// <https://drafts.csswg.org/css-color-4/#hwb-to-rgb>
#[inline]
pub fn hwb_to_rgb(h: f32, w: f32, b: f32) -> (f32, f32, f32) {
    if w + b >= 1.0 {
        let gray = w / (w + b);
        return (gray, gray, gray);
    }

    // hue is expected in the range [0..1].
    let (mut red, mut green, mut blue) = hsl_to_rgb(h, 1.0, 0.5);
    let x = 1.0 - w - b;
    red = red * x + w;
    green = green * x + w;
    blue = blue * x + w;
    (red, green, blue)
}

/// <https://drafts.csswg.org/css-color/#hsl-color>
/// except with h pre-multiplied by 3, to avoid some rounding errors.
#[inline]
pub fn hsl_to_rgb(hue: f32, saturation: f32, lightness: f32) -> (f32, f32, f32) {
    debug_assert!((0.0..=1.0).contains(&hue));

    fn hue_to_rgb(m1: f32, m2: f32, mut h3: f32) -> f32 {
        if h3 < 0. {
            h3 += 3.
        }
        if h3 > 3. {
            h3 -= 3.
        }
        if h3 * 2. < 1. {
            m1 + (m2 - m1) * h3 * 2.
        } else if h3 * 2. < 3. {
            m2
        } else if h3 < 2. {
            m1 + (m2 - m1) * (2. - h3) * 2.
        } else {
            m1
        }
    }
    let m2 = if lightness <= 0.5 {
        lightness * (saturation + 1.)
    } else {
        lightness + saturation - lightness * saturation
    };
    let m1 = lightness * 2. - m2;
    let hue_times_3 = hue * 3.;
    let red = hue_to_rgb(m1, m2, hue_times_3 + 1.);
    let green = hue_to_rgb(m1, m2, hue_times_3);
    let blue = hue_to_rgb(m1, m2, hue_times_3 - 1.);
    (red, green, blue)
}

type IntoColorFn<Output> =
    fn(l: Option<f32>, a: Option<f32>, b: Option<f32>, alpha: Option<f32>) -> Output;

#[inline]
fn parse_lab_like<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    lightness_range: f32,
    a_b_range: f32,
    into_color: IntoColorFn<P::Output>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let (lightness, a, b, alpha) = parse_components(
        color_parser,
        arguments,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
    )?;

    let lightness = lightness.map(|l| l.value(lightness_range).max(0.0));
    let a = a.map(|a| a.value(a_b_range));
    let b = b.map(|b| b.value(a_b_range));

    Ok(into_color(lightness, a, b, alpha))
}

#[inline]
fn parse_lch_like<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    lightness_range: f32,
    chroma_range: f32,
    into_color: IntoColorFn<P::Output>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let (lightness, chroma, hue, alpha) = parse_components(
        color_parser,
        arguments,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
        P::parse_angle_or_number,
    )?;

    let lightness = lightness.map(|l| l.value(lightness_range).max(0.0));
    let chroma = chroma.map(|c| c.value(chroma_range).max(0.0));
    let hue = hue.map(|h| normalize_hue(h.degrees()));

    Ok(into_color(lightness, chroma, hue, alpha))
}

/// Parse the color() function.
#[inline]
fn parse_color_with_color_space<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let color_space = {
        let location = arguments.current_source_location();

        let ident = arguments.expect_ident()?;
        PredefinedColorSpace::from_str(ident)
            .map_err(|_| location.new_unexpected_token_error(Token::Ident(ident.clone())))?
    };

    let (c1, c2, c3, alpha) = parse_components(
        color_parser,
        arguments,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
    )?;

    let c1 = c1.map(|c| c.unit_value());
    let c2 = c2.map(|c| c.unit_value());
    let c3 = c3.map(|c| c.unit_value());

    Ok(P::Output::from_color_function(
        color_space,
        c1,
        c2,
        c3,
        alpha,
    ))
}

type ComponentParseResult<'i, R1, R2, R3, Error> =
    Result<(Option<R1>, Option<R2>, Option<R3>, Option<f32>), ParseError<'i, Error>>;

/// Parse the color components and alpha with the modern [color-4] syntax.
pub fn parse_components<'i, 't, P, F1, F2, F3, R1, R2, R3>(
    color_parser: &P,
    input: &mut Parser<'i, 't>,
    f1: F1,
    f2: F2,
    f3: F3,
) -> ComponentParseResult<'i, R1, R2, R3, P::Error>
where
    P: ColorParser<'i>,
    F1: FnOnce(&P, &mut Parser<'i, 't>) -> Result<R1, ParseError<'i, P::Error>>,
    F2: FnOnce(&P, &mut Parser<'i, 't>) -> Result<R2, ParseError<'i, P::Error>>,
    F3: FnOnce(&P, &mut Parser<'i, 't>) -> Result<R3, ParseError<'i, P::Error>>,
{
    let r1 = parse_none_or(input, |p| f1(color_parser, p))?;
    let r2 = parse_none_or(input, |p| f2(color_parser, p))?;
    let r3 = parse_none_or(input, |p| f3(color_parser, p))?;

    let alpha = parse_modern_alpha(color_parser, input)?;

    Ok((r1, r2, r3, alpha))
}
