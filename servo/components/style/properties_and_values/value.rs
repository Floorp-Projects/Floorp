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
use crate::stylesheets::{CssRuleType, Origin, UrlExtraData};
use crate::values::{
    computed::{self, ToComputedValue},
    specified, CustomIdent,
};
use cssparser::{BasicParseErrorKind, ParseErrorKind, Parser as CSSParser, TokenSerializationType};
use selectors::matching::QuirksMode;
use servo_arc::{Arc, ThinArc};
use smallvec::SmallVec;
use style_traits::{
    owned_str::OwnedStr, CssWriter, ParseError as StyleParseError, ParsingMode,
    PropertySyntaxParseError, StyleParseErrorKind, ToCss,
};

/// A single component of the computed value.
pub type ComputedValueComponent = GenericValueComponent<
    // TODO(zrhoffman, bug 1856524): Use computed::Length
    specified::Length,
    computed::Number,
    computed::Percentage,
    // TODO(zrhoffman, bug 1856524): Use computed::LengthPercentage
    specified::LengthPercentage,
    computed::Color,
    computed::Image,
    computed::url::ComputedUrl,
    computed::Integer,
    computed::Angle,
    computed::Time,
    computed::Resolution,
    computed::Transform,
    ComputedValueComponentList,
>;

/// A single component of the specified value.
pub type SpecifiedValueComponent = GenericValueComponent<
    specified::Length,
    specified::Number,
    specified::Percentage,
    specified::LengthPercentage,
    specified::Color,
    specified::Image,
    specified::url::SpecifiedUrl,
    specified::Integer,
    specified::Angle,
    specified::Time,
    specified::Resolution,
    specified::Transform,
    SpecifiedValueComponentList,
>;

impl SpecifiedValueComponent {
    fn serialization_types(&self) -> (TokenSerializationType, TokenSerializationType) {
        let first_token_type = match self {
            Self::Length(_) | Self::Angle(_) | Self::Time(_) | Self::Resolution(_) => {
                TokenSerializationType::Dimension
            },
            Self::Number(_) | Self::Integer(_) => TokenSerializationType::Number,
            Self::Percentage(_) | Self::LengthPercentage(_) => TokenSerializationType::Percentage,
            Self::Color(_) |
            Self::Image(_) |
            Self::Url(_) |
            Self::TransformFunction(_) |
            Self::TransformList(_) => TokenSerializationType::Function,
            Self::CustomIdent(_) => TokenSerializationType::Ident,
            Self::String(_) => TokenSerializationType::Other,
        };
        let last_token_type = if first_token_type == TokenSerializationType::Function {
            TokenSerializationType::Other
        } else {
            first_token_type
        };
        (first_token_type, last_token_type)
    }
}

/// A generic enum used for both specified value components and computed value components.
#[derive(Clone, ToCss)]
pub enum GenericValueComponent<
    Length,
    Number,
    Percentage,
    LengthPercentage,
    Color,
    Image,
    Url,
    Integer,
    Angle,
    Time,
    Resolution,
    TransformFunction,
    TransformList,
> {
    /// A <length> value
    Length(Length),
    /// A <number> value
    Number(Number),
    /// A <percentage> value
    Percentage(Percentage),
    /// A <length-percentage> value
    LengthPercentage(LengthPercentage),
    /// A <color> value
    Color(Color),
    /// An <image> value
    Image(Image),
    /// A <url> value
    Url(Url),
    /// An <integer> value
    Integer(Integer),
    /// An <angle> value
    Angle(Angle),
    /// A <time> value
    Time(Time),
    /// A <resolution> value
    Resolution(Resolution),
    /// A <transform-function> value
    TransformFunction(TransformFunction),
    /// A <custom-ident> value
    CustomIdent(CustomIdent),
    /// A <transform-list> value, equivalent to <transform-function>+
    TransformList(TransformList),
    /// A <string> value
    String(OwnedStr),
}

impl ToComputedValue for SpecifiedValueComponent {
    type ComputedValue = ComputedValueComponent;

