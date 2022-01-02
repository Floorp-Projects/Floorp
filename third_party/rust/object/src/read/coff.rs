use crate::alloc::borrow::Cow;
use crate::alloc::fmt;
use crate::alloc::vec::Vec;
use goblin::pe;
use std::{iter, slice};
use target_lexicon::Architecture;

use crate::read::{
    self, Object, ObjectSection, ObjectSegment, Relocation, RelocationEncoding, RelocationKind,
    RelocationTarget, SectionIndex, SectionKind, Symbol, SymbolIndex, SymbolKind, SymbolMap,
    SymbolScope,
};

/// A COFF object file.
#[derive(Debug)]
pub struct CoffFile<'data> {
    coff: pe::Coff<'data>,
    data: &'data [u8],
}

/// An iterator over the loadable sections of a `CoffFile`.
#[derive(Debug)]
pub struct CoffSegmentIterator<'data, 'file>
where
    'data: 'file,
{
    file: &'file CoffFile<'data>,
    iter: slice::Iter<'file, pe::section_table::SectionTable>,
}

/// A loadable section of a `CoffFile`.
#[derive(Debug)]
pub struct CoffSegment<'data, 'file>
where
    'data: 'file,
{
    file: &'file CoffFile<'data>,
    section: &'file pe::section_table::SectionTable,
}

/// An iterator over the sections of a `CoffFile`.
#[derive(Debug)]
pub struct CoffSectionIterator<'data, 'file>
where
    'data: 'file,
{
    file: &'file CoffFile<'data>,
    iter: iter::Enumerate<slice::Iter<'file, pe::section_table::SectionTable>>,
}

/// A section of a `CoffFile`.
#[derive(Debug)]
pub struct CoffSection<'data, 'file>
where
    'data: 'file,
{
    file: &'file CoffFile<'data>,
    index: SectionIndex,
    section: &'file pe::section_table::SectionTable,
}

/// An iterator over the symbols of a `CoffFile`.
pub struct CoffSymbolIterator<'data, 'file>
where
    'data: 'file,
{
    file: &'file CoffFile<'data>,
    symbols: pe::symbol::SymbolIterator<'data>,
}

/// An iterator over the relocations in an `CoffSection`.
pub struct CoffRelocationIterator<'data, 'file> {
    file: &'file CoffFile<'data>,
    relocations: pe::relocation::Relocations<'data>,
}

impl<'data> CoffFile<'data> {
    /// Get the COFF headers of the file.
    // TODO: this is temporary to allow access to features this crate doesn't provide yet
    #[inline]
    pub fn coff(&self) -> &pe::Coff<'data> {
        &self.coff
    }

    /// Parse the raw COFF file data.
    pub fn parse(data: &'data [u8]) -> Result<Self, &'static str> {
        let coff = pe::Coff::parse(data).map_err(|_| "Could not parse COFF header")?;
        Ok(CoffFile { coff, data })
    }
}

impl<'data, 'file> Object<'data, 'file> for CoffFile<'data>
where
    'data: 'file,
{
    type Segment = CoffSegment<'data, 'file>;
    type SegmentIterator = CoffSegmentIterator<'data, 'file>;
    type Section = CoffSection<'data, 'file>;
    type SectionIterator = CoffSectionIterator<'data, 'file>;
    type SymbolIterator = CoffSymbolIterator<'data, 'file>;

    fn architecture(&self) -> Architecture {
        match self.coff.header.machine {
            pe::header::COFF_MACHINE_X86 => Architecture::I386,
            pe::header::COFF_MACHINE_X86_64 => Architecture::X86_64,
            _ => Architecture::Unknown,
        }
    }

    #[inline]
    fn is_little_endian(&self) -> bool {
        true
    }

    #[inline]
    fn is_64(&self) -> bool {
        false
    }

    fn segments(&'file self) -> CoffSegmentIterator<'data, 'file> {
        CoffSegmentIterator {
            file: self,
            iter: self.coff.sections.iter(),
        }
    }

    fn section_by_name(&'file self, section_name: &str) -> Option<CoffSection<'data, 'file>> {
        self.sections()
            .find(|section| section.name() == Some(section_name))
    }

    fn section_by_index(&'file self, index: SectionIndex) -> Option<CoffSection<'data, 'file>> {
        self.sections().find(|section| section.index() == index)
    }

    fn sections(&'file self) -> CoffSectionIterator<'data, 'file> {
        CoffSectionIterator {
            file: self,
            iter: self.coff.sections.iter().enumerate(),
        }
    }

    fn symbol_by_index(&self, index: SymbolIndex) -> Option<Symbol<'data>> {
        self.coff
            .symbols
            .get(index.0)
            .map(|(name, symbol)| parse_symbol(index.0, name, &symbol, &self.coff))
    }

    fn symbols(&'file self) -> CoffSymbolIterator<'data, 'file> {
        CoffSymbolIterator {
            file: self,
            symbols: self.coff.symbols.iter(),
        }
    }

    fn dynamic_symbols(&'file self) -> CoffSymbolIterator<'data, 'file> {
        CoffSymbolIterator {
            file: self,
            symbols: goblin::pe::symbol::SymbolIterator::default(),
        }
    }

    fn symbol_map(&self) -> SymbolMap<'data> {
        // TODO: untested
        let mut symbols: Vec<_> = self
            .symbols()
            .map(|(_, s)| s)
            .filter(SymbolMap::filter)
            .collect();
        symbols.sort_by_key(|x| x.address);
        SymbolMap { symbols }
    }

    fn has_debug_symbols(&self) -> bool {
        for section in &self.coff.sections {
            if let Ok(name) = section.name() {
                if name == ".debug_info" {
                    return true;
                }
            }
        }
        false
    }

    fn entry(&self) -> u64 {
        0
    }
}

