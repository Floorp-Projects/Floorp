use crate::component::*;
use crate::core;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, NameAnnotation, Span};

/// A core WebAssembly module to be created as part of a component.
///
/// This is a member of the core module section.
#[derive(Debug)]
pub struct CoreModule<'a> {
    /// Where this `core module` was defined.
    pub span: Span,
    /// An identifier that this module is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this module stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: core::InlineExport<'a>,
    /// What kind of module this is, be it an inline-defined or imported one.
    pub kind: CoreModuleKind<'a>,
}

/// Possible ways to define a core module in the text format.
#[derive(Debug)]
pub enum CoreModuleKind<'a> {
    /// A core module which is actually defined as an import
    Import {
        /// Where this core module is imported from
        import: InlineImport<'a>,
        /// The type that this core module will have.
        ty: CoreTypeUse<'a, ModuleType<'a>>,
    },

    /// Modules that are defined inline.
    Inline {
        /// Fields in the core module.
        fields: Vec<core::ModuleField<'a>>,
    },
}

impl<'a> Parse<'a> for CoreModule<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.depth_check()?;

        let span = parser.parse::<kw::core>()?.0;
        parser.parse::<kw::module>()?;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;

        let kind = if let Some(import) = parser.parse()? {
            CoreModuleKind::Import {
                import,
                ty: parser.parse()?,
            }
        } else {
            let mut fields = Vec::new();
            while !parser.is_empty() {
                fields.push(parser.parens(|p| p.parse())?);
            }
            CoreModuleKind::Inline { fields }
        };

        Ok(Self {
            span,
            id,
            name,
            exports,
            kind,
        })
    }
}
