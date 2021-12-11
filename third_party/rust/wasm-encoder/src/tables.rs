use super::*;

/// An encoder for the table section.
///
/// # Example
///
/// ```
/// use wasm_encoder::{Module, TableSection, TableType, ValType};
///
/// let mut tables = TableSection::new();
/// tables.table(TableType {
///     element_type: ValType::FuncRef,
///     minimum: 128,
///     maximum: None,
/// });
///
/// let mut module = Module::new();
/// module.section(&tables);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Debug)]
pub struct TableSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl TableSection {
    /// Construct a new table section encoder.
    pub fn new() -> TableSection {
        TableSection {
            bytes: vec![],
            num_added: 0,
        }
    }

    /// How many tables have been defined inside this section so far?
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Define a table.
    pub fn table(&mut self, table_type: TableType) -> &mut Self {
        table_type.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Section for TableSection {
    fn id(&self) -> u8 {
        SectionId::Table.into()
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

/// A table's type.
#[derive(Clone, Copy, Debug)]
pub struct TableType {
    /// The table's element type.
    pub element_type: ValType,
    /// Minimum size, in elements, of this table
    pub minimum: u32,
    /// Maximum size, in elements, of this table
    pub maximum: Option<u32>,
}

impl TableType {
    pub(crate) fn encode(&self, bytes: &mut Vec<u8>) {
        bytes.push(self.element_type.into());
        let mut flags = 0;
        if self.maximum.is_some() {
            flags |= 0b001;
        }
        bytes.push(flags);
        bytes.extend(encoders::u32(self.minimum));
        if let Some(max) = self.maximum {
            bytes.extend(encoders::u32(max));
        }
    }
}
