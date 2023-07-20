/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Parsing for registered custom properties.

use super::syntax::{
    data_type::DataType, Component as SyntaxComponent, ComponentName, Descriptor, Multiplier,
};
use crate::custom_properties::ComputedValue as ComputedPropertyValue;
use crate::parser::{Parse, ParserContext};
use crate::stylesheets::{CssRuleType, Origin, UrlExtraData};
use crate::values::{specified, CustomIdent};
use cssparser::{BasicParseErrorKind, ParseErrorKind, Parser as CSSParser};
use selectors::matching::QuirksMode;
use servo_arc::Arc;
use smallvec::SmallVec;
use style_traits::{
    arc_slice::ArcSlice, ParseError as StyleParseError, ParsingMode, PropertySyntaxParseError,
    StyleParseErrorKind,
};

#[derive(Clone)]
/// A single component of the computed value.
pub enum ValueComponent {
    /// A <length> value
    Length(specified::Length),
    /// A <number> value
    Number(specified::Number),
    /// A <percentage> value
    Percentage(specified::Percentage),
    /// A <length-percentage> value
    LengthPercentage(specified::LengthPercentage),
    /// A <color> value
    Color(specified::Color),
    /// An <image> value
    Image(specified::Image),
    /// A <url> value
    Url(specified::url::SpecifiedUrl),
    /// An <integer> value
    Integer(specified::Integer),
    /// An <angle> value
    Angle(specified::Angle),
    /// A <time> value
    Time(specified::Time),
    /// A <resolution> value
    Resolution(specified::Resolution),
    /// A <transform-function> value
    TransformFunction(specified::Transform),
    /// A <custom-ident> value
    CustomIdent(CustomIdent),
    /// A <transform-list> value
    TransformList(ArcSlice<specified::Transform>),
}

/// A parsed property value.
pub enum ComputedValue {
    /// A single parsed component value whose matched syntax descriptor component did not have a
    /// multiplier.
    Component(ValueComponent),
    /// A parsed value whose syntax descriptor was the universal syntax definition.
    Universal(Arc<ComputedPropertyValue>),
    /// A list of parsed component values, whose matched syntax descriptor component had a
    /// multiplier.
    List(ArcSlice<ValueComponent>),
}

impl ComputedValue {
    /// Parse and validate a registered custom property value according to its syntax descriptor,
    /// and check for computational independence.
    pub fn parse<'i, 't>(
        mut input: &mut CSSParser<'i, 't>,
        syntax: &Descriptor,
        url_data: &UrlExtraData,
    ) -> Result<Self, StyleParseError<'i>> {
        if syntax.is_universal() {
            return Ok(Self::Universal(ComputedPropertyValue::parse(&mut input)?));
        }

        let mut values = SmallComponentVec::new();
        let mut has_multiplier = false;
        {
            let mut parser = Parser::new(syntax, &mut values, &mut has_multiplier);
            parser.parse(&mut input, url_data)?;
        }
        Ok(if has_multiplier {
            Self::List(ArcSlice::from_iter(values.into_iter()))
        } else {
            Self::Component(values[0].clone())
        })
    }
}

type SmallComponentVec = SmallVec<[ValueComponent; 1]>;

struct Parser<'a> {
    syntax: &'a Descriptor,
    output: &'a mut SmallComponentVec,
    output_has_multiplier: &'a mut bool,
}

impl<'a> Parser<'a> {
    fn new(
        syntax: &'a Descriptor,
        output: &'a mut SmallComponentVec,
        output_has_multiplier: &'a mut bool,
    ) -> Self {
        Self {
            syntax,
            output,
            output_has_multiplier,
        }
    }

