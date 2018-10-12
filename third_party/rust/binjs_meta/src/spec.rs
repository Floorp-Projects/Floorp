//! Definition of a spec for a version (or subset) of JavaScript.

pub use util::ToStr;

use std;
use std::cell::*;
use std::collections::{ HashMap, HashSet };
use std::fmt::{ Debug, Display };
use std::hash::*;
use std::rc::*;


/// The name of an interface or enum.
#[derive(Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct NodeName(Rc<String>);
impl NodeName {
    pub fn to_string(&self) -> &String {
        self.0.as_ref()
    }
}
impl Debug for NodeName {
    fn fmt(&self, formatter: &mut std::fmt::Formatter) -> Result<(), std::fmt::Error> {
        Debug::fmt(self.to_str(), formatter)
    }
}
impl Display for NodeName {
    fn fmt(&self, formatter: &mut std::fmt::Formatter) -> Result<(), std::fmt::Error> {
        Display::fmt(self.to_str(), formatter)
    }
}
impl ToStr for NodeName {
    fn to_str(&self) -> &str {
        &self.0
    }
}


/// The name of a field in an interface.
#[derive(Clone, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub struct FieldName(Rc<String>);
impl FieldName {
    pub fn to_string(&self) -> &String {
        self.0.as_ref()
    }
}
impl Debug for FieldName {
    fn fmt(&self, formatter: &mut std::fmt::Formatter) -> Result<(), std::fmt::Error> {
        Debug::fmt(self.to_str(), formatter)
    }
}
impl ToStr for FieldName {
    fn to_str(&self) -> &str {
        self.0.as_ref()
    }
}

/// An enumeration of strings.
///
/// A valid value is any of these strings.
#[derive(Debug)]
pub struct StringEnum {
    name: NodeName,
    // Invariant: values are distinct // FIXME: Not checked yet.
    values: Vec<String>,
}

/// An enumeration of interfaces.
#[derive(Clone, Debug, PartialEq, Eq)] // FIXME: Get rid of Eq
pub struct TypeSum {
    types: Vec<TypeSpec>,
    /// Once we have called `into_spec`, this is guaranteed to resolve
    /// to at least one Interface.
    interfaces: HashSet<NodeName>,
}
impl TypeSum {
    pub fn new(types: Vec<TypeSpec>) -> Self {
        TypeSum {
            types,
            interfaces: HashSet::new()
        }
    }
    pub fn types(&self) -> &[TypeSpec] {
        &self.types
    }
    pub fn types_mut(&mut self) -> &mut [TypeSpec] {
        &mut self.types
    }
    pub fn interfaces(&self) -> &HashSet<NodeName> {
        &self.interfaces
    }
    pub fn get_interface(&self, spec: &Spec, name: &NodeName) -> Option<Rc<Interface>> {
        debug!(target: "spec", "get_interface, looking for {:?} in sum {:?}", name, self);
        for item in &self.types {
            let result = item.get_interface(spec, name);
            if result.is_some() {
                return result
            }
        }
        None
    }
}

/// Lazy for a field with [lazy] attribute. Eager for others.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Laziness {
    Eager,
    Lazy,
}

/// Representation of a field in an interface.
#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Field {
    name: FieldName,
    type_: Type,
    documentation: Option<String>,
    laziness: Laziness,
}
impl Hash for Field {
    fn hash<H>(&self, state: &mut H) where H: Hasher {
        self.name.hash(state)
    }
}
impl Field {
    pub fn new(name: FieldName, type_: Type) -> Self {
        Field {
            name,
            type_,
            documentation: None,
            laziness: Laziness::Eager,
        }
    }
    pub fn name(&self) -> &FieldName {
        &self.name
    }
    pub fn type_(&self) -> &Type {
        &self.type_
    }
    pub fn is_lazy(&self) -> bool {
        self.laziness == Laziness::Lazy
    }
    pub fn laziness(&self) -> Laziness {
        self.laziness.clone()
    }
    pub fn doc(&self) -> Option<&str> {
        match self.documentation {
            None => None,
            Some(ref s) => Some(&*s)
        }
    }
}

