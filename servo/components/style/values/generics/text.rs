/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Generic types for text properties.

use crate::parser::ParserContext;
use cssparser::Parser;
use style_traits::ParseError;

/// A generic value for the `initial-letter` property.
#[derive(
    Clone,
    Copy,
    Debug,
    MallocSizeOf,
    PartialEq,
    SpecifiedValueInfo,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
pub enum InitialLetter<Number, Integer> {
    /// `normal`
    Normal,
    /// `<number> <integer>?`
    Specified(Number, Option<Integer>),
}

impl<N, I> InitialLetter<N, I> {
    /// Returns `normal`.
    #[inline]
    pub fn normal() -> Self {
        InitialLetter::Normal
    }
}

/// A generic spacing value for the `letter-spacing` and `word-spacing` properties.
#[derive(Clone, Copy, Debug, MallocSizeOf, PartialEq, SpecifiedValueInfo, ToCss, ToShmem)]
pub enum Spacing<Value> {
    /// `normal`
    Normal,
    /// `<value>`
    Value(Value),
}

impl<Value> Spacing<Value> {
    /// Returns `normal`.
    #[inline]
    pub fn normal() -> Self {
        Spacing::Normal
    }

    /// Parses.
    #[inline]
    pub fn parse_with<'i, 't, F>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
        parse: F,
    ) -> Result<Self, ParseError<'i>>
    where
        F: FnOnce(&ParserContext, &mut Parser<'i, 't>) -> Result<Value, ParseError<'i>>,
    {
        if input
            .try_parse(|i| i.expect_ident_matching("normal"))
            .is_ok()
        {
            return Ok(Spacing::Normal);
        }
        parse(context, input).map(Spacing::Value)
    }
}

/// Implements type for text-decoration-thickness
/// which takes the grammar of auto | from-font | <length> | <percentage>
///
/// https://drafts.csswg.org/css-text-decor-4/
#[repr(C, u8)]
#[cfg_attr(feature = "servo", derive(Deserialize, Serialize))]
#[derive(
    Animate,
    Clone,
    Copy,
    ComputeSquaredDistance,
    ToAnimatedZero,
    Debug,
    Eq,
    MallocSizeOf,
    Parse,
    PartialEq,
    SpecifiedValueInfo,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[allow(missing_docs)]
pub enum GenericTextDecorationLength<L> {
    LengthPercentage(L),
    Auto,
    FromFont,
}
