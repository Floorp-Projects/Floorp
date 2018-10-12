use spec::*;
use util::*;

use std::collections::{ HashMap, HashSet };

use itertools::Itertools;

/// A tool designed to replace all anonymous types in a specification
/// of the language by explicitly named types.
///
/// Consider the following mini-specifications for JSON:
///
/// ```idl
/// interface Value {
///     attribute (Object or String or Number or Array or Boolean)? value;
/// }
/// interface Object {
///     attribute FrozenArray<Property> properties;
/// }
/// interface Property {
///     attribute DOMString name;
///     attribute Value value;
/// }
/// interface Array {
///     attribute FrozenArray<Value?> items;
/// }
/// // ... Skipping definitions of String, Number, Boolean
/// ```
///
/// The deanonymizer will rewrite them as follows:
///
/// ```idl
/// interface Value { // Deanonymized optional sum
///     attribute OptionalObjectOrStringOrNumberOrArrayOrBoolean value;
/// }
/// interface Object { // Deanonymized list
///     attribute ListOfProperty properties;
/// }
/// interface Property { // No change
///     attribute DOMString name;
///     attribute Value value;
/// }
/// interface Array { // Deanonymized list of options
///     attribute ListOfOptionalValue items;
/// }
/// // ... Skipping definitions of String, Number, Boolean
///
/// typedef ObjectOrStringOrNumberOrArrayOrBoolean? OptionalObjectOrStringOrNumberOrArrayOrBoolean;
/// typedef (Object
///          or String
///          or Number
///          or Array
///          or Boolean)
///          ObjectOrStringOrNumberOrArrayOrBoolean;
/// typedef FrozenArray<Property> ListOfProperty;
/// typedef FrozenArray<OptionalValue> ListOfOptionalValue;
/// typedef Value? Optionalvalue;
/// ```
///
/// This deanonymization lets us cleanly define intermediate data structures and/or parsers
/// implementing the webidl specification.
pub struct TypeDeanonymizer {
    builder: SpecBuilder,

    /// When we encounter `typedef (A or B) C`
    /// and `typedef (C or D) E`, we deanonymize into
    /// `typedef (A or B or D) E`.
    ///
    /// This maintains the relationship that `E` (value)
    /// contains `C` (key).
    supersums_of: HashMap<NodeName, HashSet<NodeName>>,
}
impl TypeDeanonymizer {
    /// Create an empty TypeDeanonymizer.
    pub fn new(spec: &Spec) -> Self {
        let mut result = TypeDeanonymizer {
            builder: SpecBuilder::new(),
            supersums_of: HashMap::new(),
        };
        let mut skip_name_map: HashMap<&FieldName, FieldName> = HashMap::new();

        // Copy field names
        for (_, name) in spec.field_names() {
            result.builder.import_field_name(name)
        }

        for (_, interface) in spec.interfaces_by_name() {
            for field in interface.contents().fields() {
                if field.is_lazy() {
                    let skip_name = result.builder.field_name(format!("{}_skip", field.name().to_str()).to_str());
                    skip_name_map.insert(field.name(), skip_name);
                }
            }
        }

        // Copy and deanonymize interfaces.
        for (name, interface) in spec.interfaces_by_name() {
            result.builder.import_node_name(name);
            // Collect interfaces to copy them into the `builder`
            // and walk through their fields to deanonymize types.

            let mut fields = vec![];

            // Copy other fields.
            for field in interface.contents().fields() {
                result.import_type(spec, field.type_(), None);
                fields.push(field.clone());
            }

            // Copy the declaration.
            let mut declaration = result.builder.add_interface(name)
                .unwrap();
            for field in fields.drain(..) {
                // Create *_skip field just before the lazy field.
                // See also tagged_tuple in write.rs.
                if field.is_lazy() {
                    declaration.with_field(skip_name_map.get(field.name()).unwrap(),
                                           Type::offset().required(),
                                           Laziness::Eager);
                }
                declaration.with_field(field.name(), field.type_().clone(),
                                       field.laziness());
            }
        }
        // Copy and deanonymize typedefs
        for (name, definition) in spec.typedefs_by_name() {
            result.builder.import_node_name(name);
            if result.builder.get_typedef(name).is_some() {
                // Already imported by following links.
                continue
            }
            result.import_type(spec, &definition, Some(name.clone()));
        }
        // Copy and deanonymize string enums
        for (name, definition) in spec.string_enums_by_name() {
            result.builder.import_node_name(name);
            let mut strings: Vec<_> = definition.strings()
                .iter()
                .collect();
            let mut declaration = result.builder.add_string_enum(name)
                .unwrap();
            for string in strings.drain(..) {
                declaration.with_string(&string);
            }
        }
        debug!(target: "export_utils", "Names: {:?}", result.builder.names().keys().format(", "));

        result
    }

