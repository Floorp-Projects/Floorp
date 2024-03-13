/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![deny(missing_docs)]

//! Fairly complete css-color implementation.
//! Relative colors, color-mix, system colors, and other such things require better calc() support
//! and integration.

use crate::color::convert::normalize_hue;
use crate::values::normalize;
use cssparser::color::{
    clamp_floor_256_f32, clamp_unit_f32, parse_hash_color, PredefinedColorSpace, OPAQUE,
};
use cssparser::{match_ignore_ascii_case, CowRcStr, Parser, Token};
use std::str::FromStr;
use style_traits::ParseError;

use super::component::ColorComponent;

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
) -> Result<P::Output, ParseError<'i>>
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
) -> Result<P::Output, ParseError<'i>>
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
) -> Result<f32, ParseError<'i>>
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
) -> Result<f32, ParseError<'i>>
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
) -> Result<ColorComponent<f32>, ParseError<'i>>
where
    P: ColorParser<'i>,
{
    if !arguments.is_exhausted() {
        arguments.expect_delim('/')?;
        parse_none_or(arguments, |p| parse_alpha_component(color_parser, p))
    } else {
        Ok(ColorComponent::Value(OPAQUE))
    }
}

#[inline]
fn parse_rgb<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i>>
where
    P: ColorParser<'i>,
{
    let maybe_red = parse_none_or(arguments, |p| color_parser.parse_number_or_percentage(p))?;

    // If the first component is not "none" and is followed by a comma, then we
    // are parsing the legacy syntax.
    let is_legacy_syntax =
        !maybe_red.is_none() && arguments.try_parse(|p| p.expect_comma()).is_ok();

    let (red, green, blue, alpha) = if is_legacy_syntax {
        let (red, green, blue) = match maybe_red.into_value() {
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
        fn get_component_value(c: ColorComponent<NumberOrPercentage>) -> u8 {
            c.map_value(|c| match c {
                NumberOrPercentage::Number { value } => clamp_floor_256_f32(value),
                NumberOrPercentage::Percentage { unit_value } => clamp_unit_f32(unit_value),
            })
            .into_value_or(0)
        }

        let red = get_component_value(maybe_red);

        let green = get_component_value(parse_none_or(arguments, |p| {
            color_parser.parse_number_or_percentage(p)
        })?);

        let blue = get_component_value(parse_none_or(arguments, |p| {
            color_parser.parse_number_or_percentage(p)
        })?);

        let alpha = parse_modern_alpha(color_parser, arguments)?.into_value_or(0.0);

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
) -> Result<P::Output, ParseError<'i>>
where
    P: ColorParser<'i>,
{
    // Percent reference range for S and L: 0% = 0.0, 100% = 100.0
    const LIGHTNESS_RANGE: f32 = 100.0;
    const SATURATION_RANGE: f32 = 100.0;

    let maybe_hue = parse_none_or(arguments, |p| color_parser.parse_number_or_angle(p))?;

    // If the hue is not "none" and is followed by a comma, then we are parsing
    // the legacy syntax.
    let is_legacy_syntax =
        !maybe_hue.is_none() && arguments.try_parse(|p| p.expect_comma()).is_ok();

    let saturation: ColorComponent<f32>;
    let lightness: ColorComponent<f32>;

    let alpha = if is_legacy_syntax {
        saturation =
            ColorComponent::Value(color_parser.parse_percentage(arguments)? * SATURATION_RANGE);
        arguments.expect_comma()?;
        lightness =
            ColorComponent::Value(color_parser.parse_percentage(arguments)? * LIGHTNESS_RANGE);
        ColorComponent::Value(parse_legacy_alpha(color_parser, arguments)?)
    } else {
        saturation = parse_none_or(arguments, |p| color_parser.parse_number_or_percentage(p))?
            .map_value(|v| v.to_number(SATURATION_RANGE));
        lightness = parse_none_or(arguments, |p| color_parser.parse_number_or_percentage(p))?
            .map_value(|v| v.to_number(LIGHTNESS_RANGE));
        parse_modern_alpha(color_parser, arguments)?
    };

    let hue = maybe_hue.map_value(|h| normalize_hue(h.degrees()));
    let saturation = saturation.map_value(|s| s.clamp(0.0, SATURATION_RANGE));
    let lightness = lightness.map_value(|l| l.clamp(0.0, LIGHTNESS_RANGE));

    Ok(P::Output::from_hsl(hue, saturation, lightness, alpha))
}

/// Parses hwb syntax.
///
/// <https://drafts.csswg.org/css-color/#the-hbw-notation>
#[inline]
fn parse_hwb<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i>>
where
    P: ColorParser<'i>,
{
    // Percent reference range for W and B: 0% = 0.0, 100% = 100.0
    const WHITENESS_RANGE: f32 = 100.0;
    const BLACKNESS_RANGE: f32 = 100.0;

    let (hue, whiteness, blackness, alpha) = parse_components(
        color_parser,
        arguments,
        P::parse_number_or_angle,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
    )?;

    let hue = hue.map_value(|h| normalize_hue(h.degrees()));
    let whiteness =
        whiteness.map_value(|w| w.to_number(WHITENESS_RANGE).clamp(0.0, WHITENESS_RANGE));
    let blackness =
        blackness.map_value(|b| b.to_number(BLACKNESS_RANGE).clamp(0.0, BLACKNESS_RANGE));

    Ok(P::Output::from_hwb(hue, whiteness, blackness, alpha))
}

type IntoColorFn<Output> = fn(
    l: ColorComponent<f32>,
    a: ColorComponent<f32>,
    b: ColorComponent<f32>,
    alpha: ColorComponent<f32>,
) -> Output;

#[inline]
fn parse_lab_like<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    lightness_range: f32,
    a_b_range: f32,
    into_color: IntoColorFn<P::Output>,
) -> Result<P::Output, ParseError<'i>>
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

    let lightness = lightness.map_value(|l| l.to_number(lightness_range));
    let a = a.map_value(|a| a.to_number(a_b_range));
    let b = b.map_value(|b| b.to_number(a_b_range));

    Ok(into_color(lightness, a, b, alpha))
}

