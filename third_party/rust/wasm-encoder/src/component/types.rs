use crate::{
    encode_section, ComponentOuterAliasKind, ComponentSection, ComponentSectionId,
    ComponentTypeRef, CoreOuterAliasKind, Encode, EntityType, ValType,
};

/// Represents the type of a core module.
#[derive(Debug, Clone, Default)]
pub struct ModuleType {
    bytes: Vec<u8>,
    num_added: u32,
    types_added: u32,
}

impl ModuleType {
    /// Creates a new core module type.
    pub fn new() -> Self {
        Self::default()
    }

    /// Defines an outer core type alias in this module type.
    pub fn alias_outer_core_type(&mut self, count: u32, index: u32) -> &mut Self {
        self.bytes.push(0x02);
        CoreOuterAliasKind::Type.encode(&mut self.bytes);
        self.bytes.push(0x01);
        count.encode(&mut self.bytes);
        index.encode(&mut self.bytes);
        self.num_added += 1;
        self.types_added += 1;
        self
    }

    /// Defines an import in this module type.
    pub fn import(&mut self, module: &str, name: &str, ty: EntityType) -> &mut Self {
        self.bytes.push(0x00);
        module.encode(&mut self.bytes);
        name.encode(&mut self.bytes);
        ty.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }

    /// Define a type in this module type.
    ///
    /// The returned encoder must be used before adding another definition.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn ty(&mut self) -> CoreTypeEncoder {
        self.bytes.push(0x01);
        self.num_added += 1;
        self.types_added += 1;
        CoreTypeEncoder(&mut self.bytes)
    }

    /// Defines an export in this module type.
    pub fn export(&mut self, name: &str, ty: EntityType) -> &mut Self {
        self.bytes.push(0x03);
        name.encode(&mut self.bytes);
        ty.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }

    /// Gets the number of types that have been added to this module type.
    pub fn type_count(&self) -> u32 {
        self.types_added
    }
}

impl Encode for ModuleType {
    fn encode(&self, sink: &mut Vec<u8>) {
        sink.push(0x50);
        self.num_added.encode(sink);
        sink.extend(&self.bytes);
    }
}

/// Used to encode core types.
#[derive(Debug)]
pub struct CoreTypeEncoder<'a>(pub(crate) &'a mut Vec<u8>);

impl<'a> CoreTypeEncoder<'a> {
    /// Define a function type.
    pub fn function<P, R>(self, params: P, results: R)
    where
        P: IntoIterator<Item = ValType>,
        P::IntoIter: ExactSizeIterator,
        R: IntoIterator<Item = ValType>,
        R::IntoIter: ExactSizeIterator,
    {
        let params = params.into_iter();
        let results = results.into_iter();

        self.0.push(0x60);
        params.len().encode(self.0);
        self.0.extend(params.map(u8::from));
        results.len().encode(self.0);
        self.0.extend(results.map(u8::from));
    }

    /// Define a module type.
    pub fn module(self, ty: &ModuleType) {
        ty.encode(self.0);
    }
}

/// An encoder for the core type section of WebAssembly components.
///
/// # Example
///
/// ```rust
/// use wasm_encoder::{Component, CoreTypeSection, ModuleType};
///
/// let mut types = CoreTypeSection::new();
///
/// types.module(&ModuleType::new());
///
/// let mut component = Component::new();
/// component.section(&types);
///
/// let bytes = component.finish();
/// ```
#[derive(Clone, Debug, Default)]
pub struct CoreTypeSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl CoreTypeSection {
    /// Create a new core type section encoder.
    pub fn new() -> Self {
        Self::default()
    }

    /// The number of types in the section.
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Determines if the section is empty.
    pub fn is_empty(&self) -> bool {
        self.num_added == 0
    }