    fn to_computed_value(&self, context: &computed::Context) -> Self::ComputedValue {
        match self {
            SpecifiedValueComponent::Length(length) => ComputedValueComponent::Length(
                // TODO(zrhoffman, bug 1856524): Compute <length>, which may contain font-relative
                // units
                length.clone(),
            ),
            SpecifiedValueComponent::Number(number) => {
                ComputedValueComponent::Number(number.to_computed_value(context))
            },
            SpecifiedValueComponent::Percentage(percentage) => {
                ComputedValueComponent::Percentage(percentage.to_computed_value(context))
            },
            SpecifiedValueComponent::LengthPercentage(length_percentage) => {
                // TODO(zrhoffman, bug 1856524): Compute <length-percentage>, which may contain
                // font-relative units
                ComputedValueComponent::LengthPercentage(length_percentage.clone())
            },
            SpecifiedValueComponent::Color(color) => {
                ComputedValueComponent::Color(color.to_computed_value(context))
            },
            SpecifiedValueComponent::Image(image) => {
                ComputedValueComponent::Image(image.to_computed_value(context))
            },
            SpecifiedValueComponent::Url(url) => ComputedValueComponent::Url(
                // TODO(zrhoffman, bug 1846625): Compute <url>
                url.to_computed_value(context),
            ),
            SpecifiedValueComponent::Integer(integer) => {
                ComputedValueComponent::Integer(integer.to_computed_value(context))
            },
            SpecifiedValueComponent::Angle(angle) => {
                ComputedValueComponent::Angle(angle.to_computed_value(context))
            },
            SpecifiedValueComponent::Time(time) => {
                ComputedValueComponent::Time(time.to_computed_value(context))
            },
            SpecifiedValueComponent::Resolution(resolution) => {
                ComputedValueComponent::Resolution(resolution.to_computed_value(context))
            },
            SpecifiedValueComponent::TransformFunction(transform_function) => {
                ComputedValueComponent::TransformFunction(
                    transform_function.to_computed_value(context),
                )
            },
            SpecifiedValueComponent::CustomIdent(custom_ident) => {
                ComputedValueComponent::CustomIdent(custom_ident.to_computed_value(context))
            },
            SpecifiedValueComponent::TransformList(transform_list) => {
                ComputedValueComponent::TransformList(transform_list.to_computed_value(context))
            },
            SpecifiedValueComponent::String(string) => {
                ComputedValueComponent::String(string.to_computed_value(context))
            },
        }
    }

    fn from_computed_value(computed: &Self::ComputedValue) -> Self {
        match computed {
            ComputedValueComponent::Length(length) => SpecifiedValueComponent::Length(
                // TODO(zrhoffman, bug 1856524): Use computed <length>
                length.clone(),
            ),
            ComputedValueComponent::Number(number) => {
                SpecifiedValueComponent::Number(ToComputedValue::from_computed_value(number))
            },
            ComputedValueComponent::Percentage(percentage) => SpecifiedValueComponent::Percentage(
                ToComputedValue::from_computed_value(percentage),
            ),
            ComputedValueComponent::LengthPercentage(length_percentage) => {
                // TODO(zrhoffman, bug 1856524): Use computed <length-percentage>
                SpecifiedValueComponent::LengthPercentage(length_percentage.clone())
            },
            ComputedValueComponent::Color(color) => {
                SpecifiedValueComponent::Color(ToComputedValue::from_computed_value(color))
            },
            ComputedValueComponent::Image(image) => {
                SpecifiedValueComponent::Image(ToComputedValue::from_computed_value(image))
            },
            ComputedValueComponent::Url(url) => SpecifiedValueComponent::Url(
                // TODO(zrhoffman, bug 1846625): Compute <url>
                ToComputedValue::from_computed_value(url),
            ),
            ComputedValueComponent::Integer(integer) => {
                SpecifiedValueComponent::Integer(ToComputedValue::from_computed_value(integer))
            },
            ComputedValueComponent::Angle(angle) => {
                SpecifiedValueComponent::Angle(ToComputedValue::from_computed_value(angle))
            },
            ComputedValueComponent::Time(time) => {
                SpecifiedValueComponent::Time(ToComputedValue::from_computed_value(time))
            },
            ComputedValueComponent::Resolution(resolution) => SpecifiedValueComponent::Resolution(
                ToComputedValue::from_computed_value(resolution),
            ),
            ComputedValueComponent::TransformFunction(transform_function) => {
                SpecifiedValueComponent::TransformFunction(ToComputedValue::from_computed_value(
                    transform_function,
                ))
            },
            ComputedValueComponent::CustomIdent(custom_ident) => {
                SpecifiedValueComponent::CustomIdent(ToComputedValue::from_computed_value(
                    custom_ident,
                ))
            },
            ComputedValueComponent::TransformList(transform_list) => {
                SpecifiedValueComponent::TransformList(ToComputedValue::from_computed_value(
                    transform_list,
                ))
            },
            ComputedValueComponent::String(string) => {
                SpecifiedValueComponent::String(ToComputedValue::from_computed_value(string))
            },
        }
    }
}

