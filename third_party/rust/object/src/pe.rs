use alloc::borrow::Cow;
use alloc::vec::Vec;
use std::slice;

use goblin::pe;

use {
    Machine, Object, ObjectSection, ObjectSegment, SectionKind, Symbol, SymbolKind, SymbolMap,
};

/// A PE object file.
#[derive(Debug)]
pub struct PeFile<'data> {
    pe: pe::PE<'data>,
    data: &'data [u8],
}

/// An iterator over the loadable sections of a `PeFile`.
#[derive(Debug)]
pub struct PeSegmentIterator<'data, 'file>
where
    'data: 'file,
{
    file: &'file PeFile<'data>,
    iter: slice::Iter<'file, pe::section_table::SectionTable>,
}

/// A loadable section of a `PeFile`.
#[derive(Debug)]
pub struct PeSegment<'data, 'file>
where
    'data: 'file,
{
    file: &'file PeFile<'data>,
    section: &'file pe::section_table::SectionTable,
}

/// An iterator over the sections of a `PeFile`.
#[derive(Debug)]
pub struct PeSectionIterator<'data, 'file>
where
    'data: 'file,
{
    file: &'file PeFile<'data>,
    iter: slice::Iter<'file, pe::section_table::SectionTable>,
}

/// A section of a `PeFile`.
#[derive(Debug)]
pub struct PeSection<'data, 'file>
where
    'data: 'file,
{
    file: &'file PeFile<'data>,
    section: &'file pe::section_table::SectionTable,
}

/// An iterator over the symbols of a `PeFile`.
#[derive(Debug)]
pub struct PeSymbolIterator<'data, 'file>
where
    'data: 'file,
{
    exports: slice::Iter<'file, pe::export::Export<'data>>,
    imports: slice::Iter<'file, pe::import::Import<'data>>,
}

impl<'data> PeFile<'data> {
    /// Get the PE headers of the file.
    // TODO: this is temporary to allow access to features this crate doesn't provide yet
    #[inline]
    pub fn pe(&self) -> &pe::PE<'data> {
        &self.pe
    }

    /// Parse the raw PE file data.
    pub fn parse(data: &'data [u8]) -> Result<Self, &'static str> {
        let pe = pe::PE::parse(data).map_err(|_| "Could not parse PE header")?;
        Ok(PeFile { pe, data })
    }
}

impl<'data, 'file> Object<'data, 'file> for PeFile<'data>
where
    'data: 'file,
{
    type Segment = PeSegment<'data, 'file>;
    type SegmentIterator = PeSegmentIterator<'data, 'file>;
    type Section = PeSection<'data, 'file>;
    type SectionIterator = PeSectionIterator<'data, 'file>;
    type SymbolIterator = PeSymbolIterator<'data, 'file>;

    fn machine(&self) -> Machine {
        match self.pe.header.coff_header.machine {
            // TODO: Arm/Arm64
            pe::header::COFF_MACHINE_X86 => Machine::X86,
            pe::header::COFF_MACHINE_X86_64 => Machine::X86_64,
            _ => Machine::Other,
        }
    }

    fn segments(&'file self) -> PeSegmentIterator<'data, 'file> {
        PeSegmentIterator {
            file: self,
            iter: self.pe.sections.iter(),
        }
    }

    fn section_data_by_name(&self, section_name: &str) -> Option<Cow<'data, [u8]>> {
        for section in &self.pe.sections {
            if let Ok(name) = section.name() {
                if name == section_name {
                    return Some(Cow::from(
                        &self.data[section.pointer_to_raw_data as usize..]
                            [..section.size_of_raw_data as usize],
                    ));
                }
            }
        }
        None
    }

    fn sections(&'file self) -> PeSectionIterator<'data, 'file> {
        PeSectionIterator {
            file: self,
            iter: self.pe.sections.iter(),
        }
    }

    fn symbols(&'file self) -> PeSymbolIterator<'data, 'file> {
        // TODO: return COFF symbols for object files
        PeSymbolIterator {
            exports: [].iter(),
            imports: [].iter(),
        }
    }

    fn dynamic_symbols(&'file self) -> PeSymbolIterator<'data, 'file> {
        PeSymbolIterator {
            exports: self.pe.exports.iter(),
            imports: self.pe.imports.iter(),
        }
    }

    fn symbol_map(&self) -> SymbolMap<'data> {
        // TODO: untested
        let mut symbols: Vec<_> = self.symbols().filter(SymbolMap::filter).collect();
        symbols.sort_by_key(|x| x.address);
        SymbolMap { symbols }
    }

    #[inline]
    fn is_little_endian(&self) -> bool {
        // TODO: always little endian?  The COFF header has some bits in the
        // characteristics flags, but these are obsolete.
        true
    }

    #[inline]
    fn has_debug_symbols(&self) -> bool {
        // TODO: look at what the mingw toolchain does with DWARF-in-PE, and also
        // whether CodeView-in-PE still works?
        false
    }

    fn entry(&self) -> u64 {
        self.pe.entry as u64
    }
}