/// The contents of a type, typically that of a field.
///
/// Note that we generally use `Type`, to represent
/// the fact that some fields accept `null` while
/// others do not.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum TypeSpec {
    /// An array of values of the same type.
    Array {
        /// The type of values in the array.
        contents: Box<Type>,

        /// If `true`, the array may be empty.
        supports_empty: bool,
    },

    NamedType(NodeName),

    TypeSum(TypeSum),

    /// A boolean.
    Boolean,

    /// A string.
    String,

    /// A number, as per JavaScript specifications.
    Number,

    UnsignedLong,

    /// A number of bytes in the binary file.
    ///
    /// This spec is used only internally, as a hidden
    /// field injected by deanonymization, to represent
    /// lazy fields.
    Offset,

    /// Nothing.
    ///
    /// For the moment, this spec is used only internally.
    Void,
}

#[derive(Clone, Debug)]
pub enum NamedType {
    Interface(Rc<Interface>),
    Typedef(Rc<Type>), // FIXME: Check that there are no cycles.
    StringEnum(Rc<StringEnum>),
}

impl NamedType {
    pub fn as_interface(&self, spec: &Spec) -> Option<Rc<Interface>> {
        match *self {
            NamedType::Interface(ref result) => Some(result.clone()),
            NamedType::Typedef(ref type_) => {
                if let TypeSpec::NamedType(ref named) = *type_.spec() {
                    let named = spec.get_type_by_name(named)
                        .expect("Type not found");
                    named.as_interface(spec)
                } else {
                    None
                }
            }
            NamedType::StringEnum(_) => None,
        }
    }
}

impl TypeSpec {
    pub fn array(self) -> Type {
        TypeSpec::Array {
            contents: Box::new(Type {
                spec: self,
                or_null: false
            }),
            supports_empty: true,
        }.required()
    }

    pub fn non_empty_array(self) -> Type {
        TypeSpec::Array {
            contents: Box::new(Type {
                spec: self,
                or_null: false,
            }),
            supports_empty: false,
        }.required()
    }

    pub fn optional(self) -> Option<Type> {
        if let TypeSpec::Offset = self {
            None
        }  else {
            Some(Type {
                spec: self,
                or_null: true
            })
        }
    }

    pub fn required(self) -> Type {
        Type {
            spec: self,
            or_null: false
        }
    }

    fn typespecs<'a>(&'a self) -> Vec<&'a TypeSpec> {
        let mut result = Vec::new();
        let mut stack = vec![self];
        while let Some(current) = stack.pop() {
            result.push(current);
            match *current {
                TypeSpec::Array { ref contents, .. } => {
                    stack.push(contents.spec());
                }
                TypeSpec::TypeSum(ref sum) => {
                    for item in sum.types() {
                        stack.push(item)
                    }
                }
                _ => {}
            }
        }
        result
    }

    pub fn typenames<'a>(&'a self) -> HashSet<&'a NodeName> {
        let mut result = HashSet::new();
        for spec in self.typespecs() {
            if let TypeSpec::NamedType(ref name) = *spec {
                result.insert(name);
            }
        }
        result
    }

    pub fn get_primitive(&self, spec: &Spec) -> Option<IsNullable<Primitive>> {
        match *self {
            TypeSpec::Boolean => Some(IsNullable::non_nullable(Primitive::Boolean)),
            TypeSpec::Void => Some(IsNullable::non_nullable(Primitive::Void)),
            TypeSpec::Number => Some(IsNullable::non_nullable(Primitive::Number)),
            TypeSpec::UnsignedLong => Some(IsNullable::non_nullable(Primitive::UnsignedLong)),
            TypeSpec::String => Some(IsNullable::non_nullable(Primitive::String)),
            TypeSpec::Offset => Some(IsNullable::non_nullable(Primitive::Offset)),
            TypeSpec::NamedType(ref name) => {
                match spec.get_type_by_name(name).unwrap() {
                    NamedType::Interface(ref interface) =>
                        Some(IsNullable::non_nullable(Primitive::Interface(interface.clone()))),
                    NamedType::Typedef(ref type_) =>
                        type_.get_primitive(spec),
                    NamedType::StringEnum(_) => None
                }
            }
            _ => None
        }
    }
}