#[inline]
fn parse_lch_like<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    lightness_range: f32,
    chroma_range: f32,
    into_color: IntoColorFn<P::Output>,
) -> Result<P::Output, ParseError<'i>>
where
    P: ColorParser<'i>,
{
    let (lightness, chroma, hue, alpha) = parse_components(
        color_parser,
        arguments,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
        P::parse_number_or_angle,
    )?;

    let lightness = lightness.map_value(|l| l.to_number(lightness_range));
    let chroma = chroma.map_value(|c| c.to_number(chroma_range));
    let hue = hue.map_value(|h| normalize_hue(h.degrees()));

    Ok(into_color(lightness, chroma, hue, alpha))
}

/// Parse the color() function.
#[inline]
fn parse_color_with_color_space<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<P::Output, ParseError<'i>>
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

    let c1 = c1.map_value(|c| c.to_number(1.0));
    let c2 = c2.map_value(|c| c.to_number(1.0));
    let c3 = c3.map_value(|c| c.to_number(1.0));

    Ok(P::Output::from_color_function(
        color_space,
        c1,
        c2,
        c3,
        alpha,
    ))
}

type ComponentParseResult<'i, R1, R2, R3> = Result<
    (
        ColorComponent<R1>,
        ColorComponent<R2>,
        ColorComponent<R3>,
        ColorComponent<f32>,
    ),
    ParseError<'i>,
>;

/// Parse the color components and alpha with the modern [color-4] syntax.
pub fn parse_components<'i, 't, P, F1, F2, F3, R1, R2, R3>(
    color_parser: &P,
    input: &mut Parser<'i, 't>,
    f1: F1,
    f2: F2,
    f3: F3,
) -> ComponentParseResult<'i, R1, R2, R3>
where
    P: ColorParser<'i>,
    F1: FnOnce(&P, &mut Parser<'i, 't>) -> Result<R1, ParseError<'i>>,
    F2: FnOnce(&P, &mut Parser<'i, 't>) -> Result<R2, ParseError<'i>>,
    F3: FnOnce(&P, &mut Parser<'i, 't>) -> Result<R3, ParseError<'i>>,
{
    let r1 = parse_none_or(input, |p| f1(color_parser, p))?;
    let r2 = parse_none_or(input, |p| f2(color_parser, p))?;
    let r3 = parse_none_or(input, |p| f3(color_parser, p))?;

    let alpha = parse_modern_alpha(color_parser, input)?;

    Ok((r1, r2, r3, alpha))
}

