use crate::component::*;
use crate::core;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, LParen, NameAnnotation, Span};

/// A core instance defined by instantiation or exporting core items.
#[derive(Debug)]
pub struct CoreInstance<'a> {
    /// Where this `core instance` was defined.
    pub span: Span,
    /// An identifier that this instance is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this instance stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// What kind of instance this is.
    pub kind: CoreInstanceKind<'a>,
}

impl<'a> Parse<'a> for CoreInstance<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::core>()?.0;
        parser.parse::<kw::instance>()?;
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

/// The kinds of core instances in the text format.
#[derive(Debug)]
pub enum CoreInstanceKind<'a> {
    /// Instantiate a core module.
    Instantiate {
        /// The module being instantiated.
        module: ItemRef<'a, kw::module>,
        /// Arguments used to instantiate the instance.
        args: Vec<CoreInstantiationArg<'a>>,
    },
    /// The instance is defined by exporting local items as an instance.
    BundleOfExports(Vec<CoreInstanceExport<'a>>),
}

impl<'a> Parse<'a> for CoreInstanceKind<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<LParen>() && parser.peek2::<kw::instantiate>() {
            parser.parens(|parser| {
                parser.parse::<kw::instantiate>()?;
                Ok(Self::Instantiate {
                    module: parser.parse::<IndexOrRef<'_, _>>()?.0,
                    args: parser.parse()?,
                })
            })
        } else {
            Ok(Self::BundleOfExports(parser.parse()?))
        }
    }
}

impl Default for kw::module {
    fn default() -> kw::module {
        kw::module(Span::from_offset(0))
    }
}

/// An argument to instantiate a core module.
#[derive(Debug)]
pub struct CoreInstantiationArg<'a> {
    /// The name of the instantiation argument.
    pub name: &'a str,
    /// The kind of core instantiation argument.
    pub kind: CoreInstantiationArgKind<'a>,
}

impl<'a> Parse<'a> for CoreInstantiationArg<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::with>()?;
        Ok(Self {
            name: parser.parse()?,
            kind: parser.parse()?,
        })
    }
}

impl<'a> Parse<'a> for Vec<CoreInstantiationArg<'a>> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut args = Vec::new();
        while !parser.is_empty() {
            args.push(parser.parens(|parser| parser.parse())?);
        }
        Ok(args)
    }
}

/// The kind of core instantiation argument.
#[derive(Debug)]
pub enum CoreInstantiationArgKind<'a> {
    /// The argument is a reference to an instance.
    Instance(CoreItemRef<'a, kw::instance>),
    /// The argument is an instance created from local exported core items.
    ///
    /// This is syntactic sugar for defining a core instance and also using it
    /// as an instantiation argument.
    BundleOfExports(Span, Vec<CoreInstanceExport<'a>>),
}

impl<'a> Parse<'a> for CoreInstantiationArgKind<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parens(|parser| {
            if let Some(r) = parser.parse()? {
                Ok(Self::Instance(r))
            } else {
                let span = parser.parse::<kw::instance>()?.0;
                Ok(Self::BundleOfExports(span, parser.parse()?))
            }
        })
    }
}

/// An exported item as part of a core instance.
#[derive(Debug)]
pub struct CoreInstanceExport<'a> {
    /// Where this export was defined.
    pub span: Span,
    /// The name of this export from the instance.
    pub name: &'a str,
    /// What's being exported from the instance.
    pub item: CoreItemRef<'a, core::ExportKind>,
}

impl<'a> Parse<'a> for CoreInstanceExport<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        Ok(Self {
            span: parser.parse::<kw::export>()?.0,
            name: parser.parse()?,
            item: parser.parens(|parser| parser.parse())?,
        })
    }
}

impl<'a> Parse<'a> for Vec<CoreInstanceExport<'a>> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut exports = Vec::new();
        while !parser.is_empty() {
            exports.push(parser.parens(|parser| parser.parse())?);
        }
        Ok(exports)
    }
}

/// A component instance defined by instantiation or exporting items.
#[derive(Debug)]
pub struct Instance<'a> {
    /// Where this `instance` was defined.
    pub span: Span,
    /// An identifier that this instance is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this instance stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: core::InlineExport<'a>,
    /// What kind of instance this is.
    pub kind: InstanceKind<'a>,
}

impl<'a> Parse<'a> for Instance<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::instance>()?.0;
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

/// The kinds of instances in the text format.
#[derive(Debug)]
pub enum InstanceKind<'a> {
    /// The `(instance (import "x"))` sugar syntax
    Import {
        /// The name of the import
        import: InlineImport<'a>,
        /// The type of the instance being imported
        ty: ComponentTypeUse<'a, InstanceType<'a>>,
    },
    /// Instantiate a component.
    Instantiate {
        /// The component being instantiated.
        component: ItemRef<'a, kw::component>,
        /// Arguments used to instantiate the instance.
        args: Vec<InstantiationArg<'a>>,
    },
    /// The instance is defined by exporting local items as an instance.
    BundleOfExports(Vec<ComponentExport<'a>>),
}

impl<'a> Parse<'a> for InstanceKind<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if let Some(import) = parser.parse()? {
            return Ok(Self::Import {
                import,
                ty: parser.parse()?,
            });
        }

        if parser.peek::<LParen>() && parser.peek2::<kw::instantiate>() {
            parser.parens(|parser| {
                parser.parse::<kw::instantiate>()?;
                Ok(Self::Instantiate {
                    component: parser.parse::<IndexOrRef<'_, _>>()?.0,
                    args: parser.parse()?,
                })
            })
        } else {
            Ok(Self::BundleOfExports(parser.parse()?))
        }
    }
}

impl Default for kw::component {
    fn default() -> kw::component {
        kw::component(Span::from_offset(0))
    }
}

/// An argument to instantiate a component.
#[derive(Debug)]
pub struct InstantiationArg<'a> {
    /// The name of the instantiation argument.
    pub name: &'a str,
    /// The kind of instantiation argument.
    pub kind: InstantiationArgKind<'a>,
}

impl<'a> Parse<'a> for InstantiationArg<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::with>()?;
        Ok(Self {
            name: parser.parse()?,
            kind: parser.parse()?,
        })
    }
}

impl<'a> Parse<'a> for Vec<InstantiationArg<'a>> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut args = Vec::new();
        while !parser.is_empty() {
            args.push(parser.parens(|parser| parser.parse())?);
        }
        Ok(args)
    }
}

/// The kind of instantiation argument.
#[derive(Debug)]
pub enum InstantiationArgKind<'a> {
    /// The argument is a reference to a component item.
    Item(ComponentExportKind<'a>),
    /// The argument is an instance created from local exported items.
    ///
    /// This is syntactic sugar for defining an instance and also using it
    /// as an instantiation argument.
    BundleOfExports(Span, Vec<ComponentExport<'a>>),
}

impl<'a> Parse<'a> for InstantiationArgKind<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if let Some(item) = parser.parse()? {
            Ok(Self::Item(item))
        } else {
            parser.parens(|parser| {
                let span = parser.parse::<kw::instance>()?.0;
                Ok(Self::BundleOfExports(span, parser.parse()?))
            })
        }
    }
}