impl<'data, 'file> Iterator for CoffSegmentIterator<'data, 'file> {
    type Item = CoffSegment<'data, 'file>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|section| CoffSegment {
            file: self.file,
            section,
        })
    }
}

fn section_alignment(characteristics: u32) -> u64 {
    match characteristics & pe::section_table::IMAGE_SCN_ALIGN_MASK {
        pe::section_table::IMAGE_SCN_ALIGN_2BYTES => 2,
        pe::section_table::IMAGE_SCN_ALIGN_4BYTES => 4,
        pe::section_table::IMAGE_SCN_ALIGN_8BYTES => 8,
        pe::section_table::IMAGE_SCN_ALIGN_16BYTES => 16,
        pe::section_table::IMAGE_SCN_ALIGN_32BYTES => 32,
        pe::section_table::IMAGE_SCN_ALIGN_64BYTES => 64,
        pe::section_table::IMAGE_SCN_ALIGN_128BYTES => 128,
        pe::section_table::IMAGE_SCN_ALIGN_256BYTES => 256,
        pe::section_table::IMAGE_SCN_ALIGN_512BYTES => 512,
        pe::section_table::IMAGE_SCN_ALIGN_1024BYTES => 1024,
        pe::section_table::IMAGE_SCN_ALIGN_2048BYTES => 2048,
        pe::section_table::IMAGE_SCN_ALIGN_4096BYTES => 4096,
        pe::section_table::IMAGE_SCN_ALIGN_8192BYTES => 8192,
        _ => 1,
    }
}

impl<'data, 'file> ObjectSegment<'data> for CoffSegment<'data, 'file> {
    #[inline]
    fn address(&self) -> u64 {
        u64::from(self.section.virtual_address)
    }

    #[inline]
    fn size(&self) -> u64 {
        u64::from(self.section.virtual_size)
    }

    #[inline]
    fn align(&self) -> u64 {
        section_alignment(self.section.characteristics)
    }

    #[inline]
    fn file_range(&self) -> (u64, u64) {
        (
            self.section.pointer_to_raw_data as u64,
            self.section.size_of_raw_data as u64,
        )
    }

    fn data(&self) -> &'data [u8] {
        let offset = self.section.pointer_to_raw_data as usize;
        let size = self.section.size_of_raw_data as usize;
        &self.file.data[offset..][..size]
    }

    fn data_range(&self, address: u64, size: u64) -> Option<&'data [u8]> {
        read::data_range(self.data(), self.address(), address, size)
    }

    #[inline]
    fn name(&self) -> Option<&str> {
        self.section.name().ok()
    }
}

impl<'data, 'file> Iterator for CoffSectionIterator<'data, 'file> {
    type Item = CoffSection<'data, 'file>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|(index, section)| CoffSection {
            file: self.file,
            index: SectionIndex(index),
            section,
        })
    }
}

impl<'data, 'file> CoffSection<'data, 'file> {
    fn raw_data(&self) -> &'data [u8] {
        let offset = self.section.pointer_to_raw_data as usize;
        let size = self.section.size_of_raw_data as usize;
        &self.file.data[offset..][..size]
    }
}

