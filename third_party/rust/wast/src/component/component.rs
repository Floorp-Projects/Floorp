use crate::component::*;
use crate::core;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::{Id, NameAnnotation, Span};

/// A parsed WebAssembly component module.
#[derive(Debug)]
pub struct Component<'a> {
    /// Where this `component` was defined
    pub span: Span,
    /// An optional identifier this component is known by
    pub id: Option<Id<'a>>,
    /// An optional `@name` annotation for this component
    pub name: Option<NameAnnotation<'a>>,
    /// What kind of component this was parsed as.
    pub kind: ComponentKind<'a>,
}

/// The different kinds of ways to define a component.
#[derive(Debug)]
pub enum ComponentKind<'a> {
    /// A component defined in the textual s-expression format.
    Text(Vec<ComponentField<'a>>),
    /// A component that had its raw binary bytes defined via the `binary`
    /// directive.
    Binary(Vec<&'a [u8]>),
}

impl<'a> Component<'a> {
    /// Performs a name resolution pass on this [`Component`], resolving all
    /// symbolic names to indices.
    ///
    /// The WAT format contains a number of shorthands to make it easier to
    /// write, such as inline exports, inline imports, inline type definitions,
    /// etc. Additionally it allows using symbolic names such as `$foo` instead
    /// of using indices. This module will postprocess an AST to remove all of
    /// this syntactic sugar, preparing the AST for binary emission.  This is
    /// where expansion and name resolution happens.
    ///
    /// This function will mutate the AST of this [`Component`] and replace all
    /// [`Index`](crate::token::Index) arguments with `Index::Num`. This will
    /// also expand inline exports/imports listed on fields and handle various
    /// other shorthands of the text format.
    ///
    /// If successful the AST was modified to be ready for binary encoding.
    ///
    /// # Errors
    ///
    /// If an error happens during resolution, such a name resolution error or
    /// items are found in the wrong order, then an error is returned.
    pub fn resolve(&mut self) -> std::result::Result<(), crate::Error> {
        match &mut self.kind {
            ComponentKind::Text(fields) => {
                crate::component::expand::expand(fields);
            }
            ComponentKind::Binary(_) => {}
        }
        crate::component::resolve::resolve(self)
    }

    /// Encodes this [`Component`] to its binary form.
    ///
    /// This function will take the textual representation in [`Component`] and
    /// perform all steps necessary to convert it to a binary WebAssembly
    /// component, suitable for writing to a `*.wasm` file. This function may
    /// internally modify the [`Component`], for example:
    ///
    /// * Name resolution is performed to ensure that `Index::Id` isn't present
    ///   anywhere in the AST.
    ///
    /// * Inline shorthands such as imports/exports/types are all expanded to be
    ///   dedicated fields of the component.
    ///
    /// * Component fields may be shuffled around to preserve index ordering from
    ///   expansions.
    ///
    /// After all of this expansion has happened the component will be converted to
    /// its binary form and returned as a `Vec<u8>`. This is then suitable to
    /// hand off to other wasm runtimes and such.
    ///
    /// # Errors
    ///
    /// This function can return an error for name resolution errors and other
    /// expansion-related errors.
    pub fn encode(&mut self) -> std::result::Result<Vec<u8>, crate::Error> {
        self.resolve()?;
        Ok(crate::component::binary::encode(self))
    }

    pub(crate) fn validate(&self, parser: Parser<'_>) -> Result<()> {
        let mut starts = 0;
        if let ComponentKind::Text(fields) = &self.kind {
            for item in fields.iter() {
                if let ComponentField::Start(_) = item {
                    starts += 1;
                }
            }
        }
        if starts > 1 {
            return Err(parser.error("multiple start sections found"));
        }
        Ok(())
    }
}

impl<'a> Parse<'a> for Component<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let _r = parser.register_annotation("custom");

        let span = parser.parse::<kw::component>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;

        let kind = if parser.peek::<kw::binary>() {
            parser.parse::<kw::binary>()?;
            let mut data = Vec::new();
            while !parser.is_empty() {
                data.push(parser.parse()?);
            }
            ComponentKind::Binary(data)
        } else {
            ComponentKind::Text(ComponentField::parse_remaining(parser)?)
        };
        Ok(Component {
            span,
            id,
            name,
            kind,
        })
    }
}

