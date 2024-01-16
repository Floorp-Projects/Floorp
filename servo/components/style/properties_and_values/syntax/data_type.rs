/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Used for parsing and serializing component names from the syntax string.

use super::{Component, ComponentName, Multiplier};
use std::fmt::{self, Debug, Write};
use style_traits::{CssWriter, ToCss};

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#supported-names>
#[derive(Clone, Copy, Debug, MallocSizeOf, PartialEq)]
pub enum DataType {
    /// Any valid `<length>` value
    Length,
    /// `<number>` values
    Number,
    /// Any valid <percentage> value
    Percentage,
    /// Any valid `<length>` or `<percentage>` value, any valid `<calc()>` expression combining
    /// `<length>` and `<percentage>` components.
    LengthPercentage,
    /// Any valid `<color>` value
    Color,
    /// Any valid `<image>` value
    Image,
    /// Any valid `<url>` value
    Url,
    /// Any valid `<integer>` value
    Integer,
    /// Any valid `<angle>` value
    Angle,
    /// Any valid `<time>` value
    Time,
    /// Any valid `<resolution>` value
    Resolution,
    /// Any valid `<transform-function>` value
    TransformFunction,
    /// Any valid `<custom-ident>` value
    CustomIdent,
    /// A list of valid `<transform-function>` values. Note that "<transform-list>" is a pre-multiplied
    /// data type name equivalent to "<transform-function>+"
    TransformList,
    /// Any valid `<string>` value
    ///
    /// <https://github.com/w3c/css-houdini-drafts/issues/1103>
    String,
}

impl DataType {
    /// Converts a component name from a pre-multiplied data type to its un-pre-multiplied equivalent.
    ///
    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#pre-multiplied-data-type-name>
    pub fn unpremultiply(&self) -> Option<Component> {
        match *self {
            DataType::TransformList => Some(Component {
                name: ComponentName::DataType(DataType::TransformFunction),
                multiplier: Some(Multiplier::Space),
            }),
            _ => None,
        }
    }

    /// Parses a syntax component name.
    pub fn from_str(ty: &str) -> Option<Self> {
        Some(match ty.as_bytes() {
            b"length" => DataType::Length,
            b"number" => DataType::Number,
            b"percentage" => DataType::Percentage,
            b"length-percentage" => DataType::LengthPercentage,
            b"color" => DataType::Color,
            b"image" => DataType::Image,
            b"url" => DataType::Url,
            b"integer" => DataType::Integer,
            b"angle" => DataType::Angle,
            b"time" => DataType::Time,
            b"resolution" => DataType::Resolution,
            b"transform-function" => DataType::TransformFunction,
            b"custom-ident" => DataType::CustomIdent,
            b"transform-list" => DataType::TransformList,
            b"string" => DataType::String,
            _ => return None,
        })
    }

    /// Returns true if this data type requires deferring computation to properly
    /// resolve font-dependent lengths.
    pub fn may_reference_font_relative_length(&self) -> bool {
        match self {
            DataType::Length |
            DataType::LengthPercentage |
            DataType::TransformFunction |
            DataType::TransformList => true,
            DataType::Number |
            DataType::Percentage |
            DataType::Color |
            DataType::Image |
            DataType::Url |
            DataType::Integer |
            DataType::Angle |
            DataType::Time |
            DataType::Resolution |
            DataType::CustomIdent |
            DataType::String => false,
        }
    }
}

impl ToCss for DataType {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_char('<')?;
        dest.write_str(match *self {
            DataType::Length => "length",
            DataType::Number => "number",
            DataType::Percentage => "percentage",
            DataType::LengthPercentage => "length-percentage",
            DataType::Color => "color",
            DataType::Image => "image",
            DataType::Url => "url",
            DataType::Integer => "integer",
            DataType::Angle => "angle",
            DataType::Time => "time",
            DataType::Resolution => "resolution",
            DataType::TransformFunction => "transform-function",
            DataType::CustomIdent => "custom-ident",
            DataType::TransformList => "transform-list",
            DataType::String => "string",
        })?;
        dest.write_char('>')
    }
}
