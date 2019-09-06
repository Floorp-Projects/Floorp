use crc32fast;
use scroll::ctx::SizeWith;
use scroll::IOwrite;
use std::iter;
use std::string::String;

use crate::alloc::vec::Vec;
use crate::write::string::*;
use crate::write::util::*;
use crate::write::*;

mod coff {
    pub use goblin::pe::characteristic::*;
    pub use goblin::pe::header::*;
    pub use goblin::pe::relocation::*;
    pub use goblin::pe::section_table::*;
    pub use goblin::pe::symbol::*;
}

#[derive(Default, Clone, Copy)]
struct SectionOffsets {
    offset: usize,
    str_id: Option<StringId>,
    reloc_offset: usize,
}

#[derive(Default, Clone, Copy)]
struct SymbolOffsets {
    index: usize,
    str_id: Option<StringId>,
    aux_count: u8,
}

impl Object {
    pub(crate) fn coff_section_info(
        &self,
        section: StandardSection,
    ) -> (&'static [u8], &'static [u8], SectionKind) {
        match section {
            StandardSection::Text => (&[], &b".text"[..], SectionKind::Text),
            StandardSection::Data => (&[], &b".data"[..], SectionKind::Data),
            StandardSection::ReadOnlyData
            | StandardSection::ReadOnlyDataWithRel
            | StandardSection::ReadOnlyString => (&[], &b".rdata"[..], SectionKind::ReadOnlyData),
            StandardSection::UninitializedData => {
                (&[], &b".bss"[..], SectionKind::UninitializedData)
            }
        }
    }

    pub(crate) fn coff_subsection_name(&self, section: &[u8], value: &[u8]) -> Vec<u8> {
        let mut name = section.to_vec();
        name.push(b'$');
        name.extend(value);
        name
    }

    pub(crate) fn coff_fixup_relocation(&mut self, mut relocation: &mut Relocation) -> i64 {
        if relocation.kind == RelocationKind::GotRelative {
            // Use a stub symbol for the relocation instead.
            // This isn't really a GOT, but it's a similar purpose.
            // TODO: need to handle DLL imports differently?
            relocation.kind = RelocationKind::Relative;
            relocation.symbol = self.coff_add_stub_symbol(relocation.symbol);
        } else if relocation.kind == RelocationKind::PltRelative {
            // Windows doesn't need a separate relocation type for
            // references to functions in import libraries.
            // For convenience, treat this the same as Relative.
            relocation.kind = RelocationKind::Relative;
        }

        let constant = match self.architecture {
            Architecture::I386 => match relocation.kind {
                RelocationKind::Relative => {
                    // IMAGE_REL_I386_REL32
                    relocation.addend + 4
                }
                _ => relocation.addend,
            },
            Architecture::X86_64 => match relocation.kind {
                RelocationKind::Relative => {
                    // IMAGE_REL_AMD64_REL32 through to IMAGE_REL_AMD64_REL32_5
                    if relocation.addend >= -4 && relocation.addend <= -9 {
                        0
                    } else {
                        relocation.addend + 4
                    }
                }
                _ => relocation.addend,
            },
            _ => unimplemented!(),
        };
        relocation.addend -= constant;
        constant
    }

    fn coff_add_stub_symbol(&mut self, symbol_id: SymbolId) -> SymbolId {
        if let Some(stub_id) = self.stub_symbols.get(&symbol_id) {
            return *stub_id;
        }
        let stub_size = self.architecture.pointer_width().unwrap().bytes();

        let mut name = b".rdata$.refptr.".to_vec();
        name.extend(&self.symbols[symbol_id.0].name);
        let section_id = self.add_section(Vec::new(), name, SectionKind::ReadOnlyData);
        let section = self.section_mut(section_id);
        section.set_data(vec![0; stub_size as usize], u64::from(stub_size));
        section.relocations = vec![Relocation {
            offset: 0,
            size: stub_size * 8,
            kind: RelocationKind::Absolute,
            encoding: RelocationEncoding::Generic,
            symbol: symbol_id,
            addend: 0,
        }];

        let mut name = b".refptr.".to_vec();
        name.extend(&self.symbol(symbol_id).name);
        let stub_id = self.add_symbol(Symbol {
            name,
            value: 0,
            size: u64::from(stub_size),
            kind: SymbolKind::Data,
            scope: SymbolScope::Compilation,
            weak: false,
            section: Some(section_id),
        });
        self.stub_symbols.insert(symbol_id, stub_id);

        stub_id
    }

