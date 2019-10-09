use crate::alloc::borrow::Cow;
use crate::alloc::vec::Vec;
use goblin::container;
use goblin::mach;
use goblin::mach::load_command::CommandVariant;
use std::{fmt, iter, ops, slice};
use target_lexicon::{Aarch64Architecture, Architecture, ArmArchitecture};
use uuid::Uuid;

use crate::read::{
    self, Object, ObjectSection, ObjectSegment, Relocation, RelocationEncoding, RelocationKind,
    RelocationTarget, SectionIndex, SectionKind, Symbol, SymbolIndex, SymbolKind, SymbolMap,
    SymbolScope,
};

/// A Mach-O object file.
#[derive(Debug)]
pub struct MachOFile<'data> {
    macho: mach::MachO<'data>,
    data: &'data [u8],
    ctx: container::Ctx,
    sections: Vec<MachOSectionInternal<'data>>,
}

impl<'data> MachOFile<'data> {
    /// Get the Mach-O headers of the file.
    // TODO: this is temporary to allow access to features this crate doesn't provide yet
    #[inline]
    pub fn macho(&self) -> &mach::MachO<'data> {
        &self.macho
    }

    /// Parse the raw Mach-O file data.
    pub fn parse(data: &'data [u8]) -> Result<Self, &'static str> {
        let (_magic, ctx) =
            mach::parse_magic_and_ctx(data, 0).map_err(|_| "Could not parse Mach-O magic")?;
        let ctx = ctx.ok_or("Invalid Mach-O magic")?;
        let macho = mach::MachO::parse(data, 0).map_err(|_| "Could not parse Mach-O header")?;
        // Build a list of sections to make some operations more efficient.
        let mut sections = Vec::new();
        'segments: for segment in &macho.segments {
            for section_result in segment {
                if let Ok((section, data)) = section_result {
                    sections.push(MachOSectionInternal::parse(section, data));
                } else {
                    break 'segments;
                }
            }
        }
        Ok(MachOFile {
            macho,
            data,
            ctx,
            sections,
        })
    }

    /// Return the section at the given index.
    #[inline]
    fn section_internal(&self, index: SectionIndex) -> Option<&MachOSectionInternal<'data>> {
        index
            .0
            .checked_sub(1)
            .and_then(|index| self.sections.get(index))
    }
}

