/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Used for parsing and serializing the [`@property`] syntax string.
//!
//! <https://drafts.css-houdini.org/css-properties-values-api-1/#parsing-syntax>

use std::fmt::{self, Debug};
use std::{borrow::Cow, fmt::Write};

use crate::parser::{Parse, ParserContext};
use crate::values::CustomIdent;
use cssparser::{Parser as CSSParser, ParserInput as CSSParserInput};
use style_traits::{
    CssWriter, ParseError as StyleParseError, PropertySyntaxParseError as ParseError,
    StyleParseErrorKind, ToCss,
};

use self::data_type::DataType;

mod ascii;
pub mod data_type;

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#parsing-syntax>
#[derive(Debug, Clone, Default, MallocSizeOf, PartialEq)]
pub struct Descriptor {
    /// The parsed components, if any.
    pub components: Box<[Component]>,
    /// The specified css syntax, if any.
    specified: Option<Box<str>>,
}

impl Descriptor {
    /// Returns whether this is the universal syntax descriptor.
    #[inline]
    pub fn is_universal(&self) -> bool {
        self.components.is_empty()
    }

    /// Returns the specified string, if any.
    #[inline]
    pub fn specified_string(&self) -> Option<&str> {
        self.specified.as_deref()
    }

    /// Parse a syntax descriptor.
    /// https://drafts.css-houdini.org/css-properties-values-api-1/#consume-a-syntax-definition
    pub fn from_str(css: &str, save_specified: bool) -> Result<Self, ParseError> {
        // 1. Strip leading and trailing ASCII whitespace from string.
        let input = ascii::trim_ascii_whitespace(css);

        // 2. If string's length is 0, return failure.
        if input.is_empty() {
            return Err(ParseError::EmptyInput);
        }

        let specified = if save_specified {
            Some(Box::from(css))
        } else {
            None
        };

        // 3. If string's length is 1, and the only code point in string is U+002A
        //    ASTERISK (*), return the universal syntax descriptor.
        if input.len() == 1 && input.as_bytes()[0] == b'*' {
            return Ok(Self {
                components: Default::default(),
                specified,
            });
        }

        // 4. Let stream be an input stream created from the code points of string,
        //    preprocessed as specified in [css-syntax-3]. Let descriptor be an
        //    initially empty list of syntax components.
        //
        // NOTE(emilio): Instead of preprocessing we cheat and treat new-lines and
        // nulls in the parser specially.
        let mut components = vec![];
        {
            let mut parser = Parser::new(input, &mut components);
            // 5. Repeatedly consume the next input code point from stream.
            parser.parse()?;
        }
        Ok(Self {
            components: components.into_boxed_slice(),
            specified,
        })
    }

    /// Returns true if the syntax permits the value to be computed as a length.
    pub fn may_compute_length(&self) -> bool {
        for component in self.components.iter() {
            match &component.name {
                ComponentName::DataType(ref t) => {
                    if matches!(t, DataType::Length | DataType::LengthPercentage) {
                        return true;
                    }
                },
                ComponentName::Ident(_) => (),
            };
        }
        false
    }

    /// Returns true if the syntax requires deferring computation to properly
    /// resolve font-dependent lengths.
    pub fn may_reference_font_relative_length(&self) -> bool {
        for component in self.components.iter() {
            match &component.name {
                ComponentName::DataType(ref t) => {
                    if t.may_reference_font_relative_length() {
                        return true;
                    }
                },
                ComponentName::Ident(_) => (),
            };
        }
        false
    }
}

impl ToCss for Descriptor {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        if let Some(ref specified) = self.specified {
            return specified.to_css(dest);
        }

        if self.is_universal() {
            return dest.write_char('*');
        }

        let mut first = true;
        for component in &*self.components {
            if !first {
                dest.write_str(" | ")?;
            }
            component.to_css(dest)?;
            first = false;
        }

        Ok(())
    }
}

impl Parse for Descriptor {
    /// Parse a syntax descriptor.
    fn parse<'i>(
        _: &ParserContext,
        parser: &mut CSSParser<'i, '_>,
    ) -> Result<Self, StyleParseError<'i>> {
        let input = parser.expect_string()?;
        Descriptor::from_str(input.as_ref(), /* save_specified = */ true)
            .map_err(|err| parser.new_custom_error(StyleParseErrorKind::PropertySyntaxField(err)))
    }
}

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#multipliers>
#[derive(Clone, Copy, Debug, MallocSizeOf, PartialEq, ToComputedValue)]
pub enum Multiplier {
    /// Indicates a space-separated list.
    Space,
    /// Indicates a comma-separated list.
    Comma,
}

impl ToCss for Multiplier {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_char(match *self {
            Multiplier::Space => '+',
            Multiplier::Comma => '#',
        })
    }
}

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#syntax-component>
#[derive(Clone, Debug, MallocSizeOf, PartialEq)]
pub struct Component {
    name: ComponentName,
    multiplier: Option<Multiplier>,
}

impl Component {
    /// Returns the component's name.
    #[inline]
    pub fn name(&self) -> &ComponentName {
        &self.name
    }