#[derive(Debug)]
pub struct IsNullable<T> {
    pub is_nullable: bool,
    pub content: T,
}
impl<T> IsNullable<T> {
    fn non_nullable(value: T) -> Self {
        IsNullable {
            is_nullable: false,
            content: value
        }
    }
}

#[derive(Debug)]
pub enum Primitive {
    String,
    Boolean,
    Void,
    Number,
    UnsignedLong,
    Offset,
    Interface(Rc<Interface>),
}

#[derive(Clone, Debug, PartialEq)]
pub struct Type {
    pub spec: TypeSpec,
    or_null: bool,
}
impl Eq for Type {}

impl Type {
    pub fn or_null(&mut self) -> &mut Self {
        self.or_null = true;
        self
    }

    pub fn with_spec(&mut self, spec: TypeSpec) -> &mut Self {
        self.spec = spec;
        self
    }

    pub fn with_type(&mut self, type_: Type) -> &mut Self {
        self.spec = type_.spec;
        self.or_null = type_.or_null;
        self
    }

    pub fn spec(&self) -> &TypeSpec {
        &self.spec
    }
    pub fn spec_mut(&mut self) -> &mut TypeSpec {
        &mut self.spec
    }
    pub fn is_optional(&self) -> bool {
        self.or_null
    }

    /// Shorthand constructors.
    pub fn named(name: &NodeName) -> TypeSpec {
        TypeSpec::NamedType(name.clone())
    }
    pub fn sum(types: &[TypeSpec]) -> TypeSpec {
        let specs = types.iter()
            .cloned()
            .collect();
        TypeSpec::TypeSum(TypeSum::new(specs))
    }
    pub fn string() -> TypeSpec {
        TypeSpec::String
    }
    pub fn number() -> TypeSpec {
        TypeSpec::Number
    }
    pub fn unsigned_long() -> TypeSpec {
        TypeSpec::UnsignedLong
    }
    pub fn bool() -> TypeSpec {
        TypeSpec::Boolean
    }
    pub fn void() -> TypeSpec {
        TypeSpec::Void
    }

    /// An `offset` type, holding a number of bytes in the binary file.
    pub fn offset() -> TypeSpec {
        TypeSpec::Offset
    }

    pub fn array(self) -> TypeSpec {
        TypeSpec::Array {
            contents: Box::new(self),
            supports_empty: true,
        }
    }

    pub fn non_empty_array(self) -> TypeSpec {
        TypeSpec::Array {
            contents: Box::new(self),
            supports_empty: false,
        }
    }

    pub fn get_primitive(&self, spec: &Spec) -> Option<IsNullable<Primitive>> {
        if let Some(mut primitive) = self.spec.get_primitive(spec) {
            primitive.is_nullable = primitive.is_nullable || self.or_null;
            Some(primitive)
        } else {
            None
        }
    }
}

/// Representation of an object, i.e. a set of fields.
///
/// Field order is *not* specified, but is expected to remain stable during encoding
/// operations and during decoding operations. Note in particular that the order may
/// change between encoding and decoding.
#[derive(Clone, Debug)]
pub struct Obj {
    fields: Vec<Field>,
}
impl PartialEq for Obj {
    fn eq(&self, other: &Self) -> bool {
        // Normalize order before comparing.
        let me : HashSet<_> = self.fields.iter().collect();
        let other : HashSet<_> = other.fields.iter().collect();
        me == other
    }
}
impl Eq for Obj {}

