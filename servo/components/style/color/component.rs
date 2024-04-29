/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Parse/serialize and resolve a single color component.

use super::{
    parsing::{rcs_enabled, ChannelKeyword},
    AbsoluteColor,
};
use crate::{
    parser::ParserContext,
    values::{
        generics::calc::CalcUnits,
        specified::calc::{CalcNode as SpecifiedCalcNode, Leaf as SpecifiedLeaf},
    },
};
use cssparser::{Parser, Token};
use style_traits::{ParseError, StyleParseErrorKind};

/// A single color component.
#[derive(Clone, Debug, MallocSizeOf, PartialEq, ToShmem)]
pub enum ColorComponent<ValueType> {
    /// The "none" keyword.
    None,
    /// A absolute value.
    Value(ValueType),
}

impl<ValueType> ColorComponent<ValueType> {
    /// Return true if the component is "none".
    #[inline]
    pub fn is_none(&self) -> bool {
        matches!(self, Self::None)
    }

    /// If the component contains a value, map it to another value.
    pub fn map_value<OutType>(
        self,
        f: impl FnOnce(ValueType) -> OutType,
    ) -> ColorComponent<OutType> {
        match self {
            Self::None => ColorComponent::None,
            Self::Value(value) => ColorComponent::Value(f(value)),
        }
    }
    /// Return the component as its value.
    pub fn into_value(self) -> ValueType {
        match self {
            Self::None => panic!("value not available when component is None"),
            Self::Value(value) => value,
        }
    }

    /// Return the component as its value or a default value.
    pub fn into_value_or(self, default: ValueType) -> ValueType {
        match self {
            Self::None => default,
            Self::Value(value) => value,
        }
    }
}

/// An utility trait that allows the construction of [ColorComponent]
/// `ValueType`'s after parsing a color component.
pub trait ColorComponentType: Sized {
    // TODO(tlouw): This function should be named according to the rules in the spec
    //              stating that all the values coming from color components are
    //              numbers and that each has their own rules dependeing on types.
    /// Construct a new component from a single value.
    fn from_value(value: f32) -> Self;

    /// Return the [CalcUnits] flags that the impl can handle.
    fn units() -> CalcUnits;

    /// Try to create a new component from the given token.
    fn try_from_token(token: &Token) -> Result<Self, ()>;

    /// Try to create a new component from the given [CalcNodeLeaf] that was
    /// resolved from a [CalcNode].
    fn try_from_leaf(leaf: &SpecifiedLeaf) -> Result<Self, ()>;
}

impl<ValueType: ColorComponentType> ColorComponent<ValueType> {
    /// Parse a single [ColorComponent].
    pub fn parse<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
        allow_none: bool,
        origin_color: Option<&AbsoluteColor>,
    ) -> Result<Self, ParseError<'i>> {
        let location = input.current_source_location();

        match *input.next()? {
            Token::Ident(ref value) if allow_none && value.eq_ignore_ascii_case("none") => {
                Ok(ColorComponent::None)
            },
            ref t @ Token::Ident(ref ident) if origin_color.is_some() => {
                if let Ok(channel_keyword) = ChannelKeyword::from_ident(ident) {
                    if let Ok(value) = origin_color
                        .unwrap()
                        .get_component_by_channel_keyword(channel_keyword)
                    {
                        Ok(Self::Value(ValueType::from_value(value.unwrap_or(0.0))))
                    } else {
                        Err(location.new_unexpected_token_error(t.clone()))
                    }
                } else {
                    Err(location.new_unexpected_token_error(t.clone()))
                }
            },
            Token::Function(ref name) => {
                let function = SpecifiedCalcNode::math_function(context, name, location)?;
                let units = if rcs_enabled() {
                    ValueType::units() | CalcUnits::COLOR_COMPONENT
                } else {
                    ValueType::units()
                };
                let node = SpecifiedCalcNode::parse(context, input, function, units)?;

                let Ok(resolved_leaf) = node.resolve_map(|leaf| {
                    //
                    Ok(match leaf {
                        SpecifiedLeaf::ColorComponent(channel_keyword) => {
                            if let Some(origin_color) = origin_color {
                                if let Ok(value) =
                                    origin_color.get_component_by_channel_keyword(*channel_keyword)
                                {
                                    SpecifiedLeaf::Number(value.unwrap_or(0.0))
                                } else {
                                    return Err(());
                                }
                            } else {
                                return Err(());
                            }
                        },
                        l => l.clone(),
                    })
                }) else {
                    return Err(location.new_custom_error(StyleParseErrorKind::UnspecifiedError));
                };

                ValueType::try_from_leaf(&resolved_leaf)
                    .map(Self::Value)
                    .map_err(|_| location.new_custom_error(StyleParseErrorKind::UnspecifiedError))
            },
            ref t => ValueType::try_from_token(t)
                .map(Self::Value)
                .map_err(|_| location.new_unexpected_token_error(t.clone())),
        }
    }
}
