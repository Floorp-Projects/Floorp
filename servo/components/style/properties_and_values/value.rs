/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Parsing for registered custom properties.

use std::fmt::{self, Write};

use super::{
    registry::PropertyRegistration,
    syntax::{
        data_type::DataType, Component as SyntaxComponent, ComponentName, Descriptor, Multiplier,
    },
};
use crate::custom_properties::ComputedValue as ComputedPropertyValue;
use crate::parser::{Parse, ParserContext};
use crate::properties::StyleBuilder;
use crate::rule_cache::RuleCacheConditions;
use crate::stylesheets::{container_rule::ContainerSizeQuery, CssRuleType, Origin, UrlExtraData};
use crate::stylist::Stylist;
use crate::values::{
    computed::{self, ToComputedValue},
    specified, CustomIdent,
};
use cssparser::{BasicParseErrorKind, ParseErrorKind, Parser as CSSParser, ParserInput};
use selectors::matching::QuirksMode;
use servo_arc::{Arc, ThinArc};
use smallvec::SmallVec;
use style_traits::{
    owned_str::OwnedStr, CssWriter, ParseError as StyleParseError, ParsingMode,
    PropertySyntaxParseError, StyleParseErrorKind, ToCss,
};

/// A single component of the computed value.
#[derive(Clone, ToCss)]
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
    /// A <transform-list> value, equivalent to <transform-function>+
    TransformList(ValueComponentList),
    /// A <string> value
    String(OwnedStr),
}

impl ToComputedValue for ValueComponent {
    // TODO(zrhoffman, bug 1857716): Use separate type for computed value
    type ComputedValue = Self;

    fn to_computed_value(&self, context: &computed::Context) -> Self::ComputedValue {
        match self {
            ValueComponent::Length(length) => ValueComponent::Length(
                // TODO(zrhoffman, bug 1856524): Compute <length>, which may contain font-relative
                // units
                length.clone(),
            ),
            ValueComponent::Number(number) => ValueComponent::Number(
                ToComputedValue::from_computed_value(&number.to_computed_value(context)),
            ),
            ValueComponent::Percentage(percentage) => ValueComponent::Percentage(
                ToComputedValue::from_computed_value(&percentage.to_computed_value(context)),
            ),
            ValueComponent::LengthPercentage(length_percentage) => {
                // TODO(zrhoffman, bug 1856524): Compute <length-percentage>, which may contain
                // font-relative units
                ValueComponent::LengthPercentage(length_percentage.clone())
            },
            ValueComponent::Color(color) => ValueComponent::Color(
                ToComputedValue::from_computed_value(&color.to_computed_value(context)),
            ),
            ValueComponent::Image(image) => ValueComponent::Image(
                ToComputedValue::from_computed_value(&image.to_computed_value(context)),
            ),
            ValueComponent::Url(url) => ValueComponent::Url(ToComputedValue::from_computed_value(
                // TODO(zrhoffman, bug 1846625): Compute <url>
                &url.to_computed_value(context),
            )),
            ValueComponent::Integer(integer) => ValueComponent::Integer(
                ToComputedValue::from_computed_value(&integer.to_computed_value(context)),
            ),
            ValueComponent::Angle(angle) => ValueComponent::Angle(
                ToComputedValue::from_computed_value(&angle.to_computed_value(context)),
            ),
            ValueComponent::Time(time) => ValueComponent::Time(
                ToComputedValue::from_computed_value(&time.to_computed_value(context)),
            ),
            ValueComponent::Resolution(resolution) => ValueComponent::Resolution(
                ToComputedValue::from_computed_value(&resolution.to_computed_value(context)),
            ),
            ValueComponent::TransformFunction(transform_function) => {
                ValueComponent::TransformFunction(ToComputedValue::from_computed_value(
                    &transform_function.to_computed_value(context),
                ))
            },
            ValueComponent::CustomIdent(custom_ident) => ValueComponent::CustomIdent(
                ToComputedValue::from_computed_value(&custom_ident.to_computed_value(context)),
            ),
            ValueComponent::TransformList(transform_list) => ValueComponent::TransformList(
                ToComputedValue::from_computed_value(&transform_list.to_computed_value(context)),
            ),
            ValueComponent::String(string) => ValueComponent::String(
                ToComputedValue::from_computed_value(&string.to_computed_value(context)),
            ),
        }
    }

    fn from_computed_value(computed: &Self::ComputedValue) -> Self {
        computed.clone()
    }
}

/// A list of component values, including the list's multiplier.
#[derive(Clone)]
pub struct ValueComponentList(ThinArc<Multiplier, ValueComponent>);

impl ToComputedValue for ValueComponentList {
    type ComputedValue = Self;

    fn to_computed_value(&self, context: &computed::Context) -> Self::ComputedValue {
        let iter = self
            .0
            .slice()
            .iter()
            .map(|item| item.to_computed_value(context));
        Self::new(self.0.header, iter)
    }

    fn from_computed_value(computed: &Self::ComputedValue) -> Self {
        computed.clone()
    }
}

impl ToCss for ValueComponentList {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        let mut iter = self.0.slice().iter();
        let Some(first) = iter.next() else {
            return Ok(());
        };
        first.to_css(dest)?;

        // The separator implied by the multiplier for this list.
        let separator = match self.0.header {
            // <https://drafts.csswg.org/cssom-1/#serialize-a-whitespace-separated-list>
            Multiplier::Space => " ",
            // <https://drafts.csswg.org/cssom-1/#serialize-a-comma-separated-list>
            Multiplier::Comma => ", ",
        };
        for component in iter {
            dest.write_str(separator)?;
            component.to_css(dest)?;
        }
        Ok(())
    }
}