    pub fn supersums(&self) -> &HashMap<NodeName, HashSet<NodeName>> {
        &self.supersums_of
    }

    /// Convert into a new specification.
    pub fn into_spec(self, options: SpecOptions) -> Spec {
        self.builder.into_spec(options)
    }

    /// If `name` is the name of a (deanonymized) type, return the corresponding type.
    pub fn get_node_name(&self, name: &str) -> Option<NodeName> {
        self.builder.get_node_name(name)
    }

    /// Returns `(sum, name)` where `sum` is `Some(names)` iff this type can be resolved to a sum of interfaces.
    fn import_type(&mut self, spec: &Spec, type_: &Type, public_name: Option<NodeName>) -> (Option<HashSet<NodeName>>, NodeName) {
        debug!(target: "export_utils", "import_type {:?} => {:?}", public_name, type_);
        if type_.is_optional() {
            let (_, spec_name) = self.import_typespec(spec, &type_.spec, None);
            let my_name =
                match public_name {
                    None => self.builder.node_name(&format!("Optional{}", spec_name)),
                    Some(ref name) => name.clone()
                };
            let deanonymized = Type::named(&spec_name).optional()
                .unwrap(); // Named types can always be made optional.
            if let Some(ref mut typedef) = self.builder.add_typedef(&my_name) {
                debug!(target: "export_utils", "import_type introduced {:?}", my_name);
                typedef.with_type(deanonymized.clone());
            } else {
                debug!(target: "export_utils", "import_type: Attempting to redefine typedef {name}", name = my_name.to_str());
            }
            (None, my_name)
        } else {
            self.import_typespec(spec, &type_.spec, public_name)
        }
    }
    fn import_typespec(&mut self, spec: &Spec, type_spec: &TypeSpec, public_name: Option<NodeName>) -> (Option<HashSet<NodeName>>, NodeName) {
        debug!(target: "export_utils", "import_typespec {:?} => {:?}", public_name, type_spec);
        match *type_spec {
            TypeSpec::Boolean |
            TypeSpec::Number |
            TypeSpec::UnsignedLong |
            TypeSpec::String |
            TypeSpec::Offset |
            TypeSpec::Void    => {
                if let Some(ref my_name) = public_name {
                    if let Some(ref mut typedef) = self.builder.add_typedef(&my_name) {
                        debug!(target: "export_utils", "import_typespec: Defining {name} (primitive)", name = my_name.to_str());
                        typedef.with_type(type_spec.clone().required());
                    } else {
                        debug!(target: "export_utils", "import_typespec: Attempting to redefine typedef {name}", name = my_name.to_str());
                    }
                }
                (None, self.builder.node_name("@@"))
            }
            TypeSpec::NamedType(ref link) => {
                let resolved = spec.get_type_by_name(link)
                    .unwrap_or_else(|| panic!("While deanonymizing, could not find the definition of {} in the original spec.", link.to_str()));
                let (sum, rewrite, primitive) = match resolved {
                    NamedType::StringEnum(_) => {
                        // - Can't use in a sum
                        // - No rewriting happened.
                        (None, None, None)
                    }
                    NamedType::Typedef(ref type_) => {
                        // - Might use in a sum.
                        // - Might be rewritten.
                        let (sum, name) = self.import_type(spec, type_, Some(link.clone()));
                        (sum, Some(name), type_.get_primitive(spec))
                    }
                    NamedType::Interface(_) => {
                        // - May use in a sum.
                        // - If a rewriting takes place, it didn't change the names.
                        let sum = [link.clone()].iter()
                            .cloned()
                            .collect();
                        (Some(sum), None, None)
                    }
                };
                debug!(target: "export_utils", "import_typespec dealing with named type {}, public name {:?} => {:?}",
                    link, public_name, rewrite);
                if let Some(ref my_name) = public_name {
                    // If we have a public name, alias it to `content`
                    if let Some(content) = rewrite {
                        let deanonymized = match primitive {
                            None |
                            Some(IsNullable { is_nullable: true, .. }) |
                            Some(IsNullable { content: Primitive::Interface(_), .. }) => Type::named(&content).required(),
                            Some(IsNullable { content: Primitive::String, .. }) => Type::string().required(),
                            Some(IsNullable { content: Primitive::Number, .. }) => Type::number().required(),
                            Some(IsNullable { content: Primitive::UnsignedLong, .. }) => Type::unsigned_long().required(),
                            Some(IsNullable { content: Primitive::Boolean, .. }) => Type::bool().required(),
                            Some(IsNullable { content: Primitive::Offset, .. }) => Type::offset().required(),
                            Some(IsNullable { content: Primitive::Void, .. }) => Type::void().required()
                        };
                        debug!(target: "export_utils", "import_typespec aliasing {:?} => {:?}",
                            my_name, deanonymized);
                        if let Some(ref mut typedef) = self.builder.add_typedef(&my_name) {
                            debug!(target: "export_utils", "import_typespec: Defining {name} (name to content)", name = my_name.to_str());
                            typedef.with_type(deanonymized.clone());
                        } else {
                            debug!(target: "export_utils", "import_typespec: Attempting to redefine typedef {name}", name = my_name.to_str());
                        }
                    }
                    // Also, don't forget to copy the typedef and alias `link`
                    let deanonymized = Type::named(link).required();
                    if let Some(ref mut typedef) = self.builder.add_typedef(&my_name) {
                        debug!(target: "export_utils", "import_typespec: Defining {name} (name to link)", name = my_name.to_str());
                        typedef.with_type(deanonymized.clone());
                    } else {
                        debug!(target: "export_utils", "import_typespec: Attempting to redefine typedef {name}", name = my_name.to_str());
                    }
                }
                (sum, link.clone())
            }
            TypeSpec::Array {
                ref contents,
                ref supports_empty
            } => {
                let (_, contents_name) = self.import_type(spec, contents, None);
                let my_name =
                    match public_name {
                        None => self.builder.node_name(&format!("{non_empty}ListOf{content}",
                            non_empty =
                                if *supports_empty {
                                    ""
                                } else {
                                    "NonEmpty"
                                },
                            content = contents_name.to_str())),
                        Some(ref name) => name.clone()
                    };
                let deanonymized =
                    if *supports_empty {
                        Type::named(&contents_name).array()
                    } else {
                        Type::named(&contents_name).non_empty_array()
                    };
                if let Some(ref mut typedef) = self.builder.add_typedef(&my_name) {
                    debug!(target: "export_utils", "import_typespec: Defining {name} (name to list)",
                        name = my_name.to_str());
                    typedef.with_type(deanonymized.clone());
                } else {
                    debug!(target: "export_utils", "import_typespec: Attempting to redefine typedef {name}", name = my_name.to_str());
                }
                (None, my_name)
            }
            TypeSpec::TypeSum(ref sum) => {
                let mut full_sum = HashSet::new();
                let mut names = vec![];
                let mut subsums = vec![];
                for sub_type in sum.types() {
                    let (mut sub_sum, name) = self.import_typespec(spec, sub_type, None);
                    let mut sub_sum = sub_sum.unwrap_or_else(
                        || panic!("While treating {:?}, attempting to create a sum containing {}, which isn't an interface or a sum of interfaces", type_spec, name)
                    );
                    if sub_sum.len() > 1 {
                        // The subtype is itself a sum.
                        subsums.push(name.clone())
                    }
                    names.push(name);
                    for item in sub_sum.drain() {
                        full_sum.insert(item);
                    }
                }
                let my_name =
                    match public_name {
                        None => self.builder.node_name(&format!("{}",
                            names.drain(..)
                                .format("Or"))),
                        Some(ref name) => name.clone()
                    };
                for subsum_name in subsums {
                    // So, `my_name` is a superset of `subsum_name`.
                    let mut supersum_entry = self.supersums_of.entry(subsum_name.clone())
                        .or_insert_with(|| HashSet::new());
                    supersum_entry.insert(my_name.clone());
                }
                let sum : Vec<_> = full_sum.iter()
                    .map(Type::named)
                    .collect();
                let deanonymized = Type::sum(&sum).required();
                if let Some(ref mut typedef) = self.builder.add_typedef(&my_name) {
                    debug!(target: "export_utils", "import_typespec: Defining {name} (name to sum)", name = my_name.to_str());
                    typedef.with_type(deanonymized.clone());
                } else {
                    debug!(target: "export_utils", "import_type: Attempting to redefine typedef {name}", name = my_name.to_str());
                }
                (Some(full_sum), my_name)
            }
        }
    }
}