fn parse_none_or<'i, 't, F, T, E>(
    input: &mut Parser<'i, 't>,
    thing: F,
) -> Result<ColorComponent<T>, E>
where
    F: FnOnce(&mut Parser<'i, 't>) -> Result<T, E>,
{
    match input.try_parse(|p| p.expect_ident_matching("none")) {
        Ok(_) => Ok(ColorComponent::None),
        Err(_) => Ok(ColorComponent::Value(thing(input)?)),
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
pub enum NumberOrAngle {
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

impl NumberOrAngle {
    /// Return the angle in degrees. `NumberOrAngle::Number` is returned as
    /// degrees, because it is the canonical unit.
    pub fn degrees(&self) -> f32 {
        match *self {
            Self::Number { value } => value,
            Self::Angle { degrees } => degrees,
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

    /// Parse an `<angle>` or `<number>`.
    ///
    /// Returns the result in degrees.
    fn parse_number_or_angle<'t>(
        &self,
        input: &mut Parser<'i, 't>,
    ) -> Result<NumberOrAngle, ParseError<'i>>;

    /// Parse a `<percentage>` value.
    ///
    /// Returns the result in a number from 0.0 to 1.0.
    fn parse_percentage<'t>(&self, input: &mut Parser<'i, 't>) -> Result<f32, ParseError<'i>>;

    /// Parse a `<number>` value.
    fn parse_number<'t>(&self, input: &mut Parser<'i, 't>) -> Result<f32, ParseError<'i>>;

    /// Parse a `<number>` value or a `<percentage>` value.
    fn parse_number_or_percentage<'t>(
        &self,
        input: &mut Parser<'i, 't>,
    ) -> Result<NumberOrPercentage, ParseError<'i>>;
}

/// This trait is used by the [`ColorParser`] to construct colors of any type.
pub trait FromParsedColor {
    /// Construct a new color from the CSS `currentcolor` keyword.
    fn from_current_color() -> Self;

    /// Construct a new color from red, green, blue and alpha components.
    fn from_rgba(red: u8, green: u8, blue: u8, alpha: f32) -> Self;

    /// Construct a new color from hue, saturation, lightness and alpha components.
    fn from_hsl(
        hue: ColorComponent<f32>,
        saturation: ColorComponent<f32>,
        lightness: ColorComponent<f32>,
        alpha: ColorComponent<f32>,
    ) -> Self;

    /// Construct a new color from hue, blackness, whiteness and alpha components.
    fn from_hwb(
        hue: ColorComponent<f32>,
        whiteness: ColorComponent<f32>,
        blackness: ColorComponent<f32>,
        alpha: ColorComponent<f32>,
    ) -> Self;

    /// Construct a new color from the `lab` notation.
    fn from_lab(
        lightness: ColorComponent<f32>,
        a: ColorComponent<f32>,
        b: ColorComponent<f32>,
        alpha: ColorComponent<f32>,
    ) -> Self;

    /// Construct a new color from the `lch` notation.
    fn from_lch(
        lightness: ColorComponent<f32>,
        chroma: ColorComponent<f32>,
        hue: ColorComponent<f32>,
        alpha: ColorComponent<f32>,
    ) -> Self;

    /// Construct a new color from the `oklab` notation.
    fn from_oklab(
        lightness: ColorComponent<f32>,
        a: ColorComponent<f32>,
        b: ColorComponent<f32>,
        alpha: ColorComponent<f32>,
    ) -> Self;

    /// Construct a new color from the `oklch` notation.
    fn from_oklch(
        lightness: ColorComponent<f32>,
        chroma: ColorComponent<f32>,
        hue: ColorComponent<f32>,
        alpha: ColorComponent<f32>,
    ) -> Self;

    /// Construct a new color with a predefined color space.
    fn from_color_function(
        color_space: PredefinedColorSpace,
        c1: ColorComponent<f32>,
        c2: ColorComponent<f32>,
        c3: ColorComponent<f32>,
        alpha: ColorComponent<f32>,
    ) -> Self;
}