    /// Encode a type into this section.
    ///
    /// The returned encoder must be finished before adding another type.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn ty(&mut self) -> CoreTypeEncoder<'_> {
        self.num_added += 1;
        CoreTypeEncoder(&mut self.bytes)
    }

    /// Define a function type in this type section.
    pub fn function<P, R>(&mut self, params: P, results: R) -> &mut Self
    where
        P: IntoIterator<Item = ValType>,
        P::IntoIter: ExactSizeIterator,
        R: IntoIterator<Item = ValType>,
        R::IntoIter: ExactSizeIterator,
    {
        self.ty().function(params, results);
        self
    }

    /// Define a module type in this type section.
    ///
    /// Currently this is only used for core type sections in components.
    pub fn module(&mut self, ty: &ModuleType) -> &mut Self {
        self.ty().module(ty);
        self
    }
}

impl Encode for CoreTypeSection {
    fn encode(&self, sink: &mut Vec<u8>) {
        encode_section(sink, self.num_added, &self.bytes);
    }
}

impl ComponentSection for CoreTypeSection {
    fn id(&self) -> u8 {
        ComponentSectionId::CoreType.into()
    }
}

/// Represents a component type.
#[derive(Debug, Clone, Default)]
pub struct ComponentType {
    bytes: Vec<u8>,
    num_added: u32,
    core_types_added: u32,
    types_added: u32,
}

impl ComponentType {
    /// Creates a new component type.
    pub fn new() -> Self {
        Self::default()
    }

    /// Define a core type in this component type.
    ///
    /// The returned encoder must be used before adding another definition.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn core_type(&mut self) -> CoreTypeEncoder {
        self.bytes.push(0x00);
        self.num_added += 1;
        self.core_types_added += 1;
        CoreTypeEncoder(&mut self.bytes)
    }

    /// Define a type in this component type.
    ///
    /// The returned encoder must be used before adding another definition.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn ty(&mut self) -> ComponentTypeEncoder {
        self.bytes.push(0x01);
        self.num_added += 1;
        self.types_added += 1;
        ComponentTypeEncoder(&mut self.bytes)
    }

    /// Defines an outer core type alias in this component type.
    pub fn alias_outer_core_type(&mut self, count: u32, index: u32) -> &mut Self {
        self.bytes.push(0x02);
        ComponentOuterAliasKind::CoreType.encode(&mut self.bytes);
        self.bytes.push(0x01);
        count.encode(&mut self.bytes);
        index.encode(&mut self.bytes);
        self.num_added += 1;
        self.types_added += 1;
        self
    }

    /// Defines an outer type alias in this component type.
    pub fn alias_outer_type(&mut self, count: u32, index: u32) -> &mut Self {
        self.bytes.push(0x02);
        ComponentOuterAliasKind::Type.encode(&mut self.bytes);
        self.bytes.push(0x01);
        count.encode(&mut self.bytes);
        index.encode(&mut self.bytes);
        self.num_added += 1;
        self.types_added += 1;
        self
    }

    /// Defines an import in this component type.
    pub fn import(&mut self, name: &str, ty: ComponentTypeRef) -> &mut Self {
        self.bytes.push(0x03);
        name.encode(&mut self.bytes);
        ty.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }

    /// Defines an export in this component type.
    pub fn export(&mut self, name: &str, ty: ComponentTypeRef) -> &mut Self {
        self.bytes.push(0x04);
        name.encode(&mut self.bytes);
        ty.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }

    /// Gets the number of core types that have been added to this component type.
    pub fn core_type_count(&self) -> u32 {
        self.core_types_added
    }

    /// Gets the number of types that have been added or aliased in this component type.
    pub fn type_count(&self) -> u32 {
        self.types_added
    }
}

impl Encode for ComponentType {
    fn encode(&self, sink: &mut Vec<u8>) {
        sink.push(0x41);
        self.num_added.encode(sink);
        sink.extend(&self.bytes);
    }
}

/// Represents an instance type.
#[derive(Debug, Clone, Default)]
pub struct InstanceType {
    bytes: Vec<u8>,
    num_added: u32,
    core_types_added: u32,
    types_added: u32,
}

