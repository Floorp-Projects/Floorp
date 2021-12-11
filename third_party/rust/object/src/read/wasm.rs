use crate::alloc::vec::Vec;
use parity_wasm::elements::{self, Deserialize};
use std::borrow::Cow;
use std::{iter, slice};
use target_lexicon::Architecture;

use crate::read::{
    Object, ObjectSection, ObjectSegment, Relocation, SectionIndex, SectionKind, Symbol,
    SymbolIndex, SymbolMap,
};

/// A WebAssembly object file.
#[derive(Debug)]
pub struct WasmFile {
    module: elements::Module,
}

impl<'data> WasmFile {
    /// Parse the raw wasm data.
    pub fn parse(mut data: &'data [u8]) -> Result<Self, &'static str> {
        let module =
            elements::Module::deserialize(&mut data).map_err(|_| "failed to parse wasm")?;
        Ok(WasmFile { module })
    }
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

    #[inline]
    fn architecture(&self) -> Architecture {
        Architecture::Wasm32
    }

    #[inline]
    fn is_little_endian(&self) -> bool {
        true
    }

    #[inline]
    fn is_64(&self) -> bool {
        false
    }

    fn segments(&'file self) -> Self::SegmentIterator {
        WasmSegmentIterator { file: self }
    }

    fn entry(&'file self) -> u64 {
        self.module
            .start_section()
            .map_or(u64::max_value(), u64::from)
    }

    fn section_by_name(&'file self, section_name: &str) -> Option<WasmSection<'file>> {
        self.sections()
            .find(|section| section.name() == Some(section_name))
    }

    fn section_by_index(&'file self, index: SectionIndex) -> Option<WasmSection<'file>> {
        self.sections().find(|section| section.index() == index)
    }

    fn sections(&'file self) -> Self::SectionIterator {
        WasmSectionIterator {
            sections: self.module.sections().iter().enumerate(),
        }
    }

    fn symbol_by_index(&self, _index: SymbolIndex) -> Option<Symbol<'static>> {
        unimplemented!()
    }

    fn symbols(&'file self) -> Self::SymbolIterator {
        WasmSymbolIterator { file: self }
    }

    fn dynamic_symbols(&'file self) -> Self::SymbolIterator {
        WasmSymbolIterator { file: self }
    }

    fn symbol_map(&self) -> SymbolMap<'static> {
        SymbolMap {
            symbols: Vec::new(),
        }
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

/// An iterator over the segments of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSegmentIterator<'file> {
    file: &'file WasmFile,
}

impl<'file> Iterator for WasmSegmentIterator<'file> {
    type Item = WasmSegment<'file>;

    fn next(&mut self) -> Option<Self::Item> {
        None
    }
}

/// A segment of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSegment<'file> {
    file: &'file WasmFile,
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

    #[inline]
    fn align(&self) -> u64 {
        unreachable!()
    }

    #[inline]
    fn file_range(&self) -> (u64, u64) {
        unreachable!()
    }

    fn data(&self) -> &'static [u8] {
        unreachable!()
    }

    fn data_range(&self, _address: u64, _size: u64) -> Option<&'static [u8]> {
        unreachable!()
    }

    #[inline]
    fn name(&self) -> Option<&str> {
        unreachable!()
    }
}

/// An iterator over the sections of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSectionIterator<'file> {
    sections: iter::Enumerate<slice::Iter<'file, elements::Section>>,
}

impl<'file> Iterator for WasmSectionIterator<'file> {
    type Item = WasmSection<'file>;

    fn next(&mut self) -> Option<Self::Item> {
        self.sections.next().map(|(index, section)| WasmSection {
            index: SectionIndex(index),
            section,
        })
    }
}

/// A section of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSection<'file> {
    index: SectionIndex,
    section: &'file elements::Section,
}

impl<'file> ObjectSection<'static> for WasmSection<'file> {
    type RelocationIterator = WasmRelocationIterator;

    #[inline]
    fn index(&self) -> SectionIndex {
        self.index
    }

    #[inline]
    fn address(&self) -> u64 {
        1
    }

    #[inline]
    fn size(&self) -> u64 {
        serialize_to_cow(self.section.clone()).map_or(0, |b| b.len() as u64)
    }

    #[inline]
    fn align(&self) -> u64 {
        1
    }

    #[inline]
    fn file_range(&self) -> Option<(u64, u64)> {
        None
    }

    fn data(&self) -> Cow<'static, [u8]> {
        match *self.section {
            elements::Section::Custom(ref section) => Some(section.payload().to_vec().into()),
            elements::Section::Start(section) => {
                serialize_to_cow(elements::VarUint32::from(section))
            }
            _ => serialize_to_cow(self.section.clone()),
        }
        .unwrap_or_else(|| Cow::from(&[][..]))
    }

    fn data_range(&self, _address: u64, _size: u64) -> Option<&'static [u8]> {
        unimplemented!()
    }

    #[inline]
    fn uncompressed_data(&self) -> Cow<'static, [u8]> {
        // TODO: does wasm support compression?
        self.data()
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
            elements::Section::DataCount(_) => Some("DataCount"),
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
            elements::Section::DataCount(_) => SectionKind::Other,
            elements::Section::Data(_) => SectionKind::Data,
            elements::Section::Name(_) => SectionKind::Other,
            elements::Section::Reloc(_) => SectionKind::Other,
        }
    }

    fn relocations(&self) -> WasmRelocationIterator {
        WasmRelocationIterator
    }
}

/// An iterator over the symbols of an `WasmFile`.
#[derive(Debug)]
pub struct WasmSymbolIterator<'file> {
    file: &'file WasmFile,
}

impl<'file> Iterator for WasmSymbolIterator<'file> {
    type Item = (SymbolIndex, Symbol<'static>);

    fn next(&mut self) -> Option<Self::Item> {
        unimplemented!()
    }
}

/// An iterator over the relocations in an `WasmSection`.
#[derive(Debug)]
pub struct WasmRelocationIterator;

impl Iterator for WasmRelocationIterator {
    type Item = (u64, Relocation);

    fn next(&mut self) -> Option<Self::Item> {
        None
    }
}
