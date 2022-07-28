use crate::annotation;
use crate::parser::{Parse, Parser, Result};
use crate::token::Span;

/// A custom section within a component.
#[derive(Debug)]
pub struct Custom<'a> {
    /// Where this `@custom` was defined.
    pub span: Span,

    /// Name of the custom section.
    pub name: &'a str,

    /// Payload of this custom section.
    pub data: Vec<&'a [u8]>,
}

impl<'a> Parse<'a> for Custom<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<annotation::custom>()?.0;
        let name = parser.parse()?;
        let mut data = Vec::new();
        while !parser.is_empty() {
            data.push(parser.parse()?);
        }
        Ok(Self { span, name, data })
    }
}
