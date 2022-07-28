use super::{COMPONENT_SORT, CORE_MODULE_SORT, CORE_SORT, CORE_TYPE_SORT, TYPE_SORT};
use crate::{
    encode_section, ComponentExportKind, ComponentSection, ComponentSectionId, Encode, ExportKind,
};

/// Represents the kinds of outer core aliasable items in a component.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CoreOuterAliasKind {
    /// The alias is to a core type.
    Type,
}

impl Encode for CoreOuterAliasKind {
    fn encode(&self, sink: &mut Vec<u8>) {
        match self {
            Self::Type => {
                sink.push(CORE_TYPE_SORT);
            }
        }
    }
}

/// An encoder for the core alias section of WebAssembly components.
///
/// # Example
///
/// ```rust
/// use wasm_encoder::{Component, AliasSection, ExportKind};
///
/// let mut aliases = AliasSection::new();
/// aliases.instance_export(0, ExportKind::Func, "f");
///
/// let mut component = Component::new();
/// component.section(&aliases);
///
/// let bytes = component.finish();
/// ```
#[derive(Clone, Debug, Default)]
pub struct AliasSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl AliasSection {
    /// Create a new core alias section encoder.
    pub fn new() -> Self {
        Self::default()
    }

    /// The number of aliases in the section.
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Determines if the section is empty.
    pub fn is_empty(&self) -> bool {
        self.num_added == 0
    }

    /// Define an alias to an instance's export.
    pub fn instance_export(
        &mut self,
        instance_index: u32,
        kind: ExportKind,
        name: &str,
    ) -> &mut Self {
        kind.encode(&mut self.bytes);
        self.bytes.push(0x00);
        instance_index.encode(&mut self.bytes);
        name.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }

    /// Define an alias to an outer core item.
    ///
    /// The count starts at 0 to indicate the current component, 1 indicates the direct
    /// parent, 2 the grandparent, etc.
    pub fn outer(&mut self, count: u32, kind: CoreOuterAliasKind, index: u32) -> &mut Self {
        kind.encode(&mut self.bytes);
        self.bytes.push(0x01);
        count.encode(&mut self.bytes);
        index.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Encode for AliasSection {
    fn encode(&self, sink: &mut Vec<u8>) {
        encode_section(sink, self.num_added, &self.bytes);
    }
}

impl ComponentSection for AliasSection {
    fn id(&self) -> u8 {
        ComponentSectionId::CoreAlias.into()
    }
}

/// Represents the kinds of outer aliasable items in a component.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ComponentOuterAliasKind {
    /// The alias is to a core module.
    CoreModule,
    /// The alias is to a core type.
    CoreType,
    /// The alias is to a type.
    Type,
    /// The alias is to a component.
    Component,
}

impl Encode for ComponentOuterAliasKind {
    fn encode(&self, sink: &mut Vec<u8>) {
        match self {
            Self::CoreModule => {
                sink.push(CORE_SORT);
                sink.push(CORE_MODULE_SORT);
            }
            Self::CoreType => {
                sink.push(CORE_SORT);
                sink.push(CORE_TYPE_SORT);
            }
            Self::Type => sink.push(TYPE_SORT),
            Self::Component => sink.push(COMPONENT_SORT),
        }
    }
}

/// An encoder for the alias section of WebAssembly component.
///
/// # Example
///
/// ```rust
/// use wasm_encoder::{Component, ComponentAliasSection, ComponentExportKind, ComponentOuterAliasKind};
///
/// let mut aliases = ComponentAliasSection::new();
/// aliases.instance_export(0, ComponentExportKind::Func, "f");
/// aliases.outer(0, ComponentOuterAliasKind::Type, 1);
///
/// let mut component = Component::new();
/// component.section(&aliases);
///
/// let bytes = component.finish();
/// ```
#[derive(Clone, Debug, Default)]
pub struct ComponentAliasSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl ComponentAliasSection {
    /// Create a new alias section encoder.
    pub fn new() -> Self {
        Self::default()
    }

    /// The number of aliases in the section.
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Determines if the section is empty.
    pub fn is_empty(&self) -> bool {
        self.num_added == 0
    }

    /// Define an alias to an instance's export.
    pub fn instance_export(
        &mut self,
        instance_index: u32,
        kind: ComponentExportKind,
        name: &str,
    ) -> &mut Self {
        kind.encode(&mut self.bytes);
        self.bytes.push(0x00);
        instance_index.encode(&mut self.bytes);
        name.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }

    /// Define an alias to an outer component item.
    ///
    /// The count starts at 0 to indicate the current component, 1 indicates the direct
    /// parent, 2 the grandparent, etc.
    pub fn outer(&mut self, count: u32, kind: ComponentOuterAliasKind, index: u32) -> &mut Self {
        kind.encode(&mut self.bytes);
        self.bytes.push(0x01);
        count.encode(&mut self.bytes);
        index.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Encode for ComponentAliasSection {
    fn encode(&self, sink: &mut Vec<u8>) {
        encode_section(sink, self.num_added, &self.bytes);
    }
}

impl ComponentSection for ComponentAliasSection {
    fn id(&self) -> u8 {
        ComponentSectionId::Alias.into()
    }
}
