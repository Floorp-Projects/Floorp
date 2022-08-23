use crate::{encode_section, ConstExpr, Encode, Section, SectionId, ValType};

/// An encoder for the element section.
///
/// Element sections are only supported for modules.
///
/// # Example
///
/// ```
/// use wasm_encoder::{
///     Elements, ElementSection, Module, TableSection, TableType,
///     ValType, ConstExpr
/// };
///
/// let mut tables = TableSection::new();
/// tables.table(TableType {
///     element_type: ValType::FuncRef,
///     minimum: 128,
///     maximum: None,
/// });
///
/// let mut elements = ElementSection::new();
/// let table_index = 0;
/// let offset = ConstExpr::i32_const(42);
/// let element_type = ValType::FuncRef;
/// let functions = Elements::Functions(&[
///     // Function indices...
/// ]);
/// elements.active(Some(table_index), &offset, element_type, functions);
///
/// let mut module = Module::new();
/// module
///     .section(&tables)
///     .section(&elements);
///
/// let wasm_bytes = module.finish();
/// ```
#[derive(Clone, Default, Debug)]
pub struct ElementSection {
    bytes: Vec<u8>,
    num_added: u32,
}

/// A sequence of elements in a segment in the element section.
#[derive(Clone, Copy, Debug)]
pub enum Elements<'a> {
    /// A sequences of references to functions by their indices.
    Functions(&'a [u32]),
    /// A sequence of reference expressions.
    Expressions(&'a [ConstExpr]),
}

/// An element segment's mode.
#[derive(Clone, Debug)]
pub enum ElementMode<'a> {
    /// A passive element segment.
    ///
    /// Passive segments are part of the bulk memory proposal.
    Passive,
    /// A declared element segment.
    ///
    /// Declared segments are part of the bulk memory proposal.
    Declared,
    /// An active element segment.
    Active {
        /// The table index.
        ///
        /// `Active` element specifying a `None` table forces the MVP encoding and refers to the
        /// 0th table holding `funcref`s. Non-`None` tables use the encoding introduced with the
        /// bulk memory proposal and can refer to tables with any valid reference type.
        table: Option<u32>,
        /// The offset within the table to place this segment.
        offset: &'a ConstExpr,
    },
}

/// An element segment in the element section.
#[derive(Clone, Debug)]
pub struct ElementSegment<'a> {
    /// The element segment's mode.
    pub mode: ElementMode<'a>,
    /// The element segment's type.
    pub element_type: ValType,
    /// This segment's elements.
    pub elements: Elements<'a>,
}

impl ElementSection {
    /// Create a new element section encoder.
    pub fn new() -> Self {
        Self::default()
    }

    /// The number of element segments in the section.
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Determines if the section is empty.
    pub fn is_empty(&self) -> bool {
        self.num_added == 0
    }

    /// Define an element segment.
    pub fn segment<'a>(&mut self, segment: ElementSegment<'a>) -> &mut Self {
        let expr_bit = match segment.elements {
            Elements::Expressions(_) => 0b100u32,
            Elements::Functions(_) => 0b000u32,
        };
        match &segment.mode {
            ElementMode::Active {
                table: None,
                offset,
            } => {
                (/* 0x00 | */expr_bit).encode(&mut self.bytes);
                offset.encode(&mut self.bytes);
            }
            ElementMode::Passive => {
                (0x01 | expr_bit).encode(&mut self.bytes);
                if expr_bit == 0 {
                    self.bytes.push(0x00); // elemkind == funcref
                } else {
                    segment.element_type.encode(&mut self.bytes);
                }
            }
            ElementMode::Active {
                table: Some(i),
                offset,
            } => {
                (0x02 | expr_bit).encode(&mut self.bytes);
                i.encode(&mut self.bytes);
                offset.encode(&mut self.bytes);
                if expr_bit == 0 {
                    self.bytes.push(0x00); // elemkind == funcref
                } else {
                    segment.element_type.encode(&mut self.bytes);
                }
            }
            ElementMode::Declared => {
                (0x03 | expr_bit).encode(&mut self.bytes);
                if expr_bit == 0 {
                    self.bytes.push(0x00); // elemkind == funcref
                } else {
                    segment.element_type.encode(&mut self.bytes);
                }
            }
        }

        match segment.elements {
            Elements::Functions(fs) => {
                fs.encode(&mut self.bytes);
            }
            Elements::Expressions(e) => {
                e.len().encode(&mut self.bytes);
                for expr in e {
                    expr.encode(&mut self.bytes);
                }
            }
        }

        self.num_added += 1;
        self
    }

    /// Define an active element segment.
    ///
    /// `Active` element specifying a `None` table forces the MVP encoding and refers to the 0th
    /// table holding `funcref`s. Non-`None` tables use the encoding introduced with the bulk
    /// memory proposal and can refer to tables with any valid reference type.
    pub fn active(
        &mut self,
        table_index: Option<u32>,
        offset: &ConstExpr,
        element_type: ValType,
        elements: Elements<'_>,
    ) -> &mut Self {
        self.segment(ElementSegment {
            mode: ElementMode::Active {
                table: table_index,
                offset,
            },
            element_type,
            elements,
        })
    }

    /// Encode a passive element segment.
    ///
    /// Passive segments are part of the bulk memory proposal.
    pub fn passive<'a>(&mut self, element_type: ValType, elements: Elements<'a>) -> &mut Self {
        self.segment(ElementSegment {
            mode: ElementMode::Passive,
            element_type,
            elements,
        })
    }

    /// Encode a declared element segment.
    ///
    /// Declared segments are part of the bulk memory proposal.
    pub fn declared<'a>(&mut self, element_type: ValType, elements: Elements<'a>) -> &mut Self {
        self.segment(ElementSegment {
            mode: ElementMode::Declared,
            element_type,
            elements,
        })
    }

    /// Copy a raw, already-encoded element segment into this elements section.
    pub fn raw(&mut self, raw_bytes: &[u8]) -> &mut Self {
        self.bytes.extend_from_slice(raw_bytes);
        self.num_added += 1;
        self
    }
}

impl Encode for ElementSection {
    fn encode(&self, sink: &mut Vec<u8>) {
        encode_section(sink, self.num_added, &self.bytes);
    }
}

impl Section for ElementSection {
    fn id(&self) -> u8 {
        SectionId::Element.into()
    }
}