impl Obj {
    /// Create a new empty structure
    pub fn new() -> Self {
        Obj {
            fields: Vec::new()
        }
    }
    /// A list of the fields in the structure.
    pub fn fields<'a>(&'a self) -> &'a [Field] {
        &self.fields
    }
    /// Fetch a specific field in the structure
    pub fn field<'a>(&'a self, name: &FieldName) -> Option<&'a Field> {
        self.fields.iter().find(|field| &field.name == name)
    }

    pub fn with_full_field(&mut self, field: Field) -> &mut Self {
        if self.field(field.name()).is_some() {
            warn!("Field: attempting to overwrite {:?}", field.name());
            return self
        }
        self.fields.push(field);
        self
    }

    fn with_field_aux(self, name: &FieldName, type_: Type, laziness: Laziness,
                      doc: Option<&str>) -> Self {
        if self.field(name).is_some() {
            warn!("Field: attempting to overwrite {:?}", name);
            return self
        }
        let mut fields = self.fields;
        fields.push(Field {
            name: name.clone(),
            type_,
            documentation: doc.map(str::to_string),
            laziness,
        });
        Obj {
            fields
        }

    }

    /// Extend a structure with a field.
    pub fn with_field(self, name: &FieldName, type_: Type, laziness: Laziness) -> Self {
        self.with_field_aux(name, type_, laziness, None)
    }

    pub fn with_field_doc(self, name: &FieldName, type_: Type, laziness: Laziness, doc: &str) -> Self {
        self.with_field_aux(name, type_, laziness, Some(doc))
    }
}

impl StringEnum {
    pub fn name(&self) -> &NodeName {
        &self.name
    }

    pub fn strings(&self) -> &[String] {
        &self.values
    }

    /// Add a string to the enum. Idempotent.
    pub fn with_string(&mut self, string: &str) -> &mut Self {
        let string = string.to_string();
        if self.values.iter().find(|x| **x == string).is_none() {
            self.values.push(string.to_string())
        }
        self
    }
    /// Add several enums to the list. Idempotent.
    pub fn with_strings(&mut self, strings: &[&str]) -> &mut Self {
        for string in strings {
            self.with_string(string);
        }
        self
    }
}

#[derive(Clone, Debug)]
pub struct InterfaceDeclaration {
    /// The name of the interface, e.g. `Node`.
    name: NodeName,

    /// The contents of this interface, excluding the contents of parent interfaces.
    contents: Obj,
}

impl InterfaceDeclaration {
    pub fn with_full_field(&mut self, contents: Field) -> &mut Self {
        let _ = self.contents.with_full_field(contents);
        self
    }
    pub fn with_field(&mut self, name: &FieldName, type_: Type, laziness: Laziness) -> &mut Self {
        self.with_field_aux(name, type_, laziness, None)
    }
    pub fn with_field_doc(&mut self, name: &FieldName, type_: Type, laziness: Laziness, doc: &str) -> &mut Self {
        self.with_field_aux(name, type_, laziness, Some(doc))
    }
    fn with_field_aux(&mut self, name: &FieldName, type_: Type, laziness: Laziness, doc: Option<&str>) -> &mut Self {
        let mut contents = Obj::new();
        std::mem::swap(&mut self.contents, &mut contents);
        self.contents = contents.with_field_aux(name, type_, laziness, doc);
        self
    }
}

/// A data structure used to progressively construct the `Spec`.
pub struct SpecBuilder {
    /// All the interfaces entered so far.
    interfaces_by_name: HashMap<NodeName, RefCell<InterfaceDeclaration>>,

    /// All the enums entered so far.
    string_enums_by_name: HashMap<NodeName, RefCell<StringEnum>>,

    typedefs_by_name: HashMap<NodeName, RefCell<Type>>,

    names: HashMap<String, Rc<String>>,
}

impl SpecBuilder {
    pub fn new() -> Self {
        SpecBuilder {
            interfaces_by_name: HashMap::new(),
            string_enums_by_name: HashMap::new(),
            typedefs_by_name: HashMap::new(),
            names: HashMap::new()
        }
    }

    pub fn names(&self) -> &HashMap<String, Rc<String>> {
        &self.names
    }

