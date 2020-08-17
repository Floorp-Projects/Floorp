use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};

/// An `alias` statement used to juggle indices with nested modules.
#[derive(Debug)]
pub struct Alias<'a> {
    /// Where this `alias` was defined.
    pub span: ast::Span,
    /// An identifier that this alias is resolved with (optionally) for name
    /// resolution.
    pub id: Option<ast::Id<'a>>,
    /// An optional name for this alias stored in the custom `name` section.
    pub name: Option<ast::NameAnnotation<'a>>,
    /// The instance that we're aliasing. If `None` then we're aliasing the
    /// parent instance.
    pub instance: Option<ast::Index<'a>>,
    /// The item in the parent instance that we're aliasing.
    pub kind: ast::ExportKind<'a>,
}

impl<'a> Parse<'a> for Alias<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::alias>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let instance = if parser.parse::<Option<kw::parent>>()?.is_some() {
            None
        } else {
            Some(parser.parens(|p| {
                p.parse::<kw::instance>()?;
                p.parse()
            })?)
        };

        Ok(Alias {
            span,
            id,
            name,
            instance,
            kind: parser.parens(|p| p.parse())?,
        })
    }
}
