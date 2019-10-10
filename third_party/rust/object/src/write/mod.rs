//! Interface for writing object files.

#![allow(clippy::collapsible_if)]
#![allow(clippy::cognitive_complexity)]
#![allow(clippy::module_inception)]

use scroll::Pwrite;
use std::collections::HashMap;
use std::string::String;

use crate::alloc::vec::Vec;
use crate::target_lexicon::{Architecture, BinaryFormat, Endianness, PointerWidth};
use crate::{RelocationEncoding, RelocationKind, SectionKind, SymbolKind, SymbolScope};

mod coff;
mod elf;
mod macho;
mod string;
mod util;

/// A writable object file.
#[derive(Debug)]
pub struct Object {
    format: BinaryFormat,
    architecture: Architecture,
    sections: Vec<Section>,
    standard_sections: HashMap<StandardSection, SectionId>,
    symbols: Vec<Symbol>,
    symbol_map: HashMap<Vec<u8>, SymbolId>,
    stub_symbols: HashMap<SymbolId, SymbolId>,
    subsection_via_symbols: bool,
    /// The symbol name mangling scheme.
    pub mangling: Mangling,
}

impl Object {
    /// Create an empty object file.
    pub fn new(format: BinaryFormat, architecture: Architecture) -> Object {
        Object {
            format,
            architecture,
            sections: Vec::new(),
            standard_sections: HashMap::new(),
            symbols: Vec::new(),
            symbol_map: HashMap::new(),
            stub_symbols: HashMap::new(),
            subsection_via_symbols: false,
            mangling: Mangling::default(format, architecture),
        }
    }

    /// Return the file format.
    #[inline]
    pub fn format(&self) -> BinaryFormat {
        self.format
    }

    /// Return the architecture.
    #[inline]
    pub fn architecture(&self) -> Architecture {
        self.architecture
    }

    /// Return the current mangling setting.
    #[inline]
    pub fn mangling(&self) -> Mangling {
        self.mangling
    }

    /// Return the current mangling setting.
    #[inline]
    pub fn set_mangling(&mut self, mangling: Mangling) {
        self.mangling = mangling;
    }