    /// Return an `NodeName` for a name. Equality comparison
    /// on `NodeName` can be performed by checking physical
    /// equality.
    pub fn node_name(&mut self, name: &str) -> NodeName {
        if let Some(result) = self.names.get(name) {
            return NodeName(result.clone())
        }
        let shared = Rc::new(name.to_string());
        let result = NodeName(shared.clone());
        self.names.insert(name.to_string(), shared);
        result
    }
    pub fn get_node_name(&self, name: &str) -> Option<NodeName> {
        self.names.get(name)
            .map(|hit| NodeName(hit.clone()))
    }
    pub fn import_node_name(&mut self, node_name: &NodeName) {
        self.names.insert(node_name.to_string().clone(), node_name.0.clone());
    }

    pub fn field_name(&mut self, name: &str) -> FieldName {
        if let Some(result) = self.names.get(name) {
            return FieldName(result.clone());
        }
        let shared = Rc::new(name.to_string());
        let result = FieldName(shared.clone());
        self.names.insert(name.to_string(), shared);
        result
    }
    pub fn import_field_name(&mut self, field_name: &FieldName) {
        self.names.insert(field_name.to_string().clone(), field_name.0.clone());
    }

    pub fn add_interface(&mut self, name: &NodeName) -> Option<RefMut<InterfaceDeclaration>> {
        if self.interfaces_by_name.get(name).is_some() {
            return None;
        }
        let result = RefCell::new(InterfaceDeclaration {
            name: name.clone(),
            contents: Obj::new(),
        });
        self.interfaces_by_name.insert(name.clone(), result);
        self.interfaces_by_name.get(name)
            .map(RefCell::borrow_mut)
    }
    pub fn get_interface(&mut self, name: &NodeName) -> Option<RefMut<InterfaceDeclaration>> {
        self.interfaces_by_name.get(name)
            .map(RefCell::borrow_mut)
    }

    /// Add a named enumeration.
    pub fn add_string_enum(&mut self, name: &NodeName) -> Option<RefMut<StringEnum>> {
        if self.string_enums_by_name.get(name).is_some() {
            return None;
        }
        let e = RefCell::new(StringEnum {
            name: name.clone(),
            values: vec![]
        });
        self.string_enums_by_name.insert(name.clone(), e);
        self.string_enums_by_name.get(name).map(RefCell::borrow_mut)
    }

    pub fn add_typedef(&mut self, name: &NodeName) -> Option<RefMut<Type>> {
        if self.typedefs_by_name.get(name).is_some() {
            return None;
        }
        let e = RefCell::new(TypeSpec::Void.required());
        self.typedefs_by_name.insert(name.clone(), e);
        self.typedefs_by_name.get(name).map(RefCell::borrow_mut)
    }

    pub fn get_typedef(&self, name: &NodeName) -> Option<Ref<Type>> {
        self.typedefs_by_name.get(name).
            map(RefCell::borrow)
    }

