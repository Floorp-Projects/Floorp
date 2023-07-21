use crate::component::*;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, Index, LParen, NameAnnotation, Span};

/// A declared core function.
///
/// This is a member of both the core alias and canon sections.
#[derive(Debug)]
pub struct CoreFunc<'a> {
    /// Where this `core func` was defined.
    pub span: Span,
    /// An identifier that this function is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// The kind of core function.
    pub kind: CoreFuncKind<'a>,
}

impl<'a> Parse<'a> for CoreFunc<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::core>()?.0;
        parser.parse::<kw::func>()?;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let kind = parser.parse()?;

        Ok(Self {
            span,
            id,
            name,
            kind,
        })
    }
}

/// Represents the kind of core functions.
#[derive(Debug)]
#[allow(missing_docs)]
pub enum CoreFuncKind<'a> {
    /// The core function is defined in terms of lowering a component function.
    ///
    /// The core function is actually a member of the canon section.
    Lower(CanonLower<'a>),
    /// The core function is defined in terms of aliasing a module instance export.
    ///
    /// The core function is actually a member of the core alias section.
    Alias(InlineExportAlias<'a, true>),
    ResourceNew(CanonResourceNew<'a>),
    ResourceDrop(CanonResourceDrop<'a>),
    ResourceRep(CanonResourceRep<'a>),
}

impl<'a> Parse<'a> for CoreFuncKind<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parens(|parser| {
            let mut l = parser.lookahead1();
            if l.peek::<kw::canon>()? {
                parser.parse::<kw::canon>()?;
            } else if l.peek::<kw::alias>()? {
                return Ok(Self::Alias(parser.parse()?));
            } else {
                return Err(l.error());
            }
            let mut l = parser.lookahead1();
            if l.peek::<kw::lower>()? {
                Ok(CoreFuncKind::Lower(parser.parse()?))
            } else if l.peek::<kw::resource_new>()? {
                Ok(CoreFuncKind::ResourceNew(parser.parse()?))
            } else if l.peek::<kw::resource_drop>()? {
                Ok(CoreFuncKind::ResourceDrop(parser.parse()?))
            } else if l.peek::<kw::resource_rep>()? {
                Ok(CoreFuncKind::ResourceRep(parser.parse()?))
            } else {
                Err(l.error())
            }
        })
    }
}

/// A declared component function.
///
/// This may be a member of the import, alias, or canon sections.
#[derive(Debug)]
pub struct Func<'a> {
    /// Where this `func` was defined.
    pub span: Span,
    /// An identifier that this function is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: InlineExport<'a>,
    /// The kind of function.
    pub kind: FuncKind<'a>,
}

impl<'a> Parse<'a> for Func<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::func>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;
        let kind = parser.parse()?;

        Ok(Self {
            span,
            id,
            name,
            exports,
            kind,
        })
    }
}

/// Represents the kind of component functions.
#[derive(Debug)]
pub enum FuncKind<'a> {
    /// A function which is actually defined as an import, such as:
    ///
    /// ```text
    /// (func (import "foo") (param string))
    /// ```
    Import {
        /// The import name of this import.
        import: InlineImport<'a>,
        /// The type that this function will have.
        ty: ComponentTypeUse<'a, ComponentFunctionType<'a>>,
    },
    /// The function is defined in terms of lifting a core function.
    ///
    /// The function is actually a member of the canon section.
    Lift {
        /// The lifted function's type.
        ty: ComponentTypeUse<'a, ComponentFunctionType<'a>>,
        /// Information relating to the lifting of the core function.
        info: CanonLift<'a>,
    },
    /// The function is defined in terms of aliasing a component instance export.
    ///
    /// The function is actually a member of the alias section.
    Alias(InlineExportAlias<'a, false>),
}

impl<'a> Parse<'a> for FuncKind<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if let Some(import) = parser.parse()? {
            Ok(Self::Import {
                import,
                ty: parser.parse()?,
            })
        } else if parser.peek::<LParen>()? && parser.peek2::<kw::alias>()? {
            parser.parens(|parser| Ok(Self::Alias(parser.parse()?)))
        } else {
            Ok(Self::Lift {
                ty: parser.parse()?,
                info: parser.parens(|parser| {
                    parser.parse::<kw::canon>()?;
                    parser.parse()
                })?,
            })
        }
    }
}