    /// Return the name for a standard segment.
    ///
    /// This will vary based on the file format.
    pub fn segment_name(&self, segment: StandardSegment) -> &'static [u8] {
        match self.format {
            BinaryFormat::Elf | BinaryFormat::Coff => &[],
            BinaryFormat::Macho => self.macho_segment_name(segment),
            _ => unimplemented!(),
        }
    }

    /// Get the section with the given `SectionId`.
    #[inline]
    pub fn section(&self, section: SectionId) -> &Section {
        &self.sections[section.0]
    }

    /// Mutably get the section with the given `SectionId`.
    #[inline]
    pub fn section_mut(&mut self, section: SectionId) -> &mut Section {
        &mut self.sections[section.0]
    }

    /// Append data to an existing section. Returns the section offset of the data.
    pub fn append_section_data(&mut self, section: SectionId, data: &[u8], align: u64) -> u64 {
        self.sections[section.0].append_data(data, align)
    }

    /// Append zero-initialized data to an existing section. Returns the section offset of the data.
    pub fn append_section_bss(&mut self, section: SectionId, size: u64, align: u64) -> u64 {
        self.sections[section.0].append_bss(size, align)
    }

    /// Return the `SectionId` of a standard section.
    ///
    /// If the section doesn't already exist then it is created.
    pub fn section_id(&mut self, section: StandardSection) -> SectionId {
        self.standard_sections
            .get(&section)
            .cloned()
            .unwrap_or_else(|| {
                let (segment, name, kind) = self.section_info(section);
                self.add_section(segment.to_vec(), name.to_vec(), kind)
            })
    }

    /// Add a new section and return its `SectionId`.
    ///
    /// This also creates a section symbol.
    pub fn add_section(&mut self, segment: Vec<u8>, name: Vec<u8>, kind: SectionKind) -> SectionId {
        let id = SectionId(self.sections.len());
        self.sections.push(Section {
            segment,
            name,
            kind,
            size: 0,
            align: 1,
            data: Vec::new(),
            relocations: Vec::new(),
            symbol: None,
        });

        // Add to self.standard_sections if required. This may match multiple standard sections.
        let section = &self.sections[id.0];
        for standard_section in StandardSection::all() {
            if !self.standard_sections.contains_key(standard_section) {
                let (segment, name, kind) = self.section_info(*standard_section);
                if segment == &*section.segment && name == &*section.name && kind == section.kind {
                    self.standard_sections.insert(*standard_section, id);
                }
            }
        }

        id
    }

    fn section_info(
        &self,
        section: StandardSection,
    ) -> (&'static [u8], &'static [u8], SectionKind) {
        match self.format {
            BinaryFormat::Elf => self.elf_section_info(section),
            BinaryFormat::Coff => self.coff_section_info(section),
            BinaryFormat::Macho => self.macho_section_info(section),
            _ => unimplemented!(),
        }
    }

    /// Add a subsection. Returns the `SectionId` and section offset of the data.
    pub fn add_subsection(
        &mut self,
        section: StandardSection,
        name: &[u8],
        data: &[u8],
        align: u64,
    ) -> (SectionId, u64) {
        let section_id = if self.has_subsection_via_symbols() {
            self.subsection_via_symbols = true;
            self.section_id(section)
        } else {
            let (segment, name, kind) = self.subsection_info(section, name);
            self.add_section(segment.to_vec(), name, kind)
        };
        let offset = self.append_section_data(section_id, data, align);
        (section_id, offset)
    }

    fn has_subsection_via_symbols(&self) -> bool {
        match self.format {
            BinaryFormat::Elf | BinaryFormat::Coff => false,
            BinaryFormat::Macho => true,
            _ => unimplemented!(),
        }
    }

    fn subsection_info(
        &self,
        section: StandardSection,
        value: &[u8],
    ) -> (&'static [u8], Vec<u8>, SectionKind) {
        let (segment, section, kind) = self.section_info(section);
        let name = self.subsection_name(section, value);
        (segment, name, kind)
    }

    fn subsection_name(&self, section: &[u8], value: &[u8]) -> Vec<u8> {
        debug_assert!(!self.has_subsection_via_symbols());
        match self.format {
            BinaryFormat::Elf => self.elf_subsection_name(section, value),
            BinaryFormat::Coff => self.coff_subsection_name(section, value),
            _ => unimplemented!(),
        }
    }

    /// Get the `SymbolId` of the symbol with the given name.
    pub fn symbol_id(&self, name: &[u8]) -> Option<SymbolId> {
        self.symbol_map.get(name).cloned()
    }

    /// Get the symbol with the given `SymbolId`.
    #[inline]
    pub fn symbol(&self, symbol: SymbolId) -> &Symbol {
        &self.symbols[symbol.0]
    }

    /// Mutably get the symbol with the given `SymbolId`.
    #[inline]
    pub fn symbol_mut(&mut self, symbol: SymbolId) -> &mut Symbol {
        &mut self.symbols[symbol.0]
    }

    /// Add a new symbol and return its `SymbolId`.
    pub fn add_symbol(&mut self, mut symbol: Symbol) -> SymbolId {
        // Defined symbols must have a scope.
        debug_assert!(symbol.is_undefined() || symbol.scope != SymbolScope::Unknown);
        if symbol.kind == SymbolKind::Section {
            return self.section_symbol(symbol.section.unwrap());
        }
        let symbol_id = SymbolId(self.symbols.len());
        if !symbol.name.is_empty()
            && (symbol.kind == SymbolKind::Text || symbol.kind == SymbolKind::Data)
        {
            self.symbol_map.insert(symbol.name.clone(), symbol_id);
            if let Some(prefix) = self.mangling.global_prefix() {
                symbol.name.insert(0, prefix);
            }
        }
        self.symbols.push(symbol);
        symbol_id
    }

    /// Add a new file symbol and return its `SymbolId`.
    pub fn add_file_symbol(&mut self, name: Vec<u8>) -> SymbolId {
        self.add_symbol(Symbol {
            name,
            value: 0,
            size: 0,
            kind: SymbolKind::File,
            scope: SymbolScope::Compilation,
            weak: false,
            section: None,
        })
    }

    /// Get the symbol for a section.
    pub fn section_symbol(&mut self, section_id: SectionId) -> SymbolId {
        let section = &mut self.sections[section_id.0];
        if let Some(symbol) = section.symbol {
            return symbol;
        }
        let name = if self.format == BinaryFormat::Coff {
            section.name.clone()
        } else {
            Vec::new()
        };
        let symbol_id = SymbolId(self.symbols.len());
        self.symbols.push(Symbol {
            name,
            value: 0,
            size: 0,
            kind: SymbolKind::Section,
            scope: SymbolScope::Compilation,
            weak: false,
            section: Some(section_id),
        });
        section.symbol = Some(symbol_id);
        symbol_id
    }

    /// Append data to an existing section, and update a symbol to refer to it.
    ///
    /// Returns the section offset of the data.
    pub fn add_symbol_data(
        &mut self,
        symbol: SymbolId,
        section: SectionId,
        data: &[u8],
        align: u64,
    ) -> u64 {
        let offset = self.append_section_data(section, data, align);
        let symbol = self.symbol_mut(symbol);
        symbol.value = offset;
        symbol.size = data.len() as u64;
        symbol.section = Some(section);
        offset
    }

    /// Add a relocation to a section.
    ///
    /// Relocations must only be added after the referenced symbols have been added
    /// and defined (if applicable).
    pub fn add_relocation(
        &mut self,
        section: SectionId,
        mut relocation: Relocation,
    ) -> Result<(), String> {
        let addend = match self.format {
            BinaryFormat::Elf => self.elf_fixup_relocation(&mut relocation)?,
            BinaryFormat::Coff => self.coff_fixup_relocation(&mut relocation),
            BinaryFormat::Macho => self.macho_fixup_relocation(&mut relocation),
            _ => unimplemented!(),
        };
        if addend != 0 {
            self.write_relocation_addend(section, &relocation, addend)?;
        }
        self.sections[section.0].relocations.push(relocation);
        Ok(())
    }

    fn write_relocation_addend(
        &mut self,
        section: SectionId,
        relocation: &Relocation,
        addend: i64,
    ) -> Result<(), String> {
        let endian = match self.architecture.endianness().unwrap() {
            Endianness::Little => scroll::LE,
            Endianness::Big => scroll::BE,
        };

        let data = &mut self.sections[section.0].data;
        if relocation.offset + (u64::from(relocation.size) + 7) / 8 > data.len() as u64 {
            return Err(format!(
                "invalid relocation offset {}+{} (max {})",
                relocation.offset,
                relocation.size,
                data.len()
            ));
        }
        match relocation.size {
            32 => {
                data.pwrite_with(addend as i32, relocation.offset as usize, endian)
                    .unwrap();
            }
            64 => {
                data.pwrite_with(addend, relocation.offset as usize, endian)
                    .unwrap();
            }
            _ => return Err(format!("unimplemented relocation addend {:?}", relocation)),
        }
        Ok(())
    }

    /// Write the object to a `Vec`.
    pub fn write(&self) -> Result<Vec<u8>, String> {
        match self.format {
            BinaryFormat::Elf => self.elf_write(),
            BinaryFormat::Coff => self.coff_write(),
            BinaryFormat::Macho => self.macho_write(),
            _ => unimplemented!(),
        }
    }
}