    /// Generate the graph.
    pub fn into_spec<'a>(self, options: SpecOptions<'a>) -> Spec {
        // 1. Collect node names.
        let mut interfaces_by_name = self.interfaces_by_name;
        let interfaces_by_name : HashMap<_, _> = interfaces_by_name.drain()
            .map(|(k, v)| (k, Rc::new(Interface {
                declaration: RefCell::into_inner(v)
            })))
            .collect();
        let mut string_enums_by_name = self.string_enums_by_name;
        let string_enums_by_name : HashMap<_, _> = string_enums_by_name.drain()
            .map(|(k, v)| (k, Rc::new(RefCell::into_inner(v))))
            .collect();
        let mut typedefs_by_name = self.typedefs_by_name;
        let typedefs_by_name : HashMap<_, _> = typedefs_by_name.drain()
            .map(|(k, v)| (k, Rc::new(RefCell::into_inner(v))))
            .collect();

        let mut node_names = HashMap::new();
        for name in interfaces_by_name.keys().chain(string_enums_by_name.keys()).chain(typedefs_by_name.keys()) {
            node_names.insert(name.to_string().clone(), name.clone());
        }

        // 2. Collect all field names.
        let mut fields = HashMap::new();
        for interface in interfaces_by_name.values() {
            for field in &interface.declaration.contents.fields {
                fields.insert(field.name.to_string().clone(), field.name.clone());
            }
        }

        let mut resolved_type_sums_by_name : HashMap<NodeName, HashSet<NodeName>> = HashMap::new();
        {
            // 3. Check that node names are used but not duplicated.
            for name in node_names.values() {
                let mut instances = 0;
                if interfaces_by_name.contains_key(name) {
                    instances += 1;
                }
                if string_enums_by_name.contains_key(name) {
                    instances += 1;
                }
                if typedefs_by_name.contains_key(name) {
                    instances += 1;
                }
                assert!(instances > 0, "Type name {} is never used", name.to_str());
                assert_eq!(instances, 1, "Duplicate type name {}", name.to_str());
            }

            // 4. Check that all instances of `TypeSpec::NamedType` refer to an existing name.
            let mut used_typenames = HashSet::new();
            for type_ in typedefs_by_name.values() {
                for name in type_.spec().typenames() {
                    used_typenames.insert(name);
                }
            }
            for interface in interfaces_by_name.values() {
                for field in interface.declaration.contents.fields() {
                    for name in field.type_().spec().typenames() {
                        used_typenames.insert(name);
                    }
                }
            }
            for name in &used_typenames {
                if typedefs_by_name.contains_key(name) {
                    continue;
                }
                if interfaces_by_name.contains_key(name) {
                    continue;
                }
                if string_enums_by_name.contains_key(name) {
                    continue;
                }
                panic!("No definition for type {}", name.to_str());
            }

            #[derive(Clone, Debug)]
            enum TypeClassification {
                SumOfInterfaces(HashSet<NodeName>),
                Array,
                Primitive,
                StringEnum,
                Optional,
            }

            // 5. Classify typedefs between
            // - stuff that can only be put in a sum of interfaces (interfaces, sums of interfaces, typedefs thereof);
            // - stuff that can never be put in a sum of interfaces (other stuff)
            // - bad stuff that attempts to mix both

            // name => unbound if we haven't seen the name yet
            //      => `None` if we are currently classifying (used to detect cycles),
            //      => `Some(SumOfInterfaces(set))` if the name describes a sum of interfaces
            //      => `Some(BadForSumOfInterfaces)` if the name describes something that can't be summed with an interface
            let mut classification : HashMap<NodeName, Option<TypeClassification>> = HashMap::new();
            fn classify_type(typedefs_by_name: &HashMap<NodeName, Rc<Type>>,
                string_enums_by_name: &HashMap<NodeName, Rc<StringEnum>>,
                interfaces_by_name: &HashMap<NodeName, Rc<Interface>>,
                cache: &mut HashMap<NodeName, Option<TypeClassification>>, type_: &TypeSpec, name: &NodeName) -> TypeClassification
            {
                debug!(target: "spec", "classify_type for {:?}: walking {:?}", name, type_);
                match *type_ {
                    TypeSpec::Array { ref contents, .. } => {
                        // Check that the contents are correct.
                        let _ = classify_type(typedefs_by_name, string_enums_by_name, interfaces_by_name, cache, contents.spec(), name);
                        // Regardless, the result is bad for a sum of interfaces.
                        debug!(target: "spec", "classify_type => don't put me in an interface");
                        TypeClassification::Array
                    },
                    TypeSpec::Boolean | TypeSpec::Number | TypeSpec::UnsignedLong | TypeSpec::String | TypeSpec::Void | TypeSpec::Offset => {
                        debug!(target: "spec", "classify_type => don't put me in an interface");
                        TypeClassification::Primitive
                    }
                    TypeSpec::NamedType(ref name) => {
                        if let Some(fetch) = cache.get(name) {
                            if let Some(ref result) = *fetch {
                                debug!(target: "spec", "classify_type {:?} => (cached) {:?}", name, result);
                                return result.clone();
                            } else {
                                panic!("Cycle detected while examining {}", name.to_str());
                            }
                        }
                        // Start lookup for this name.
                        cache.insert(name.clone(), None);
                        let result = if interfaces_by_name.contains_key(name) {
                            let mut names = HashSet::new();
                            names.insert(name.clone());
                            TypeClassification::SumOfInterfaces(names)
                        } else if string_enums_by_name.contains_key(name) {
                            TypeClassification::StringEnum
                        } else {
                            let type_ = typedefs_by_name.get(name)
                                .unwrap(); // Completeness checked abover in this method.
                            classify_type(typedefs_by_name, string_enums_by_name, interfaces_by_name, cache, type_.spec(), name)
                        };
                        debug!(target: "spec", "classify_type {:?} => (inserting in cache) {:?}", name, result);
                        cache.insert(name.clone(), Some(result.clone()));
                        result
                    }
                    TypeSpec::TypeSum(ref sum) => {
                        let mut names = HashSet::new();
                        for type_ in sum.types() {
                            match classify_type(typedefs_by_name, string_enums_by_name, interfaces_by_name, cache, type_, name) {
                                TypeClassification::SumOfInterfaces(sum) => {
                                    names.extend(sum);
                                }
                                class =>
                                    panic!("In type {name}, there is a non-interface type {class:?} ({type_:?}) in a sum {sum:?}",
                                        name = name.to_str(),
                                        class = class,
                                        sum = sum,
                                        type_ = type_),
                            }
                        }
                        debug!(target: "spec", "classify_type => built sum {:?}", names);
                        TypeClassification::SumOfInterfaces(names)
                    }
                }
            }
            for (name, type_) in &typedefs_by_name {
                classification.insert(name.clone(), None);
                let class = classify_type(&typedefs_by_name, &string_enums_by_name, &interfaces_by_name, &mut classification, type_.spec(), name);
                if !type_.is_optional() {
                    classification.insert(name.clone(), Some(class));
                } else {
                    // FIXME: That looks weird.
                    classification.insert(name.clone(), Some(TypeClassification::Optional));
                }
            }

            // 6. Using this classification, check that the attributes of interfaces don't mix
            // poorly items of both kinds.
            for (name, interface) in &interfaces_by_name {
                for field in interface.declaration.contents.fields() {
                    classify_type(&typedefs_by_name, &string_enums_by_name, &interfaces_by_name, &mut classification, field.type_().spec(), name);
                }
            }

            // 7. Fill resolved_type_sums_by_name, for later use.
            for (name, class) in classification.drain() {
                if !typedefs_by_name.contains_key(&name) {
                    continue;
                }
                if let Some(TypeClassification::SumOfInterfaces(sum)) = class {
                    resolved_type_sums_by_name.insert(name, sum);
                }
            }
        }

        let spec = Spec {
            interfaces_by_name,
            string_enums_by_name,
            typedefs_by_name,
            resolved_type_sums_by_name,
            node_names,
            fields,
            root: options.root.clone(),
            null: options.null.clone(),
        };

        spec
    }
}