impl<'data, 'file> Iterator for PeSegmentIterator<'data, 'file> {
    type Item = PeSegment<'data, 'file>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|section| PeSegment {
            file: self.file,
            section,
        })
    }
}

impl<'data, 'file> ObjectSegment<'data> for PeSegment<'data, 'file> {
    #[inline]
    fn address(&self) -> u64 {
        u64::from(self.section.virtual_address)
    }

    #[inline]
    fn size(&self) -> u64 {
        u64::from(self.section.virtual_size)
    }

    fn data(&self) -> &'data [u8] {
        &self.file.data[self.section.pointer_to_raw_data as usize..]
            [..self.section.size_of_raw_data as usize]
    }

    #[inline]
    fn name(&self) -> Option<&str> {
        self.section.name().ok()
    }
}

impl<'data, 'file> Iterator for PeSectionIterator<'data, 'file> {
    type Item = PeSection<'data, 'file>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|section| PeSection {
            file: self.file,
            section,
        })
    }
}

impl<'data, 'file> ObjectSection<'data> for PeSection<'data, 'file> {
    #[inline]
    fn address(&self) -> u64 {
        u64::from(self.section.virtual_address)
    }

    #[inline]
    fn size(&self) -> u64 {
        u64::from(self.section.virtual_size)
    }

    fn data(&self) -> Cow<'data, [u8]> {
        Cow::from(
            &self.file.data[self.section.pointer_to_raw_data as usize..]
                [..self.section.size_of_raw_data as usize],
        )
    }

    fn name(&self) -> Option<&str> {
        self.section.name().ok()
    }

    #[inline]
    fn segment_name(&self) -> Option<&str> {
        None
    }

    #[inline]
    fn kind(&self) -> SectionKind {
        if self.section.characteristics
            & (pe::section_table::IMAGE_SCN_CNT_CODE | pe::section_table::IMAGE_SCN_MEM_EXECUTE)
            != 0
        {
            SectionKind::Text
        } else if self.section.characteristics & pe::section_table::IMAGE_SCN_CNT_INITIALIZED_DATA
            != 0
        {
            SectionKind::Data
        } else if self.section.characteristics & pe::section_table::IMAGE_SCN_CNT_UNINITIALIZED_DATA
            != 0
        {
            SectionKind::UninitializedData
        } else {
            SectionKind::Unknown
        }
    }
}

impl<'data, 'file> Iterator for PeSymbolIterator<'data, 'file> {
    type Item = Symbol<'data>;

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(export) = self.exports.next() {
            return Some(Symbol {
                kind: SymbolKind::Unknown,
                section_kind: Some(SectionKind::Unknown),
                global: true,
                name: export.name,
                address: export.rva as u64,
                size: 0,
            });
        }
        if let Some(import) = self.imports.next() {
            let name = match import.name {
                Cow::Borrowed(name) => Some(name),
                _ => None,
            };
            return Some(Symbol {
                kind: SymbolKind::Unknown,
                section_kind: None,
                global: true,
                name: name,
                address: 0,
                size: 0,
            });
        }
        None
    }
}