/// A listing of all possible fields that can make up a WebAssembly component.
#[allow(missing_docs)]
#[derive(Debug)]
pub enum ComponentField<'a> {
    Type(TypeField<'a>),
    Import(ComponentImport<'a>),
    Func(ComponentFunc<'a>),
    Export(ComponentExport<'a>),
    Start(Start<'a>),
    Instance(Instance<'a>),
    Module(Module<'a>),
    Component(NestedComponent<'a>),
    Alias(Alias<'a>),
}

impl<'a> ComponentField<'a> {
    fn parse_remaining(parser: Parser<'a>) -> Result<Vec<ComponentField>> {
        let mut fields = Vec::new();
        while !parser.is_empty() {
            fields.push(parser.parens(ComponentField::parse)?);
        }
        Ok(fields)
    }
}

impl<'a> Parse<'a> for ComponentField<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        if parser.peek::<kw::r#type>() {
            return Ok(ComponentField::Type(parser.parse()?));
        }
        if parser.peek::<kw::import>() {
            return Ok(ComponentField::Import(parser.parse()?));
        }
        if parser.peek::<kw::func>() {
            return Ok(ComponentField::Func(parser.parse()?));
        }
        if parser.peek::<kw::export>() {
            return Ok(ComponentField::Export(parser.parse()?));
        }
        if parser.peek::<kw::start>() {
            return Ok(ComponentField::Start(parser.parse()?));
        }
        if parser.peek::<kw::instance>() {
            return Ok(ComponentField::Instance(parser.parse()?));
        }
        if parser.peek::<kw::module>() {
            return Ok(ComponentField::Module(parser.parse()?));
        }
        if parser.peek::<kw::component>() {
            return Ok(ComponentField::Component(parser.parse()?));
        }
        if parser.peek::<kw::alias>() {
            return Ok(ComponentField::Alias(parser.parse()?));
        }
        Err(parser.error("expected valid component field"))
    }
}

impl<'a> From<TypeField<'a>> for ComponentField<'a> {
    fn from(field: TypeField<'a>) -> ComponentField<'a> {
        ComponentField::Type(field)
    }
}

impl<'a> From<Alias<'a>> for ComponentField<'a> {
    fn from(field: Alias<'a>) -> ComponentField<'a> {
        ComponentField::Alias(field)
    }
}

/// A function to call at instantiation time.
#[derive(Debug)]
pub struct Start<'a> {
    /// The function to call.
    pub func: ItemRef<'a, kw::func>,
    /// The arguments to pass to the function.
    pub args: Vec<ItemRef<'a, kw::value>>,
    /// Name of the result value.
    pub result: Option<Id<'a>>,
}

impl<'a> Parse<'a> for Start<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::start>()?;
        let func = parser.parse::<IndexOrRef<_>>()?.0;
        let mut args = Vec::new();
        while !parser.is_empty() && !parser.peek2::<kw::result>() {
            args.push(parser.parse()?);
        }
        let result = if !parser.is_empty() {
            parser.parens(|parser| {
                parser.parse::<kw::result>()?;
                if !parser.is_empty() {
                    parser.parens(|parser| {
                        parser.parse::<kw::value>()?;
                        let id = parser.parse()?;
                        Ok(Some(id))
                    })
                } else {
                    Ok(None)
                }
            })?
        } else {
            None
        };
        Ok(Start { func, args, result })
    }
}

/// A parsed WebAssembly component module.
#[derive(Debug)]
pub struct NestedComponent<'a> {
    /// Where this `component` was defined
    pub span: Span,
    /// An optional identifier this component is known by
    pub id: Option<Id<'a>>,
    /// An optional `@name` annotation for this component
    pub name: Option<NameAnnotation<'a>>,
    /// If present, inline export annotations which indicate names this
    /// definition should be exported under.
    pub exports: core::InlineExport<'a>,
    /// What kind of component this was parsed as.
    pub kind: NestedComponentKind<'a>,
}

/// The different kinds of ways to define a nested component.
#[derive(Debug)]
pub enum NestedComponentKind<'a> {
    /// This is actually an inline import of a component
    Import {
        /// The information about where this is being imported from.
        import: InlineImport<'a>,
        /// The type of component being imported.
        ty: ComponentTypeUse<'a, ComponentType<'a>>,
    },
    /// The component is defined inline as a local definition with its fields
    /// listed here.
    Inline(Vec<ComponentField<'a>>),
}

impl<'a> Parse<'a> for NestedComponent<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        let span = parser.parse::<kw::component>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;
        let exports = parser.parse()?;

        let kind = if let Some(import) = parser.parse()? {
            NestedComponentKind::Import {
                import,
                ty: parser.parse()?,
            }
        } else {
            let mut fields = Vec::new();
            while !parser.is_empty() {
                fields.push(parser.parens(|p| p.parse())?);
            }
            NestedComponentKind::Inline(fields)
        };

        Ok(NestedComponent {
            span,
            id,
            name,
            exports,
            kind,
        })
    }
}
