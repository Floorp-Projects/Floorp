/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![deny(missing_docs)]

//! Fairly complete css-color implementation.
//! Relative colors, color-mix, system colors, and other such things require better calc() support
//! and integration.

use super::{
    color_function::ColorFunction,
    component::{ColorComponent, ColorComponentType},
    AbsoluteColor, ColorSpace,
};
use crate::{
    parser::ParserContext,
    values::{
        generics::calc::CalcUnits,
        specified::{
            angle::Angle as SpecifiedAngle, calc::Leaf as SpecifiedLeaf,
            color::Color as SpecifiedColor,
        },
    },
};
use cssparser::{
    color::{clamp_floor_256_f32, clamp_unit_f32, parse_hash_color, PredefinedColorSpace, OPAQUE},
    match_ignore_ascii_case, CowRcStr, Parser, Token,
};
use std::str::FromStr;
use style_traits::{ParseError, StyleParseErrorKind};

/// Returns true if the relative color syntax pref is enabled.
#[inline]
pub fn rcs_enabled() -> bool {
    static_prefs::pref!("layout.css.relative-color-syntax.enabled")
}

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
pub fn parse_color_keyword(ident: &str) -> Result<SpecifiedColor, ()> {
    Ok(match_ignore_ascii_case! { ident,
        "transparent" => {
            SpecifiedColor::from_absolute_color(AbsoluteColor::srgb_legacy(0u8, 0u8, 0u8, 0.0))
        },
        "currentcolor" => SpecifiedColor::CurrentColor,
        _ => {
            let (r, g, b) = cssparser::color::parse_named_color(ident)?;
            SpecifiedColor::from_absolute_color(AbsoluteColor::srgb_legacy(r, g, b, OPAQUE))
        },
    })
}

/// Parse a CSS color using the specified [`ColorParser`] and return a new color
/// value on success.
pub fn parse_color_with<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    input: &mut Parser<'i, 't>,
) -> Result<SpecifiedColor, ParseError<'i>> {
    let location = input.current_source_location();
    let token = input.next()?;
    match *token {
        Token::Hash(ref value) | Token::IDHash(ref value) => parse_hash_color(value.as_bytes())
            .map(|(r, g, b, a)| {
                SpecifiedColor::from_absolute_color(AbsoluteColor::srgb_legacy(r, g, b, a))
            }),
        Token::Ident(ref value) => parse_color_keyword(value),
        Token::Function(ref name) => {
            let name = name.clone();
            return input.parse_nested_block(|arguments| {
                Ok(SpecifiedColor::from_absolute_color(
                    parse_color_function(color_parser, name, arguments)?.resolve_to_absolute(),
                ))
            });
        },
        _ => Err(()),
    }
    .map_err(|()| location.new_unexpected_token_error(token.clone()))
}

/// Parse one of the color functions: rgba(), lab(), color(), etc.
#[inline]
fn parse_color_function<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    name: CowRcStr<'i>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let origin_color = parse_origin_color(color_parser, arguments)?;
    let color_parser = ColorParser {
        context: color_parser.context,
        origin_color: origin_color.as_ref(),
    };

    let color = match_ignore_ascii_case! { &name,
        "rgb" | "rgba" => parse_rgb(&color_parser, arguments),
        "hsl" | "hsla" => parse_hsl(&color_parser, arguments),
        "hwb" => parse_hwb(&color_parser, arguments),
        "lab" => parse_lab_like(&color_parser, arguments, ColorSpace::Lab, ColorFunction::Lab),
        "lch" => parse_lch_like(&color_parser, arguments, ColorSpace::Lch, ColorFunction::Lch),
        "oklab" => parse_lab_like(&color_parser, arguments, ColorSpace::Oklab, ColorFunction::Oklab),
        "oklch" => parse_lch_like(&color_parser, arguments, ColorSpace::Oklch, ColorFunction::Oklch),
        "color" => parse_color_with_color_space(&color_parser, arguments),
        _ => return Err(arguments.new_unexpected_token_error(Token::Ident(name))),
    }?;

    arguments.expect_exhausted()?;

    Ok(color)
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

/// Parse the relative color syntax "from" syntax `from <color>`.
fn parse_origin_color<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<Option<AbsoluteColor>, ParseError<'i>> {
    if !rcs_enabled() {
        return Ok(None);
    }

    // Not finding the from keyword is not an error, it just means we don't
    // have an origin color.
    if arguments
        .try_parse(|p| p.expect_ident_matching("from"))
        .is_err()
    {
        return Ok(None);
    }

    let location = arguments.current_source_location();

    // We still fail if we can't parse the origin color.
    let origin_color = parse_color_with(color_parser, arguments)?;

    // Right now we only handle absolute colors.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1890972
    let SpecifiedColor::Absolute(absolute) = origin_color else {
        return Err(location.new_custom_error(StyleParseErrorKind::UnspecifiedError));
    };

    Ok(Some(absolute.color))
}