impl ValueComponentList {
    fn new<I>(multiplier: Multiplier, values: I) -> Self
    where
        I: Iterator<Item = ValueComponent> + ExactSizeIterator,
    {
        Self(ThinArc::from_header_and_iter(multiplier, values))
    }
}

/// A parsed property value.
#[derive(Clone, ToCss)]
pub enum ComputedValue {
    /// A single parsed component value whose matched syntax descriptor component did not have a
    /// multiplier.
    Component(ValueComponent),
    /// A parsed value whose syntax descriptor was the universal syntax definition.
    Universal(Arc<ComputedPropertyValue>),
    /// A list of parsed component values whose matched syntax descriptor component had a
    /// multiplier.
    List(ValueComponentList),
}

impl ToComputedValue for ComputedValue {
    type ComputedValue = Self;

    fn to_computed_value(&self, context: &computed::Context) -> Self::ComputedValue {
        match self {
            ComputedValue::Component(component) => {
                ComputedValue::Component(component.to_computed_value(context))
            },
            ComputedValue::Universal(value) => ComputedValue::Universal(value.clone()),
            ComputedValue::List(list) => ComputedValue::List(list.to_computed_value(context)),
        }
    }

    fn from_computed_value(computed: &Self::ComputedValue) -> Self {
        computed.clone()
    }
}

impl ComputedValue {
    /// Convert a registered custom property to a VariableValue, given input and a property
    /// registration.
    pub fn compute<'i, 't>(
        input: &mut CSSParser<'i, 't>,
        registration: &PropertyRegistration,
        stylist: &Stylist,
    ) -> Result<Arc<ComputedPropertyValue>, ()> {
        let Ok(value) = Self::parse(
            input,
            &registration.syntax,
            &registration.url_data,
            AllowComputationallyDependent::Yes,
        ) else {
            return Err(());
        };

        let mut rule_cache_conditions = RuleCacheConditions::default();
        let context = computed::Context::new(
            // TODO(zrhoffman, bug 1857674): Pass the existing StyleBuilder from the computed
            // context.
            StyleBuilder::new(stylist.device(), Some(stylist), None, None, None, false),
            stylist.quirks_mode(),
            &mut rule_cache_conditions,
            ContainerSizeQuery::none(),
        );
        let value = value.to_computed_value(&context).to_css_string();

        let result = {
            let mut input = ParserInput::new(&value);
            let mut input = CSSParser::new(&mut input);
            // TODO(zrhoffman, bug 1858305): Get the variable without parsing
            ComputedPropertyValue::parse(&mut input)
        };
        if let Ok(value) = result {
            Ok(value)
        } else {
            Err(())
        }
    }

    /// Parse and validate a registered custom property value according to its syntax descriptor,
    /// and check for computational independence.
    pub fn parse<'i, 't>(
        mut input: &mut CSSParser<'i, 't>,
        syntax: &Descriptor,
        url_data: &UrlExtraData,
        allow_computationally_dependent: AllowComputationallyDependent,
    ) -> Result<Self, StyleParseError<'i>> {
        if syntax.is_universal() {
            return Ok(Self::Universal(ComputedPropertyValue::parse(&mut input)?));
        }

        let mut values = SmallComponentVec::new();
        let mut multiplier = None;
        {
            let mut parser = Parser::new(syntax, &mut values, &mut multiplier);
            parser.parse(&mut input, url_data, allow_computationally_dependent)?;
        }
        let computed_value = if let Some(ref multiplier) = multiplier {
            Self::List(ValueComponentList::new(*multiplier, values.into_iter()))
        } else {
            Self::Component(values[0].clone())
        };
        Ok(computed_value)
    }
}

/// Whether the computed value parsing should allow computationaly dependent values like 3em or
/// var(-foo).
///
/// https://drafts.css-houdini.org/css-properties-values-api-1/#computationally-independent
pub enum AllowComputationallyDependent {
    /// Only computationally independent values are allowed.
    No,
    /// Computationally independent and dependent values are allowed.
    Yes,
}

type SmallComponentVec = SmallVec<[ValueComponent; 1]>;

struct Parser<'a> {
    syntax: &'a Descriptor,
    output: &'a mut SmallComponentVec,
    output_multiplier: &'a mut Option<Multiplier>,
}

impl<'a> Parser<'a> {
    fn new(
        syntax: &'a Descriptor,
        output: &'a mut SmallComponentVec,
        output_multiplier: &'a mut Option<Multiplier>,
    ) -> Self {
        Self {
            syntax,
            output,
            output_multiplier,
        }
    }

    fn parse<'i, 't>(
        &mut self,
        input: &mut CSSParser<'i, 't>,
        url_data: &UrlExtraData,
        allow_computationally_dependent: AllowComputationallyDependent,
    ) -> Result<(), StyleParseError<'i>> {
        use self::AllowComputationallyDependent::*;
        let parsing_mode = match allow_computationally_dependent {
            No => ParsingMode::DISALLOW_FONT_RELATIVE,
            Yes => ParsingMode::DEFAULT,
        };
        let ref context = ParserContext::new(
            Origin::Author,
            url_data,
            Some(CssRuleType::Style),
            parsing_mode,
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
            *self.output_multiplier = component.multiplier();
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
                    values.push(ValueComponent::TransformFunction(
                        specified::Transform::parse(context, input)?,
                    ));
                    let result = Self::expect_multiplier(input, &multiplier);
                    if Self::expect_multiplier_yielded_eof_error(&result) {
                        break;
                    }
                    result?;
                }
                let list = ValueComponentList::new(multiplier, values.into_iter());
                ValueComponent::TransformList(list)
            },
            DataType::String => {
                let string = input.expect_string()?;
                ValueComponent::String(string.as_ref().to_owned().into())
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
