use scroll::ctx::SizeWith;
use scroll::{IOwrite, Pwrite};
use std::string::String;

use crate::alloc::vec::Vec;
use crate::write::string::*;
use crate::write::util::*;
use crate::write::*;

mod mach {
    pub use goblin::mach::constants::cputype::*;
    pub use goblin::mach::constants::*;
    pub use goblin::mach::header::*;
    pub use goblin::mach::load_command::*;
    pub use goblin::mach::relocation::*;
    pub use goblin::mach::segment::*;
    pub use goblin::mach::symbols::*;
}

#[derive(Default, Clone, Copy)]
struct SectionOffsets {
    index: usize,
    offset: usize,
    address: u64,
    reloc_offset: usize,
}

#[derive(Default, Clone, Copy)]
struct SymbolOffsets {
    index: usize,
    str_id: Option<StringId>,
}

impl Object {
    pub(crate) fn macho_segment_name(&self, segment: StandardSegment) -> &'static [u8] {
        match segment {
            StandardSegment::Text => &b"__TEXT"[..],
            StandardSegment::Data => &b"__DATA"[..],
            StandardSegment::Debug => &b"__DWARF"[..],
        }
    }

    pub(crate) fn macho_section_info(
        &self,
        section: StandardSection,
    ) -> (&'static [u8], &'static [u8], SectionKind) {
        match section {
            StandardSection::Text => (&b"__TEXT"[..], &b"__text"[..], SectionKind::Text),
            StandardSection::Data => (&b"__DATA"[..], &b"__data"[..], SectionKind::Data),
            StandardSection::ReadOnlyData => {
                (&b"__TEXT"[..], &b"__const"[..], SectionKind::ReadOnlyData)
            }
            StandardSection::ReadOnlyDataWithRel => {
                (&b"__DATA"[..], &b"__const"[..], SectionKind::ReadOnlyData)
            }
            StandardSection::ReadOnlyString => (
                &b"__TEXT"[..],
                &b"__cstring"[..],
                SectionKind::ReadOnlyString,
            ),
            StandardSection::UninitializedData => (
                &b"__DATA"[..],
                &b"__bss"[..],
                SectionKind::UninitializedData,
            ),
        }
    }

    pub(crate) fn macho_fixup_relocation(&mut self, mut relocation: &mut Relocation) -> i64 {
        let constant = match relocation.kind {
            RelocationKind::Relative
            | RelocationKind::GotRelative
            | RelocationKind::PltRelative => relocation.addend + 4,
            _ => relocation.addend,
        };
        relocation.addend -= constant;
        constant
    }

    pub(crate) fn macho_write(&self) -> Result<Vec<u8>, String> {
        let endian = match self.architecture.endianness().unwrap() {
            Endianness::Little => goblin::container::Endian::Little,
            Endianness::Big => goblin::container::Endian::Big,
        };
        let (container, pointer_align) = match self.architecture.pointer_width().unwrap() {
            PointerWidth::U16 | PointerWidth::U32 => (goblin::container::Container::Little, 4),
            PointerWidth::U64 => (goblin::container::Container::Big, 8),
        };
        let ctx = goblin::container::Ctx::new(container, endian);

        // Calculate offsets of everything, and build strtab.
        let mut offset = 0;

        // Calculate size of Mach-O header.
        offset += mach::Header::size_with(&ctx);

        // Calculate size of commands.
        let mut ncmds = 0;
        let command_offset = offset;

        // Calculate size of segment command and section headers.
        let segment_command_offset = offset;
        let segment_command_len =
            mach::Segment::size_with(&ctx) + self.sections.len() * mach::Section::size_with(&ctx);
        offset += segment_command_len;
        ncmds += 1;

        // Calculate size of symtab command.
        let symtab_command_offset = offset;
        let symtab_command_len = mach::SymtabCommand::size_with(&ctx.le);
        offset += symtab_command_len;
        ncmds += 1;

        let sizeofcmds = offset - command_offset;

        // Calculate size of section data.
        let segment_data_offset = offset;
        let mut section_offsets = vec![SectionOffsets::default(); self.sections.len()];
        let mut address = 0;
        for (index, section) in self.sections.iter().enumerate() {
            section_offsets[index].index = 1 + index;
            let len = section.data.len();
            if len != 0 {
                offset = align(offset, section.align as usize);
                section_offsets[index].offset = offset;
                offset += len;
            } else {
                section_offsets[index].offset = offset;
            }
            address = align_u64(address, section.align);
            section_offsets[index].address = address;
            address += section.size;
        }
        let segment_data_size = offset - segment_data_offset;

        // Count symbols and add symbol strings to strtab.
        let mut strtab = StringTable::default();
        let mut symbol_offsets = vec![SymbolOffsets::default(); self.symbols.len()];
        let mut nsyms = 0;
        for (index, symbol) in self.symbols.iter().enumerate() {
            if !symbol.is_undefined() {
                match symbol.kind {
                    SymbolKind::Text | SymbolKind::Data => {}
                    SymbolKind::File | SymbolKind::Section => continue,
                    _ => return Err(format!("unimplemented symbol {:?}", symbol)),
                }
            }
            symbol_offsets[index].index = nsyms;
            nsyms += 1;
            if !symbol.name.is_empty() {
                symbol_offsets[index].str_id = Some(strtab.add(&symbol.name));
            }
        }

        // Calculate size of symtab.
        offset = align(offset, pointer_align);
        let symtab_offset = offset;
        let symtab_len = nsyms * mach::Nlist::size_with(&ctx);
        offset += symtab_len;

        // Calculate size of strtab.
        let strtab_offset = offset;
        let mut strtab_data = Vec::new();
        // Null name.
        strtab_data.push(0);
        strtab.write(1, &mut strtab_data);
        offset += strtab_data.len();

        // Calculate size of relocations.
        for (index, section) in self.sections.iter().enumerate() {
            let count = section.relocations.len();
            if count != 0 {
                offset = align(offset, 4);
                section_offsets[index].reloc_offset = offset;
                let len = count * mach::RelocationInfo::size_with(&ctx.le);
                offset += len;
            }
        }

        // Start writing.
        let mut buffer = Vec::with_capacity(offset);

        // Write file header.
        let (cputype, cpusubtype) = match self.architecture {
            Architecture::Arm(_) => (mach::CPU_TYPE_ARM, mach::CPU_SUBTYPE_ARM_ALL),
            Architecture::Aarch64(_) => (mach::CPU_TYPE_ARM64, mach::CPU_SUBTYPE_ARM64_ALL),
            Architecture::I386 => (mach::CPU_TYPE_I386, mach::CPU_SUBTYPE_I386_ALL),
            Architecture::X86_64 => (mach::CPU_TYPE_X86_64, mach::CPU_SUBTYPE_X86_64_ALL),
            _ => {
                return Err(format!(
                    "unimplemented architecture {:?}",
                    self.architecture
                ))
            }
        };

        let header = mach::Header {
            magic: if ctx.is_big() {
                mach::MH_MAGIC_64
            } else {
                mach::MH_MAGIC
            },
            cputype,
            cpusubtype,
            filetype: mach::MH_OBJECT,
            ncmds,
            sizeofcmds: sizeofcmds as u32,
            flags: if self.subsection_via_symbols {
                mach::MH_SUBSECTIONS_VIA_SYMBOLS
            } else {
                0
            },
            reserved: 0,
        };
        buffer.iowrite_with(header, ctx).unwrap();

        // Write segment command.
        debug_assert_eq!(segment_command_offset, buffer.len());
        let mut segment_command = mach::Segment::new(ctx, &[]);
        segment_command.cmd = if ctx.is_big() {
            mach::LC_SEGMENT_64
        } else {
            mach::LC_SEGMENT
        };
        segment_command.cmdsize = segment_command_len as u32;
        segment_command.segname = [0; 16];
        segment_command.vmaddr = 0;
        segment_command.vmsize = address;
        segment_command.fileoff = segment_data_offset as u64;
        segment_command.filesize = segment_data_size as u64;
        segment_command.maxprot = mach::VM_PROT_READ | mach::VM_PROT_WRITE | mach::VM_PROT_EXECUTE;
        segment_command.initprot = mach::VM_PROT_READ | mach::VM_PROT_WRITE | mach::VM_PROT_EXECUTE;
        segment_command.nsects = self.sections.len() as u32;
        segment_command.flags = 0;
        buffer.iowrite_with(segment_command, ctx).unwrap();

        // Write section headers.
        for (index, section) in self.sections.iter().enumerate() {
            let mut sectname = [0; 16];
            sectname.pwrite(&*section.name, 0).unwrap();
            let mut segname = [0; 16];
            segname.pwrite(&*section.segment, 0).unwrap();
            let flags = match section.kind {
                SectionKind::Text => {
                    mach::S_ATTR_PURE_INSTRUCTIONS | mach::S_ATTR_SOME_INSTRUCTIONS
                }
                SectionKind::Data => 0,
                SectionKind::ReadOnlyData => 0,
                SectionKind::ReadOnlyString => mach::S_CSTRING_LITERALS,
                SectionKind::UninitializedData => mach::S_ZEROFILL,
                SectionKind::Tls => mach::S_THREAD_LOCAL_REGULAR,
                SectionKind::UninitializedTls => mach::S_THREAD_LOCAL_ZEROFILL,
                SectionKind::TlsVariables => mach::S_THREAD_LOCAL_VARIABLES,
                SectionKind::Debug => mach::S_ATTR_DEBUG,
                SectionKind::OtherString => mach::S_CSTRING_LITERALS,
                SectionKind::Other
                | SectionKind::Unknown
                | SectionKind::Linker
                | SectionKind::Metadata => 0,
            };
            buffer
                .iowrite_with(
                    mach::Section {
                        sectname,
                        segname,
                        addr: section_offsets[index].address,
                        size: section.size,
                        offset: section_offsets[index].offset as u32,
                        align: section.align.trailing_zeros(),
                        reloff: section_offsets[index].reloc_offset as u32,
                        nreloc: section.relocations.len() as u32,
                        flags,
                    },
                    ctx,
                )
                .unwrap();
        }

        // Write symtab command.
        debug_assert_eq!(symtab_command_offset, buffer.len());
        buffer
            .iowrite_with(
                mach::SymtabCommand {
                    cmd: mach::LC_SYMTAB,
                    cmdsize: symtab_command_len as u32,
                    symoff: symtab_offset as u32,
                    nsyms: nsyms as u32,
                    stroff: strtab_offset as u32,
                    strsize: strtab_data.len() as u32,
                },
                ctx.le,
            )
            .unwrap();

        // Write section data.
        debug_assert_eq!(segment_data_offset, buffer.len());
        for (index, section) in self.sections.iter().enumerate() {
            let len = section.data.len();
            if len != 0 {
                write_align(&mut buffer, section.align as usize);
                debug_assert_eq!(section_offsets[index].offset, buffer.len());
                buffer.extend(&section.data);
            }
        }

        // Write symtab.
        write_align(&mut buffer, pointer_align);
        debug_assert_eq!(symtab_offset, buffer.len());
        for (index, symbol) in self.symbols.iter().enumerate() {
            if !symbol.is_undefined() {
                match symbol.kind {
                    SymbolKind::Text | SymbolKind::Data => {}
                    SymbolKind::File | SymbolKind::Section => continue,
                    _ => return Err(format!("unimplemented symbol {:?}", symbol)),
                }
            }
            // TODO: N_STAB
            // TODO: N_ABS
            let mut n_type = if symbol.is_undefined() {
                mach::N_UNDF | mach::N_EXT
            } else {
                mach::N_SECT
            };
            match symbol.scope {
                SymbolScope::Unknown | SymbolScope::Compilation => {}
                SymbolScope::Linkage => {
                    n_type |= mach::N_EXT | mach::N_PEXT;
                }
                SymbolScope::Dynamic => {
                    n_type |= mach::N_EXT;
                }
            }

            let mut n_desc = 0;
            if symbol.weak {
                if symbol.is_undefined() {
                    n_desc |= mach::N_WEAK_REF;
                } else {
                    n_desc |= mach::N_WEAK_DEF;
                }
            }

            let n_value = match symbol.section {
                Some(section) => section_offsets[section.0].address + symbol.value,
                None => symbol.value,
            };

            let n_strx = symbol_offsets[index]
                .str_id
                .map(|id| strtab.get_offset(id))
                .unwrap_or(0);

            buffer
                .iowrite_with(
                    mach::Nlist {
                        n_strx,
                        n_type,
                        n_sect: symbol.section.map(|x| x.0 + 1).unwrap_or(0),
                        n_desc,
                        n_value,
                    },
                    ctx,
                )
                .unwrap();
        }

        // Write strtab.
        debug_assert_eq!(strtab_offset, buffer.len());
        buffer.extend(&strtab_data);

        // Write relocations.
        for (index, section) in self.sections.iter().enumerate() {
            if !section.relocations.is_empty() {
                write_align(&mut buffer, 4);
                debug_assert_eq!(section_offsets[index].reloc_offset, buffer.len());
                for reloc in &section.relocations {
                    let r_extern;
                    let r_symbolnum;
                    let symbol = &self.symbols[reloc.symbol.0];
                    if symbol.kind == SymbolKind::Section {
                        r_symbolnum = section_offsets[symbol.section.unwrap().0].index as u32;
                        r_extern = 0;
                    } else {
                        r_symbolnum = symbol_offsets[reloc.symbol.0].index as u32;
                        r_extern = 1;
                    }
                    let r_length = match reloc.size {
                        8 => 0,
                        16 => 1,
                        32 => 2,
                        64 => 3,
                        _ => return Err(format!("unimplemented reloc size {:?}", reloc)),
                    };
                    let (r_pcrel, r_type) = match self.architecture {
                        Architecture::I386 => match reloc.kind {
                            RelocationKind::Absolute => (0, mach::GENERIC_RELOC_VANILLA),
                            _ => return Err(format!("unimplemented relocation {:?}", reloc)),
                        },
                        Architecture::X86_64 => match (reloc.kind, reloc.encoding, reloc.addend) {
                            (RelocationKind::Absolute, RelocationEncoding::Generic, 0) => {
                                (0, mach::X86_64_RELOC_UNSIGNED)
                            }
                            (RelocationKind::Relative, RelocationEncoding::Generic, -4) => {
                                (1, mach::X86_64_RELOC_SIGNED)
                            }
                            (RelocationKind::Relative, RelocationEncoding::X86RipRelative, -4) => {
                                (1, mach::X86_64_RELOC_SIGNED)
                            }
                            (RelocationKind::Relative, RelocationEncoding::X86Branch, -4) => {
                                (1, mach::X86_64_RELOC_BRANCH)
                            }
                            (RelocationKind::PltRelative, RelocationEncoding::X86Branch, -4) => {
                                (1, mach::X86_64_RELOC_BRANCH)
                            }
                            (RelocationKind::GotRelative, RelocationEncoding::Generic, -4) => {
                                (1, mach::X86_64_RELOC_GOT)
                            }
                            (
                                RelocationKind::GotRelative,
                                RelocationEncoding::X86RipRelativeMovq,
                                -4,
                            ) => (1, mach::X86_64_RELOC_GOT_LOAD),
                            _ => return Err(format!("unimplemented relocation {:?}", reloc)),
                        },
                        _ => {
                            return Err(format!(
                                "unimplemented architecture {:?}",
                                self.architecture
                            ))
                        }
                    };
                    let r_info = r_symbolnum
                        | r_pcrel << 24
                        | r_length << 25
                        | r_extern << 27
                        | u32::from(r_type) << 28;
                    buffer
                        .iowrite_with(
                            mach::RelocationInfo {
                                r_address: reloc.offset as i32,
                                r_info,
                            },
                            ctx.le,
                        )
                        .unwrap();
                }
            }
        }

        Ok(buffer)
    }
}