impl<'data, 'file> Object<'data, 'file> for MachOFile<'data>
where
    'data: 'file,
{
    type Segment = MachOSegment<'data, 'file>;
    type SegmentIterator = MachOSegmentIterator<'data, 'file>;
    type Section = MachOSection<'data, 'file>;
    type SectionIterator = MachOSectionIterator<'data, 'file>;
    type SymbolIterator = MachOSymbolIterator<'data, 'file>;

    fn architecture(&self) -> Architecture {
        match self.macho.header.cputype {
            mach::cputype::CPU_TYPE_ARM => Architecture::Arm(ArmArchitecture::Arm),
            mach::cputype::CPU_TYPE_ARM64 => Architecture::Aarch64(Aarch64Architecture::Aarch64),
            mach::cputype::CPU_TYPE_X86 => Architecture::I386,
            mach::cputype::CPU_TYPE_X86_64 => Architecture::X86_64,
            mach::cputype::CPU_TYPE_MIPS => Architecture::Mips,
            _ => Architecture::Unknown,
        }
    }

    #[inline]
    fn is_little_endian(&self) -> bool {
        self.macho.little_endian
    }

    #[inline]
    fn is_64(&self) -> bool {
        self.macho.is_64
    }

    fn segments(&'file self) -> MachOSegmentIterator<'data, 'file> {
        MachOSegmentIterator {
            segments: self.macho.segments.iter(),
        }
    }

    fn section_by_name(&'file self, section_name: &str) -> Option<MachOSection<'data, 'file>> {
        // Translate the "." prefix to the "__" prefix used by OSX/Mach-O, eg
        // ".debug_info" to "__debug_info".
        let system_section = section_name.starts_with('.');
        let cmp_section_name = |section: &MachOSection| {
            section
                .name()
                .map(|name| {
                    section_name == name
                        || (system_section
                            && name.starts_with("__")
                            && &section_name[1..] == &name[2..])
                })
                .unwrap_or(false)
        };

        self.sections().find(cmp_section_name)
    }

    fn section_by_index(&'file self, index: SectionIndex) -> Option<MachOSection<'data, 'file>> {
        self.section_internal(index)
            .map(|_| MachOSection { file: self, index })
    }

    fn sections(&'file self) -> MachOSectionIterator<'data, 'file> {
        MachOSectionIterator {
            file: self,
            iter: 0..self.sections.len(),
        }
    }

    fn symbol_by_index(&self, index: SymbolIndex) -> Option<Symbol<'data>> {
        self.macho
            .symbols
            .as_ref()
            .and_then(|symbols| symbols.get(index.0).ok())
            .and_then(|(name, nlist)| parse_symbol(self, name, &nlist))
    }

    fn symbols(&'file self) -> MachOSymbolIterator<'data, 'file> {
        let symbols = match self.macho.symbols {
            Some(ref symbols) => symbols.into_iter(),
            None => mach::symbols::SymbolIterator::default(),
        }
        .enumerate();

        MachOSymbolIterator {
            file: self,
            symbols,
        }
    }

    fn dynamic_symbols(&'file self) -> MachOSymbolIterator<'data, 'file> {
        // The LC_DYSYMTAB command contains indices into the same symbol
        // table as the LC_SYMTAB command, so return all of them.
        self.symbols()
    }

    fn symbol_map(&self) -> SymbolMap<'data> {
        let mut symbols: Vec<_> = self.symbols().map(|(_, s)| s).collect();

        // Add symbols for the end of each section.
        for section in self.sections() {
            symbols.push(Symbol {
                name: None,
                address: section.address() + section.size(),
                size: 0,
                kind: SymbolKind::Section,
                section_index: None,
                undefined: false,
                weak: false,
                scope: SymbolScope::Compilation,
            });
        }

        // Calculate symbol sizes by sorting and finding the next symbol.
        symbols.sort_by(|a, b| {
            a.address.cmp(&b.address).then_with(|| {
                // Place the end of section symbols last.
                (a.kind == SymbolKind::Section).cmp(&(b.kind == SymbolKind::Section))
            })
        });

        for i in 0..symbols.len() {
            let (before, after) = symbols.split_at_mut(i + 1);
            let symbol = &mut before[i];
            if symbol.kind != SymbolKind::Section {
                if let Some(next) = after
                    .iter()
                    .skip_while(|x| x.kind != SymbolKind::Section && x.address == symbol.address)
                    .next()
                {
                    symbol.size = next.address - symbol.address;
                }
            }
        }

        symbols.retain(SymbolMap::filter);
        SymbolMap { symbols }
    }

    fn has_debug_symbols(&self) -> bool {
        self.section_data_by_name(".debug_info").is_some()
    }

    fn mach_uuid(&self) -> Option<Uuid> {
        // Return the UUID from the `LC_UUID` load command, if one is present.
        self.macho
            .load_commands
            .iter()
            .filter_map(|lc| {
                match lc.command {
                    CommandVariant::Uuid(ref cmd) => {
                        //TODO: Uuid should have a `from_array` method that can't fail.
                        Uuid::from_slice(&cmd.uuid).ok()
                    }
                    _ => None,
                }
            })
            .nth(0)
    }

    fn entry(&self) -> u64 {
        self.macho.entry
    }
}

/// An iterator over the segments of a `MachOFile`.
#[derive(Debug)]
pub struct MachOSegmentIterator<'data, 'file>
where
    'data: 'file,
{
    segments: slice::Iter<'file, mach::segment::Segment<'data>>,
}

impl<'data, 'file> Iterator for MachOSegmentIterator<'data, 'file> {
    type Item = MachOSegment<'data, 'file>;

    fn next(&mut self) -> Option<Self::Item> {
        self.segments.next().map(|segment| MachOSegment { segment })
    }
}

/// A segment of a `MachOFile`.
#[derive(Debug)]
pub struct MachOSegment<'data, 'file>
where
    'data: 'file,
{
    segment: &'file mach::segment::Segment<'data>,
}

impl<'data, 'file> ObjectSegment<'data> for MachOSegment<'data, 'file> {
    #[inline]
    fn address(&self) -> u64 {
        self.segment.vmaddr
    }

    #[inline]
    fn size(&self) -> u64 {
        self.segment.vmsize
    }

    #[inline]
    fn align(&self) -> u64 {
        // Page size.
        0x1000
    }

    #[inline]
    fn data(&self) -> &'data [u8] {
        self.segment.data
    }

    fn data_range(&self, address: u64, size: u64) -> Option<&'data [u8]> {
        read::data_range(self.data(), self.address(), address, size)
    }

    #[inline]
    fn name(&self) -> Option<&str> {
        self.segment.name().ok()
    }
}

/// An iterator over the sections of a `MachOFile`.
pub struct MachOSectionIterator<'data, 'file>
where
    'data: 'file,
{
    file: &'file MachOFile<'data>,
    iter: ops::Range<usize>,
}

impl<'data, 'file> fmt::Debug for MachOSectionIterator<'data, 'file> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // It's painful to do much better than this
        f.debug_struct("MachOSectionIterator").finish()
    }
}

