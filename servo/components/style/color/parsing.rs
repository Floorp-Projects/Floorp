/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![deny(missing_docs)]

//! Fairly complete css-color implementation.
//! Relative colors, color-mix, system colors, and other such things require better calc() support
//! and integration.

use super::{
    convert::{hsl_to_rgb, hwb_to_rgb, normalize_hue},
    ColorComponents,
};
use crate::values::normalize;
use cssparser::color::{
    clamp_floor_256_f32, clamp_unit_f32, parse_hash_color, serialize_color_alpha,
    PredefinedColorSpace, OPAQUE,
};
use cssparser::{match_ignore_ascii_case, CowRcStr, ParseError, Parser, ToCss, Token};
#[cfg(feature = "serde")]
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::f32::consts::PI;
use std::fmt;
use std::str::FromStr;

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
    Ok(match_ignore_ascii_case! { ident ,
        "transparent" => Output::from_rgba(0, 0, 0, 0.0),
        "currentcolor" => Output::from_current_color(),
        _ => {
            let (r, g, b) = cssparser::color::parse_named_color(ident)?;
            Output::from_rgba(r, g, b, OPAQUE)
        }
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
        Token::Hash(ref value) | Token::IDHash(ref value) => {
            parse_hash_color(value.as_bytes()).map(|(r, g, b, a)| P::Output::from_rgba(r, g, b, a))
        },
        Token::Ident(ref value) => parse_color_keyword(value),
        Token::Function(ref name) => {
            let name = name.clone();
            return input.parse_nested_block(|arguments| {
                parse_color_function(color_parser, name, arguments)
            });
        },
        _ => Err(()),
    }
    .map_err(|()| location.new_unexpected_token_error(token.clone()))
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
    // Percent reference range for alpha: 0% = 0.0, 100% = 1.0
    let alpha = color_parser
        .parse_number_or_percentage(arguments)?
        .to_number(1.0);
    Ok(normalize(alpha).clamp(0.0, OPAQUE))
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

    let (red, green, blue, alpha) = if is_legacy_syntax {
        let (red, green, blue) = match maybe_red.unwrap() {
            NumberOrPercentage::Number { value } => {
                let red = clamp_floor_256_f32(value);
                let green = clamp_floor_256_f32(color_parser.parse_number(arguments)?);
                arguments.expect_comma()?;
                let blue = clamp_floor_256_f32(color_parser.parse_number(arguments)?);
                (red, green, blue)
            },
            NumberOrPercentage::Percentage { unit_value } => {
                let red = clamp_unit_f32(unit_value);
                let green = clamp_unit_f32(color_parser.parse_percentage(arguments)?);
                arguments.expect_comma()?;
                let blue = clamp_unit_f32(color_parser.parse_percentage(arguments)?);
                (red, green, blue)
            },
        };

        let alpha = parse_legacy_alpha(color_parser, arguments)?;

        (red, green, blue, alpha)
    } else {
        #[inline]
        fn get_component_value(c: Option<NumberOrPercentage>) -> u8 {
            c.map(|c| match c {
                NumberOrPercentage::Number { value } => clamp_floor_256_f32(value),
                NumberOrPercentage::Percentage { unit_value } => clamp_unit_f32(unit_value),
            })
            .unwrap_or(0)
        }

        let red = get_component_value(maybe_red);

        let green = get_component_value(parse_none_or(arguments, |p| {
            color_parser.parse_number_or_percentage(p)
        })?);

        let blue = get_component_value(parse_none_or(arguments, |p| {
            color_parser.parse_number_or_percentage(p)
        })?);

        let alpha = parse_modern_alpha(color_parser, arguments)?.unwrap_or(0.0);

        (red, green, blue, alpha)
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
    // Percent reference range for S and L: 0% = 0.0, 100% = 100.0
    const LIGHTNESS_RANGE: f32 = 100.0;
    const SATURATION_RANGE: f32 = 100.0;

    let maybe_hue = parse_none_or(arguments, |p| color_parser.parse_angle_or_number(p))?;

    // If the hue is not "none" and is followed by a comma, then we are parsing
    // the legacy syntax.
    let is_legacy_syntax = maybe_hue.is_some() && arguments.try_parse(|p| p.expect_comma()).is_ok();

    let saturation: Option<f32>;
    let lightness: Option<f32>;

    let alpha = if is_legacy_syntax {
        saturation = Some(color_parser.parse_percentage(arguments)? * SATURATION_RANGE);
        arguments.expect_comma()?;
        lightness = Some(color_parser.parse_percentage(arguments)? * LIGHTNESS_RANGE);
        Some(parse_legacy_alpha(color_parser, arguments)?)
    } else {
        saturation = parse_none_or(arguments, |p| color_parser.parse_number_or_percentage(p))?
            .map(|v| v.to_number(SATURATION_RANGE));
        lightness = parse_none_or(arguments, |p| color_parser.parse_number_or_percentage(p))?
            .map(|v| v.to_number(LIGHTNESS_RANGE));
        parse_modern_alpha(color_parser, arguments)?
    };

    let hue = maybe_hue.map(|h| normalize_hue(h.degrees()));
    let saturation = saturation.map(|s| s.clamp(0.0, SATURATION_RANGE));
    let lightness = lightness.map(|l| l.clamp(0.0, LIGHTNESS_RANGE));

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
    // Percent reference range for W and B: 0% = 0.0, 100% = 100.0
    const WHITENESS_RANGE: f32 = 100.0;
    const BLACKNESS_RANGE: f32 = 100.0;

    let (hue, whiteness, blackness, alpha) = parse_components(
        color_parser,
        arguments,
        P::parse_angle_or_number,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
    )?;

    let hue = hue.map(|h| normalize_hue(h.degrees()));
    let whiteness = whiteness.map(|w| w.to_number(WHITENESS_RANGE).clamp(0.0, WHITENESS_RANGE));
    let blackness = blackness.map(|b| b.to_number(BLACKNESS_RANGE).clamp(0.0, BLACKNESS_RANGE));

    Ok(P::Output::from_hwb(hue, whiteness, blackness, alpha))
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

    let lightness = lightness.map(|l| l.to_number(lightness_range));
    let a = a.map(|a| a.to_number(a_b_range));
    let b = b.map(|b| b.to_number(a_b_range));

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

    let lightness = lightness.map(|l| l.to_number(lightness_range));
    let chroma = chroma.map(|c| c.to_number(chroma_range));
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

    let c1 = c1.map(|c| c.to_number(1.0));
    let c2 = c2.map(|c| c.to_number(1.0));
    let c3 = c3.map(|c| c.to_number(1.0));

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

fn parse_none_or<'i, 't, F, T, E>(input: &mut Parser<'i, 't>, thing: F) -> Result<Option<T>, E>
where
    F: FnOnce(&mut Parser<'i, 't>) -> Result<T, E>,
{
    match input.try_parse(|p| p.expect_ident_matching("none")) {
        Ok(_) => Ok(None),
        Err(_) => Ok(Some(thing(input)?)),
    }
}

/// A [`ModernComponent`] can serialize to `none`, `nan`, `infinity` and
/// floating point values.
struct ModernComponent<'a>(&'a Option<f32>);

impl<'a> ToCss for ModernComponent<'a> {
    fn to_css<W>(&self, dest: &mut W) -> fmt::Result
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

#[cfg(feature = "serde")]
impl Serialize for RgbaLegacy {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        (self.red, self.green, self.blue, self.alpha).serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl<'de> Deserialize<'de> for RgbaLegacy {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let (r, g, b, a) = Deserialize::deserialize(deserializer)?;
        Ok(RgbaLegacy::new(r, g, b, a))
    }
}

impl ToCss for RgbaLegacy {
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

        // Legacy syntax does not allow none components.
        serialize_color_alpha(dest, Some(self.alpha), true)?;

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
        let ColorComponents(red, green, blue) = hsl_to_rgb(&ColorComponents(
            self.hue.unwrap_or(0.0) / 360.0,
            self.saturation.unwrap_or(0.0),
            self.lightness.unwrap_or(0.0),
        ));

        RgbaLegacy::from_floats(red, green, blue, self.alpha.unwrap_or(OPAQUE)).to_css(dest)
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
        let ColorComponents(red, green, blue) = hwb_to_rgb(&ColorComponents(
            self.hue.unwrap_or(0.0) / 360.0,
            self.whiteness.unwrap_or(0.0),
            self.blackness.unwrap_or(0.0),
        ));

        RgbaLegacy::from_floats(red, green, blue, self.alpha.unwrap_or(OPAQUE)).to_css(dest)
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
        ModernComponent(&self.c1).to_css(dest)?;
        dest.write_char(' ')?;
        ModernComponent(&self.c2).to_css(dest)?;
        dest.write_char(' ')?;
        ModernComponent(&self.c3).to_css(dest)?;

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
    Rgba(RgbaLegacy),
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
    /// Return the value as a number. Percentages will be adjusted to the range
    /// [0..percent_basis].
    pub fn to_number(&self, percentage_basis: f32) -> f32 {
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
    /// Return the angle in degrees. `AngleOrNumber::Number` is returned as
    /// degrees, because it is the canonical unit.
    pub fn degrees(&self) -> f32 {
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
            },
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
    fn from_rgba(red: u8, green: u8, blue: u8, alpha: f32) -> Self;

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

impl FromParsedColor for Color {
    #[inline]
    fn from_current_color() -> Self {
        Color::CurrentColor
    }

    #[inline]
    fn from_rgba(red: u8, green: u8, blue: u8, alpha: f32) -> Self {
        Color::Rgba(RgbaLegacy::new(red, green, blue, alpha))
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