/// A WebAssembly canonical function to be inserted into a component.
///
/// This is a member of the canonical section.
#[derive(Debug)]
pub struct CanonicalFunc<'a> {
    /// Where this `func` was defined.
    pub span: Span,
    /// An identifier that this function is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this function stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// What kind of function this is, be it a lowered or lifted function.
    pub kind: CanonicalFuncKind<'a>,
}

impl<'a> Parse<'a> for CanonicalFunc<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::canon>()?.0;

        if parser.peek::<kw::lift>()? {
            let info = parser.parse()?;
            let (id, name, ty) = parser.parens(|parser| {
                parser.parse::<kw::func>()?;
                let id = parser.parse()?;
                let name = parser.parse()?;
                let ty = parser.parse()?;
                Ok((id, name, ty))
            })?;

            Ok(Self {
                span,
                id,
                name,
                kind: CanonicalFuncKind::Lift { info, ty },
            })
        } else if parser.peek::<kw::lower>()? {
            Self::parse_core_func(span, parser, CanonicalFuncKind::Lower)
        } else if parser.peek::<kw::resource_new>()? {
            Self::parse_core_func(span, parser, CanonicalFuncKind::ResourceNew)
        } else if parser.peek::<kw::resource_drop>()? {
            Self::parse_core_func(span, parser, CanonicalFuncKind::ResourceDrop)
        } else if parser.peek::<kw::resource_rep>()? {
            Self::parse_core_func(span, parser, CanonicalFuncKind::ResourceRep)
        } else {
            Err(parser.error("expected `canon lift` or `canon lower`"))
        }
    }
}

impl<'a> CanonicalFunc<'a> {
    fn parse_core_func<T>(
        span: Span,
        parser: Parser<'a>,
        variant: fn(T) -> CanonicalFuncKind<'a>,
    ) -> Result<Self>
    where
        T: Parse<'a>,
    {
        let info = parser.parse()?;
        let (id, name) = parser.parens(|parser| {
            parser.parse::<kw::core>()?;
            parser.parse::<kw::func>()?;
            let id = parser.parse()?;
            let name = parser.parse()?;
            Ok((id, name))
        })?;

        Ok(Self {
            span,
            id,
            name,
            kind: variant(info),
        })
    }
}

/// Possible ways to define a canonical function in the text format.
#[derive(Debug)]
#[allow(missing_docs)]
pub enum CanonicalFuncKind<'a> {
    /// A canonical function that is defined in terms of lifting a core function.
    Lift {
        /// The lifted function's type.
        ty: ComponentTypeUse<'a, ComponentFunctionType<'a>>,
        /// Information relating to the lifting of the core function.
        info: CanonLift<'a>,
    },
    /// A canonical function that is defined in terms of lowering a component function.
    Lower(CanonLower<'a>),

    ResourceNew(CanonResourceNew<'a>),
    ResourceDrop(CanonResourceDrop<'a>),
    ResourceRep(CanonResourceRep<'a>),
}

/// Information relating to lifting a core function.
#[derive(Debug)]
pub struct CanonLift<'a> {
    /// The core function being lifted.
    pub func: CoreItemRef<'a, kw::func>,
    /// The canonical options for the lifting.
    pub opts: Vec<CanonOpt<'a>>,
}

impl<'a> Parse<'a> for CanonLift<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::lift>()?;

        Ok(Self {
            func: parser.parens(|parser| {
                parser.parse::<kw::core>()?;
                parser.parse()
            })?,
            opts: parser.parse()?,
        })
    }
}

impl Default for CanonLift<'_> {
    fn default() -> Self {
        let span = Span::from_offset(0);
        Self {
            func: CoreItemRef {
                kind: kw::func(span),
                idx: Index::Num(0, span),
                export_name: None,
            },
            opts: Vec::new(),
        }
    }
}

/// Information relating to lowering a component function.
#[derive(Debug)]
pub struct CanonLower<'a> {
    /// The function being lowered.
    pub func: ItemRef<'a, kw::func>,
    /// The canonical options for the lowering.
    pub opts: Vec<CanonOpt<'a>>,
}

impl<'a> Parse<'a> for CanonLower<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::lower>()?;

        Ok(Self {
            func: parser.parens(|parser| parser.parse())?,
            opts: parser.parse()?,
        })
    }
}