/// Utility to give a name to a type or type spec.
pub struct TypeName;
impl TypeName {
    pub fn type_(type_: &Type) -> String {
        let spec_name = Self::type_spec(type_.spec());
        if type_.is_optional() {
            format!("Optional{}", spec_name)
        } else {
            spec_name
        }
    }

    pub fn type_spec(spec: &TypeSpec) -> String {
        match *spec {
            TypeSpec::Array { ref contents, supports_empty: false } =>
                format!("NonEmptyListOf{}", Self::type_(contents)),
            TypeSpec::Array { ref contents, supports_empty: true } =>
                format!("ListOf{}", Self::type_(contents)),
            TypeSpec::NamedType(ref name) =>
                name.to_string().clone(),
            TypeSpec::Offset =>
                "_Offset".to_string(),
            TypeSpec::Boolean =>
                "_Bool".to_string(),
            TypeSpec::Number =>
                "_Number".to_string(),
            TypeSpec::UnsignedLong =>
                "_UnsignedLong".to_string(),
            TypeSpec::String =>
                "_String".to_string(),
            TypeSpec::Void =>
                "_Void".to_string(),
            TypeSpec::TypeSum(ref sum) => {
                format!("{}", sum.types()
                    .iter()
                    .map(Self::type_spec)
                    .format("Or"))
            }
        }
    }
}