#[inline]
fn parse_rgb<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let origin_color = color_parser
        .origin_color
        .map(|c| c.to_color_space(ColorSpace::Srgb));
    let color_parser = ColorParser {
        context: color_parser.context,
        origin_color: origin_color.as_ref(),
    };

    let location = arguments.current_source_location();

    let maybe_red = color_parser.parse_number_or_percentage(arguments, true)?;

    // If the first component is not "none" and is followed by a comma, then we
    // are parsing the legacy syntax.  Legacy syntax also doesn't support an
    // origin color.
    let is_legacy_syntax = color_parser.origin_color.is_none() &&
        !maybe_red.is_none() &&
        arguments.try_parse(|p| p.expect_comma()).is_ok();

    Ok(if is_legacy_syntax {
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

        let alpha = color_parser.parse_legacy_alpha(arguments)?;

        ColorFunction::Rgb(red, green, blue, alpha)
    } else {
        let green = color_parser.parse_number_or_percentage(arguments, true)?;
        let blue = color_parser.parse_number_or_percentage(arguments, true)?;
        let alpha = color_parser.parse_modern_alpha(arguments)?;

        // When using the relative color syntax (having an origin color), the
        // resulting color is always in the modern syntax.
        if color_parser.origin_color.is_some() {
            ColorFunction::Color(PredefinedColorSpace::Srgb, maybe_red, green, blue, alpha)
        } else {
            fn clamp(v: NumberOrPercentage) -> u8 {
                clamp_floor_256_f32(v.to_number(255.0))
            }

            let red = maybe_red.map_value(clamp);
            let green = green.map_value(clamp);
            let blue = blue.map_value(clamp);

            ColorFunction::Rgb(red, green, blue, alpha)
        }
    })
}

/// Parses hsl syntax.
///
/// <https://drafts.csswg.org/css-color/#the-hsl-notation>
#[inline]
fn parse_hsl<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let origin_color = color_parser
        .origin_color
        .map(|c| c.to_color_space(ColorSpace::Hsl));
    let color_parser = ColorParser {
        context: color_parser.context,
        origin_color: origin_color.as_ref(),
    };

    let hue = color_parser.parse_number_or_angle(arguments, true)?;

    // If the hue is not "none" and is followed by a comma, then we are parsing
    // the legacy syntax. Legacy syntax also doesn't support an origin color.
    let is_legacy_syntax = color_parser.origin_color.is_none() &&
        !hue.is_none() &&
        arguments.try_parse(|p| p.expect_comma()).is_ok();

    let (saturation, lightness, alpha) = if is_legacy_syntax {
        let saturation = color_parser
            .parse_percentage(arguments, false)?
            .map_value(|unit_value| NumberOrPercentage::Percentage { unit_value });
        arguments.expect_comma()?;
        let lightness = color_parser
            .parse_percentage(arguments, false)?
            .map_value(|unit_value| NumberOrPercentage::Percentage { unit_value });
        let alpha = color_parser.parse_legacy_alpha(arguments)?;
        (saturation, lightness, alpha)
    } else {
        let saturation = color_parser.parse_number_or_percentage(arguments, true)?;
        let lightness = color_parser.parse_number_or_percentage(arguments, true)?;
        let alpha = color_parser.parse_modern_alpha(arguments)?;
        (saturation, lightness, alpha)
    };

    Ok(ColorFunction::Hsl(hue, saturation, lightness, alpha))
}

/// Parses hwb syntax.
///
/// <https://drafts.csswg.org/css-color/#the-hbw-notation>
#[inline]
fn parse_hwb<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let origin_color = color_parser
        .origin_color
        .map(|c| c.to_color_space(ColorSpace::Hwb));
    let color_parser = ColorParser {
        context: color_parser.context,
        origin_color: origin_color.as_ref(),
    };

    let (hue, whiteness, blackness, alpha) = parse_components(
        &color_parser,
        arguments,
        ColorParser::parse_number_or_angle,
        ColorParser::parse_number_or_percentage,
        ColorParser::parse_number_or_percentage,
    )?;

    Ok(ColorFunction::Hwb(hue, whiteness, blackness, alpha))
}

type IntoLabFn<Output> = fn(
    l: ColorComponent<NumberOrPercentage>,
    a: ColorComponent<NumberOrPercentage>,
    b: ColorComponent<NumberOrPercentage>,
    alpha: ColorComponent<NumberOrPercentage>,
) -> Output;