impl<'data, 'file> ObjectSection<'data> for CoffSection<'data, 'file> {
    type RelocationIterator = CoffRelocationIterator<'data, 'file>;

    #[inline]
    fn index(&self) -> SectionIndex {
        self.index
    }

    #[inline]
    fn address(&self) -> u64 {
        u64::from(self.section.virtual_address)
    }

    #[inline]
    fn size(&self) -> u64 {
        u64::from(self.section.size_of_raw_data)
    }

    #[inline]
    fn align(&self) -> u64 {
        section_alignment(self.section.characteristics)
    }

    #[inline]
    fn file_range(&self) -> Option<(u64, u64)> {
        Some((
            self.section.pointer_to_raw_data as u64,
            self.section.size_of_raw_data as u64,
        ))
    }

    fn data(&self) -> Cow<'data, [u8]> {
        Cow::from(self.raw_data())
    }

    fn data_range(&self, address: u64, size: u64) -> Option<&'data [u8]> {
        read::data_range(self.raw_data(), self.address(), address, size)
    }

    #[inline]
    fn uncompressed_data(&self) -> Cow<'data, [u8]> {
        self.data()
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
            if self.section.characteristics & pe::section_table::IMAGE_SCN_MEM_DISCARDABLE != 0 {
                SectionKind::Other
            } else if self.section.characteristics & pe::section_table::IMAGE_SCN_MEM_WRITE != 0 {
                SectionKind::Data
            } else {
                SectionKind::ReadOnlyData
            }
        } else if self.section.characteristics & pe::section_table::IMAGE_SCN_CNT_UNINITIALIZED_DATA
            != 0
        {
            SectionKind::UninitializedData
        } else if self.section.characteristics & pe::section_table::IMAGE_SCN_LNK_INFO != 0 {
            SectionKind::Linker
        } else {
            SectionKind::Unknown
        }
    }

    fn relocations(&self) -> CoffRelocationIterator<'data, 'file> {
        CoffRelocationIterator {
            file: self.file,
            relocations: self.section.relocations(self.file.data).unwrap_or_default(),
        }
    }
}

impl<'data, 'file> fmt::Debug for CoffSymbolIterator<'data, 'file> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("CoffSymbolIterator").finish()
    }
}

impl<'data, 'file> Iterator for CoffSymbolIterator<'data, 'file> {
    type Item = (SymbolIndex, Symbol<'data>);

    fn next(&mut self) -> Option<Self::Item> {
        self.symbols.next().map(|(index, name, symbol)| {
            (
                SymbolIndex(index),
                parse_symbol(index, name, &symbol, &self.file.coff),
            )
        })
    }
}

fn parse_symbol<'data>(
    index: usize,
    name: Option<&'data str>,
    symbol: &pe::symbol::Symbol,
    coff: &pe::Coff<'data>,
) -> Symbol<'data> {
    let name = if symbol.is_file() {
        coff.symbols
            .aux_file(index + 1, symbol.number_of_aux_symbols as usize)
    } else {
        name.or_else(|| {
            symbol.name_offset().and_then(|offset| {
                coff.strings
                    .get(offset as usize)
                    .map(Result::ok)
                    .unwrap_or_default()
            })
        })
    };
    let size = if symbol.is_function_definition() && symbol.number_of_aux_symbols > 0 {
        coff.symbols
            .aux_function_definition(index + 1)
            .map(|aux| u64::from(aux.total_size))
            .unwrap_or(0)
    } else {
        0
    };
    let kind = if symbol.is_section_definition() {
        SymbolKind::Section
    } else {
        match symbol.storage_class {
            pe::symbol::IMAGE_SYM_CLASS_SECTION => SymbolKind::Section,
            pe::symbol::IMAGE_SYM_CLASS_FILE => SymbolKind::File,
            pe::symbol::IMAGE_SYM_CLASS_LABEL => SymbolKind::Label,
            pe::symbol::IMAGE_SYM_CLASS_EXTERNAL
            | pe::symbol::IMAGE_SYM_CLASS_WEAK_EXTERNAL
            | pe::symbol::IMAGE_SYM_CLASS_STATIC => {
                if symbol.derived_type() == pe::symbol::IMAGE_SYM_DTYPE_FUNCTION {
                    SymbolKind::Text
                } else {
                    SymbolKind::Data
                }
            }
            _ => SymbolKind::Unknown,
        }
    };
    let section_index = if symbol.section_number <= 0 {
        None
    } else {
        Some(SectionIndex(symbol.section_number as usize - 1))
    };
    let undefined = symbol.section_number == pe::symbol::IMAGE_SYM_UNDEFINED;
    let weak = symbol.storage_class == pe::symbol::IMAGE_SYM_CLASS_WEAK_EXTERNAL;
    let scope = match symbol.storage_class {
        _ if undefined => SymbolScope::Unknown,
        pe::symbol::IMAGE_SYM_CLASS_EXTERNAL
        | pe::symbol::IMAGE_SYM_CLASS_EXTERNAL_DEF
        | pe::symbol::IMAGE_SYM_CLASS_WEAK_EXTERNAL => {
            // TODO: determine if symbol is exported
            SymbolScope::Linkage
        }
        _ => SymbolScope::Compilation,
    };
    Symbol {
        name,
        address: u64::from(symbol.value),
        size,
        kind,
        section_index,
        undefined,
        weak,
        scope,
    }
}

