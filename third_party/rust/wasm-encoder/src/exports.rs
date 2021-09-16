use super::*;

/// An encoder for the export section.
///
/// # Example
///
/// ```
/// use wasm_encoder::{
///     Export, ExportSection, TableSection, TableType, Module, ValType,
/// };
///
/// let mut tables = TableSection::new();
/// tables.table(TableType {
///     element_type: ValType::FuncRef,
///     minimum: 128,
///     maximum: None,
/// });
///
/// let mut exports = ExportSection::new();
/// exports.export("my-table", Export::Table(0));
///
/// let mut module = Module::new();
/// module
///     .section(&tables)
///     .section(&exports);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct ExportSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl ExportSection {
    /// Create a new export section encoder.
    pub fn new() -> ExportSection {
        ExportSection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many exports have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Define an export.
    pub fn export(&mut self, name: &str, export: Export) -> &mut Self {
        self.bytes.extend(encoders::str(name));
        export.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Section for ExportSection {
    fn id(&self) -> u8 {
        SectionId::Export.into()
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

/// A WebAssembly export.
#[derive(Clone, Copy, Debug)]
pub enum Export {
    /// An export of the `n`th function.
    Function(u32),
    /// An export of the `n`th table.
    Table(u32),
    /// An export of the `n`th memory.
    Memory(u32),
    /// An export of the `n`th global.
    Global(u32),
    /// An export of the `n`th instance.
    ///
    /// Note that this is part of the [module linking proposal][proposal] and is
    /// not currently part of stable WebAssembly.
    ///
    /// [proposal]: https://github.com/webassembly/module-linking
    Instance(u32),
    /// An export of the `n`th module.
    ///
    /// Note that this is part of the [module linking proposal][proposal] and is
    /// not currently part of stable WebAssembly.
    ///
    /// [proposal]: https://github.com/webassembly/module-linking
    Module(u32),
}

impl Export {
    pub(crate) fn encode(&self, bytes: &mut Vec<u8>) {
        let idx = match *self {
            Export::Function(x) => {
                bytes.push(ItemKind::Function as u8);
                x
            }
            Export::Table(x) => {
                bytes.push(ItemKind::Table as u8);
                x
            }
            Export::Memory(x) => {
                bytes.push(ItemKind::Memory as u8);
                x
            }
            Export::Global(x) => {
                bytes.push(ItemKind::Global as u8);
                x
            }
            Export::Instance(x) => {
                bytes.push(ItemKind::Instance as u8);
                x
            }
            Export::Module(x) => {
                bytes.push(ItemKind::Module as u8);
                x
            }
        };
        bytes.extend(encoders::u32(idx));
    }
}

/// Kinds of WebAssembly items
#[allow(missing_docs)]
#[repr(u8)]
#[derive(Clone, Copy, Debug)]
pub enum ItemKind {
    Function = 0x00,
    Table = 0x01,
    Memory = 0x02,
    Global = 0x03,
    Module = 0x05,
    Instance = 0x06,
}