#[inline]
fn parse_lab_like<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
    color_space: ColorSpace,
    into_color: IntoLabFn<ColorFunction>,
) -> Result<ColorFunction, ParseError<'i>> {
    let origin_color = color_parser
        .origin_color
        .map(|c| c.to_color_space(color_space));
    let color_parser = ColorParser {
        context: color_parser.context,
        origin_color: origin_color.as_ref(),
    };

    let (lightness, a, b, alpha) = parse_components(
        &color_parser,
        arguments,
        ColorParser::parse_number_or_percentage,
        ColorParser::parse_number_or_percentage,
        ColorParser::parse_number_or_percentage,
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
fn parse_lch_like<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
    color_space: ColorSpace,
    into_color: IntoLchFn<ColorFunction>,
) -> Result<ColorFunction, ParseError<'i>> {
    let origin_color = color_parser
        .origin_color
        .map(|c| c.to_color_space(color_space));
    let color_parser = ColorParser {
        context: color_parser.context,
        origin_color: origin_color.as_ref(),
    };

    let (lightness, chroma, hue, alpha) = parse_components(
        &color_parser,
        arguments,
        ColorParser::parse_number_or_percentage,
        ColorParser::parse_number_or_percentage,
        ColorParser::parse_number_or_angle,
    )?;

    Ok(into_color(lightness, chroma, hue, alpha))
}

/// Parse the color() function.
#[inline]
fn parse_color_with_color_space<'i, 't>(
    color_parser: &ColorParser<'_, '_>,
    arguments: &mut Parser<'i, 't>,
) -> Result<ColorFunction, ParseError<'i>> {
    let color_space = {
        let location = arguments.current_source_location();

        let ident = arguments.expect_ident()?;
        PredefinedColorSpace::from_str(ident)
            .map_err(|_| location.new_unexpected_token_error(Token::Ident(ident.clone())))?
    };

    let origin_color = color_parser
        .origin_color
        .map(|c| c.to_color_space(color_space.into()));
    let color_parser = ColorParser {
        context: color_parser.context,
        origin_color: origin_color.as_ref(),
    };

    let (c1, c2, c3, alpha) = parse_components(
        &color_parser,
        arguments,
        ColorParser::parse_number_or_percentage,
        ColorParser::parse_number_or_percentage,
        ColorParser::parse_number_or_percentage,
    )?;

    Ok(ColorFunction::Color(color_space, c1, c2, c3, alpha))
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
pub fn parse_components<'a, 'b: 'a, 'i, 't, F1, F2, F3, R1, R2, R3>(
    color_parser: &ColorParser<'a, 'b>,
    input: &mut Parser<'i, 't>,
    f1: F1,
    f2: F2,
    f3: F3,
) -> ComponentParseResult<'i, R1, R2, R3>
where
    F1: FnOnce(
        &ColorParser<'a, 'b>,
        &mut Parser<'i, 't>,
        bool,
    ) -> Result<ColorComponent<R1>, ParseError<'i>>,
    F2: FnOnce(
        &ColorParser<'a, 'b>,
        &mut Parser<'i, 't>,
        bool,
    ) -> Result<ColorComponent<R2>, ParseError<'i>>,
    F3: FnOnce(
        &ColorParser<'a, 'b>,
        &mut Parser<'i, 't>,
        bool,
    ) -> Result<ColorComponent<R3>, ParseError<'i>>,
{
    let r1 = f1(color_parser, input, true)?;
    let r2 = f2(color_parser, input, true)?;
    let r3 = f3(color_parser, input, true)?;

    let alpha = color_parser.parse_modern_alpha(input)?;

    Ok((r1, r2, r3, alpha))
}

/// Either a number or a percentage.
#[derive(Clone, Copy, Debug)]
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

impl ColorComponentType for NumberOrPercentage {
    fn from_value(value: f32) -> Self {
        Self::Number { value }
    }

    fn units() -> CalcUnits {
        CalcUnits::PERCENTAGE
    }

    fn try_from_token(token: &Token) -> Result<Self, ()> {
        Ok(match *token {
            Token::Number { value, .. } => Self::Number { value },
            Token::Percentage { unit_value, .. } => Self::Percentage { unit_value },
            _ => {
                return Err(());
            },
        })
    }

    fn try_from_leaf(leaf: &SpecifiedLeaf) -> Result<Self, ()> {
        Ok(match *leaf {
            SpecifiedLeaf::Percentage(unit_value) => Self::Percentage { unit_value },
            SpecifiedLeaf::Number(value) => Self::Number { value },
            _ => return Err(()),
        })
    }
}