/// A list of component values, including the list's multiplier.
#[derive(Clone)]
pub struct SpecifiedValueComponentList(ThinArc<Multiplier, SpecifiedValueComponent>);

impl ToComputedValue for SpecifiedValueComponentList {
    type ComputedValue = ComputedValueComponentList;

    fn to_computed_value(&self, context: &computed::Context) -> Self::ComputedValue {
        let iter = self
            .0
            .slice()
            .iter()
            .map(|item| item.to_computed_value(context));
        ComputedValueComponentList::new(self.0.header, iter)
    }

    fn from_computed_value(computed: &Self::ComputedValue) -> Self {
        let iter = computed
            .0
            .slice()
            .iter()
            .map(SpecifiedValueComponent::from_computed_value);
        Self::new(computed.0.header, iter)
    }
}

impl ToCss for SpecifiedValueComponentList {
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

impl SpecifiedValueComponentList {
    fn new<I>(multiplier: Multiplier, values: I) -> Self
    where
        I: Iterator<Item = SpecifiedValueComponent> + ExactSizeIterator,
    {
        Self(ThinArc::from_header_and_iter(multiplier, values))
    }

    fn serialization_types(&self) -> (TokenSerializationType, TokenSerializationType) {
        if let Some(first) = self.0.slice().first() {
            // Each element has the same serialization types, so we can use only the first element.
            first.serialization_types()
        } else {
            Default::default()
        }
    }
}

/// A list of computed component values, including the list's unchanged multiplier.
#[derive(Clone)]
pub struct ComputedValueComponentList(ThinArc<Multiplier, ComputedValueComponent>);

impl ComputedValueComponentList {
    fn new<I>(multiplier: Multiplier, values: I) -> Self
    where
        I: Iterator<Item = ComputedValueComponent> + ExactSizeIterator,
    {
        Self(ThinArc::from_header_and_iter(multiplier, values))
    }
}

/// A specified registered custom property value.
#[derive(ToCss)]
pub enum SpecifiedValue {
    /// A single specified component value whose syntax descriptor component did not have a
    /// multiplier.
    Component(SpecifiedValueComponent),
    /// A specified value whose syntax descriptor was the universal syntax definition.
    Universal(Arc<ComputedPropertyValue>),
    /// A list of specified component values whose syntax descriptor component had a multiplier.
    List(SpecifiedValueComponentList),
}

impl ToComputedValue for SpecifiedValue {
    type ComputedValue = ComputedValue;

    fn to_computed_value(&self, context: &computed::Context) -> Self::ComputedValue {
        match self {
            SpecifiedValue::Component(component) => {
                ComputedValue::Component(component.to_computed_value(context))
            },
            SpecifiedValue::Universal(value) => ComputedValue::Universal(value.clone()),
            SpecifiedValue::List(list) => ComputedValue::List(list.to_computed_value(context)),
        }
    }

    fn from_computed_value(computed: &Self::ComputedValue) -> Self {
        match computed {
            ComputedValue::Component(component) => {
                SpecifiedValue::Component(SpecifiedValueComponent::from_computed_value(component))
            },
            ComputedValue::Universal(value) => SpecifiedValue::Universal(value.clone()),
            ComputedValue::List(list) => {
                SpecifiedValue::List(SpecifiedValueComponentList::from_computed_value(list))
            },
        }
    }
}

impl SpecifiedValue {
    /// Convert a registered custom property to a VariableValue, given input and a property
    /// registration.
    pub fn compute<'i, 't>(
        input: &mut CSSParser<'i, 't>,
        registration: &PropertyRegistration,
        context: &computed::Context,
        allow_computationally_dependent: AllowComputationallyDependent,
    ) -> Result<Arc<ComputedPropertyValue>, ()> {
        let Ok(value) = Self::parse(
            input,
            &registration.syntax,
            &registration.url_data,
            allow_computationally_dependent,
        ) else {
            return Err(());
        };

        // TODO(zrhoffman, bug 1856522): All font-* properties should already be applied before
        // computing the value of the registered custom property.
        let value = value.to_computed_value(context);
        let value = SpecifiedValue::from_computed_value(&value);
        Ok(value.to_var(&registration.url_data))
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
            return Ok(Self::Universal(ComputedPropertyValue::parse(&mut input, url_data)?));
        }

