use spec::{self, Laziness, SpecBuilder, TypeSpec, TypeSum};
use weedle::common::Identifier;
use weedle::types::*;
use weedle::*;

fn nullable<T: std::fmt::Debug>(src: &MayBeNull<T>, dst: TypeSpec) -> spec::Type {
    if src.q_mark.is_some() {
        dst.optional()
            .unwrap_or_else(|| panic!("This type could not be made optional {:?}", src.type_))
    } else {
        dst.required()
    }
}

pub struct Importer {
    builder: SpecBuilder,
    /// The interfaces we have traversed so far.
    path: Vec<String>,
}

impl Importer {
    /// Import a WebIDL spec into a SpecBuilder.
    ///
    /// A WebIDL spec may consist in several files. Files are parsed in the order
    /// of `sources`. An extension file (e.g. `es6-extended.webidl`) MUST appear
    /// after the files it extends.
    ///
    /// ```
    /// extern crate binjs_meta;
    /// use binjs_meta::spec::SpecOptions;
    ///
    /// let mut builder = binjs_meta::import::Importer::import(vec!["
    ///    interface FooContents {
    ///      attribute boolean value;
    ///    };
    ///    interface LazyFoo {
    ///       [Lazy] attribute FooContents contents;
    ///    };
    ///    interface EagerFoo {
    ///       attribute FooContents contents;
    ///    };
    /// "].into_iter()).expect("Could not parse");
    ///
    /// let fake_root = builder.node_name("@@ROOT@@"); // Unused
    /// let null = builder.node_name(""); // Used
    /// let spec = builder.into_spec(SpecOptions {
    ///     root: &fake_root,
    ///     null: &null,
    /// });
    ///
    /// let name_eager = spec.get_node_name("EagerFoo")
    ///     .expect("Missing name EagerFoo");
    /// let name_lazy = spec.get_node_name("LazyFoo")
    ///     .expect("Missing name LazyFoo");
    /// let name_contents = spec.get_field_name("contents")
    ///     .expect("Missing name contents");
    ///
    /// {
    ///     let interface_eager = spec.get_interface_by_name(&name_eager)
    ///         .expect("Missing interface EagerFoo");
    ///     let contents_field =
    ///         interface_eager.get_field_by_name(&name_contents)
    ///         .expect("Missing field contents");
    ///     assert_eq!(contents_field.is_lazy(), false);
    /// }
    ///
    /// {
    ///     let interface_lazy = spec.get_interface_by_name(&name_lazy)
    ///         .expect("Missing interface LazyFoo");
    ///     let contents_field =
    ///         interface_lazy.get_field_by_name(&name_contents)
    ///         .expect("Missing field contents");
    ///     assert_eq!(contents_field.is_lazy(), true);
    /// }
    /// ```
    pub fn import<'a>(
        sources: impl IntoIterator<Item = &'a str>,
    ) -> Result<SpecBuilder, weedle::Err<CompleteStr<'a>>> {
        let mut importer = Importer {
            path: Vec::with_capacity(256),
            builder: SpecBuilder::new(),
        };
        for source in sources {
            let ast = weedle::parse(source)?;
            importer.import_all_definitions(&ast);
        }
        Ok(importer.builder)
    }

    fn import_all_definitions(&mut self, ast: &Definitions) {
        for definition in ast {
            self.import_definition(&definition)
        }
    }

    fn import_definition(&mut self, def: &Definition) {
        match *def {
            Definition::Enum(ref enum_) => self.import_enum(enum_),
            Definition::Typedef(ref typedef) => self.import_typedef(typedef),
            Definition::Interface(ref interface) => self.import_interface(interface),
            _ => panic!("Not implemented: importing {:?}", def),
        }
    }

    fn import_enum(&mut self, enum_: &EnumDefinition) {
        let name = self.builder.node_name(enum_.identifier.0);
        let mut node = self
            .builder
            .add_string_enum(&name)
            .expect("Name already present");
        for variant in &enum_.values.body.list {
            node.with_string(&variant.0);
        }
    }

    fn import_typedef(&mut self, typedef: &TypedefDefinition) {
        let name = self.builder.node_name(typedef.identifier.0);
        // The following are, unfortunately, not true typedefs.
        // Ignore their definition.
        let type_ = match typedef.identifier.0 {
            "Identifier" => TypeSpec::IdentifierName.required(),
            "IdentifierName" => TypeSpec::IdentifierName.required(),
            "PropertyKey" => TypeSpec::PropertyKey.required(),
            _ => self.convert_type(&typedef.type_.type_),
        };
        debug!(target: "meta::import", "Importing typedef {type_:?} {name:?}",
            type_ = type_,
            name = name);
        let mut node = self.builder.add_typedef(&name).unwrap_or_else(|| {
            panic!(
                "Error: Name {} is defined more than once in the spec.",
                name
            )
        });
        assert!(!type_.is_optional());
        node.with_spec(type_.spec);
    }

    fn import_interface(&mut self, interface: &InterfaceDefinition) {
        // Handle special, hardcoded, interfaces.
        match interface.identifier.0 {
            "Node" => {
                // We're not interested in the root interface.
                return;
            }
            "IdentifierName" => unimplemented!(),
            _ => {}
        }
        if let Some(ref parent) = interface.inheritance {
            assert_eq!(parent.identifier.0, "Node");
        }

        self.path.push(interface.identifier.0.to_owned());

        // Now handle regular stuff.
        let mut fields = Vec::new();
        for member in &interface.members.body {
            if let interface::InterfaceMember::Attribute(interface::AttributeInterfaceMember {
                modifier: None,
                attributes,
                identifier,
                type_,
                ..
            }) = member
            {
                let name = self.builder.field_name(identifier.0);
                let type_ = self.convert_type(&type_.type_);

                let is_lazy = attributes
                    .iter()
                    .flat_map(|attribute| &attribute.body.list)
                    .find(|attribute| match attribute {
                        attribute::ExtendedAttribute::NoArgs(
                            attribute::ExtendedAttributeNoArgs(Identifier("Lazy")),
                        ) => true,
                        _ => false,
                    })
                    .is_some();

                fields.push((
                    name,
                    type_,
                    if is_lazy {
                        Laziness::Lazy
                    } else {
                        Laziness::Eager
                    },
                ));
            } else {
                panic!("Expected an attribute, got {:?}", member);
            }
        }
        let name = self.builder.node_name(interface.identifier.0);

        // Set to `Some("Foo")` if this interface has attribute
        // `[ExtendsTypeSum=Foo]`.
        let mut extends_type_sum = None;
        let mut scoped_dictionary = None;
        {
            let mut node = self
                .builder
                .add_interface(&name)
                .expect("Name already present");
            for (field_name, field_type, laziness) in fields.drain(..) {
                node.with_field_laziness(&field_name, field_type, laziness);
            }

            for attribute in interface
                .attributes
                .iter()
                .flat_map(|attribute| &attribute.body.list)
            {
                use weedle::attribute::ExtendedAttribute::*;
                use weedle::attribute::*;
                match *attribute {
                    NoArgs(ExtendedAttributeNoArgs(Identifier("Skippable"))) => {
                        panic!("Encountered deprecated attribute [Skippable]");
                    }
                    NoArgs(ExtendedAttributeNoArgs(Identifier("Scope"))) => {
                        node.with_scope(true);
                    }
                    Ident(ExtendedAttributeIdent {
                        lhs_identifier: Identifier("ExtendsTypeSum"),
                        assign: _,
                        rhs: IdentifierOrString::Identifier(ref rhs),
                    }) => {
                        assert!(extends_type_sum.is_none());
                        extends_type_sum = Some(rhs.0);
                    }
                    Ident(ExtendedAttributeIdent {
                        lhs_identifier: Identifier("ScopedDictionary"),
                        assign: _,
                        rhs: IdentifierOrString::Identifier(ref rhs),
                    }) => {
                        assert!(scoped_dictionary.is_none());
                        scoped_dictionary = Some(rhs.0);
                    }
                    _ => panic!("Unknown attribute {:?}", attribute),
                }
            }

            // If the node contains an attribute `[ScopedDictionary=field]`,
            // mark the node as inserting a scoped dictionary with this field.
            if let Some(ref field_name) = scoped_dictionary {
                node.with_scoped_dictionary_str(field_name);
            }
        }

        // If the node contains an attribute `[ExtendsTypeSum=Foobar]`,
        // extend `typedef (... or ... or ...) Foobar` into
        // `typedef (... or ... or ... or CurrentNode) Foobar`.
        if let Some(ref extended) = extends_type_sum {
            let node_name = self
                .builder
                .get_node_name(extended)
                .unwrap_or_else(|| panic!("Could not find node name {}", extended));
            let mut typedef = self
                .builder
                .get_typedef_mut(&node_name)
                .unwrap_or_else(|| panic!("Could not find typedef {}", extended));
            let mut typespec = typedef.spec_mut();
            let typesum = if let TypeSpec::TypeSum(ref mut typesum) = *typespec {
                typesum
            } else {
                panic!(
                    "Attempting to extend a node that is not a type sum {}",
                    extended
                );
            };
            typesum.with_type_case(TypeSpec::NamedType(name));
        }

        self.path.pop();
    }

    fn convert_single_type(&mut self, t: &NonAnyType) -> spec::Type {
        match t {
            NonAnyType::Boolean(ref b) => nullable(b, TypeSpec::Boolean),
            NonAnyType::Identifier(ref id) => nullable(id, {
                let name = self.builder.node_name(id.type_.0);
                // Sadly, some identifiers are not truly `typedef`s.
                match name.to_str() {
                    "IdentifierName" if self.is_at_interface("StaticMemberAssignmentTarget") => {
                        TypeSpec::PropertyKey
                    }
                    "IdentifierName" if self.is_at_interface("StaticMemberExpression") => {
                        TypeSpec::PropertyKey
                    }
                    "IdentifierName" if self.is_at_interface("ImportSpecifier") => {
                        TypeSpec::PropertyKey
                    }
                    "IdentifierName" if self.is_at_interface("ExportSpecifier") => {
                        TypeSpec::PropertyKey
                    }
                    "IdentifierName" if self.is_at_interface("ExportLocalSpecifier") => {
                        TypeSpec::PropertyKey
                    }
                    "IdentifierName" => TypeSpec::IdentifierName,
                    "Identifier" => TypeSpec::IdentifierName,
                    _ => TypeSpec::NamedType(name.clone()),
                }
            }),
            NonAnyType::DOMString(ref s) => nullable(
                s,
                if self.is_at_interface("LiteralPropertyName") {
                    TypeSpec::PropertyKey
                } else {
                    TypeSpec::String
                },
            ),
            NonAnyType::FrozenArrayType(ref t) => nullable(
                t,
                TypeSpec::Array {
                    contents: Box::new(self.convert_type(&t.type_.generics.body)),
                    supports_empty: true,
                },
            ),
            NonAnyType::FloatingPoint(ref t) => nullable(t, TypeSpec::Number),
            NonAnyType::Integer(ref t) => nullable(t, TypeSpec::UnsignedLong),
            _ => {
                panic!("I don't know how to import {:?} yet", t);
            }
        }
    }

    fn convert_union_type(&mut self, types: &MayBeNull<UnionType>) -> spec::Type {
        let converted_types: Vec<_> = types
            .type_
            .body
            .list
            .iter()
            .map(|t| match t {
                UnionMemberType::Single(t) => self.convert_single_type(t),
                UnionMemberType::Union(t) => self.convert_union_type(t),
            })
            .map(|t| t.spec)
            .collect();

        nullable(types, TypeSpec::TypeSum(TypeSum::new(converted_types)))
    }

    fn convert_type(&mut self, t: &Type) -> spec::Type {
        match t {
            Type::Single(SingleType::NonAny(t)) => self.convert_single_type(t),
            Type::Union(types) => self.convert_union_type(types),
            _ => panic!("I don't know how to import {:?} yet", t),
        }
    }

    fn is_at_interface(&self, name: &str) -> bool {
        if self.path.len() == 0 {
            return false;
        }
        self.path[0].as_str() == name
    }
}