/// Representation of an interface in a grammar declaration.
///
/// Interfaces represent nodes in the AST. Each interface
/// has a name, a type, defines properties (also known as
/// `attribute` in webidl) which hold values.
#[derive(Debug)]
pub struct Interface {
    declaration: InterfaceDeclaration,
}

impl Interface {
    /// Returns the full list of fields for this structure.
    /// This method is in charge of:
    /// - ensuring that the fields of parent structures are properly accounted for;
    /// - disregarding ignored fields (i.e. `position`, `type`);
    /// - disregarding fields with a single possible value.
    pub fn contents(&self) -> &Obj {
        &self.declaration.contents
    }

    /// Returns the name of the interface.
    pub fn name(&self) -> &NodeName {
        &self.declaration.name
    }

    /// Returns a type specification for this interface.
    ///
    /// The result is a `NamedType` with this interface's name.
    pub fn spec(&self) -> TypeSpec {
        TypeSpec::NamedType(self.name().clone())
    }

    pub fn type_(&self) -> Type {
        self.spec().required()
    }

    pub fn get_field_by_name(&self, name: &FieldName) -> Option<&Field> {
        for field in self.contents().fields() {
            if name == field.name() {
                return Some(field)
            }
        }
        None
    }
}

/// Immutable representation of the spec.
pub struct Spec {
    interfaces_by_name: HashMap<NodeName, Rc<Interface>>,
    string_enums_by_name: HashMap<NodeName, Rc<StringEnum>>,
    typedefs_by_name: HashMap<NodeName, Rc<Type>>,