impl<'data, 'file> Iterator for CoffRelocationIterator<'data, 'file> {
    type Item = (u64, Relocation);

    fn next(&mut self) -> Option<Self::Item> {
        self.relocations.next().map(|relocation| {
            let (kind, size, addend) = match self.file.coff.header.machine {
                pe::header::COFF_MACHINE_X86 => match relocation.typ {
                    pe::relocation::IMAGE_REL_I386_DIR16 => (RelocationKind::Absolute, 16, 0),
                    pe::relocation::IMAGE_REL_I386_REL16 => (RelocationKind::Relative, 16, 0),
                    pe::relocation::IMAGE_REL_I386_DIR32 => (RelocationKind::Absolute, 32, 0),
                    pe::relocation::IMAGE_REL_I386_DIR32NB => (RelocationKind::ImageOffset, 32, 0),
                    pe::relocation::IMAGE_REL_I386_SECTION => (RelocationKind::SectionIndex, 16, 0),
                    pe::relocation::IMAGE_REL_I386_SECREL => (RelocationKind::SectionOffset, 32, 0),
                    pe::relocation::IMAGE_REL_I386_SECREL7 => (RelocationKind::SectionOffset, 7, 0),
                    pe::relocation::IMAGE_REL_I386_REL32 => (RelocationKind::Relative, 32, -4),
                    _ => (RelocationKind::Coff(relocation.typ), 0, 0),
                },
                pe::header::COFF_MACHINE_X86_64 => match relocation.typ {
                    pe::relocation::IMAGE_REL_AMD64_ADDR64 => (RelocationKind::Absolute, 64, 0),
                    pe::relocation::IMAGE_REL_AMD64_ADDR32 => (RelocationKind::Absolute, 32, 0),
                    pe::relocation::IMAGE_REL_AMD64_ADDR32NB => {
                        (RelocationKind::ImageOffset, 32, 0)
                    }
                    pe::relocation::IMAGE_REL_AMD64_REL32 => (RelocationKind::Relative, 32, -4),
                    pe::relocation::IMAGE_REL_AMD64_REL32_1 => (RelocationKind::Relative, 32, -5),
                    pe::relocation::IMAGE_REL_AMD64_REL32_2 => (RelocationKind::Relative, 32, -6),
                    pe::relocation::IMAGE_REL_AMD64_REL32_3 => (RelocationKind::Relative, 32, -7),
                    pe::relocation::IMAGE_REL_AMD64_REL32_4 => (RelocationKind::Relative, 32, -8),
                    pe::relocation::IMAGE_REL_AMD64_REL32_5 => (RelocationKind::Relative, 32, -9),
                    pe::relocation::IMAGE_REL_AMD64_SECTION => {
                        (RelocationKind::SectionIndex, 16, 0)
                    }
                    pe::relocation::IMAGE_REL_AMD64_SECREL => {
                        (RelocationKind::SectionOffset, 32, 0)
                    }
                    pe::relocation::IMAGE_REL_AMD64_SECREL7 => {
                        (RelocationKind::SectionOffset, 7, 0)
                    }
                    _ => (RelocationKind::Coff(relocation.typ), 0, 0),
                },
                _ => (RelocationKind::Coff(relocation.typ), 0, 0),
            };
            let target =
                RelocationTarget::Symbol(SymbolIndex(relocation.symbol_table_index as usize));
            (
                u64::from(relocation.virtual_address),
                Relocation {
                    kind,
                    encoding: RelocationEncoding::Generic,
                    size,
                    target,
                    addend,
                    implicit_addend: true,
                },
            )
        })
    }
}

impl<'data, 'file> fmt::Debug for CoffRelocationIterator<'data, 'file> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("CoffRelocationIterator").finish()
    }
}
