use super::*;
use std::convert::TryFrom;

/// An encoder for the import section.
///
/// # Example
///
/// ```
/// use wasm_encoder::{Module, ImportSection, MemoryType};
///
/// let mut imports = ImportSection::new();
/// imports.import(
///     "env",
///     Some("memory"),
///     MemoryType {
///         minimum: 1,
///         maximum: None,
///         memory64: false,
///     }
/// );
///
/// let mut module = Module::new();
/// module.section(&imports);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct ImportSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl ImportSection {
    /// Construct a new import section encoder.
    pub fn new() -> ImportSection {
        ImportSection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many imports have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Define an import.
    pub fn import(
        &mut self,
        module: &str,
        name: Option<&str>,
        ty: impl Into<EntityType>,
    ) -> &mut Self {
        self.bytes.extend(encoders::str(module));
        match name {
            Some(name) => self.bytes.extend(encoders::str(name)),
            None => {
                self.bytes.push(0x00);
                self.bytes.push(0xff);
            }
        }
        ty.into().encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Section for ImportSection {
    fn id(&self) -> u8 {
        SectionId::Import.into()
    }

    fn encode<S>(&self, sink: &mut S)
    where
        S: Extend<u8>,
    {
        let num_added = encoders::u32(self.num_added);
        let n = num_added.len();
        sink.extend(
            encoders::u32(u32::try_from(n + self.bytes.len()).unwrap())
                .chain(num_added)
                .chain(self.bytes.iter().copied()),
        );
    }
}

/// The type of an entity.
#[derive(Clone, Copy, Debug)]
pub enum EntityType {
    /// The `n`th type, which is a function.
    Function(u32),
    /// A table type.
    Table(TableType),
    /// A memory type.
    Memory(MemoryType),
    /// A global type.
    Global(GlobalType),
    /// The `n`th type, which is an instance.
    Instance(u32),
    /// The `n`th type, which is a module.
    Module(u32),
}

// NB: no `impl From<u32> for ImportType` because instances and modules also use
// `u32` indices in module linking, so we would have to remove that impl when
// adding support for module linking anyways.

impl From<TableType> for EntityType {
    fn from(t: TableType) -> Self {
        EntityType::Table(t)
    }
}

impl From<MemoryType> for EntityType {
    fn from(m: MemoryType) -> Self {
        EntityType::Memory(m)
    }
}

impl From<GlobalType> for EntityType {
    fn from(g: GlobalType) -> Self {
        EntityType::Global(g)
    }
}

impl EntityType {
    pub(crate) fn encode(&self, dst: &mut Vec<u8>) {
        match self {
            EntityType::Function(x) => {
                dst.push(0x00);
                dst.extend(encoders::u32(*x));
            }
            EntityType::Table(ty) => {
                dst.push(0x01);
                ty.encode(dst);
            }
            EntityType::Memory(ty) => {
                dst.push(0x02);
                ty.encode(dst);
            }
            EntityType::Global(ty) => {
                dst.push(0x03);
                ty.encode(dst);
            }
            EntityType::Module(ty) => {
                dst.push(0x05);
                dst.extend(encoders::u32(*ty));
            }
            EntityType::Instance(ty) => {
                dst.push(0x06);
                dst.extend(encoders::u32(*ty));
            }
        }
    }
}
