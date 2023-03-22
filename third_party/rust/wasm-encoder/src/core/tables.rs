use crate::{encode_section, Encode, Section, SectionId, ValType};

/// An encoder for the table section.
///
/// Table sections are only supported for modules.
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
#[derive(Clone, Default, Debug)]
pub struct TableSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl TableSection {
    /// Construct a new table section encoder.
    pub fn new() -> Self {
        Self::default()
    }

    /// The number of tables in the section.
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Determines if the section is empty.
    pub fn is_empty(&self) -> bool {
        self.num_added == 0
    }

    /// Define a table.
    pub fn table(&mut self, table_type: TableType) -> &mut Self {
        table_type.encode(&mut self.bytes);
        self.num_added += 1;
        self
    }
}

impl Encode for TableSection {
    fn encode(&self, sink: &mut Vec<u8>) {
        encode_section(sink, self.num_added, &self.bytes);
    }
}

impl Section for TableSection {
    fn id(&self) -> u8 {
        SectionId::Table.into()
    }
}

/// A table's type.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct TableType {
    /// The table's element type.
    pub element_type: ValType,
    /// Minimum size, in elements, of this table
    pub minimum: u32,
    /// Maximum size, in elements, of this table
    pub maximum: Option<u32>,
}

impl Encode for TableType {
    fn encode(&self, sink: &mut Vec<u8>) {
        let mut flags = 0;
        if self.maximum.is_some() {
            flags |= 0b001;
        }

        self.element_type.encode(sink);
        sink.push(flags);
        self.minimum.encode(sink);

        if let Some(max) = self.maximum {
            max.encode(sink);
        }
    }
}