    resolved_type_sums_by_name: HashMap<NodeName, HashSet<NodeName>>,

    node_names: HashMap<String, NodeName>,
    fields: HashMap<String, FieldName>,
    root: NodeName,
    null: NodeName,
}

impl Spec {
    pub fn get_interface_by_name(&self, name: &NodeName) -> Option<&Interface> {
        self.interfaces_by_name.get(name)
            .map(std::borrow::Borrow::borrow)
    }
    pub fn interfaces_by_name(&self) -> &HashMap<NodeName, Rc<Interface>> {
        &self.interfaces_by_name
    }
    pub fn string_enums_by_name(&self) -> &HashMap<NodeName, Rc<StringEnum>> {
        &self.string_enums_by_name
    }
    pub fn typedefs_by_name(&self) -> &HashMap<NodeName, Rc<Type>> {
        &self.typedefs_by_name
    }
    pub fn resolved_sums_of_interfaces_by_name(&self) -> &HashMap<NodeName, HashSet<NodeName>> {
        &self.resolved_type_sums_by_name
    }

    pub fn get_type_by_name(&self, name: &NodeName) -> Option<NamedType> {
        if let Some(interface) = self.interfaces_by_name
            .get(name) {
            return Some(NamedType::Interface(interface.clone()))
        }
        if let Some(strings_enum) = self.string_enums_by_name
            .get(name) {
            return Some(NamedType::StringEnum(strings_enum.clone()))
        }
        if let Some(type_) = self.typedefs_by_name
            .get(name) {
            return Some(NamedType::Typedef(type_.clone()))
        }
        None
    }
    pub fn get_field_name(&self, name: &str) -> Option<&FieldName> {
        self.fields
            .get(name)
    }
    pub fn get_node_name(&self, name: &str) -> Option<&NodeName> {
        self.node_names
            .get(name)
    }
    pub fn node_names(&self) -> &HashMap<String, NodeName> {
        &self.node_names
    }
    pub fn field_names(&self) -> &HashMap<String, FieldName> {
        &self.fields
    }
    pub fn get_root_name(&self) -> &NodeName {
        &self.root
    }
    pub fn get_null_name(&self) -> &NodeName {
        &self.null
    }

    /// The starting point for parsing.
    pub fn get_root(&self) -> NamedType {
        self.get_type_by_name(&self.root)
            .unwrap()
    }
}

/// Informations passed during the creation of a `Spec` object.
pub struct SpecOptions<'a> {
    /// The name of the node used to start encoding.
    pub root: &'a NodeName,
    pub null: &'a NodeName,
}

pub trait HasInterfaces {
    fn get_interface(&self, spec: &Spec, name: &NodeName) -> Option<Rc<Interface>>;
}

impl HasInterfaces for NamedType {
    fn get_interface(&self, spec: &Spec, name: &NodeName) -> Option<Rc<Interface>> {
        debug!(target: "spec", "get_interface, looking for {:?} in named type {:?}", name, self);
        match *self {
            NamedType::Interface(_) => None,
            NamedType::StringEnum(_) => None,
            NamedType::Typedef(ref type_) =>
                type_.spec().get_interface(spec, name)
        }
    }
}

impl HasInterfaces for TypeSpec {
    fn get_interface(&self, spec: &Spec, name: &NodeName) -> Option<Rc<Interface>> {
        debug!(target: "spec", "get_interface, looking for {:?} in spec {:?}", name, self);
        match *self {
            TypeSpec::NamedType(ref my_name) => {
                let follow = spec.get_type_by_name(my_name);
                if let Some(follow) = follow {
                    if name == my_name {
                        follow.as_interface(spec)
                    } else {
                        follow.get_interface(spec, name)
                    }
                } else {
                    None
                }
            },
            TypeSpec::TypeSum(ref sum) =>
                sum.get_interface(spec, name),
            _ => None
        }
    }
}