impl InstanceType {
    /// Creates a new instance type.
    pub fn new() -> Self {
        Self::default()
    }

    /// Define a core type in this instance type.
    ///
    /// The returned encoder must be used before adding another definition.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn core_type(&mut self) -> CoreTypeEncoder {
        self.bytes.push(0x00);
        self.num_added += 1;
        self.core_types_added += 1;
        CoreTypeEncoder(&mut self.bytes)
    }

    /// Define a type in this instance type.
    ///
    /// The returned encoder must be used before adding another definition.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn ty(&mut self) -> ComponentTypeEncoder {
        self.bytes.push(0x01);
        self.num_added += 1;
        self.types_added += 1;
        ComponentTypeEncoder(&mut self.bytes)
    }

    /// Defines an outer core type alias in this component type.
    pub fn alias_outer_core_type(&mut self, count: u32, index: u32) -> &mut Self {
        self.bytes.push(0x02);
        ComponentOuterAliasKind::CoreType.encode(&mut self.bytes);
        self.bytes.push(0x01);
        count.encode(&mut self.bytes);
        index.encode(&mut self.bytes);
        self.num_added += 1;
        self.types_added += 1;
        self
    }

    /// Defines an alias in this instance type.
    pub fn alias_outer_type(&mut self, count: u32, index: u32) -> &mut Self {
        self.bytes.push(0x02);
        ComponentOuterAliasKind::Type.encode(&mut self.bytes);
        self.bytes.push(0x01);
        count.encode(&mut self.bytes);
        index.encode(&mut self.bytes);
        self.num_added += 1;
        self.types_added += 1;
        self
    }

    /// Defines an export in this instance type.
    pub fn export(&mut self, name: &str, ty: ComponentTypeRef) -> &mut Self {
        self.bytes.push(0x04);
        name.encode(&mut self.bytes);
        ty.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }

    /// Gets the number of core types that have been added to this instance type.
    pub fn core_type_count(&self) -> u32 {
        self.core_types_added
    }

    /// Gets the number of types that have been added or aliased in this instance type.
    pub fn type_count(&self) -> u32 {
        self.types_added
    }
}

impl Encode for InstanceType {
    fn encode(&self, sink: &mut Vec<u8>) {
        sink.push(0x42);
        self.num_added.encode(sink);
        sink.extend(&self.bytes);
    }
}

/// Used to encode component and instance types.
#[derive(Debug)]
pub struct ComponentTypeEncoder<'a>(&'a mut Vec<u8>);

impl<'a> ComponentTypeEncoder<'a> {
    /// Define a component type.
    pub fn component(self, ty: &ComponentType) {
        ty.encode(self.0);
    }

    /// Define an instance type.
    pub fn instance(self, ty: &InstanceType) {
        ty.encode(self.0);
    }

    /// Define a function type.
    pub fn function<'b, P, T>(self, params: P, result: impl Into<ComponentValType>)
    where
        P: IntoIterator<Item = (Option<&'b str>, T)>,
        P::IntoIter: ExactSizeIterator,
        T: Into<ComponentValType>,
    {
        let params = params.into_iter();
        self.0.push(0x40);

        params.len().encode(self.0);
        for (name, ty) in params {
            match name {
                Some(name) => {
                    self.0.push(0x01);
                    name.encode(self.0);
                }
                None => self.0.push(0x00),
            }
            ty.into().encode(self.0);
        }

        result.into().encode(self.0);
    }

    /// Define a defined component type.
    ///
    /// The returned encoder must be used before adding another type.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn defined_type(self) -> ComponentDefinedTypeEncoder<'a> {
        ComponentDefinedTypeEncoder(self.0)
    }
}

