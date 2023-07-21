use crate::core::ExportKind;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, Index, NameAnnotation, Span};

/// A inline alias for component exported items.
///
/// Handles both `core export` and `export` aliases
#[derive(Debug)]
pub struct InlineExportAlias<'a, const CORE: bool> {
    /// The instance to alias the export from.
    pub instance: Index<'a>,
    /// The name of the export to alias.
    pub name: &'a str,
}

impl<'a, const CORE: bool> Parse<'a> for InlineExportAlias<'a, CORE> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::alias>()?;
        if CORE {
            parser.parse::<kw::core>()?;
        }
        parser.parse::<kw::export>()?;
        let instance = parser.parse()?;
        let name = parser.parse()?;
        Ok(Self { instance, name })
    }
}

/// An alias to a component item.
#[derive(Debug)]
pub struct Alias<'a> {
    /// Where this `alias` was defined.
    pub span: Span,
    /// An identifier that this alias is resolved with (optionally) for name
    /// resolution.
    pub id: Option<Id<'a>>,
    /// An optional name for this alias stored in the custom `name` section.
    pub name: Option<NameAnnotation<'a>>,
    /// The target of this alias.
    pub target: AliasTarget<'a>,
}

impl<'a> Alias<'a> {
    /// Parses only an outer type alias.
    pub fn parse_outer_core_type_alias(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::alias>()?.0;
        parser.parse::<kw::outer>()?;
        let outer = parser.parse()?;
        let index = parser.parse()?;

        let (kind, id, name) = parser.parens(|parser| {
            let mut kind: ComponentOuterAliasKind = parser.parse()?;
            match kind {
                ComponentOuterAliasKind::CoreType => {
                    return Err(parser.error("expected type for outer alias"))
                }
                ComponentOuterAliasKind::Type => {
                    kind = ComponentOuterAliasKind::CoreType;
                }
                _ => return Err(parser.error("expected core type or type for outer alias")),
            }

            Ok((kind, parser.parse()?, parser.parse()?))
        })?;

        Ok(Self {
            span,
            target: AliasTarget::Outer { outer, index, kind },
            id,
            name,
        })
    }
}

impl<'a> Parse<'a> for Alias<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::alias>()?.0;

        let mut l = parser.lookahead1();

        let (target, id, name) = if l.peek::<kw::outer>()? {
            parser.parse::<kw::outer>()?;
            let outer = parser.parse()?;
            let index = parser.parse()?;
            let (kind, id, name) =
                parser.parens(|parser| Ok((parser.parse()?, parser.parse()?, parser.parse()?)))?;

            (AliasTarget::Outer { outer, index, kind }, id, name)
        } else if l.peek::<kw::export>()? {
            parser.parse::<kw::export>()?;
            let instance = parser.parse()?;
            let export_name = parser.parse()?;
            let (kind, id, name) =
                parser.parens(|parser| Ok((parser.parse()?, parser.parse()?, parser.parse()?)))?;

            (
                AliasTarget::Export {
                    instance,
                    name: export_name,
                    kind,
                },
                id,
                name,
            )
        } else if l.peek::<kw::core>()? {
            parser.parse::<kw::core>()?;
            parser.parse::<kw::export>()?;
            let instance = parser.parse()?;
            let export_name = parser.parse()?;
            let (kind, id, name) = parser.parens(|parser| {
                parser.parse::<kw::core>()?;
                Ok((parser.parse()?, parser.parse()?, parser.parse()?))
            })?;

            (
                AliasTarget::CoreExport {
                    instance,
                    name: export_name,
                    kind,
                },
                id,
                name,
            )
        } else {
            return Err(l.error());
        };

        Ok(Self {
            span,
            target,
            id,
            name,
        })
    }
}

/// Represents the kind of instance export alias.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum ComponentExportAliasKind {
    /// The alias is to a core module export.
    CoreModule,
    /// The alias is to a function export.
    Func,
    /// The alias is to a value export.
    Value,
    /// The alias is to a type export.
    Type,
    /// The alias is to a component export.
    Component,
    /// The alias is to an instance export.
    Instance,
}

impl<'a> Parse<'a> for ComponentExportAliasKind {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::core>()? {
            parser.parse::<kw::core>()?;
            let mut l = parser.lookahead1();
            if l.peek::<kw::module>()? {
                parser.parse::<kw::module>()?;
                Ok(Self::CoreModule)
            } else {
                Err(l.error())
            }
        } else if l.peek::<kw::func>()? {
            parser.parse::<kw::func>()?;
            Ok(Self::Func)
        } else if l.peek::<kw::value>()? {
            parser.parse::<kw::value>()?;
            Ok(Self::Value)
        } else if l.peek::<kw::r#type>()? {
            parser.parse::<kw::r#type>()?;
            Ok(Self::Type)
        } else if l.peek::<kw::component>()? {
            parser.parse::<kw::component>()?;
            Ok(Self::Component)
        } else if l.peek::<kw::instance>()? {
            parser.parse::<kw::instance>()?;
            Ok(Self::Instance)
        } else {
            Err(l.error())
        }
    }
}

/// Represents the kind of outer alias.
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum ComponentOuterAliasKind {
    /// The alias is to an outer core module.
    CoreModule,
    /// The alias is to an outer core type.
    CoreType,
    /// The alias is to an outer type.
    Type,
    /// The alias is to an outer component.
    Component,
}

impl<'a> Parse<'a> for ComponentOuterAliasKind {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let mut l = parser.lookahead1();
        if l.peek::<kw::core>()? {
            parser.parse::<kw::core>()?;
            let mut l = parser.lookahead1();
            if l.peek::<kw::module>()? {
                parser.parse::<kw::module>()?;
                Ok(Self::CoreModule)
            } else if l.peek::<kw::r#type>()? {
                parser.parse::<kw::r#type>()?;
                Ok(Self::CoreType)
            } else {
                Err(l.error())
            }
        } else if l.peek::<kw::r#type>()? {
            parser.parse::<kw::r#type>()?;
            Ok(Self::Type)
        } else if l.peek::<kw::component>()? {
            parser.parse::<kw::component>()?;
            Ok(Self::Component)
        } else {
            Err(l.error())
        }
    }
}

/// The target of a component alias.
#[derive(Debug)]
pub enum AliasTarget<'a> {
    /// The alias is to an export of a component instance.
    Export {
        /// The component instance exporting the item.
        instance: Index<'a>,
        /// The name of the exported item to alias.
        name: &'a str,
        /// The export kind of the alias.
        kind: ComponentExportAliasKind,
    },
    /// The alias is to an export of a module instance.
    CoreExport {
        /// The module instance exporting the item.
        instance: Index<'a>,
        /// The name of the exported item to alias.
        name: &'a str,
        /// The export kind of the alias.
        kind: ExportKind,
    },
    /// The alias is to an item from an outer component.
    Outer {
        /// The number of enclosing components to skip.
        outer: Index<'a>,
        /// The index of the item being aliased.
        index: Index<'a>,
        /// The outer alias kind.
        kind: ComponentOuterAliasKind,
    },
}
