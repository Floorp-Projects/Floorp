use crate::component::*;
use crate::core;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, LParen, NameAnnotation, Span};

/// A WebAssembly function to be inserted into a module.
///
/// This is a member of both the function and code sections.
#[derive(Debug)]
pub struct ComponentFunc<'a> {
    /// Where this `func` was defined.
    pub span: Span,
    /// An identifier that this function is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: core::InlineExport<'a>,
    /// What kind of function this is, be it an inline-defined or imported
    /// function.
    pub kind: ComponentFuncKind<'a>,
}

/// Possible ways to define a function in the text format.
#[derive(Debug)]
pub enum ComponentFuncKind<'a> {
    /// A function which is actually defined as an import, such as:
    ///
    /// ```text
    /// (func (import "foo") (type 3))
    /// ```
    Import {
        /// The import name of this import
        import: InlineImport<'a>,
        /// The type that this function will have.
        ty: ComponentTypeUse<'a, ComponentFunctionType<'a>>,
    },

    /// Almost all functions, those defined inline in a wasm module.
    Inline {
        /// The body of the function.
        body: ComponentFuncBody<'a>,
    },
}

impl<'a> Parse<'a> for ComponentFunc<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::func>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;

        let kind = if let Some(import) = parser.parse()? {
            ComponentFuncKind::Import {
                import,
                ty: parser.parse()?,
            }
        } else {
            ComponentFuncKind::Inline {
                body: parser.parens(|p| p.parse())?,
            }
        };

        Ok(ComponentFunc {
            span,
            id,
            name,
            exports,
            kind,
        })
    }
}

/// The body of a `ComponentFunc`.
#[derive(Debug)]
pub enum ComponentFuncBody<'a> {
    /// A `canon.lift`.
    CanonLift(CanonLift<'a>),
    /// A `canon.lower`.
    CanonLower(CanonLower<'a>),
}

impl<'a> Parse<'a> for ComponentFuncBody<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<kw::canon_lift>() {
            Ok(ComponentFuncBody::CanonLift(parser.parse()?))
        } else if parser.peek::<kw::canon_lower>() {
            Ok(ComponentFuncBody::CanonLower(parser.parse()?))
        } else {
            Err(parser.error("Expected canon.lift or canon.lower"))
        }
    }
}

/// Extra information associated with canon.lift instructions.
#[derive(Debug)]
pub struct CanonLift<'a> {
    /// The type exported to other components
    pub type_: ComponentTypeUse<'a, ComponentFunctionType<'a>>,
    /// Configuration options
    pub opts: Vec<CanonOpt<'a>>,
    /// The function to wrap
    pub func: ItemRef<'a, kw::func>,
}

impl<'a> Parse<'a> for CanonLift<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::canon_lift>()?;
        let type_ = if parser.peek2::<kw::func>() {
            ComponentTypeUse::Inline(parser.parens(|p| {
                p.parse::<kw::func>()?;
                p.parse()
            })?)
        } else {
            ComponentTypeUse::Ref(parser.parse()?)
        };
        let mut opts = Vec::new();
        while !parser.peek2::<kw::func>() {
            opts.push(parser.parse()?);
        }
        let func = parser.parse()?;
        Ok(CanonLift { type_, opts, func })
    }
}

/// Extra information associated with canon.lower instructions.
#[derive(Debug)]
pub struct CanonLower<'a> {
    /// Configuration options
    pub opts: Vec<CanonOpt<'a>>,
    /// The function being wrapped
    pub func: ItemRef<'a, kw::func>,
}

impl<'a> Parse<'a> for CanonLower<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::canon_lower>()?;
        let mut opts = Vec::new();
        while !parser.is_empty() && (!parser.peek::<LParen>() || !parser.peek2::<kw::func>()) {
            opts.push(parser.parse()?);
        }
        let func = parser.parse()?;
        Ok(CanonLower { opts, func })
    }
}

#[derive(Debug)]
/// Canonical ABI options.
pub enum CanonOpt<'a> {
    /// Encode strings as UTF-8.
    StringUtf8,
    /// Encode strings as UTF-16.
    StringUtf16,
    /// Encode strings as "compact UTF-16".
    StringLatin1Utf16,
    /// A target instance which supplies the memory that the canonical ABI
    /// should operate on as well as functions that the canonical ABI can call
    /// to allocate, reallocate and free linear memory
    Into(ItemRef<'a, kw::instance>),
}

impl<'a> Parse<'a> for CanonOpt<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::string_utf8>() {
            parser.parse::<kw::string_utf8>()?;
            Ok(CanonOpt::StringUtf8)
        } else if l.peek::<kw::string_utf16>() {
            parser.parse::<kw::string_utf16>()?;
            Ok(CanonOpt::StringUtf16)
        } else if l.peek::<kw::string_latin1_utf16>() {
            parser.parse::<kw::string_latin1_utf16>()?;
            Ok(CanonOpt::StringLatin1Utf16)
        } else if l.peek::<LParen>() {
            parser.parens(|parser| {
                let mut l = parser.lookahead1();
                if l.peek::<kw::into>() {
                    parser.parse::<kw::into>()?;
                    Ok(CanonOpt::Into(parser.parse::<IndexOrRef<'_, _>>()?.0))
                } else {
                    Err(l.error())
                }
            })
        } else {
            Err(l.error())
        }
    }
}

impl Default for kw::instance {
    fn default() -> kw::instance {
        kw::instance(Span::from_offset(0))
    }
}
