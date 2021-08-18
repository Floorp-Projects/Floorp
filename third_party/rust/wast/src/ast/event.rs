use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};

/// A WebAssembly event directive, part of the exception handling proposal.
#[derive(Debug)]
pub struct Event<'a> {
    /// Where this event was defined
    pub span: ast::Span,
    /// An optional name by which to refer to this event in name resolution.
    pub id: Option<ast::Id<'a>>,
    /// Optional export directives for this event.
    pub exports: ast::InlineExport<'a>,
    /// The type of event that is defined.
    pub ty: EventType<'a>,
    /// What kind of event this is defined as.
    pub kind: EventKind<'a>,
}

/// Listing of various types of events that can be defined in a wasm module.
#[derive(Clone, Debug)]
pub enum EventType<'a> {
    /// An exception event, where the payload is the type signature of the event
    /// (constructor parameters, etc).
    Exception(ast::TypeUse<'a, ast::FunctionType<'a>>),
}

/// Different kinds of events that can be defined in a module.
#[derive(Debug)]
pub enum EventKind<'a> {
    /// An event which is actually defined as an import, such as:
    ///
    /// ```text
    /// (event (type 0) (import "foo" "bar"))
    /// ```
    Import(ast::InlineImport<'a>),

    /// An event defined inline in the module itself
    Inline(),
}

impl<'a> Parse<'a> for Event<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::event>()?.0;
        let id = parser.parse()?;
        let exports = parser.parse()?;
        let (ty, kind) = if let Some(import) = parser.parse()? {
            (parser.parse()?, EventKind::Import(import))
        } else {
            (parser.parse()?, EventKind::Inline())
        };
        Ok(Event {
            span,
            id,
            exports,
            ty,
            kind,
        })
    }
}

impl<'a> Parse<'a> for EventType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(EventType::Exception(parser.parse()?))
    }
}