/// A standard segment kind.
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum StandardSegment {
    Text,
    Data,
    Debug,
}

/// A standard section kind.
#[allow(missing_docs)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum StandardSection {
    Text,
    Data,
    ReadOnlyData,
    ReadOnlyDataWithRel,
    ReadOnlyString,
    UninitializedData,
}

impl StandardSection {
    /// Return the section kind of a standard section.
    pub fn kind(self) -> SectionKind {
        match self {
            StandardSection::Text => SectionKind::Text,
            StandardSection::Data => SectionKind::Data,
            StandardSection::ReadOnlyData | StandardSection::ReadOnlyDataWithRel => {
                SectionKind::ReadOnlyData
            }
            StandardSection::ReadOnlyString => SectionKind::ReadOnlyString,
            StandardSection::UninitializedData => SectionKind::UninitializedData,
        }
    }

    fn all() -> &'static [StandardSection] {
        &[
            StandardSection::Text,
            StandardSection::Data,
            StandardSection::ReadOnlyData,
            StandardSection::ReadOnlyDataWithRel,
            StandardSection::ReadOnlyString,
            StandardSection::UninitializedData,
        ]
    }
}

/// An identifier used to reference a section.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct SectionId(usize);

/// A section in an object file.
#[derive(Debug)]
pub struct Section {
    segment: Vec<u8>,
    name: Vec<u8>,
    kind: SectionKind,
    size: u64,
    align: u64,
    data: Vec<u8>,
    relocations: Vec<Relocation>,
    symbol: Option<SymbolId>,
}