impl<'data, 'file> Iterator for MachOSectionIterator<'data, 'file> {
    type Item = MachOSection<'data, 'file>;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next().map(|index| MachOSection {
            file: self.file,
            index: SectionIndex(index + 1),
        })
    }
}

/// A section of a `MachOFile`.
#[derive(Debug)]
pub struct MachOSection<'data, 'file>
where
    'data: 'file,
{
    file: &'file MachOFile<'data>,
    index: SectionIndex,
}

impl<'data, 'file> MachOSection<'data, 'file> {
    #[inline]
    fn internal(&self) -> &'file MachOSectionInternal<'data> {
        // We ensure the index is always valid.
        &self.file.section_internal(self.index).unwrap()
    }
}

impl<'data, 'file> ObjectSection<'data> for MachOSection<'data, 'file> {
    type RelocationIterator = MachORelocationIterator<'data, 'file>;

    #[inline]
    fn index(&self) -> SectionIndex {
        self.index
    }

    #[inline]
    fn address(&self) -> u64 {
        self.internal().section.addr
    }

    #[inline]
    fn size(&self) -> u64 {
        self.internal().section.size
    }

    #[inline]
    fn align(&self) -> u64 {
        1 << self.internal().section.align
    }

    #[inline]
    fn data(&self) -> Cow<'data, [u8]> {
        Cow::from(self.internal().data)
    }

    fn data_range(&self, address: u64, size: u64) -> Option<&'data [u8]> {
        read::data_range(self.internal().data, self.address(), address, size)
    }

    #[inline]
    fn uncompressed_data(&self) -> Cow<'data, [u8]> {
        // TODO: does MachO support compression?
        self.data()
    }

    #[inline]
    fn name(&self) -> Option<&str> {
        self.internal().section.name().ok()
    }

    #[inline]
    fn segment_name(&self) -> Option<&str> {
        self.internal().section.segname().ok()
    }

    fn kind(&self) -> SectionKind {
        self.internal().kind
    }

    fn relocations(&self) -> MachORelocationIterator<'data, 'file> {
        MachORelocationIterator {
            file: self.file,
            relocations: self
                .internal()
                .section
                .iter_relocations(self.file.data, self.file.ctx),
        }
    }
}

#[derive(Debug)]
struct MachOSectionInternal<'data> {
    section: mach::segment::Section,
    data: mach::segment::SectionData<'data>,
    kind: SectionKind,
}

impl<'data> MachOSectionInternal<'data> {
    fn parse(section: mach::segment::Section, data: mach::segment::SectionData<'data>) -> Self {
        let kind = if let (Ok(segname), Ok(name)) = (section.segname(), section.name()) {
            match (segname, name) {
                ("__TEXT", "__text") => SectionKind::Text,
                ("__TEXT", "__const") => SectionKind::ReadOnlyData,
                ("__TEXT", "__cstring") => SectionKind::ReadOnlyString,
                ("__TEXT", "__eh_frame") => SectionKind::ReadOnlyData,
                ("__TEXT", "__gcc_except_tab") => SectionKind::ReadOnlyData,
                ("__DATA", "__data") => SectionKind::Data,
                ("__DATA", "__const") => SectionKind::ReadOnlyData,
                ("__DATA", "__bss") => SectionKind::UninitializedData,
                ("__DATA", "__thread_data") => SectionKind::Tls,
                ("__DATA", "__thread_bss") => SectionKind::UninitializedTls,
                ("__DATA", "__thread_vars") => SectionKind::TlsVariables,
                ("__DWARF", _) => SectionKind::Debug,
                _ => SectionKind::Unknown,
            }
        } else {
            SectionKind::Unknown
        };
        MachOSectionInternal {
            section,
            data,
            kind,
        }
    }
}

/// An iterator over the symbols of a `MachOFile`.
pub struct MachOSymbolIterator<'data, 'file> {
    file: &'file MachOFile<'data>,
    symbols: iter::Enumerate<mach::symbols::SymbolIterator<'data>>,
}

impl<'data, 'file> fmt::Debug for MachOSymbolIterator<'data, 'file> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MachOSymbolIterator").finish()
    }
}