impl Default for CanonLower<'_> {
    fn default() -> Self {
        let span = Span::from_offset(0);
        Self {
            func: ItemRef {
                kind: kw::func(span),
                idx: Index::Num(0, span),
                export_names: Vec::new(),
            },
            opts: Vec::new(),
        }
    }
}

/// Information relating to the `resource.new` intrinsic.
#[derive(Debug)]
pub struct CanonResourceNew<'a> {
    /// The resource type that this intrinsic creates an owned reference to.
    pub ty: Index<'a>,
}

impl<'a> Parse<'a> for CanonResourceNew<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::resource_new>()?;

        Ok(Self {
            ty: parser.parse()?,
        })
    }
}

impl Default for CanonResourceNew<'_> {
    fn default() -> Self {
        CanonResourceNew {
            ty: Index::Num(0, Span::from_offset(0)),
        }
    }
}

/// Information relating to the `resource.drop` intrinsic.
#[derive(Debug)]
pub struct CanonResourceDrop<'a> {
    /// The resource type that this intrinsic is dropping.
    pub ty: Index<'a>,
}

impl<'a> Parse<'a> for CanonResourceDrop<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::resource_drop>()?;

        Ok(Self {
            ty: parser.parse()?,
        })
    }
}

impl Default for CanonResourceDrop<'_> {
    fn default() -> Self {
        CanonResourceDrop {
            ty: Index::Num(0, Span::from_offset(0)),
        }
    }
}

/// Information relating to the `resource.rep` intrinsic.
#[derive(Debug)]
pub struct CanonResourceRep<'a> {
    /// The resource type that this intrinsic is accessing.
    pub ty: Index<'a>,
}

impl<'a> Parse<'a> for CanonResourceRep<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::resource_rep>()?;

        Ok(Self {
            ty: parser.parse()?,
        })
    }
}

impl Default for CanonResourceRep<'_> {
    fn default() -> Self {
        CanonResourceRep {
            ty: Index::Num(0, Span::from_offset(0)),
        }
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
    /// Use the specified memory for canonical ABI memory access.
    Memory(CoreItemRef<'a, kw::memory>),
    /// Use the specified reallocation function for memory allocations.
    Realloc(CoreItemRef<'a, kw::func>),
    /// Call the specified function after the lifted function has returned.
    PostReturn(CoreItemRef<'a, kw::func>),
}

impl<'a> Parse<'a> for CanonOpt<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::string_utf8>()? {
            parser.parse::<kw::string_utf8>()?;
            Ok(Self::StringUtf8)
        } else if l.peek::<kw::string_utf16>()? {
            parser.parse::<kw::string_utf16>()?;
            Ok(Self::StringUtf16)
        } else if l.peek::<kw::string_latin1_utf16>()? {
            parser.parse::<kw::string_latin1_utf16>()?;
            Ok(Self::StringLatin1Utf16)
        } else if l.peek::<LParen>()? {
            parser.parens(|parser| {
                let mut l = parser.lookahead1();
                if l.peek::<kw::memory>()? {
                    let span = parser.parse::<kw::memory>()?.0;
                    Ok(CanonOpt::Memory(parse_trailing_item_ref(
                        kw::memory(span),
                        parser,
                    )?))
                } else if l.peek::<kw::realloc>()? {
                    parser.parse::<kw::realloc>()?;
                    Ok(CanonOpt::Realloc(
                        parser.parse::<IndexOrCoreRef<'_, _>>()?.0,
                    ))
                } else if l.peek::<kw::post_return>()? {
                    parser.parse::<kw::post_return>()?;
                    Ok(CanonOpt::PostReturn(
                        parser.parse::<IndexOrCoreRef<'_, _>>()?.0,
                    ))
                } else {
                    Err(l.error())
                }
            })
        } else {
            Err(l.error())
        }
    }
}

fn parse_trailing_item_ref<T>(kind: T, parser: Parser) -> Result<CoreItemRef<T>> {
    Ok(CoreItemRef {
        kind,
        idx: parser.parse()?,
        export_name: parser.parse()?,
    })
}

impl<'a> Parse<'a> for Vec<CanonOpt<'a>> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut funcs = Vec::new();
        while !parser.is_empty() {
            funcs.push(parser.parse()?);
        }
        Ok(funcs)
    }
}
