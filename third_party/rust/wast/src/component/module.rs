use crate::component::*;
use crate::core;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, NameAnnotation, Span};

/// A nested WebAssembly module to be created as part of a component.
#[derive(Debug)]
pub struct Module<'a> {
    /// Where this `nested module` was defined.
    pub span: Span,
    /// An identifier that this nested module is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this module stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: core::InlineExport<'a>,
    /// What kind of nested module this is, be it an inline-defined or imported one.
    pub kind: ModuleKind<'a>,
}

/// Possible ways to define a nested module in the text format.
#[derive(Debug)]
pub enum ModuleKind<'a> {
    /// An nested module which is actually defined as an import, such as:
    Import {
        /// Where this nested module is imported from
        import: InlineImport<'a>,
        /// The type that this nested module will have.
        ty: ComponentTypeUse<'a, ModuleType<'a>>,
    },

    ///  modules whose instantiation is defined inline.
    Inline {
        /// Fields in the nested module.
        fields: Vec<core::ModuleField<'a>>,
    },
}

impl<'a> Parse<'a> for Module<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        // This is sort of a fundamental limitation of the way this crate is
        // designed. Everything is done through recursive descent parsing which
        // means, well, that we're recursively going down the stack as we parse
        // nested data structures. While we can handle this for wasm expressions
        // since that's a pretty local decision, handling this for nested
        // modules which be far trickier. For now we just say that when the
        // parser goes too deep we return an error saying there's too many
        // nested modules. It would be great to not return an error here,
        // though!
        if parser.parens_depth() > 100 {
            return Err(parser.error("module nesting too deep"));
        }

        let span = parser.parse::<kw::module>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;

        let kind = if let Some(import) = parser.parse()? {
            ModuleKind::Import {
                import,
                ty: parser.parse()?,
            }
        } else {
            let mut fields = Vec::new();
            while !parser.is_empty() {
                fields.push(parser.parens(|p| p.parse())?);
            }
            ModuleKind::Inline { fields }
        };

        Ok(Module {
            span,
            id,
            name,
            exports,
            kind,
        })
    }
}
