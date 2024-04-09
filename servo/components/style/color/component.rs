/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Parse/serialize and resolve a single color component.

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
    ) -> Result<Self, ParseError<'i>> {
        let location = input.current_source_location();

        match *input.next()? {
            Token::Ident(ref value) if allow_none && value.eq_ignore_ascii_case("none") => {
                Ok(ColorComponent::None)
            },
            Token::Function(ref name) => {
                let function = SpecifiedCalcNode::math_function(context, name, location)?;
                let node = SpecifiedCalcNode::parse(context, input, function, ValueType::units())?;

                let Ok(resolved_leaf) = node.resolve() else {
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
