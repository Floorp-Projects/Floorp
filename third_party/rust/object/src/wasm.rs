use alloc::vec::Vec;
use parity_wasm::elements::{self, Deserialize};
use std::borrow::{Cow, ToOwned};
use std::slice;
use std::u64;

use {
    Machine, Object, ObjectSection, ObjectSegment, SectionKind, Symbol, SymbolMap,
};

/// A WebAssembly object file.
#[derive(Debug)]
pub struct WasmFile {
    module: elements::Module,
}

impl<'data> WasmFile {
    /// Parse the raw wasm data.
    pub fn parse(mut data: &'data [u8]) -> Result<Self, &'static str> {
        let module = elements::Module::deserialize(&mut data).map_err(|_| "failed to parse wasm")?;
        Ok(WasmFile {
            module,
        })
    }
}

/// An iterator over the segments of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSegmentIterator<'file> {
    file: &'file WasmFile,
}

/// A segment of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSegment<'file> {
    file: &'file WasmFile,
}

/// An iterator over the sections of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSectionIterator<'file> {
    sections: slice::Iter<'file, elements::Section>,
}

/// A section of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSection<'file> {
    section: &'file elements::Section,
}

/// An iterator over the symbols of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSymbolIterator<'file> {
    file: &'file WasmFile,
}

fn serialize_to_cow<'a, S>(s: S) -> Option<Cow<'a, [u8]>>
where
    S: elements::Serialize,
{
    let mut buf = Vec::new();
    s.serialize(&mut buf).ok()?;
    Some(Cow::from(buf))
}

impl<'file> Object<'static, 'file> for WasmFile {
    type Segment = WasmSegment<'file>;
    type SegmentIterator = WasmSegmentIterator<'file>;
    type Section = WasmSection<'file>;
    type SectionIterator = WasmSectionIterator<'file>;
    type SymbolIterator = WasmSymbolIterator<'file>;

    fn machine(&self) -> Machine {
        Machine::Other
    }

    fn segments(&'file self) -> Self::SegmentIterator {
        WasmSegmentIterator {
            file: self,
        }
    }

    fn entry(&'file self) -> u64 {
        self.module.start_section().map_or(u64::MAX, |s| s as u64)
    }

    fn section_data_by_name(&self, section_name: &str) -> Option<Cow<'static, [u8]>> {
        match section_name {
            // Known wasm section names.
            "Type" => self.module
                .type_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Import" => self.module
                .import_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Function" => self.module
                .function_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Table" => self.module
                .table_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Memory" => self.module
                .memory_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Global" => self.module
                .global_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Export" => self.module
                .export_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Start" => self.module
                .start_section()
                .and_then(|s| serialize_to_cow(elements::VarUint32::from(s))),
            "Element" => self.module
                .elements_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Code" => self.module
                .code_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            "Data" => self.module
                .data_section()
                .and_then(|s| serialize_to_cow(s.clone())),
            // Custom sections.
            _ => {
                for s in self.module.sections() {
                    if let elements::Section::Custom(ref c) = *s {
                        if c.name() == section_name {
                            return Some(Cow::from(c.payload().to_owned()));
                        }
                    }
                }
                None
            }
        }
    }

    fn sections(&'file self) -> Self::SectionIterator {
        WasmSectionIterator {
            sections: self.module.sections().iter(),
        }
    }

    fn symbols(&'file self) -> Self::SymbolIterator {
        WasmSymbolIterator {
            file: self,
        }
    }

    fn dynamic_symbols(&'file self) -> Self::SymbolIterator {
        WasmSymbolIterator {
            file: self,
        }
    }

    fn symbol_map(&self) -> SymbolMap<'static> {
        SymbolMap {
            symbols: Vec::new(),
        }
    }

    fn is_little_endian(&self) -> bool {
        true
    }

    fn has_debug_symbols(&self) -> bool {
        // We ignore the "name" section, and use this to mean whether the wasm
        // has DWARF.
        self.module.sections().iter().any(|s| match *s {
            elements::Section::Custom(ref c) => c.name().starts_with(".debug_"),
            _ => false,
        })
    }
}

impl<'file> Iterator for WasmSegmentIterator<'file> {
    type Item = WasmSegment<'file>;

    fn next(&mut self) -> Option<Self::Item> {
        None
    }
}

impl<'file> ObjectSegment<'static> for WasmSegment<'file> {
    #[inline]
    fn address(&self) -> u64 {
        unreachable!()
    }

    #[inline]
    fn size(&self) -> u64 {
        unreachable!()
    }

    fn data(&self) -> &'static [u8] {
        unreachable!()
    }

    #[inline]
    fn name(&self) -> Option<&str> {
        unreachable!()
    }
}

impl<'file> Iterator for WasmSectionIterator<'file> {
    type Item = WasmSection<'file>;

    fn next(&mut self) -> Option<Self::Item> {
        self.sections.next().map(|s| WasmSection {
            section: s,
        })
    }
}

impl<'file> ObjectSection<'static> for WasmSection<'file> {
    #[inline]
    fn address(&self) -> u64 {
        1
    }

    #[inline]
    fn size(&self) -> u64 {
        serialize_to_cow(self.section.clone()).map_or(0, |b| b.len() as u64)
    }

    fn data(&self) -> Cow<'static, [u8]> {
        serialize_to_cow(self.section.clone()).unwrap_or(Cow::from(&[][..]))
    }

    fn name(&self) -> Option<&str> {
        match *self.section {
            elements::Section::Unparsed { .. } => None,
            elements::Section::Custom(ref c) => Some(c.name()),
            elements::Section::Type(_) => Some("Type"),
            elements::Section::Import(_) => Some("Import"),
            elements::Section::Function(_) => Some("Function"),
            elements::Section::Table(_) => Some("Table"),
            elements::Section::Memory(_) => Some("Memory"),
            elements::Section::Global(_) => Some("Global"),
            elements::Section::Export(_) => Some("Export"),
            elements::Section::Start(_) => Some("Start"),
            elements::Section::Element(_) => Some("Element"),
            elements::Section::Code(_) => Some("Code"),
            elements::Section::Data(_) => Some("Data"),
            elements::Section::Name(_) => Some("Name"),
            elements::Section::Reloc(_) => Some("Reloc"),
        }
    }

    #[inline]
    fn segment_name(&self) -> Option<&str> {
        None
    }

    fn kind(&self) -> SectionKind {
        match *self.section {
            elements::Section::Unparsed { .. } => SectionKind::Unknown,
            elements::Section::Custom(_) => SectionKind::Unknown,
            elements::Section::Type(_) => SectionKind::Other,
            elements::Section::Import(_) => SectionKind::Other,
            elements::Section::Function(_) => SectionKind::Other,
            elements::Section::Table(_) => SectionKind::Other,
            elements::Section::Memory(_) => SectionKind::Other,
            elements::Section::Global(_) => SectionKind::Other,
            elements::Section::Export(_) => SectionKind::Other,
            elements::Section::Start(_) => SectionKind::Other,
            elements::Section::Element(_) => SectionKind::Other,
            elements::Section::Code(_) => SectionKind::Text,
            elements::Section::Data(_) => SectionKind::Data,
            elements::Section::Name(_) => SectionKind::Other,
            elements::Section::Reloc(_) => SectionKind::Other,
        }
    }
}

impl<'file> Iterator for WasmSymbolIterator<'file> {
    type Item = Symbol<'static>;

    fn next(&mut self) -> Option<Self::Item> {
        unimplemented!()
    }
}