impl Section {
    /// Return true if this section contains uninitialized data.
    #[inline]
    pub fn is_bss(&self) -> bool {
        self.kind == SectionKind::UninitializedData || self.kind == SectionKind::UninitializedTls
    }

    /// Set the data for a section.
    ///
    /// Must not be called for sections that already have data, or that contain uninitialized data.
    pub fn set_data(&mut self, data: Vec<u8>, align: u64) {
        debug_assert!(!self.is_bss());
        debug_assert_eq!(align & (align - 1), 0);
        debug_assert!(self.data.is_empty());
        self.size = data.len() as u64;
        self.data = data;
        self.align = align;
    }

    /// Append data to a section.
    ///
    /// Must not be called for sections that contain uninitialized data.
    pub fn append_data(&mut self, data: &[u8], align: u64) -> u64 {
        debug_assert!(!self.is_bss());
        debug_assert_eq!(align & (align - 1), 0);
        if self.align < align {
            self.align = align;
        }
        let align = align as usize;
        let mut offset = self.data.len();
        if offset & (align - 1) != 0 {
            offset += align - (offset & (align - 1));
            self.data.resize(offset, 0);
        }
        self.data.extend(data);
        self.size = self.data.len() as u64;
        offset as u64
    }

    /// Append unitialized data to a section.
    ///
    /// Must not be called for sections that contain initialized data.
    pub fn append_bss(&mut self, size: u64, align: u64) -> u64 {
        debug_assert!(self.is_bss());
        debug_assert_eq!(align & (align - 1), 0);
        if self.align < align {
            self.align = align;
        }
        let mut offset = self.size;
        if offset & (align - 1) != 0 {
            offset += align - (offset & (align - 1));
            self.size = offset;
        }
        self.size += size;
        offset as u64
    }
}

/// An identifier used to reference a symbol.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct SymbolId(usize);

/// A symbol in an object file.
#[derive(Debug)]
pub struct Symbol {
    /// The name of the symbol.
    pub name: Vec<u8>,
    /// The value of the symbol.
    ///
    /// If the symbol defined in a section, then this is the section offset of the symbol.
    pub value: u64,
    /// The size of the symbol.
    pub size: u64,
    /// The kind of the symbol.
    pub kind: SymbolKind,
    /// The scope of the symbol.
    pub scope: SymbolScope,
    /// Whether the symbol has weak binding.
    pub weak: bool,
    /// The section containing the symbol.
    ///
    /// Set to `None` for undefined symbols.
    pub section: Option<SectionId>,
}

impl Symbol {
    /// Return true if the symbol is undefined.
    #[inline]
    pub fn is_undefined(&self) -> bool {
        self.section.is_none()
    }

    /// Return true if the symbol scope is local.
    #[inline]
    pub fn is_local(&self) -> bool {
        self.scope == SymbolScope::Compilation
    }
}

/// A relocation in an object file.
#[derive(Debug)]
pub struct Relocation {
    /// The section offset of the place of the relocation.
    pub offset: u64,
    /// The size in bits of the place of relocation.
    pub size: u8,
    /// The operation used to calculate the result of the relocation.
    pub kind: RelocationKind,
    /// Information about how the result of the relocation operation is encoded in the place.
    pub encoding: RelocationEncoding,
    /// The symbol referred to by the relocation.
    ///
    /// This may be a section symbol.
    pub symbol: SymbolId,
    /// The addend to use in the relocation calculation.
    ///
    /// This may be in addition to an implicit addend stored at the place of the relocation.
    pub addend: i64,
}

/// The symbol name mangling scheme.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Mangling {
    /// No symbol mangling.
    None,
    /// Windows COFF symbol mangling.
    Coff,
    /// ELF symbol mangling.
    Elf,
    /// Mach-O symbol mangling.
    Macho,
}

impl Mangling {
    /// Return the default symboling mangling for the given format and architecture.
    pub fn default(format: BinaryFormat, _architecture: Architecture) -> Self {
        match format {
            BinaryFormat::Coff => Mangling::Coff,
            BinaryFormat::Elf => Mangling::Elf,
            BinaryFormat::Macho => Mangling::Macho,
            _ => Mangling::None,
        }
    }

    /// Return the prefix to use for global symbols.
    pub fn global_prefix(self) -> Option<u8> {
        match self {
            Mangling::None | Mangling::Elf => None,
            Mangling::Coff | Mangling::Macho => Some(b'_'),
        }
    }
}
