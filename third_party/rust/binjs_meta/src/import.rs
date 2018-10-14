use spec::{ self, Laziness, SpecBuilder, TypeSum };

use webidl::ast::*;

pub struct Importer {
    builder: SpecBuilder,
    /// The interfaces we have traversed so far.
    path: Vec<String>,
}
impl Importer {
    /// Import an AST into a SpecBuilder.
    ///
    /// ```
    /// extern crate binjs_meta;
    /// extern crate webidl;
    /// use webidl;
    /// use binjs_meta::spec::SpecOptions;
    ///
    /// let ast = webidl::parse_string("
    ///    interface FooContents {
    ///      attribute boolean value;
    ///    };
    ///    interface LazyFoo {
    ///       [Lazy] attribute FooContents contents;
    ///    };
    ///    interface EagerFoo {
    ///       attribute FooContents contents;
    ///    };
    /// ").expect("Could not parse");
    ///
    /// let mut builder = binjs_meta::import::Importer::import(&ast);
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
    pub fn import(ast: &AST) -> SpecBuilder {
        let mut importer = Importer {
            path: Vec::with_capacity(256),
            builder: SpecBuilder::new()
        };
        importer.import_ast(ast);
        importer.builder
    }
    fn import_ast(&mut self, ast: &AST) {
        for definition in ast {
            self.import_definition(&definition)
        }
    }
    fn import_definition(&mut self, def: &Definition) {
        match *def {
            Definition::Enum(ref enum_) => self.import_enum(enum_),
            Definition::Typedef(ref typedef) => self.import_typedef(typedef),
            Definition::Interface(ref interface) => self.import_interface(interface),
            _ => panic!("Not implemented: importing {:?}", def)
        }
    }
    fn import_enum(&mut self, enum_: &Enum) {
        let name = self.builder.node_name(&enum_.name);
        let mut node = self.builder.add_string_enum(&name)
            .expect("Name already present");
        for variant in &enum_.variants {
            node.with_string(variant);
        }
    }
    fn import_typedef(&mut self, typedef: &Typedef) {
        let name = self.builder.node_name(&typedef.name);
        // The following are, unfortunately, not true typedefs.
        // Ignore their definition.
        let type_ = match typedef.name.as_ref() {
            "Identifier"  => spec::TypeSpec::IdentifierName
                .required(),
            "IdentifierName" => spec::TypeSpec::IdentifierName
                .required(),
            "PropertyKey" => spec::TypeSpec::PropertyKey
                .required(),
            _ => self.convert_type(&*typedef.type_)
        };
        debug!(target: "meta::import", "Importing typedef {type_:?} {name:?}",
            type_ = type_,
            name = name);
        let mut node = self.builder.add_typedef(&name)
            .unwrap_or_else(|| panic!("Error: Name {} is defined more than once in the spec.", name));
        assert!(!type_.is_optional());
        node.with_spec(type_.spec);
    }
    fn import_interface(&mut self, interface: &Interface) {
        let interface = if let &Interface::NonPartial(ref interface) = interface {
            interface
        } else {
            panic!("Expected a non-partial interface, got {:?}", interface);
        };

        // Handle special, hardcoded, interfaces.
        match interface.name.as_ref() {
            "Node" => {
                // We're not interested in the root interface.
                return;
            }
            "IdentifierName" => {
                unimplemented!()
            }
            _ => {

            }
        }
        if let Some(ref parent) = interface.inherits {
            assert_eq!(parent, "Node");
        }

        self.path.push(interface.name.clone());

        // Now handle regular stuff.
        let mut fields = Vec::new();
        for member in &interface.members {
            if let InterfaceMember::Attribute(Attribute::Regular(ref attribute)) = *member {
                use webidl::ast::ExtendedAttribute::NoArguments;
                use webidl::ast::Other::Identifier;

                let name = self.builder.field_name(&attribute.name);
                let type_ = self.convert_type(&*attribute.type_);

                let is_lazy = attribute.extended_attributes.iter()
                    .find(|attribute| {
                        if let &NoArguments(Identifier(ref id)) = attribute.as_ref() {
                            if &*id == "Lazy" {
                                return true;
                            }
                        }
                        false
                    })
                    .is_some();
                fields.push((name, type_, if is_lazy { Laziness::Lazy } else { Laziness:: Eager }));
            } else {
                panic!("Expected an attribute, got {:?}", member);
            }
        }
        let name = self.builder.node_name(&interface.name);
        let mut node = self.builder.add_interface(&name)
            .expect("Name already present");
        for (field_name, field_type, laziness) in fields.drain(..) {
            node.with_field_laziness(&field_name, field_type, laziness);
        }

        for extended_attribute in &interface.extended_attributes {
            use webidl::ast::ExtendedAttribute::NoArguments;
            use webidl::ast::Other::Identifier;
            if let &NoArguments(Identifier(ref id)) = extended_attribute.as_ref() {
                if &*id == "Skippable" {
                    panic!("Encountered deprecated attribute [Skippable]");
                }
                if &*id == "Scope" {
                    node.with_scope(true);
                }
            }
        }
        self.path.pop();
    }
    fn convert_type(&mut self, t: &Type) -> spec::Type {
        let spec = match t.kind {
            TypeKind::Boolean => spec::TypeSpec::Boolean,
            TypeKind::Identifier(ref id) => {
                let name = self.builder.node_name(id);
                // Sadly, some identifiers are not truly `typedef`s.
                match name.to_str() {
                    "IdentifierName" if self.is_at_interface("StaticMemberAssignmentTarget") => spec::TypeSpec::PropertyKey,
                    "IdentifierName" if self.is_at_interface("StaticMemberExpression") => spec::TypeSpec::PropertyKey,
                    "IdentifierName" if self.is_at_interface("ImportSpecifier") => spec::TypeSpec::PropertyKey,
                    "IdentifierName" if self.is_at_interface("ExportSpecifier") => spec::TypeSpec::PropertyKey,
                    "IdentifierName" if self.is_at_interface("ExportLocalSpecifier") => spec::TypeSpec::PropertyKey,
                    "IdentifierName" => spec::TypeSpec::IdentifierName,
                    "Identifier" => spec::TypeSpec::IdentifierName,
                    _ => spec::TypeSpec::NamedType(name.clone())
                }
            }
            TypeKind::DOMString if self.is_at_interface("LiteralPropertyName") => spec::TypeSpec::PropertyKey,
            TypeKind::DOMString => spec::TypeSpec::String,
            TypeKind::Union(ref types) => {
                let mut dest = Vec::with_capacity(types.len());
                for typ in types {
                    dest.push(self.convert_type(&*typ).spec)
                }
                spec::TypeSpec::TypeSum(TypeSum::new(dest))
            }
            TypeKind::FrozenArray(ref type_) => {
                spec::TypeSpec::Array {
                    contents: Box::new(self.convert_type(&*type_)),
                    supports_empty: true
                }
            }
            TypeKind::RestrictedDouble =>
                spec::TypeSpec::Number,
            TypeKind::UnsignedLong =>
                spec::TypeSpec::UnsignedLong,
            _ => {
                panic!("I don't know how to import {:?} yet", t);
            }
        };
        if t.nullable {
            spec.optional()
                .unwrap_or_else(|| panic!("This type could not be made optional {:?}", t))
        } else {
            spec.required()
        }
    }

    fn is_at_interface(&self, name: &str) -> bool {
        if self.path.len() == 0 {
            return false;
        }
        self.path[0].as_str() == name
    }
}
