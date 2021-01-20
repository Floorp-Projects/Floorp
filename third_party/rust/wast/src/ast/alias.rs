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
    /// The item in the parent instance that we're aliasing.
    pub kind: AliasKind<'a>,
}

#[derive(Debug)]
#[allow(missing_docs)]
pub enum AliasKind<'a> {
    InstanceExport {
        instance: ast::ItemRef<'a, kw::instance>,
        export: &'a str,
        kind: ast::ExportKind,
    },
    Outer {
        /// The index of the module that this reference is referring to.
        module: ast::Index<'a>,
        /// The index of the item within `module` that this alias is referering
        /// to.
        index: ast::Index<'a>,
        /// The kind of item that's being aliased.
        kind: ast::ExportKind,
    },
}

impl Alias<'_> {
    /// Returns the kind of item defined by this alias.
    pub fn item_kind(&self) -> ast::ExportKind {
        match self.kind {
            AliasKind::InstanceExport { kind, .. } => kind,
            AliasKind::Outer { kind, .. } => kind,
        }
    }
}

impl<'a> Parse<'a> for Alias<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::alias>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let kind = parser.parens(|p| {
            let kind = p.parse()?;
            Ok(if parser.parse::<Option<kw::outer>>()?.is_some() {
                AliasKind::Outer {
                    module: parser.parse()?,
                    index: parser.parse()?,
                    kind,
                }
            } else {
                AliasKind::InstanceExport {
                    instance: parser.parse::<ast::IndexOrRef<_>>()?.0,
                    export: parser.parse()?,
                    kind,
                }
            })
        })?;

        Ok(Alias {
            span,
            id,
            name,
            kind,
        })
    }
}