/// Represents a primitive component value type.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum PrimitiveValType {
    /// The type is the unit type.
    Unit,
    /// The type is a boolean.
    Bool,
    /// The type is a signed 8-bit integer.
    S8,
    /// The type is an unsigned 8-bit integer.
    U8,
    /// The type is a signed 16-bit integer.
    S16,
    /// The type is an unsigned 16-bit integer.
    U16,
    /// The type is a signed 32-bit integer.
    S32,
    /// The type is an unsigned 32-bit integer.
    U32,
    /// The type is a signed 64-bit integer.
    S64,
    /// The type is an unsigned 64-bit integer.
    U64,
    /// The type is a 32-bit floating point number.
    Float32,
    /// The type is a 64-bit floating point number.
    Float64,
    /// The type is a Unicode character.
    Char,
    /// The type is a string.
    String,
}

impl Encode for PrimitiveValType {
    fn encode(&self, sink: &mut Vec<u8>) {
        sink.push(match self {
            Self::Unit => 0x7f,
            Self::Bool => 0x7e,
            Self::S8 => 0x7d,
            Self::U8 => 0x7c,
            Self::S16 => 0x7b,
            Self::U16 => 0x7a,
            Self::S32 => 0x79,
            Self::U32 => 0x78,
            Self::S64 => 0x77,
            Self::U64 => 0x76,
            Self::Float32 => 0x75,
            Self::Float64 => 0x74,
            Self::Char => 0x73,
            Self::String => 0x72,
        });
    }
}

/// Represents a component value type.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ComponentValType {
    /// The value is a primitive type.
    Primitive(PrimitiveValType),
    /// The value is to a defined value type.
    ///
    /// The type index must be to a value type.
    Type(u32),
}

impl Encode for ComponentValType {
    fn encode(&self, sink: &mut Vec<u8>) {
        match self {
            Self::Primitive(ty) => ty.encode(sink),
            Self::Type(index) => (*index as i64).encode(sink),
        }
    }
}

impl From<PrimitiveValType> for ComponentValType {
    fn from(ty: PrimitiveValType) -> Self {
        Self::Primitive(ty)
    }
}

/// Used for encoding component defined types.
#[derive(Debug)]
pub struct ComponentDefinedTypeEncoder<'a>(&'a mut Vec<u8>);