    pub(crate) fn coff_write(&self) -> Result<Vec<u8>, String> {
        // Calculate offsets of everything, and build strtab.
        let mut offset = 0;
        let mut strtab = StringTable::default();

        // COFF header.
        let ctx = scroll::LE;
        offset += coff::CoffHeader::size_with(&ctx);

        // Section headers.
        offset += self.sections.len() * coff::SectionTable::size_with(&ctx);

        // Calculate size of section data and add section strings to strtab.
        let mut section_offsets = vec![SectionOffsets::default(); self.sections.len()];
        for (index, section) in self.sections.iter().enumerate() {
            if section.name.len() > 8 {
                section_offsets[index].str_id = Some(strtab.add(&section.name));
            }

            let len = section.data.len();
            if len != 0 {
                // TODO: not sure what alignment is required here, but this seems to match LLVM
                offset = align(offset, 4);
                section_offsets[index].offset = offset;
                offset += len;
            } else {
                section_offsets[index].offset = offset;
            }

            // Calculate size of relocations.
            let count = section.relocations.len();
            if count != 0 {
                section_offsets[index].reloc_offset = offset;
                offset += count * coff::Relocation::size_with(&ctx);
            }
        }

        // Calculate size of symbols and add symbol strings to strtab.
        let mut symbol_offsets = vec![SymbolOffsets::default(); self.symbols.len()];
        let mut symtab_count = 0;
        for (index, symbol) in self.symbols.iter().enumerate() {
            symbol_offsets[index].index = symtab_count;
            symtab_count += 1;
            match symbol.kind {
                SymbolKind::File => {
                    // Name goes in auxilary symbol records.
                    let aux_count =
                        (symbol.name.len() + coff::COFF_SYMBOL_SIZE - 1) / coff::COFF_SYMBOL_SIZE;
                    symbol_offsets[index].aux_count = aux_count as u8;
                    symtab_count += aux_count;
                    // Don't add name to strtab.
                    continue;
                }
                SymbolKind::Section => {
                    symbol_offsets[index].aux_count = 1;
                    symtab_count += 1;
                }
                _ => {}
            }
            if symbol.name.len() > 8 {
                symbol_offsets[index].str_id = Some(strtab.add(&symbol.name));
            }
        }

        // Calculate size of symtab.
        let symtab_offset = offset;
        let symtab_len = symtab_count * coff::COFF_SYMBOL_SIZE;
        offset += symtab_len;

        // Calculate size of strtab.
        let strtab_offset = offset;
        let mut strtab_data = Vec::new();
        // First 4 bytes of strtab are the length.
        strtab.write(4, &mut strtab_data);
        let strtab_len = strtab_data.len() + 4;
        offset += strtab_len;

        // Start writing.
        let mut buffer = Vec::with_capacity(offset);

        // Write file header.
        let header = coff::CoffHeader {
            machine: match self.architecture {
                Architecture::I386 => coff::COFF_MACHINE_X86,
                Architecture::X86_64 => coff::COFF_MACHINE_X86_64,
                _ => {
                    return Err(format!(
                        "unimplemented architecture {:?}",
                        self.architecture
                    ))
                }
            },
            number_of_sections: self.sections.len() as u16,
            time_date_stamp: 0,
            pointer_to_symbol_table: symtab_offset as u32,
            number_of_symbol_table: symtab_count as u32,
            size_of_optional_header: 0,
            characteristics: 0,
        };
        buffer.iowrite_with(header, ctx).unwrap();

        // Write section headers.
        for (index, section) in self.sections.iter().enumerate() {
            // TODO: IMAGE_SCN_LNK_COMDAT
            let characteristics = match section.kind {
                SectionKind::Text => {
                    coff::IMAGE_SCN_CNT_CODE
                        | coff::IMAGE_SCN_MEM_EXECUTE
                        | coff::IMAGE_SCN_MEM_READ
                }
                SectionKind::Data => {
                    coff::IMAGE_SCN_CNT_INITIALIZED_DATA
                        | coff::IMAGE_SCN_MEM_READ
                        | coff::IMAGE_SCN_MEM_WRITE
                }
                SectionKind::UninitializedData => {
                    coff::IMAGE_SCN_CNT_UNINITIALIZED_DATA
                        | coff::IMAGE_SCN_MEM_READ
                        | coff::IMAGE_SCN_MEM_WRITE
                }
                SectionKind::ReadOnlyData | SectionKind::ReadOnlyString => {
                    coff::IMAGE_SCN_CNT_INITIALIZED_DATA | coff::IMAGE_SCN_MEM_READ
                }
                SectionKind::Debug | SectionKind::Other | SectionKind::OtherString => {
                    coff::IMAGE_SCN_CNT_INITIALIZED_DATA
                        | coff::IMAGE_SCN_MEM_READ
                        | coff::IMAGE_SCN_MEM_DISCARDABLE
                }
                SectionKind::Linker => coff::IMAGE_SCN_LNK_INFO | coff::IMAGE_SCN_LNK_REMOVE,
                SectionKind::Tls
                | SectionKind::UninitializedTls
                | SectionKind::TlsVariables
                | SectionKind::Unknown
                | SectionKind::Metadata => {
                    return Err(format!("unimplemented section {:?}", section.kind))
                }
            };
            let align = match section.align {
                1 => coff::IMAGE_SCN_ALIGN_1BYTES,
                2 => coff::IMAGE_SCN_ALIGN_2BYTES,
                4 => coff::IMAGE_SCN_ALIGN_4BYTES,
                8 => coff::IMAGE_SCN_ALIGN_8BYTES,
                16 => coff::IMAGE_SCN_ALIGN_16BYTES,
                32 => coff::IMAGE_SCN_ALIGN_32BYTES,
                64 => coff::IMAGE_SCN_ALIGN_64BYTES,
                128 => coff::IMAGE_SCN_ALIGN_128BYTES,
                256 => coff::IMAGE_SCN_ALIGN_256BYTES,
                512 => coff::IMAGE_SCN_ALIGN_512BYTES,
                1024 => coff::IMAGE_SCN_ALIGN_1024BYTES,
                2048 => coff::IMAGE_SCN_ALIGN_2048BYTES,
                4096 => coff::IMAGE_SCN_ALIGN_4096BYTES,
                8192 => coff::IMAGE_SCN_ALIGN_8192BYTES,
                _ => return Err(format!("unimplemented section align {}", section.align)),
            };
            let mut coff_section = coff::SectionTable {
                name: [0; 8],
                real_name: None,
                virtual_size: if section.data.is_empty() {
                    section.size as u32
                } else {
                    0
                },
                virtual_address: 0,
                size_of_raw_data: section.data.len() as u32,
                pointer_to_raw_data: if section.data.is_empty() {
                    0
                } else {
                    section_offsets[index].offset as u32
                },
                pointer_to_relocations: section_offsets[index].reloc_offset as u32,
                pointer_to_linenumbers: 0,
                number_of_relocations: section.relocations.len() as u16,
                number_of_linenumbers: 0,
                characteristics: characteristics | align,
            };
            if section.name.len() <= 8 {
                coff_section.name[..section.name.len()].copy_from_slice(&section.name);
            } else {
                let str_offset = strtab.get_offset(section_offsets[index].str_id.unwrap());
                coff_section.set_name_offset(str_offset).unwrap();
            }
            buffer.iowrite_with(coff_section, ctx).unwrap();
        }

        // Write section data and relocations.
        for (index, section) in self.sections.iter().enumerate() {
            let len = section.data.len();
            if len != 0 {
                write_align(&mut buffer, 4);
                debug_assert_eq!(section_offsets[index].offset, buffer.len());
                buffer.extend(&section.data);
            }

            if !section.relocations.is_empty() {
                debug_assert_eq!(section_offsets[index].reloc_offset, buffer.len());
                for reloc in &section.relocations {
                    //assert!(reloc.implicit_addend);
                    let typ = match self.architecture {
                        Architecture::I386 => match (reloc.kind, reloc.size, reloc.addend) {
                            (RelocationKind::Absolute, 16, 0) => coff::IMAGE_REL_I386_DIR16,
                            (RelocationKind::Relative, 16, 0) => coff::IMAGE_REL_I386_REL16,
                            (RelocationKind::Absolute, 32, 0) => coff::IMAGE_REL_I386_DIR32,
                            (RelocationKind::ImageOffset, 32, 0) => coff::IMAGE_REL_I386_DIR32NB,
                            (RelocationKind::SectionIndex, 16, 0) => coff::IMAGE_REL_I386_SECTION,
                            (RelocationKind::SectionOffset, 32, 0) => coff::IMAGE_REL_I386_SECREL,
                            (RelocationKind::SectionOffset, 7, 0) => coff::IMAGE_REL_I386_SECREL7,
                            (RelocationKind::Relative, 32, -4) => coff::IMAGE_REL_I386_REL32,
                            (RelocationKind::Other(x), _, _) => x as u16,
                            _ => return Err(format!("unimplemented relocation {:?}", reloc)),
                        },
                        Architecture::X86_64 => match (reloc.kind, reloc.size, reloc.addend) {
                            (RelocationKind::Absolute, 64, 0) => coff::IMAGE_REL_AMD64_ADDR64,
                            (RelocationKind::Absolute, 32, 0) => coff::IMAGE_REL_AMD64_ADDR32,
                            (RelocationKind::ImageOffset, 32, 0) => coff::IMAGE_REL_AMD64_ADDR32NB,
                            (RelocationKind::Relative, 32, -4) => coff::IMAGE_REL_AMD64_REL32,
                            (RelocationKind::Relative, 32, -5) => coff::IMAGE_REL_AMD64_REL32_1,
                            (RelocationKind::Relative, 32, -6) => coff::IMAGE_REL_AMD64_REL32_2,
                            (RelocationKind::Relative, 32, -7) => coff::IMAGE_REL_AMD64_REL32_3,
                            (RelocationKind::Relative, 32, -8) => coff::IMAGE_REL_AMD64_REL32_4,
                            (RelocationKind::Relative, 32, -9) => coff::IMAGE_REL_AMD64_REL32_5,
                            (RelocationKind::SectionOffset, 32, 0) => coff::IMAGE_REL_AMD64_SECREL,
                            (RelocationKind::SectionOffset, 7, 0) => coff::IMAGE_REL_AMD64_SECREL7,
                            (RelocationKind::Other(x), _, _) => x as u16,
                            _ => return Err(format!("unimplemented relocation {:?}", reloc)),
                        },
                        _ => {
                            return Err(format!(
                                "unimplemented architecture {:?}",
                                self.architecture
                            ))
                        }
                    };
                    buffer
                        .iowrite_with(
                            coff::Relocation {
                                virtual_address: reloc.offset as u32,
                                symbol_table_index: symbol_offsets[reloc.symbol.0].index as u32,
                                typ,
                            },
                            ctx,
                        )
                        .unwrap();
                }
            }
        }

        // Write symbols.
        debug_assert_eq!(symtab_offset, buffer.len());
        for (index, symbol) in self.symbols.iter().enumerate() {
            let mut name = &symbol.name[..];
            let mut section_number = symbol.section.map(|x| x.0 + 1).unwrap_or(0) as i16;
            let typ = if symbol.kind == SymbolKind::Text {
                coff::IMAGE_SYM_DTYPE_FUNCTION << coff::IMAGE_SYM_DTYPE_SHIFT
            } else {
                coff::IMAGE_SYM_TYPE_NULL
            };
            let storage_class = match symbol.kind {
                SymbolKind::File => {
                    // Name goes in auxilary symbol records.
                    name = b".file";
                    section_number = coff::IMAGE_SYM_DEBUG;
                    coff::IMAGE_SYM_CLASS_FILE
                }
                SymbolKind::Section => coff::IMAGE_SYM_CLASS_STATIC,
                SymbolKind::Label => coff::IMAGE_SYM_CLASS_LABEL,
                SymbolKind::Text | SymbolKind::Data => {
                    match symbol.scope {
                        _ if symbol.is_undefined() => coff::IMAGE_SYM_CLASS_EXTERNAL,
                        // TODO: does this need aux symbol records too?
                        _ if symbol.weak => coff::IMAGE_SYM_CLASS_WEAK_EXTERNAL,
                        SymbolScope::Unknown => {
                            return Err(format!("unimplemented symbol scope {:?}", symbol))
                        }
                        SymbolScope::Compilation => coff::IMAGE_SYM_CLASS_STATIC,
                        SymbolScope::Linkage | SymbolScope::Dynamic => {
                            coff::IMAGE_SYM_CLASS_EXTERNAL
                        }
                    }
                }
                _ => return Err(format!("unimplemented symbol {:?}", symbol.kind)),
            };
            let number_of_aux_symbols = symbol_offsets[index].aux_count;
            let mut coff_symbol = coff::Symbol {
                name: [0; 8],
                value: symbol.value as u32,
                section_number,
                typ,
                storage_class,
                number_of_aux_symbols,
            };
            if name.len() <= 8 {
                coff_symbol.name[..name.len()].copy_from_slice(name);
            } else {
                let str_offset = strtab.get_offset(symbol_offsets[index].str_id.unwrap());
                coff_symbol.set_name_offset(str_offset as u32);
            }
            buffer.iowrite_with(coff_symbol, ctx).unwrap();

            match symbol.kind {
                SymbolKind::File => {
                    let aux_len = number_of_aux_symbols as usize * coff::COFF_SYMBOL_SIZE;
                    debug_assert!(aux_len >= symbol.name.len());
                    buffer.extend(&symbol.name);
                    buffer.extend(iter::repeat(0).take(aux_len - symbol.name.len()));
                }
                SymbolKind::Section => {
                    debug_assert_eq!(number_of_aux_symbols, 1);
                    let section = &self.sections[symbol.section.unwrap().0];
                    buffer
                        .iowrite_with(
                            coff::AuxSectionDefinition {
                                length: section.data.len() as u32,
                                number_of_relocations: section.relocations.len() as u16,
                                number_of_line_numbers: 0,
                                checksum: checksum(&section.data),
                                number: section_number as u16,
                                // TODO: COMDAT
                                selection: 0,
                                unused: [0; 3],
                            },
                            ctx,
                        )
                        .unwrap();
                }
                _ => {
                    debug_assert_eq!(number_of_aux_symbols, 0);
                }
            }
        }

        // Write strtab section.
        debug_assert_eq!(strtab_offset, buffer.len());
        buffer.iowrite_with(strtab_len as u32, ctx).unwrap();
        buffer.extend(&strtab_data);

        Ok(buffer)
    }
}

// JamCRC
fn checksum(data: &[u8]) -> u32 {
    let mut hasher = crc32fast::Hasher::new_with_initial(0xffff_ffff);
    hasher.update(data);
    !hasher.finalize()
}