/// Either an angle or a number.
#[derive(Clone, Copy, Debug)]
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

impl ColorComponentType for NumberOrAngle {
    fn from_value(value: f32) -> Self {
        Self::Number { value }
    }

    fn units() -> CalcUnits {
        CalcUnits::ANGLE
    }

    fn try_from_token(token: &Token) -> Result<Self, ()> {
        Ok(match *token {
            Token::Number { value, .. } => Self::Number { value },
            Token::Dimension {
                value, ref unit, ..
            } => {
                let degrees =
                    SpecifiedAngle::parse_dimension(value, unit, /* from_calc = */ false)
                        .map(|angle| angle.degrees())?;

                NumberOrAngle::Angle { degrees }
            },
            _ => {
                return Err(());
            },
        })
    }

    fn try_from_leaf(leaf: &SpecifiedLeaf) -> Result<Self, ()> {
        Ok(match *leaf {
            SpecifiedLeaf::Angle(angle) => Self::Angle {
                degrees: angle.degrees(),
            },
            SpecifiedLeaf::Number(value) => Self::Number { value },
            _ => return Err(()),
        })
    }
}

/// The raw f32 here is for <number>.
impl ColorComponentType for f32 {
    fn from_value(value: f32) -> Self {
        value
    }

    fn units() -> CalcUnits {
        CalcUnits::empty()
    }

    fn try_from_token(token: &Token) -> Result<Self, ()> {
        if let Token::Number { value, .. } = *token {
            Ok(value)
        } else {
            Err(())
        }
    }

    fn try_from_leaf(leaf: &SpecifiedLeaf) -> Result<Self, ()> {
        if let SpecifiedLeaf::Number(value) = *leaf {
            Ok(value)
        } else {
            Err(())
        }
    }
}

/// Used to parse the components of a color.
#[derive(Clone)]
pub struct ColorParser<'a, 'b: 'a> {
    /// Parser context used for parsing the colors.
    pub context: &'a ParserContext<'b>,
    /// A parsed origin color if one is available.
    pub origin_color: Option<&'a AbsoluteColor>,
}

impl<'a, 'b: 'a> ColorParser<'a, 'b> {
    /// Create a new [ColorParser] with the given context.
    pub fn new(context: &'a ParserContext<'b>) -> Self {
        Self {
            context,
            origin_color: None,
        }
    }

    /// Parse an `<number>` or `<angle>` value.
    fn parse_number_or_angle<'i, 't>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<NumberOrAngle>, ParseError<'i>> {
        ColorComponent::parse(self.context, input, allow_none, self.origin_color)
    }

    /// Parse a `<percentage>` value.
    fn parse_percentage<'i, 't>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<f32>, ParseError<'i>> {
        let location = input.current_source_location();

        // We can use the [NumberOrPercentage] type here, because parsing it
        // doesn't have any more overhead than just parsing a percentage on its
        // own.
        Ok(
            match ColorComponent::<NumberOrPercentage>::parse(
                self.context,
                input,
                allow_none,
                self.origin_color,
            )? {
                ColorComponent::None => ColorComponent::None,
                ColorComponent::Value(NumberOrPercentage::Percentage { unit_value }) => {
                    ColorComponent::Value(unit_value)
                },
                _ => return Err(location.new_custom_error(StyleParseErrorKind::UnspecifiedError)),
            },
        )
    }

    /// Parse a `<number>` value.
    fn parse_number<'i, 't>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<f32>, ParseError<'i>> {
        ColorComponent::parse(self.context, input, allow_none, self.origin_color)
    }

    /// Parse a `<number>` or `<percentage>` value.
    fn parse_number_or_percentage<'i, 't>(
        &self,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>> {
        ColorComponent::parse(self.context, input, allow_none, self.origin_color)
    }

    fn parse_legacy_alpha<'i, 't>(
        &self,
        arguments: &mut Parser<'i, 't>,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>> {
        if !arguments.is_exhausted() {
            arguments.expect_comma()?;
            self.parse_number_or_percentage(arguments, false)
        } else {
            Ok(ColorComponent::Value(NumberOrPercentage::Number {
                value: OPAQUE,
            }))
        }
    }

    fn parse_modern_alpha<'i, 't>(
        &self,
        arguments: &mut Parser<'i, 't>,
    ) -> Result<ColorComponent<NumberOrPercentage>, ParseError<'i>> {
        if !arguments.is_exhausted() {
            arguments.expect_delim('/')?;
            self.parse_number_or_percentage(arguments, true)
        } else {
            Ok(ColorComponent::Value(NumberOrPercentage::Number {
                value: OPAQUE,
            }))
        }
    }
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