/// Export a type specification as webidl.
///
/// Designed for generating documentation.
pub struct ToWebidl;
impl ToWebidl {
    /// Export a TypeSpec.
    pub fn spec(spec: &TypeSpec, prefix: &str, indent: &str) -> Option<String> {
        let result = match *spec {
            TypeSpec::Offset => {
                return None;
            }
            TypeSpec::Array { ref contents, ref supports_empty } => {
                match Self::type_(&*contents, prefix, indent) {
                    None => { return None; }
                    Some(description) => format!("{emptiness}FrozenArray<{}>",
                        description,
                        emptiness = if *supports_empty { "" } else {"[NonEmpty] "} ),
                }
            }
            TypeSpec::Boolean =>
                "bool".to_string(),
            TypeSpec::String =>
                "string".to_string(),
            TypeSpec::Number =>
                "number".to_string(),
            TypeSpec::UnsignedLong =>
                "unsigned long".to_string(),
            TypeSpec::NamedType(ref name) =>
                name.to_str().to_string(),
            TypeSpec::TypeSum(ref sum) => {
                format!("({})", sum.types()
                    .iter()
                    .filter_map(|x| Self::spec(x, "", indent))
                    .format(" or "))
            }
            TypeSpec::Void => "void".to_string()
        };
        Some(result)
    }

    /// Export a Type
    pub fn type_(type_: &Type, prefix: &str, indent: &str) -> Option<String> {
        let pretty_type = Self::spec(type_.spec(), prefix, indent);
        match pretty_type {
            None => None,
            Some(pretty_type) => Some(format!("{}{}",
                pretty_type,
                if type_.is_optional() { "?" } else { "" }))
        }
    }

    /// Export an Interface
    pub fn interface(interface: &Interface, prefix: &str, indent: &str) -> String {
        let mut result = format!("{prefix} interface {name} : Node {{\n", prefix=prefix, name=interface.name().to_str());
        {
            let prefix = format!("{prefix}{indent}",
                prefix=prefix,
                indent=indent);
            for field in interface.contents().fields() {
                match Self::type_(field.type_(), &prefix, indent) {
                    None => /* generated field, ignore */ {},
                    Some(description) => {
                        if let Some(ref doc) = field.doc() {
                            result.push_str(&format!("{prefix}// {doc}\n", prefix = prefix, doc = doc));
                        }
                        result.push_str(&format!("{prefix}{description} {name};\n",
                            prefix = prefix,
                            name = field.name().to_str(),
                            description = description
                        ));
                        if field.doc().is_some() {
                            result.push_str("\n");
                        }
                    }
                }
            }
        }
        result.push_str(&format!("{prefix} }}\n", prefix=prefix));
        result
    }
}
