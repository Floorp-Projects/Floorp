use crate::annotation;
use crate::component::*;
use crate::core::Producers;
use crate::kw;
use crate::parser::{Parse, Parser, Result};
use crate::token::Index;
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
        let _r = parser.register_annotation("producers");
        let _r = parser.register_annotation("name");

        let span = parser.parse::<kw::component>()?.0;
        let id = parser.parse()?;
        let name = parser.parse()?;

        let kind = if parser.peek::<kw::binary>()? {
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
    CoreModule(CoreModule<'a>),
    CoreInstance(CoreInstance<'a>),
    CoreType(CoreType<'a>),
    Component(NestedComponent<'a>),
    Instance(Instance<'a>),
    Alias(Alias<'a>),
    Type(Type<'a>),
    CanonicalFunc(CanonicalFunc<'a>),
    CoreFunc(CoreFunc<'a>), // Supports inverted forms of other items
    Func(Func<'a>),         // Supports inverted forms of other items
    Start(Start<'a>),
    Import(ComponentImport<'a>),
    Export(ComponentExport<'a>),
    Custom(Custom<'a>),
    Producers(Producers<'a>),
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
        if parser.peek::<kw::core>()? {
            if parser.peek2::<kw::module>()? {
                return Ok(Self::CoreModule(parser.parse()?));
            }
            if parser.peek2::<kw::instance>()? {
                return Ok(Self::CoreInstance(parser.parse()?));
            }
            if parser.peek2::<kw::r#type>()? {
                return Ok(Self::CoreType(parser.parse()?));
            }
            if parser.peek2::<kw::func>()? {
                return Ok(Self::CoreFunc(parser.parse()?));
            }
        } else {
            if parser.peek::<kw::component>()? {
                return Ok(Self::Component(parser.parse()?));
            }
            if parser.peek::<kw::instance>()? {
                return Ok(Self::Instance(parser.parse()?));
            }
            if parser.peek::<kw::alias>()? {
                return Ok(Self::Alias(parser.parse()?));
            }
            if parser.peek::<kw::r#type>()? {
                return Ok(Self::Type(Type::parse_maybe_with_inline_exports(parser)?));
            }
            if parser.peek::<kw::import>()? {
                return Ok(Self::Import(parser.parse()?));
            }
            if parser.peek::<kw::func>()? {
                return Ok(Self::Func(parser.parse()?));
            }
            if parser.peek::<kw::export>()? {
                return Ok(Self::Export(parser.parse()?));
            }
            if parser.peek::<kw::start>()? {
                return Ok(Self::Start(parser.parse()?));
            }
            if parser.peek::<annotation::custom>()? {
                return Ok(Self::Custom(parser.parse()?));
            }
            if parser.peek::<annotation::producers>()? {
                return Ok(Self::Producers(parser.parse()?));
            }
        }
        Err(parser.error("expected valid component field"))
    }
}

/// A function to call at instantiation time.
#[derive(Debug)]
pub struct Start<'a> {
    /// The function to call.
    pub func: Index<'a>,
    /// The arguments to pass to the function.
    pub args: Vec<ItemRef<'a, kw::value>>,
    /// Names of the result values.
    pub results: Vec<Option<Id<'a>>>,
}

impl<'a> Parse<'a> for Start<'a> {
    fn parse(parser: Parser<'a>) -> Result<Self> {
        parser.parse::<kw::start>()?;
        let func = parser.parse()?;
        let mut args = Vec::new();
        while !parser.is_empty() && !parser.peek2::<kw::result>()? {
            args.push(parser.parens(|parser| parser.parse())?);
        }

        let mut results = Vec::new();
        while !parser.is_empty() && parser.peek2::<kw::result>()? {
            results.push(parser.parens(|parser| {
                parser.parse::<kw::result>()?;
                parser.parens(|parser| {
                    parser.parse::<kw::value>()?;
                    parser.parse()
                })
            })?);
        }

        Ok(Start {
            func,
            args,
            results,
        })
    }
}

/// A nested WebAssembly component.
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
    pub exports: InlineExport<'a>,
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
        parser.depth_check()?;

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