impl<'data, 'file> Iterator for MachOSymbolIterator<'data, 'file> {
    type Item = (SymbolIndex, Symbol<'data>);

    fn next(&mut self) -> Option<Self::Item> {
        while let Some((index, Ok((name, nlist)))) = self.symbols.next() {
            if let Some(symbol) = parse_symbol(self.file, name, &nlist) {
                return Some((SymbolIndex(index), symbol));
            }
        }
        None
    }
}

fn parse_symbol<'data>(
    file: &MachOFile<'data>,
    name: &'data str,
    nlist: &mach::symbols::Nlist,
) -> Option<Symbol<'data>> {
    if nlist.n_type & mach::symbols::N_STAB != 0 {
        return None;
    }
    let n_type = nlist.n_type & mach::symbols::NLIST_TYPE_MASK;
    let section_index = if n_type == mach::symbols::N_SECT {
        if nlist.n_sect == 0 {
            None
        } else {
            Some(SectionIndex(nlist.n_sect))
        }
    } else {
        // TODO: better handling for other n_type values
        None
    };
    let kind = section_index
        .and_then(|index| file.section_internal(index))
        .map(|section| match section.kind {
            SectionKind::Text => SymbolKind::Text,
            SectionKind::Data
            | SectionKind::ReadOnlyData
            | SectionKind::ReadOnlyString
            | SectionKind::UninitializedData => SymbolKind::Data,
            SectionKind::Tls | SectionKind::UninitializedTls | SectionKind::TlsVariables => {
                SymbolKind::Tls
            }
            _ => SymbolKind::Unknown,
        })
        .unwrap_or(SymbolKind::Unknown);
    let undefined = nlist.is_undefined();
    let weak = nlist.is_weak();
    let scope = if undefined {
        SymbolScope::Unknown
    } else if nlist.n_type & mach::symbols::N_EXT == 0 {
        SymbolScope::Compilation
    } else if nlist.n_type & mach::symbols::N_PEXT != 0 {
        SymbolScope::Linkage
    } else {
        SymbolScope::Dynamic
    };
    Some(Symbol {
        name: Some(name),
        address: nlist.n_value,
        // Only calculated for symbol maps
        size: 0,
        kind,
        section_index,
        undefined,
        weak,
        scope,
    })
}

/// An iterator over the relocations in an `MachOSection`.
pub struct MachORelocationIterator<'data, 'file>
where
    'data: 'file,
{
    file: &'file MachOFile<'data>,
    relocations: mach::segment::RelocationIterator<'data>,
}

impl<'data, 'file> Iterator for MachORelocationIterator<'data, 'file> {
    type Item = (u64, Relocation);

    fn next(&mut self) -> Option<Self::Item> {
        self.relocations.next()?.ok().map(|reloc| {
            let mut encoding = RelocationEncoding::Generic;
            let kind = match self.file.macho.header.cputype {
                mach::cputype::CPU_TYPE_ARM => match (reloc.r_type(), reloc.r_pcrel()) {
                    (mach::relocation::ARM_RELOC_VANILLA, 0) => RelocationKind::Absolute,
                    _ => RelocationKind::Other(reloc.r_info),
                },
                mach::cputype::CPU_TYPE_ARM64 => match (reloc.r_type(), reloc.r_pcrel()) {
                    (mach::relocation::ARM64_RELOC_UNSIGNED, 0) => RelocationKind::Absolute,
                    _ => RelocationKind::Other(reloc.r_info),
                },
                mach::cputype::CPU_TYPE_X86 => match (reloc.r_type(), reloc.r_pcrel()) {
                    (mach::relocation::GENERIC_RELOC_VANILLA, 0) => RelocationKind::Absolute,
                    _ => RelocationKind::Other(reloc.r_info),
                },
                mach::cputype::CPU_TYPE_X86_64 => match (reloc.r_type(), reloc.r_pcrel()) {
                    (mach::relocation::X86_64_RELOC_UNSIGNED, 0) => RelocationKind::Absolute,
                    (mach::relocation::X86_64_RELOC_SIGNED, 1) => {
                        encoding = RelocationEncoding::X86RipRelative;
                        RelocationKind::Relative
                    }
                    (mach::relocation::X86_64_RELOC_BRANCH, 1) => {
                        encoding = RelocationEncoding::X86Branch;
                        RelocationKind::Relative
                    }
                    (mach::relocation::X86_64_RELOC_GOT, 1) => RelocationKind::GotRelative,
                    (mach::relocation::X86_64_RELOC_GOT_LOAD, 1) => {
                        encoding = RelocationEncoding::X86RipRelativeMovq;
                        RelocationKind::GotRelative
                    }
                    _ => RelocationKind::Other(reloc.r_info),
                },
                _ => RelocationKind::Other(reloc.r_info),
            };
            let size = 8 << reloc.r_length();
            let target = if reloc.is_extern() {
                RelocationTarget::Symbol(SymbolIndex(reloc.r_symbolnum()))
            } else {
                RelocationTarget::Section(SectionIndex(reloc.r_symbolnum()))
            };
            let addend = if reloc.r_pcrel() != 0 { -4 } else { 0 };
            (
                reloc.r_address as u64,
                Relocation {
                    kind,
                    encoding,
                    size,
                    target,
                    addend,
                    implicit_addend: true,
                },
            )
        })
    }
}

impl<'data, 'file> fmt::Debug for MachORelocationIterator<'data, 'file> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MachORelocationIterator").finish()
    }
}
