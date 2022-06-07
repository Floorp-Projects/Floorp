use crate::component::ComponentArg;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::Span;

/// A entry in a WebAssembly component's export section.
///
/// export       ::= (export <name> <componentarg>)
#[derive(Debug)]
pub struct ComponentExport<'a> {
    /// Where this export was defined.
    pub span: Span,
    /// The name of this export from the component.
    pub name: &'a str,
    /// What's being exported from the component.
    pub arg: ComponentArg<'a>,
}

impl<'a> Parse<'a> for ComponentExport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::export>()?.0;
        let name = parser.parse()?;
        let arg = parser.parse()?;
        Ok(ComponentExport { span, name, arg })
    }
}