impl ComponentDefinedTypeEncoder<'_> {
    /// Define a primitive value type.
    pub fn primitive(self, ty: PrimitiveValType) {
        ty.encode(self.0);
    }

    /// Define a record type.
    pub fn record<'a, F, T>(self, fields: F)
    where
        F: IntoIterator<Item = (&'a str, T)>,
        F::IntoIter: ExactSizeIterator,
        T: Into<ComponentValType>,
    {
        let fields = fields.into_iter();
        self.0.push(0x71);
        fields.len().encode(self.0);
        for (name, ty) in fields {
            name.encode(self.0);
            ty.into().encode(self.0);
        }
    }

    /// Define a variant type.
    pub fn variant<'a, C, T>(self, cases: C)
    where
        C: IntoIterator<Item = (&'a str, T, Option<u32>)>,
        C::IntoIter: ExactSizeIterator,
        T: Into<ComponentValType>,
    {
        let cases = cases.into_iter();
        self.0.push(0x70);
        cases.len().encode(self.0);
        for (name, ty, refines) in cases {
            name.encode(self.0);
            ty.into().encode(self.0);
            if let Some(default) = refines {
                self.0.push(0x01);
                default.encode(self.0);
            } else {
                self.0.push(0x00);
            }
        }
    }

    /// Define a list type.
    pub fn list(self, ty: impl Into<ComponentValType>) {
        self.0.push(0x6f);
        ty.into().encode(self.0);
    }

    /// Define a tuple type.
    pub fn tuple<I, T>(self, types: I)
    where
        I: IntoIterator<Item = T>,
        I::IntoIter: ExactSizeIterator,
        T: Into<ComponentValType>,
    {
        let types = types.into_iter();
        self.0.push(0x6E);
        types.len().encode(self.0);
        for ty in types {
            ty.into().encode(self.0);
        }
    }

    /// Define a flags type.
    pub fn flags<'a, I>(self, names: I)
    where
        I: IntoIterator<Item = &'a str>,
        I::IntoIter: ExactSizeIterator,
    {
        let names = names.into_iter();
        self.0.push(0x6D);
        names.len().encode(self.0);
        for name in names {
            name.encode(self.0);
        }
    }

    /// Define an enum type.
    pub fn enum_type<'a, I>(self, tags: I)
    where
        I: IntoIterator<Item = &'a str>,
        I::IntoIter: ExactSizeIterator,
    {
        let tags = tags.into_iter();
        self.0.push(0x6C);
        tags.len().encode(self.0);
        for tag in tags {
            tag.encode(self.0);
        }
    }

    /// Define a union type.
    pub fn union<I, T>(self, types: I)
    where
        I: IntoIterator<Item = T>,
        I::IntoIter: ExactSizeIterator,
        T: Into<ComponentValType>,
    {
        let types = types.into_iter();
        self.0.push(0x6B);
        types.len().encode(self.0);
        for ty in types {
            ty.into().encode(self.0);
        }
    }

    /// Define an option type.
    pub fn option(self, ty: impl Into<ComponentValType>) {
        self.0.push(0x6A);
        ty.into().encode(self.0);
    }

    /// Define an expected type.
    pub fn expected(self, ok: impl Into<ComponentValType>, error: impl Into<ComponentValType>) {
        self.0.push(0x69);
        ok.into().encode(self.0);
        error.into().encode(self.0);
    }
}

/// An encoder for the type section of WebAssembly components.
///
/// # Example
///
/// ```rust
/// use wasm_encoder::{Component, ComponentTypeSection, PrimitiveValType};
///
/// let mut types = ComponentTypeSection::new();
///
/// // Define a function type of `[string, string] -> string`.
/// types.function(
///   [
///     (Some("a"), PrimitiveValType::String),
///     (Some("b"), PrimitiveValType::String)
///   ],
///   PrimitiveValType::String
/// );
///
/// let mut component = Component::new();
/// component.section(&types);
///
/// let bytes = component.finish();
/// ```
#[derive(Clone, Debug, Default)]
pub struct ComponentTypeSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl ComponentTypeSection {
    /// Create a new component type section encoder.
    pub fn new() -> Self {
        Self::default()
    }

    /// The number of types in the section.
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Determines if the section is empty.
    pub fn is_empty(&self) -> bool {
        self.num_added == 0
    }

    /// Encode a type into this section.
    ///
    /// The returned encoder must be finished before adding another type.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn ty(&mut self) -> ComponentTypeEncoder<'_> {
        self.num_added += 1;
        ComponentTypeEncoder(&mut self.bytes)
    }

    /// Define a component type in this type section.
    pub fn component(&mut self, ty: &ComponentType) -> &mut Self {
        self.ty().component(ty);
        self
    }

    /// Define an instance type in this type section.
    pub fn instance(&mut self, ty: &InstanceType) -> &mut Self {
        self.ty().instance(ty);
        self
    }

    /// Define a function type in this type section.
    pub fn function<'a, P, T>(
        &mut self,
        params: P,
        result: impl Into<ComponentValType>,
    ) -> &mut Self
    where
        P: IntoIterator<Item = (Option<&'a str>, T)>,
        P::IntoIter: ExactSizeIterator,
        T: Into<ComponentValType>,
    {
        self.ty().function(params, result);
        self
    }

    /// Add a component defined type to this type section.
    ///
    /// The returned encoder must be used before adding another type.
    #[must_use = "the encoder must be used to encode the type"]
    pub fn defined_type(&mut self) -> ComponentDefinedTypeEncoder<'_> {
        self.ty().defined_type()
    }
}

impl Encode for ComponentTypeSection {
    fn encode(&self, sink: &mut Vec<u8>) {
        encode_section(sink, self.num_added, &self.bytes);
    }
}

impl ComponentSection for ComponentTypeSection {
    fn id(&self) -> u8 {
        ComponentSectionId::Type.into()
    }
}