    /// Returns the component's multiplier, if one exists.
    #[inline]
    pub fn multiplier(&self) -> Option<Multiplier> {
        self.multiplier
    }

    /// If the component is premultiplied, return the un-premultiplied component.
    #[inline]
    pub fn unpremultiplied(&self) -> Cow<Self> {
        match self.name.unpremultiply() {
            Some(component) => {
                debug_assert!(
                    self.multiplier.is_none(),
                    "Shouldn't have parsed a multiplier for a pre-multiplied data type name",
                );
                Cow::Owned(component)
            },
            None => Cow::Borrowed(self),
        }
    }
}

impl ToCss for Component {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        self.name().to_css(dest)?;
        self.multiplier().to_css(dest)
    }
}

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#syntax-component-name>
#[derive(Clone, Debug, MallocSizeOf, PartialEq, ToCss)]
pub enum ComponentName {
    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#data-type-name>
    DataType(DataType),
    /// <https://drafts.csswg.org/css-values-4/#custom-idents>
    Ident(CustomIdent),
}

impl ComponentName {
    fn unpremultiply(&self) -> Option<Component> {
        match *self {
            ComponentName::DataType(ref t) => t.unpremultiply(),
            ComponentName::Ident(..) => None,
        }
    }

    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#pre-multiplied-data-type-name>
    fn is_pre_multiplied(&self) -> bool {
        self.unpremultiply().is_some()
    }
}

struct Parser<'a> {
    input: &'a str,
    position: usize,
    output: &'a mut Vec<Component>,
}

/// <https://drafts.csswg.org/css-syntax-3/#letter>
fn is_letter(byte: u8) -> bool {
    match byte {
        b'A'..=b'Z' | b'a'..=b'z' => true,
        _ => false,
    }
}

/// <https://drafts.csswg.org/css-syntax-3/#non-ascii-code-point>
fn is_non_ascii(byte: u8) -> bool {
    byte >= 0x80
}

/// <https://drafts.csswg.org/css-syntax-3/#name-start-code-point>
fn is_name_start(byte: u8) -> bool {
    is_letter(byte) || is_non_ascii(byte) || byte == b'_'
}

impl<'a> Parser<'a> {
    fn new(input: &'a str, output: &'a mut Vec<Component>) -> Self {
        Self {
            input,
            position: 0,
            output,
        }
    }

    fn peek(&self) -> Option<u8> {
        self.input.as_bytes().get(self.position).cloned()
    }

    fn parse(&mut self) -> Result<(), ParseError> {
        // 5. Repeatedly consume the next input code point from stream:
        loop {
            let component = self.parse_component()?;
            self.output.push(component);
            self.skip_whitespace();

            let byte = match self.peek() {
                None => return Ok(()),
                Some(b) => b,
            };

            if byte != b'|' {
                return Err(ParseError::ExpectedPipeBetweenComponents);
            }

            self.position += 1;
        }
    }

    fn skip_whitespace(&mut self) {
        loop {
            match self.peek() {
                Some(c) if c.is_ascii_whitespace() => self.position += 1,
                _ => return,
            }
        }
    }

    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#consume-data-type-name>
    fn parse_data_type_name(&mut self) -> Result<DataType, ParseError> {
        let start = self.position;
        loop {
            let byte = match self.peek() {
                Some(b) => b,
                None => return Err(ParseError::UnclosedDataTypeName),
            };
            if byte != b'>' {
                self.position += 1;
                continue;
            }
            let ty = match DataType::from_str(&self.input[start..self.position]) {
                Some(ty) => ty,
                None => return Err(ParseError::UnknownDataTypeName),
            };
            self.position += 1;
            return Ok(ty);
        }
    }

    fn parse_name(&mut self) -> Result<ComponentName, ParseError> {
        let b = match self.peek() {
            Some(b) => b,
            None => return Err(ParseError::UnexpectedEOF),
        };

        if b == b'<' {
            self.position += 1;
            return Ok(ComponentName::DataType(self.parse_data_type_name()?));
        }

        if b != b'\\' && !is_name_start(b) {
            return Err(ParseError::InvalidNameStart);
        }

        let input = &self.input[self.position..];
        let mut input = CSSParserInput::new(input);
        let mut input = CSSParser::new(&mut input);
        let name = match CustomIdent::parse(&mut input, &[]) {
            Ok(name) => name,
            Err(_) => return Err(ParseError::InvalidName),
        };
        self.position += input.position().byte_index();
        return Ok(ComponentName::Ident(name));
    }

    fn parse_multiplier(&mut self) -> Option<Multiplier> {
        let multiplier = match self.peek()? {
            b'+' => Multiplier::Space,
            b'#' => Multiplier::Comma,
            _ => return None,
        };
        self.position += 1;
        Some(multiplier)
    }

    /// <https://drafts.css-houdini.org/css-properties-values-api-1/#consume-a-syntax-component>
    fn parse_component(&mut self) -> Result<Component, ParseError> {
        // Consume as much whitespace as possible from stream.
        self.skip_whitespace();
        let name = self.parse_name()?;
        let multiplier = if name.is_pre_multiplied() {
            None
        } else {
            self.parse_multiplier()
        };
        Ok(Component { name, multiplier })
    }
}