    fn parse<'i, 't>(
        &mut self,
        input: &mut CSSParser<'i, 't>,
        url_data: &UrlExtraData,
    ) -> Result<(), StyleParseError<'i>> {
        let ref context = ParserContext::new(
            Origin::Author,
            url_data,
            Some(CssRuleType::Style),
            ParsingMode::DISALLOW_FONT_RELATIVE,
            QuirksMode::NoQuirks,
            /* namespaces = */ Default::default(),
            None,
            None,
        );
        for component in self.syntax.0.iter() {
            let result = input.try_parse(|input| {
                input.parse_entirely(|input| {
                    Self::parse_value(context, input, &component.unpremultiplied())
                })
            });
            let Ok(values) = result else { continue };
            self.output.extend(values);
            *self.output_has_multiplier = component.multiplier().is_some();
            break;
        }
        if self.output.is_empty() {
            return Err(input.new_error(BasicParseErrorKind::EndOfInput));
        }
        Ok(())
    }

    fn parse_value<'i, 't>(
        context: &ParserContext,
        input: &mut CSSParser<'i, 't>,
        component: &SyntaxComponent,
    ) -> Result<SmallComponentVec, StyleParseError<'i>> {
        let mut values = SmallComponentVec::new();
        values.push(Self::parse_component_without_multiplier(
            context, input, component,
        )?);

        if let Some(multiplier) = component.multiplier() {
            loop {
                let result = Self::expect_multiplier(input, &multiplier);
                if Self::expect_multiplier_yielded_eof_error(&result) {
                    break;
                }
                result?;
                values.push(Self::parse_component_without_multiplier(
                    context, input, component,
                )?);
            }
        }
        Ok(values)
    }

    fn parse_component_without_multiplier<'i, 't>(
        context: &ParserContext,
        input: &mut CSSParser<'i, 't>,
        component: &SyntaxComponent,
    ) -> Result<ValueComponent, StyleParseError<'i>> {
        let data_type = match component.name() {
            ComponentName::DataType(ty) => ty,
            ComponentName::Ident(ref name) => {
                let ident = CustomIdent::parse(input, &[])?;
                if ident != *name {
                    return Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError));
                }
                return Ok(ValueComponent::CustomIdent(ident));
            },
        };

        let value = match data_type {
            DataType::Length => ValueComponent::Length(specified::Length::parse(context, input)?),
            DataType::Number => ValueComponent::Number(specified::Number::parse(context, input)?),
            DataType::Percentage => {
                ValueComponent::Percentage(specified::Percentage::parse(context, input)?)
            },
            DataType::LengthPercentage => ValueComponent::LengthPercentage(
                specified::LengthPercentage::parse(context, input)?,
            ),
            DataType::Color => ValueComponent::Color(specified::Color::parse(context, input)?),
            DataType::Image => ValueComponent::Image(specified::Image::parse(context, input)?),
            DataType::Url => {
                ValueComponent::Url(specified::url::SpecifiedUrl::parse(context, input)?)
            },
            DataType::Integer => {
                ValueComponent::Integer(specified::Integer::parse(context, input)?)
            },
            DataType::Angle => ValueComponent::Angle(specified::Angle::parse(context, input)?),
            DataType::Time => ValueComponent::Time(specified::Time::parse(context, input)?),
            DataType::Resolution => {
                ValueComponent::Resolution(specified::Resolution::parse(context, input)?)
            },
            DataType::TransformFunction => {
                ValueComponent::TransformFunction(specified::Transform::parse(context, input)?)
            },
            DataType::CustomIdent => {
                let name = CustomIdent::parse(input, &[])?;
                ValueComponent::CustomIdent(name)
            },
            DataType::TransformList => {
                let mut values = vec![];
                let Some(multiplier) = component.unpremultiplied().multiplier() else {
                    debug_assert!(false, "Unpremultiplied <transform-list> had no multiplier?");
                    return Err(
                        input.new_custom_error(StyleParseErrorKind::PropertySyntaxField(
                            PropertySyntaxParseError::UnexpectedEOF,
                        )),
                    );
                };
                debug_assert_matches!(multiplier, Multiplier::Space);
                loop {
                    values.push(specified::Transform::parse(context, input)?);
                    let result = Self::expect_multiplier(input, &multiplier);
                    if Self::expect_multiplier_yielded_eof_error(&result) {
                        break;
                    }
                    result?;
                }
                ValueComponent::TransformList(ArcSlice::from_iter(values.into_iter()))
            },
        };
        Ok(value)
    }

    fn expect_multiplier_yielded_eof_error<'i>(result: &Result<(), StyleParseError<'i>>) -> bool {
        matches!(
            result,
            Err(StyleParseError {
                kind: ParseErrorKind::Basic(BasicParseErrorKind::EndOfInput),
                ..
            })
        )
    }

    fn expect_multiplier<'i, 't>(
        input: &mut CSSParser<'i, 't>,
        multiplier: &Multiplier,
    ) -> Result<(), StyleParseError<'i>> {
        match multiplier {
            Multiplier::Space => {
                input.expect_whitespace()?;
                if input.is_exhausted() {
                    // If there was trailing whitespace, do not interpret it as a multiplier
                    return Err(input.new_error(BasicParseErrorKind::EndOfInput));
                }
                Ok(())
            },
            Multiplier::Comma => Ok(input.expect_comma()?),
        }
    }
}
