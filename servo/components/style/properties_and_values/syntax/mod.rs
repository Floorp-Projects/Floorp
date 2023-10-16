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
    arc_slice::ArcSlice,
    CssWriter, ParseError as StyleParseError, PropertySyntaxParseError as ParseError,
    StyleParseErrorKind, ToCss,
};

use self::data_type::DataType;

mod ascii;
pub mod data_type;

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#parsing-syntax>
#[derive(Debug, Clone, Default, MallocSizeOf, PartialEq)]
pub struct Descriptor(#[ignore_malloc_size_of = "arc"] pub ArcSlice<Component>);

impl Descriptor {
    /// Returns whether this is the universal syntax descriptor.
    pub fn is_universal(&self) -> bool {
        self.0.is_empty()
    }

    /// Parse a syntax descriptor.
    /// https://drafts.css-houdini.org/css-properties-values-api-1/#consume-a-syntax-definition
    pub fn from_str(css: &str) -> Result<Self, ParseError> {
        // 1. Strip leading and trailing ASCII whitespace from string.
        let input = ascii::trim_ascii_whitespace(css);

        // 2. If string's length is 0, return failure.
        if input.is_empty() {
            return Err(ParseError::EmptyInput);
        }

        // 3. If string's length is 1, and the only code point in string is U+002A
        //    ASTERISK (*), return the universal syntax descriptor.
        if input.len() == 1 && input.as_bytes()[0] == b'*' {
            return Ok(Self::default());
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
        Ok(Self(ArcSlice::from_iter(components.into_iter())))
    }
}

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#parsing-syntax>
#[derive(Debug, Clone, Default, MallocSizeOf, PartialEq)]
pub struct ParsedDescriptor {
    descriptor: Descriptor,
    css: String
}

impl ParsedDescriptor {
    /// Returns the specified syntax string.
    pub fn descriptor(&self) -> &Descriptor {
        &self.descriptor
    }

    /// Returns the specified syntax string.
    pub fn as_str(&self) -> &str {
        &self.css
    }
}

impl Parse for ParsedDescriptor {
    /// Parse a syntax descriptor.
    fn parse<'i, 't>(
        _context: &ParserContext,
        parser: &mut CSSParser<'i, 't>,
    ) -> Result<Self, StyleParseError<'i>> {
        // 1. Strip leading and trailing ASCII whitespace from string.
        let input = parser.expect_string()?;
        match Descriptor::from_str(input.as_ref()) {
            Ok(descriptor) => Ok(Self {
                descriptor,
                css: input.as_ref().to_owned(),
            }),
            Err(err) => Err(parser.new_custom_error(StyleParseErrorKind::PropertySyntaxField(err))),
        }
    }
}

impl ToCss for ParsedDescriptor {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        self.css.to_css(dest)
    }
}

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#multipliers>
#[derive(Clone, Copy, Debug, MallocSizeOf, PartialEq)]
pub enum Multiplier {
    /// Indicates a space-separated list.
    Space,
    /// Indicates a comma-separated list.
    Comma,
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

/// <https://drafts.css-houdini.org/css-properties-values-api-1/#syntax-component-name>
#[derive(Clone, Debug, MallocSizeOf, PartialEq)]
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

/// <https://drafts.csswg.org/css-syntax-3/#whitespace>
fn is_whitespace(byte: u8) -> bool {
    match byte {
        b'\t' | b'\n' | b'\r' | b' ' => true,
        _ => false,
    }
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
                Some(c) if is_whitespace(c) => self.position += 1,
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
