use crate::ast::{self, kw};
use crate::parser::{Parse, Parser, Result};

/// A WebAssembly tag directive, part of the exception handling proposal.
#[derive(Debug)]
pub struct Tag<'a> {
    /// Where this tag was defined
    pub span: ast::Span,
    /// An optional name by which to refer to this tag in name resolution.
    pub id: Option<ast::Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<ast::NameAnnotation<'a>>,
    /// Optional export directives for this tag.
    pub exports: ast::InlineExport<'a>,
    /// The type of tag that is defined.
    pub ty: TagType<'a>,
    /// What kind of tag this is defined as.
    pub kind: TagKind<'a>,
}

/// Listing of various types of tags that can be defined in a wasm module.
#[derive(Clone, Debug)]
pub enum TagType<'a> {
    /// An exception tag, where the payload is the type signature of the tag
    /// (constructor parameters, etc).
    Exception(ast::TypeUse<'a, ast::FunctionType<'a>>),
}

/// Different kinds of tags that can be defined in a module.
#[derive(Debug)]
pub enum TagKind<'a> {
    /// An tag which is actually defined as an import, such as:
    ///
    /// ```text
    /// (tag (type 0) (import "foo" "bar"))
    /// ```
    Import(ast::InlineImport<'a>),

    /// A tag defined inline in the module itself
    Inline(),
}

impl<'a> Parse<'a> for Tag<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::tag>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;
        let (ty, kind) = if let Some(import) = parser.parse()? {
            (parser.parse()?, TagKind::Import(import))
        } else {
            (parser.parse()?, TagKind::Inline())
        };
        Ok(Tag {
            span,
            id,
            name,
            exports,
            ty,
            kind,
        })
    }
}

impl<'a> Parse<'a> for TagType<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(TagType::Exception(parser.parse()?))
    }
}
