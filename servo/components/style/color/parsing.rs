/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![deny(missing_docs)]

//! Fairly complete css-color implementation.
//! Relative colors, color-mix, system colors, and other such things require better calc() support
//! and integration.

use crate::color::component::ColorComponent;
use cssparser::color::{
    clamp_floor_256_f32, clamp_unit_f32, parse_hash_color, PredefinedColorSpace, OPAQUE,
};
use cssparser::{match_ignore_ascii_case, CowRcStr, Parser, Token};
use std::str::FromStr;
use style_traits::{ParseError, StyleParseErrorKind};

impl From<u8> for ColorComponent<u8> {
    #[inline]
    fn from(value: u8) -> Self {
        ColorComponent::Value(value)
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
    Ok(match_ignore_ascii_case! { ident,
        "transparent" => Output::from_rgba(
            0u8.into(),
            0u8.into(),
            0u8.into(),
            ColorComponent::Value(NumberOrPercentage::Number { value: 0.0 }),
        ),
        "currentcolor" => Output::from_current_color(),
        _ => {
            let (r, g, b) = cssparser::color::parse_named_color(ident)?;
            Output::from_rgba(
                r.into(),
                g.into(),
                b.into(),
                ColorComponent::Value(NumberOrPercentage::Number { value: OPAQUE }),
            )
        },
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
        Token::Hash(ref value) | Token::IDHash(ref value) => parse_hash_color(value.as_bytes())
            .map(|(r, g, b, a)| {
                P::Output::from_rgba(
                    r.into(),
                    g.into(),
                    b.into(),
                    ColorComponent::Value(NumberOrPercentage::Number { value: a }),
                )
            }),
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
        "lab" => parse_lab_like(color_parser, arguments, P::Output::from_lab),
        "lch" => parse_lch_like(color_parser, arguments, P::Output::from_lch),
        "oklab" => parse_lab_like(color_parser, arguments, P::Output::from_oklab),
        "oklch" => parse_lch_like(color_parser, arguments, P::Output::from_oklch),
        "color" => parse_color_with_color_space(color_parser, arguments),
        _ => return Err(arguments.new_unexpected_token_error(Token::Ident(name))),
    }?;

    arguments.expect_exhausted()?;

    Ok(color)
}

fn parse_legacy_alpha<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>>
where
    P: ColorParser<'i>,
{
    if !arguments.is_exhausted() {
        arguments.expect_comma()?;
        color_parser.parse_number_or_percentage(arguments, false)
    } else {
        Ok(ColorComponent::Value(NumberOrPercentage::Number {
            value: OPAQUE,
        }))
    }
}

fn parse_modern_alpha<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>>
where
    P: ColorParser<'i>,
{
    if !arguments.is_exhausted() {
        arguments.expect_delim('/')?;
        color_parser.parse_number_or_percentage(arguments, true)
    } else {
        Ok(ColorComponent::Value(NumberOrPercentage::Number {
            value: OPAQUE,
        }))
    }
}

impl ColorComponent<NumberOrPercentage> {
    /// Return true if the component contains a percentage.
    pub fn is_percentage(&self) -> Result<bool, ()> {
        Ok(match self {
            Self::Value(NumberOrPercentage::Percentage { .. }) => true,
            _ => false,
        })
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
    let location = arguments.current_source_location();

    let maybe_red = color_parser.parse_number_or_percentage(arguments, true)?;

    // If the first component is not "none" and is followed by a comma, then we
    // are parsing the legacy syntax.
    let is_legacy_syntax =
        !maybe_red.is_none() && arguments.try_parse(|p| p.expect_comma()).is_ok();

    let (red, green, blue, alpha) = if is_legacy_syntax {
        let Ok(is_percentage) = maybe_red.is_percentage() else {
            return Err(location.new_custom_error(StyleParseErrorKind::UnspecifiedError));
        };
        let (red, green, blue) = if is_percentage {
            let red = maybe_red.map_value(|v| clamp_unit_f32(v.to_number(1.0)));
            let green = color_parser
                .parse_percentage(arguments, false)?
                .map_value(clamp_unit_f32);
            arguments.expect_comma()?;
            let blue = color_parser
                .parse_percentage(arguments, false)?
                .map_value(clamp_unit_f32);
            (red, green, blue)
        } else {
            let red = maybe_red.map_value(|v| clamp_floor_256_f32(v.to_number(255.0)));
            let green = color_parser
                .parse_number(arguments, false)?
                .map_value(clamp_floor_256_f32);
            arguments.expect_comma()?;
            let blue = color_parser
                .parse_number(arguments, false)?
                .map_value(clamp_floor_256_f32);
            (red, green, blue)
        };

        let alpha = parse_legacy_alpha(color_parser, arguments)?;

        (red, green, blue, alpha)
    } else {
        let red = maybe_red.map_value(|v| clamp_floor_256_f32(v.to_number(255.0)));
        let green = color_parser
            .parse_number_or_percentage(arguments, true)?
            .map_value(|v| clamp_floor_256_f32(v.to_number(255.0)));
        let blue = color_parser
            .parse_number_or_percentage(arguments, true)?
            .map_value(|v| clamp_floor_256_f32(v.to_number(255.0)));

        let alpha = parse_modern_alpha(color_parser, arguments)?;

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
    let hue = color_parser.parse_number_or_angle(arguments, true)?;

    // If the hue is not "none" and is followed by a comma, then we are parsing
    // the legacy syntax.
    let is_legacy_syntax = !hue.is_none() && arguments.try_parse(|p| p.expect_comma()).is_ok();

    let (saturation, lightness, alpha) = if is_legacy_syntax {
        let saturation = color_parser
            .parse_percentage(arguments, false)?
            .map_value(|unit_value| NumberOrPercentage::Percentage { unit_value });
        arguments.expect_comma()?;
        let lightness = color_parser
            .parse_percentage(arguments, false)?
            .map_value(|unit_value| NumberOrPercentage::Percentage { unit_value });
        (
            saturation,
            lightness,
            parse_legacy_alpha(color_parser, arguments)?,
        )
    } else {
        let saturation = color_parser.parse_number_or_percentage(arguments, true)?;
        let lightness = color_parser.parse_number_or_percentage(arguments, true)?;
        (
            saturation,
            lightness,
            parse_modern_alpha(color_parser, arguments)?,
        )
    };

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
    let (hue, whiteness, blackness, alpha) = parse_components(
        color_parser,
        arguments,
        P::parse_number_or_angle,
        P::parse_number_or_percentage,
        P::parse_number_or_percentage,
    )?;

    Ok(P::Output::from_hwb(hue, whiteness, blackness, alpha))
}

type IntoLabFn<Output> = fn(
    l: ColorComponent<NumberOrPercentage>,
    a: ColorComponent<NumberOrPercentage>,
    b: ColorComponent<NumberOrPercentage>,
    alpha: ColorComponent<NumberOrPercentage>,
) -> Output;

#[inline]
fn parse_lab_like<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    into_color: IntoLabFn<P::Output>,
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

    Ok(into_color(lightness, a, b, alpha))
}

type IntoLchFn<Output> = fn(
    l: ColorComponent<NumberOrPercentage>,
    a: ColorComponent<NumberOrPercentage>,
    b: ColorComponent<NumberOrAngle>,
    alpha: ColorComponent<NumberOrPercentage>,
) -> Output;

#[inline]
fn parse_lch_like<'i, 't, P>(
    color_parser: &P,
    arguments: &mut Parser<'i, 't>,
    into_color: IntoLchFn<P::Output>,
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
        ColorComponent<NumberOrPercentage>,
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
    F1: FnOnce(&P, &mut Parser<'i, 't>, bool) -> Result<ColorComponent<R1>, ParseError<'i>>,
    F2: FnOnce(&P, &mut Parser<'i, 't>, bool) -> Result<ColorComponent<R2>, ParseError<'i>>,
    F3: FnOnce(&P, &mut Parser<'i, 't>, bool) -> Result<ColorComponent<R3>, ParseError<'i>>,
{
    let r1 = f1(color_parser, input, true)?;
    let r2 = f2(color_parser, input, true)?;
    let r3 = f3(color_parser, input, true)?;

    let alpha = parse_modern_alpha(color_parser, input)?;

    Ok((r1, r2, r3, alpha))
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
        allow_none: bool,
    ) -> Result<ColorComponent<NumberOrAngle>, ParseError<'i>>;

    /// Parse a `<percentage>` value.
    ///
    /// Returns the result in a number from 0.0 to 1.0.
    fn parse_percentage<'t>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<f32>, ParseError<'i>>;

    /// Parse a `<number>` value.
    fn parse_number<'t>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<f32>, ParseError<'i>>;

    /// Parse a `<number>` value or a `<percentage>` value.
    fn parse_number_or_percentage<'t>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>>;
}

/// This trait is used by the [`ColorParser`] to construct colors of any type.
pub trait FromParsedColor {
    /// Construct a new color from the CSS `currentcolor` keyword.
    fn from_current_color() -> Self;

    /// Construct a new color from red, green, blue and alpha components.
    fn from_rgba(
        red: ColorComponent<u8>,
        green: ColorComponent<u8>,
        blue: ColorComponent<u8>,
        alpha: ColorComponent<NumberOrPercentage>,
    ) -> Self;

    /// Construct a new color from hue, saturation, lightness and alpha components.
    fn from_hsl(
        hue: ColorComponent<NumberOrAngle>,
        saturation: ColorComponent<NumberOrPercentage>,
        lightness: ColorComponent<NumberOrPercentage>,
        alpha: ColorComponent<NumberOrPercentage>,
    ) -> Self;

    /// Construct a new color from hue, blackness, whiteness and alpha components.
    fn from_hwb(
        hue: ColorComponent<NumberOrAngle>,
        whiteness: ColorComponent<NumberOrPercentage>,
        blackness: ColorComponent<NumberOrPercentage>,
        alpha: ColorComponent<NumberOrPercentage>,
    ) -> Self;

    /// Construct a new color from the `lab` notation.
    fn from_lab(
        lightness: ColorComponent<NumberOrPercentage>,
        a: ColorComponent<NumberOrPercentage>,
        b: ColorComponent<NumberOrPercentage>,
        alpha: ColorComponent<NumberOrPercentage>,
    ) -> Self;

    /// Construct a new color from the `lch` notation.
    fn from_lch(
        lightness: ColorComponent<NumberOrPercentage>,
        chroma: ColorComponent<NumberOrPercentage>,
        hue: ColorComponent<NumberOrAngle>,
        alpha: ColorComponent<NumberOrPercentage>,
    ) -> Self;

    /// Construct a new color from the `oklab` notation.
    fn from_oklab(
        lightness: ColorComponent<NumberOrPercentage>,
        a: ColorComponent<NumberOrPercentage>,
        b: ColorComponent<NumberOrPercentage>,
        alpha: ColorComponent<NumberOrPercentage>,
    ) -> Self;

    /// Construct a new color from the `oklch` notation.
    fn from_oklch(
        lightness: ColorComponent<NumberOrPercentage>,
        chroma: ColorComponent<NumberOrPercentage>,
        hue: ColorComponent<NumberOrAngle>,
        alpha: ColorComponent<NumberOrPercentage>,
    ) -> Self;

    /// Construct a new color with a predefined color space.
    fn from_color_function(
        color_space: PredefinedColorSpace,
        c1: ColorComponent<NumberOrPercentage>,
        c2: ColorComponent<NumberOrPercentage>,
        c3: ColorComponent<NumberOrPercentage>,
        alpha: ColorComponent<NumberOrPercentage>,
    ) -> Self;
}
