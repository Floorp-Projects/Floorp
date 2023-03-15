/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::f32::consts::PI;
use std::fmt;
use std::str::FromStr;

use super::{ParseError, Parser, ToCss, Token};

#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};

const OPAQUE: f32 = 1.0;

/// https://drafts.csswg.org/css-color-4/#serializing-alpha-values
#[inline]
fn serialize_alpha(dest: &mut impl fmt::Write, alpha: f32, legacy_syntax: bool) -> fmt::Result {
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

/// A color with red, green, blue, and alpha components, in a byte each.
#[derive(Clone, Copy, PartialEq, Debug)]
#[repr(C)]
pub struct RGBA {
    /// The red component.
    pub red: u8,
    /// The green component.
    pub green: u8,
    /// The blue component.
    pub blue: u8,
    /// The alpha component.
    pub alpha: f32,
}

impl RGBA {
    /// Constructs a new RGBA value from float components. It expects the red,
    /// green, blue and alpha channels in that order, and all values will be
    /// clamped to the 0.0 ... 1.0 range.
    #[inline]
    pub fn from_floats(red: f32, green: f32, blue: f32, alpha: f32) -> Self {
        Self::new(
            clamp_unit_f32(red),
            clamp_unit_f32(green),
            clamp_unit_f32(blue),
            alpha.max(0.0).min(1.0),
        )
    }

    /// Returns a transparent color.
    #[inline]
    pub fn transparent() -> Self {
        Self::new(0, 0, 0, 0.0)
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

    /// Returns the red channel in a floating point number form, from 0 to 1.
    #[inline]
    pub fn red_f32(&self) -> f32 {
        self.red as f32 / 255.0
    }

    /// Returns the green channel in a floating point number form, from 0 to 1.
    #[inline]
    pub fn green_f32(&self) -> f32 {
        self.green as f32 / 255.0
    }

    /// Returns the blue channel in a floating point number form, from 0 to 1.
    #[inline]
    pub fn blue_f32(&self) -> f32 {
        self.blue as f32 / 255.0
    }

    /// Returns the alpha channel in a floating point number form, from 0 to 1.
    #[inline]
    pub fn alpha_f32(&self) -> f32 {
        self.alpha
    }

    /// Parse a color hash, without the leading '#' character.
    #[inline]
    pub fn parse_hash(value: &[u8]) -> Result<Self, ()> {
        Ok(match value.len() {
            8 => Self::new(
                from_hex(value[0])? * 16 + from_hex(value[1])?,
                from_hex(value[2])? * 16 + from_hex(value[3])?,
                from_hex(value[4])? * 16 + from_hex(value[5])?,
                (from_hex(value[6])? * 16 + from_hex(value[7])?) as f32 / 255.0,
            ),
            6 => Self::new(
                from_hex(value[0])? * 16 + from_hex(value[1])?,
                from_hex(value[2])? * 16 + from_hex(value[3])?,
                from_hex(value[4])? * 16 + from_hex(value[5])?,
                OPAQUE,
            ),
            4 => Self::new(
                from_hex(value[0])? * 17,
                from_hex(value[1])? * 17,
                from_hex(value[2])? * 17,
                (from_hex(value[3])? * 17) as f32 / 255.0,
            ),
            3 => Self::new(
                from_hex(value[0])? * 17,
                from_hex(value[1])? * 17,
                from_hex(value[2])? * 17,
                OPAQUE,
            ),
            _ => return Err(()),
        })
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
        let has_alpha = self.alpha != OPAQUE;

        dest.write_str(if has_alpha { "rgba(" } else { "rgb(" })?;
        self.red.to_css(dest)?;
        dest.write_str(", ")?;
        self.green.to_css(dest)?;
        dest.write_str(", ")?;
        self.blue.to_css(dest)?;

        serialize_alpha(dest, self.alpha, true)?;

        dest.write_char(')')
    }
}

// NOTE: LAB and OKLAB is not declared inside the [impl_lab_like] macro,
// because it causes cbindgen to ignore them.

/// Color specified by lightness, a- and b-axis components.
#[derive(Clone, Copy, PartialEq, Debug)]
#[repr(C)]
pub struct Lab {
    /// The lightness component.
    pub lightness: f32,
    /// The a-axis component.
    pub a: f32,
    /// The b-axis component.
    pub b: f32,
    /// The alpha component.
    pub alpha: f32,
}

/// Color specified by lightness, a- and b-axis components.
#[derive(Clone, Copy, PartialEq, Debug)]
#[repr(C)]
pub struct Oklab {
    /// The lightness component.
    pub lightness: f32,
    /// The a-axis component.
    pub a: f32,
    /// The b-axis component.
    pub b: f32,
    /// The alpha component.
    pub alpha: f32,
}

macro_rules! impl_lab_like {
    ($cls:ident, $fname:literal) => {
        impl $cls {
            /// Construct a new Lab color format with lightness, a, b and alpha components.
            pub fn new(lightness: f32, a: f32, b: f32, alpha: f32) -> Self {
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
                self.lightness.to_css(dest)?;
                dest.write_char(' ')?;
                self.a.to_css(dest)?;
                dest.write_char(' ')?;
                self.b.to_css(dest)?;
                serialize_alpha(dest, self.alpha, false)?;
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
#[repr(C)]
pub struct Lch {
    /// The lightness component.
    pub lightness: f32,
    /// The chroma component.
    pub chroma: f32,
    /// The hue component.
    pub hue: f32,
    /// The alpha component.
    pub alpha: f32,
}

/// Color specified by lightness, chroma and hue components.
#[derive(Clone, Copy, PartialEq, Debug)]
#[repr(C)]
pub struct Oklch {
    /// The lightness component.
    pub lightness: f32,
    /// The chroma component.
    pub chroma: f32,
    /// The hue component.
    pub hue: f32,
    /// The alpha component.
    pub alpha: f32,
}

macro_rules! impl_lch_like {
    ($cls:ident, $fname:literal) => {
        impl $cls {
            /// Construct a new color with lightness, chroma and hue components.
            pub fn new(lightness: f32, chroma: f32, hue: f32, alpha: f32) -> Self {
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
                self.lightness.to_css(dest)?;
                dest.write_char(' ')?;
                self.chroma.to_css(dest)?;
                dest.write_char(' ')?;
                self.hue.to_css(dest)?;
                serialize_alpha(dest, self.alpha, false)?;
                dest.write_char(')')
            }
        }
    };
}

impl_lch_like!(Lch, "lch");
impl_lch_like!(Oklch, "oklch");

/// A Predefined color space specified in:
/// https://drafts.csswg.org/css-color-4/#predefined
#[derive(Clone, Copy, PartialEq, Debug)]
pub enum PredefinedColorSpace {
    /// https://drafts.csswg.org/css-color-4/#predefined-sRGB
    Srgb,
    /// https://drafts.csswg.org/css-color-4/#predefined-sRGB-linear
    SrgbLinear,
    /// https://drafts.csswg.org/css-color-4/#predefined-display-p3
    DisplayP3,
    /// https://drafts.csswg.org/css-color-4/#predefined-a98-rgb
    A98Rgb,
    /// https://drafts.csswg.org/css-color-4/#predefined-prophoto-rgb
    ProphotoRgb,
    /// https://drafts.csswg.org/css-color-4/#predefined-rec2020
    Rec2020,
    /// https://drafts.csswg.org/css-color-4/#predefined-xyz
    XyzD50,
    /// https://drafts.csswg.org/css-color-4/#predefined-xyz
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
/// https://drafts.csswg.org/css-color-4/#color-function
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct ColorFunction {
    /// The color space for this color.
    pub color_space: PredefinedColorSpace,
    /// The first component of the color.  Either red or x.
    pub c1: f32,
    /// The second component of the color.  Either green or y.
    pub c2: f32,
    /// The third component of the color.  Either blue or z.
    pub c3: f32,
    /// The alpha component of the color.
    pub alpha: f32,
}

impl ColorFunction {
    /// Construct a new color function definition with the given color space and
    /// color components.
    pub fn new(color_space: PredefinedColorSpace, c1: f32, c2: f32, c3: f32, alpha: f32) -> Self {
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
        self.c1.to_css(dest)?;
        dest.write_char(' ')?;
        self.c2.to_css(dest)?;
        dest.write_char(' ')?;
        self.c3.to_css(dest)?;

        serialize_alpha(dest, self.alpha, false)?;

        dest.write_char(')')
    }
}

/// A <color> value.
/// https://drafts.csswg.org/css-color-4/#color-type
#[derive(Clone, Copy, PartialEq, Debug)]
pub enum Color {
    /// The 'currentcolor' keyword.
    CurrentColor,
    /// Specify sRGB colors directly by their red/green/blue/alpha chanels.
    Rgba(RGBA),
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
                let degrees = match_ignore_ascii_case! { &*unit,
                    "deg" => v,
                    "grad" => v * 360. / 400.,
                    "rad" => v * 360. / (2. * PI),
                    "turn" => v * 360.,
                    _ => return Err(location.new_unexpected_token_error(Token::Ident(unit.clone()))),
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
    pub fn parse<'i, 't>(input: &mut Parser<'i, 't>) -> Result<Color, ParseError<'i, ()>> {
        parse_color_with(&DefaultColorParser, input)
    }
}

/// This trait is used by the [`ColorParser`] to construct colors of any type.
pub trait FromParsedColor {
    /// Construct a new color from the CSS `currentcolor` keyword.
    fn from_current_color() -> Self;

    /// Construct a new color from red, green, blue and alpha components.
    fn from_rgba(red: u8, green: u8, blue: u8, alpha: f32) -> Self;

    /// Construct a new color from the `lab` notation.
    fn from_lab(lightness: f32, a: f32, b: f32, alpha: f32) -> Self;

    /// Construct a new color from the `lch` notation.
    fn from_lch(lightness: f32, chroma: f32, hue: f32, alpha: f32) -> Self;

    /// Construct a new color from the `oklab` notation.
    fn from_oklab(lightness: f32, a: f32, b: f32, alpha: f32) -> Self;

    /// Construct a new color from the `oklch` notation.
    fn from_oklch(lightness: f32, chroma: f32, hue: f32, alpha: f32) -> Self;

    /// Construct a new color with a predefined color space.
    fn from_color_function(
        color_space: PredefinedColorSpace,
        c1: f32,
        c2: f32,
        c3: f32,
        alpha: f32,
    ) -> Self;
}

/// Parse a CSS color with the specified [`ColorComponentParser`] and return a
/// new color value on success.
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
        Token::Hash(ref value) | Token::IDHash(ref value) => RGBA::parse_hash(value.as_bytes())
            .map(|rgba| P::Output::from_rgba(rgba.red, rgba.green, rgba.blue, rgba.alpha)),
        Token::Ident(ref value) => parse_color_keyword(&*value),
        Token::Function(ref name) => {
            let name = name.clone();
            return input.parse_nested_block(|arguments| {
                parse_color_function(color_parser, &*name, arguments)
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
    fn from_rgba(red: u8, green: u8, blue: u8, alpha: f32) -> Self {
        Color::Rgba(RGBA::new(red, green, blue, alpha))
    }

    #[inline]
    fn from_lab(lightness: f32, a: f32, b: f32, alpha: f32) -> Self {
        Color::Lab(Lab::new(lightness, a, b, alpha))
    }

    #[inline]
    fn from_lch(lightness: f32, chroma: f32, hue: f32, alpha: f32) -> Self {
        Color::Lch(Lch::new(lightness, chroma, hue, alpha))
    }

    #[inline]
    fn from_oklab(lightness: f32, a: f32, b: f32, alpha: f32) -> Self {
        Color::Oklab(Oklab::new(lightness, a, b, alpha))
    }

    #[inline]
    fn from_oklch(lightness: f32, chroma: f32, hue: f32, alpha: f32) -> Self {
        Color::Oklch(Oklch::new(lightness, chroma, hue, alpha))
    }

    #[inline]
    fn from_color_function(
        color_space: PredefinedColorSpace,
        c1: f32,
        c2: f32,
        c3: f32,
        alpha: f32,
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
        "transparent" => Ok(Output::from_rgba(0, 0, 0, 0.0)),
        "currentcolor" => Ok(Output::from_current_color()),
        _ => keyword(ident)
            .map(|(r, g, b)| Output::from_rgba(*r, *g, *b, 1.0))
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
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1340484
    //
    // Clamping to 256 and rounding after would let 1.0 map to 256, and
    // `256.0_f32 as u8` is undefined behavior:
    //
    // https://github.com/rust-lang/rust/issues/10184
    clamp_floor_256_f32(val * 255.)
}

fn clamp_floor_256_f32(val: f32) -> u8 {
    val.round().max(0.).min(255.) as u8
}

/// Parse one of the color functions: rgba(), lab(), color(), etc.
#[inline]
fn parse_color_function<'i, 't, P>(
    color_parser: &P,
    name: &str,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    // FIXME: Should the parser clamp values? or should specified/computed
    // value processing handle clamping?

    let color = match_ignore_ascii_case! { name,
        "rgb" | "rgba" => parse_rgb(color_parser, arguments),

        "hsl" | "hsla" => parse_hsl_hwb(
            color_parser,
            arguments,
            hsl_to_rgb,
            /* allow_comma = */ true,
        ),

        "hwb" => parse_hsl_hwb(
            color_parser,
            arguments,
            hwb_to_rgb,
            /* allow_comma = */ false,
        ),

        // for L: 0% = 0.0, 100% = 100.0
        // for a and b: -100% = -125, 100% = 125
        "lab" => parse_lab_like(
            color_parser,
            arguments,
            100.0,
            125.0,
            P::Output::from_lab,
        ),

        // for L: 0% = 0.0, 100% = 100.0
        // for C: 0% = 0, 100% = 150
        "lch" => parse_lch_like(
            color_parser,
            arguments,
            100.0,
            150.0,
            P::Output::from_lch,
        ),

        // for L: 0% = 0.0, 100% = 1.0
        // for a and b: -100% = -0.4, 100% = 0.4
        "oklab" => parse_lab_like(
            color_parser,
            arguments,
            1.0,
            0.4,
            P::Output::from_oklab,
        ),

        // for L: 0% = 0.0, 100% = 1.0
        // for C: 0% = 0.0 100% = 0.4
        "oklch" => parse_lch_like(
            color_parser,
            arguments,
            1.0,
            0.4,
            P::Output::from_oklch,
        ),

        "color" => parse_color_color_function(color_parser, arguments),

        _ => return Err(arguments.new_unexpected_token_error(Token::Ident(name.to_owned().into()))),
    }?;

    arguments.expect_exhausted()?;

    Ok(color)
}

#[inline]
fn parse_alpha<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    uses_commas: bool,
) -> Result<f32, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    Ok(if !arguments.is_exhausted() {
        if uses_commas {
            arguments.expect_comma()?;
        } else {
            arguments.expect_delim('/')?;
        };
        color_parser
            .parse_number_or_percentage(arguments)?
            .unit_value()
            .clamp(0.0, OPAQUE)
    } else {
        OPAQUE
    })
}

#[inline]
fn parse_rgb<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    // Either integers or percentages, but all the same type.
    // https://drafts.csswg.org/css-color/#rgb-functions
    let (red, is_number) = match color_parser.parse_number_or_percentage(arguments)? {
        NumberOrPercentage::Number { value } => (clamp_floor_256_f32(value), true),
        NumberOrPercentage::Percentage { unit_value } => (clamp_unit_f32(unit_value), false),
    };

    let uses_commas = arguments.try_parse(|i| i.expect_comma()).is_ok();

    let green;
    let blue;
    if is_number {
        green = clamp_floor_256_f32(color_parser.parse_number(arguments)?);
        if uses_commas {
            arguments.expect_comma()?;
        }
        blue = clamp_floor_256_f32(color_parser.parse_number(arguments)?);
    } else {
        green = clamp_unit_f32(color_parser.parse_percentage(arguments)?);
        if uses_commas {
            arguments.expect_comma()?;
        }
        blue = clamp_unit_f32(color_parser.parse_percentage(arguments)?);
    }

    let alpha = parse_alpha(color_parser, arguments, uses_commas)?;

    Ok(P::Output::from_rgba(red, green, blue, alpha))
}

/// Parses hsl and hbw syntax, which happens to be identical.
///
/// https://drafts.csswg.org/css-color/#the-hsl-notation
/// https://drafts.csswg.org/css-color/#the-hbw-notation
#[inline]
fn parse_hsl_hwb<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    to_rgb: impl FnOnce(f32, f32, f32) -> (f32, f32, f32),
    allow_comma: bool,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    // Hue given as an angle
    // https://drafts.csswg.org/css-values/#angles
    let hue_degrees = color_parser.parse_angle_or_number(arguments)?.degrees();

    // Subtract an integer before rounding, to avoid some rounding errors:
    let hue_normalized_degrees = hue_degrees - 360. * (hue_degrees / 360.).floor();
    let hue = hue_normalized_degrees / 360.;

    // Saturation and lightness are clamped to 0% ... 100%
    let uses_commas = allow_comma && arguments.try_parse(|i| i.expect_comma()).is_ok();

    let first_percentage = color_parser.parse_percentage(arguments)?.clamp(0.0, 1.0);

    if uses_commas {
        arguments.expect_comma()?;
    }

    let second_percentage = color_parser.parse_percentage(arguments)?.clamp(0.0, 1.0);

    let (red, green, blue) = to_rgb(hue, first_percentage, second_percentage);
    let red = clamp_unit_f32(red);
    let green = clamp_unit_f32(green);
    let blue = clamp_unit_f32(blue);

    let alpha = parse_alpha(color_parser, arguments, uses_commas)?;

    Ok(P::Output::from_rgba(red, green, blue, alpha))
}

/// https://drafts.csswg.org/css-color-4/#hwb-to-rgb
#[inline]
pub fn hwb_to_rgb(h: f32, w: f32, b: f32) -> (f32, f32, f32) {
    if w + b >= 1.0 {
        let gray = w / (w + b);
        return (gray, gray, gray);
    }

    let (mut red, mut green, mut blue) = hsl_to_rgb(h, 1.0, 0.5);
    let x = 1.0 - w - b;
    red = red * x + w;
    green = green * x + w;
    blue = blue * x + w;
    (red, green, blue)
}

/// https://drafts.csswg.org/css-color/#hsl-color
/// except with h pre-multiplied by 3, to avoid some rounding errors.
#[inline]
pub fn hsl_to_rgb(hue: f32, saturation: f32, lightness: f32) -> (f32, f32, f32) {
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

#[inline]
fn parse_lab_like<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    lightness_range: f32,
    a_b_range: f32,
    into_color: fn(l: f32, a: f32, b: f32, alpha: f32) -> P::Output,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    let lightness = match color_parser.parse_number_or_percentage(arguments)? {
        NumberOrPercentage::Number { value } => value,
        NumberOrPercentage::Percentage { unit_value } => unit_value * lightness_range,
    }
    .max(0.);

    macro_rules! parse_a_b {
        () => {{
            match color_parser.parse_number_or_percentage(arguments)? {
                NumberOrPercentage::Number { value } => value,
                NumberOrPercentage::Percentage { unit_value } => unit_value * a_b_range,
            }
        }};
    }

    let a = parse_a_b!();
    let b = parse_a_b!();

    let alpha = parse_alpha(color_parser, arguments, false)?;

    Ok(into_color(lightness, a, b, alpha))
}

#[inline]
fn parse_lch_like<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    lightness_range: f32,
    chroma_range: f32,
    into_color: fn(l: f32, c: f32, h: f32, alpha: f32) -> P::Output,
) -> Result<P::Output, ParseError<'i, P::Error>>
where
    P: ColorParser<'i>,
{
    // for L: 0% = 0.0, 100% = 100.0
    let lightness = match color_parser.parse_number_or_percentage(arguments)? {
        NumberOrPercentage::Number { value } => value,
        NumberOrPercentage::Percentage { unit_value } => unit_value * lightness_range,
    }
    .max(0.);

    // for C: 0% = 0, 100% = 150
    let chroma = match color_parser.parse_number_or_percentage(arguments)? {
        NumberOrPercentage::Number { value } => value,
        NumberOrPercentage::Percentage { unit_value } => unit_value * chroma_range,
    }
    .max(0.);

    let hue_degrees = color_parser.parse_angle_or_number(arguments)?.degrees();
    let hue = hue_degrees - 360. * (hue_degrees / 360.).floor();

    let alpha = parse_alpha(color_parser, arguments, false)?;

    Ok(into_color(lightness, chroma, hue, alpha))
}

/// Parse the color() function.
#[inline]
fn parse_color_color_function<'i, 't, P>(
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

    macro_rules! parse_component {
        () => {{
            color_parser
                .parse_number_or_percentage(arguments)?
                .unit_value()
        }};
    }

    let c1 = parse_component!();
    let c2 = parse_component!();
    let c3 = parse_component!();

    let alpha = parse_alpha(color_parser, arguments, false)?;

    Ok(P::Output::from_color_function(
        color_space,
        c1,
        c2,
        c3,
        alpha,
    ))
}