        let mut values = SmallComponentVec::new();
        let mut multiplier = None;
        {
            let mut parser = Parser::new(syntax, &mut values, &mut multiplier);
            parser.parse(&mut input, url_data, allow_computationally_dependent)?;
        }
        let computed_value = if let Some(ref multiplier) = multiplier {
            Self::List(SpecifiedValueComponentList::new(
                *multiplier,
                values.into_iter(),
            ))
        } else {
            Self::Component(values[0].clone())
        };
        Ok(computed_value)
    }

    fn serialization_types(&self) -> (TokenSerializationType, TokenSerializationType) {
        match self {
            Self::Component(component) => component.serialization_types(),
            Self::Universal(_) => unreachable!(),
            Self::List(list) => list.serialization_types(),
        }
    }

    fn to_var(&self, url_data: &UrlExtraData) -> Arc<ComputedPropertyValue> {
        if let Self::Universal(var) = self {
            return var.clone();
        }
        let serialization_types = self.serialization_types();
        Arc::new(ComputedPropertyValue::new(
            // TODO(zrhoffman, 1864736): Preserve the computed type instead of converting back to a
            // string.
            self.to_css_string(),
            url_data,
            serialization_types.0,
            serialization_types.1,
        ))
    }
}

/// A computed registered custom property value.
pub enum ComputedValue {
    /// A single computed component value whose syntax descriptor component did not have a
    /// multiplier.
    Component(ComputedValueComponent),
    /// A computed value whose syntax descriptor was the universal syntax definition.
    Universal(Arc<ComputedPropertyValue>),
    /// A list of computed component values whose syntax descriptor component had a multiplier.
    List(ComputedValueComponentList),
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

type SmallComponentVec = SmallVec<[SpecifiedValueComponent; 1]>;

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
        for component in self.syntax.components.iter() {
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
    ) -> Result<SpecifiedValueComponent, StyleParseError<'i>> {
        let data_type = match component.name() {
            ComponentName::DataType(ty) => ty,
            ComponentName::Ident(ref name) => {
                let ident = CustomIdent::parse(input, &[])?;
                if ident != *name {
                    return Err(input.new_custom_error(StyleParseErrorKind::UnspecifiedError));
                }
                return Ok(SpecifiedValueComponent::CustomIdent(ident));
            },
        };

        let value = match data_type {
            DataType::Length => {
                SpecifiedValueComponent::Length(specified::Length::parse(context, input)?)
            },
            DataType::Number => {
                SpecifiedValueComponent::Number(specified::Number::parse(context, input)?)
            },
            DataType::Percentage => {
                SpecifiedValueComponent::Percentage(specified::Percentage::parse(context, input)?)
            },
            DataType::LengthPercentage => SpecifiedValueComponent::LengthPercentage(
                specified::LengthPercentage::parse(context, input)?,
            ),
            DataType::Color => {
                SpecifiedValueComponent::Color(specified::Color::parse(context, input)?)
            },
            DataType::Image => {
                SpecifiedValueComponent::Image(specified::Image::parse(context, input)?)
            },
            DataType::Url => {
                SpecifiedValueComponent::Url(specified::url::SpecifiedUrl::parse(context, input)?)
            },
            DataType::Integer => {
                SpecifiedValueComponent::Integer(specified::Integer::parse(context, input)?)
            },
            DataType::Angle => {
                SpecifiedValueComponent::Angle(specified::Angle::parse(context, input)?)
            },
            DataType::Time => {
                SpecifiedValueComponent::Time(specified::Time::parse(context, input)?)
            },
            DataType::Resolution => {
                SpecifiedValueComponent::Resolution(specified::Resolution::parse(context, input)?)
            },
            DataType::TransformFunction => SpecifiedValueComponent::TransformFunction(
                specified::Transform::parse(context, input)?,
            ),
            DataType::CustomIdent => {
                let name = CustomIdent::parse(input, &[])?;
                SpecifiedValueComponent::CustomIdent(name)
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
                    values.push(SpecifiedValueComponent::TransformFunction(
                        specified::Transform::parse(context, input)?,
                    ));
                    let result = Self::expect_multiplier(input, &multiplier);
                    if Self::expect_multiplier_yielded_eof_error(&result) {
                        break;
                    }
                    result?;
                }
                let list = SpecifiedValueComponentList::new(multiplier, values.into_iter());
                SpecifiedValueComponent::TransformList(list)
            },
            DataType::String => {
                let string = input.expect_string()?;
                SpecifiedValueComponent::String(string.as_ref().to_owned().into())
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
