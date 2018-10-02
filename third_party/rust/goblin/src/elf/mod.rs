//! The generic ELF module, which gives access to ELF constants and other helper functions, which are independent of ELF bithood.  Also defines an `Elf` struct which implements a unified parser that returns a wrapped `Elf64` or `Elf32` binary.
//!
//! To access the exact 32-bit or 64-bit versions, use [goblin::elf32::Header](header/header32/struct.Header.html)/[goblin::elf64::Header](header/header64/struct.Header.html), etc., for the various 32/64-bit structs.
//!
//! # Example
//!
//! ```rust
//! use std::fs::File;
//!
//! pub fn read (bytes: &[u8]) {
//!   match goblin::elf::Elf::parse(&bytes) {
//!     Ok(binary) => {
//!       let entry = binary.entry;
//!       for ph in binary.program_headers {
//!         if ph.p_type == goblin::elf::program_header::PT_LOAD {
//!           let mut _buf = vec![0u8; ph.p_filesz as usize];
//!           // read responsibly
//!          }
//!       }
//!     },
//!     Err(_) => ()
//!   }
//! }
//! ```
//!
//! This will properly access the underlying 32-bit or 64-bit binary automatically. Note that since
//! 32-bit binaries typically have shorter 32-bit values in some cases (specifically for addresses and pointer
//! values), these values are upcasted to u64/i64s when appropriate.
//!
//! See [goblin::elf::Elf](struct.Elf.html) for more information.
//!
//! You are still free to use the specific 32-bit or 64-bit versions by accessing them through `goblin::elf64`, etc., but you will have to parse and/or construct the various components yourself.
//! In other words, there is no unified 32/64-bit `Elf` struct.
//!
//! # Note
//! To use the automagic ELF datatype union parser, you _must_ enable/opt-in to the  `elf64`, `elf32`, and
//! `endian_fd` features if you disable `default`.

#[macro_use]
mod gnu_hash;

// These are shareable values for the 32/64 bit implementations.
//
// They are publicly re-exported by the pub-using module
pub mod header;
pub mod program_header;
pub mod section_header;
pub mod compression_header;
#[macro_use]
pub mod sym;
pub mod dyn;
#[macro_use]
pub mod reloc;
pub mod note;

macro_rules! if_sylvan {
    ($($i:item)*) => ($(
        #[cfg(all(feature = "elf32", feature = "elf64", feature = "endian_fd"))]
        $i
    )*)
}

if_sylvan! {
    use scroll::{self, ctx, Pread, Endian};
    use strtab::Strtab;
    use error;
    use container::{Container, Ctx};
    use alloc::vec::Vec;

    pub type Header = header::Header;
    pub type ProgramHeader = program_header::ProgramHeader;
    pub type SectionHeader = section_header::SectionHeader;
    pub type Symtab<'a> = sym::Symtab<'a>;
    pub type Sym = sym::Sym;
    pub type Dyn = dyn::Dyn;
    pub type Dynamic = dyn::Dynamic;
    pub type Reloc = reloc::Reloc;

    pub type ProgramHeaders = Vec<ProgramHeader>;
    pub type SectionHeaders = Vec<SectionHeader>;
    pub type ShdrIdx = usize;

    #[derive(Debug)]
    /// An ELF binary. The underlying data structures are read according to the headers byte order and container size (32 or 64).
    pub struct Elf<'a> {
        /// The ELF header, which provides a rudimentary index into the rest of the binary
        pub header: Header,
        /// The program headers; they primarily tell the kernel and the dynamic linker
        /// how to load this binary
        pub program_headers: ProgramHeaders,
        /// The sections headers. These are strippable, never count on them being
        /// here unless you're a static linker!
        pub section_headers: SectionHeaders,
        /// The section header string table
        pub shdr_strtab: Strtab<'a>,
        /// The string table for the dynamically accessible symbols
        pub dynstrtab: Strtab<'a>,
        /// The dynamically accessible symbols, i.e., exports, imports.
        /// This is what the dynamic linker uses to dynamically load and link your binary,
        /// or find imported symbols for binaries which dynamically link against your library
        pub dynsyms: Symtab<'a>,
        /// The debugging symbol table
        pub syms: Symtab<'a>,
        /// The string table for the symbol table
        pub strtab: Strtab<'a>,
        /// Contains dynamic linking information, with the _DYNAMIC array + a preprocessed DynamicInfo for that array
        pub dynamic: Option<Dynamic>,
        /// The dynamic relocation entries (strings, copy-data, etc.) with an addend
        pub dynrelas: Vec<Reloc>,
        /// The dynamic relocation entries without an addend
        pub dynrels: Vec<Reloc>,
        /// The plt relocation entries (procedure linkage table). For 32-bit binaries these are usually Rel (no addend)
        pub pltrelocs: Vec<Reloc>,
        /// Section relocations by section index (only present if this is a relocatable object file)
        pub shdr_relocs: Vec<(ShdrIdx, Vec<Reloc>)>,
        /// The binary's soname, if it has one
        pub soname: Option<&'a str>,
        /// The binary's program interpreter (e.g., dynamic linker), if it has one
        pub interpreter: Option<&'a str>,
        /// A list of this binary's dynamic libraries it uses, if there are any
        pub libraries: Vec<&'a str>,
        pub is_64: bool,
        /// Whether this is a shared object or not
        pub is_lib: bool,
        /// The binaries entry point address, if it has one
        pub entry: u64,
        /// The bias used to overflow virtual memory addresses into physical byte offsets into the binary
        pub bias: u64,
        /// Whether the binary is little endian or not
        pub little_endian: bool,
        ctx: Ctx,
    }

    impl<'a> Elf<'a> {
        /// Try to iterate notes in PT_NOTE program headers; returns `None` if there aren't any note headers in this binary
        pub fn iter_note_headers(&self, data: &'a [u8]) -> Option<note::NoteIterator<'a>> {
            let mut iters = vec![];
            for phdr in &self.program_headers {
                if phdr.p_type == program_header::PT_NOTE {
                    let offset = phdr.p_offset as usize;
                    let alignment = phdr.p_align as usize;

                    iters.push(note::NoteDataIterator {
                        data,
                        offset,
                        size: offset + phdr.p_filesz as usize,
                        ctx: (alignment, self.ctx)
                    });
                }
            }

            if iters.is_empty() {
                None
            } else {
                Some(note::NoteIterator {
                    iters: iters,
                    index: 0,
                })
            }
        }
        /// Try to iterate notes in SHT_NOTE sections; returns `None` if there aren't any note sections in this binary
        ///
        /// If a section_name is given, only the section with the according name is iterated.
        pub fn iter_note_sections(
            &self,
            data: &'a [u8],
            section_name: Option<&str>,
        ) -> Option<note::NoteIterator<'a>> {
            let mut iters = vec![];
            for sect in &self.section_headers {
                if sect.sh_type != section_header::SHT_NOTE {
                    continue;
                }

                if section_name.is_some() && !self.shdr_strtab
                    .get(sect.sh_name)
                    .map_or(false, |r| r.ok() == section_name) {
                    continue;
                }

                let offset = sect.sh_offset as usize;
                let alignment = sect.sh_addralign as usize;
                iters.push(note::NoteDataIterator {
                    data,
                    offset,
                    size: offset + sect.sh_size as usize,
                    ctx: (alignment, self.ctx)
                });
            }

            if iters.is_empty() {
                None
            } else {
                Some(note::NoteIterator {
                    iters: iters,
                    index: 0,
                })
            }
        }
        pub fn is_object_file(&self) -> bool {
            self.header.e_type == header::ET_REL
        }
        /// Parses the contents of the byte stream in `bytes`, and maybe returns a unified binary
        pub fn parse(bytes: &'a [u8]) -> error::Result<Self> {
            let header = bytes.pread::<Header>(0)?;
            let entry = header.e_entry as usize;
            let is_lib = header.e_type == header::ET_DYN;
            let is_lsb = header.e_ident[header::EI_DATA] == header::ELFDATA2LSB;
            let endianness = scroll::Endian::from(is_lsb);
            let class = header.e_ident[header::EI_CLASS];
            if class != header::ELFCLASS64 && class != header::ELFCLASS32 {
                return Err(error::Error::Malformed(format!("Unknown values in ELF ident header: class: {} endianness: {}",
                                                           class,
                                                           header.e_ident[header::EI_DATA])).into());
            }
            let is_64 = class == header::ELFCLASS64;
            let container = if is_64 { Container::Big } else { Container::Little };
            let ctx = Ctx::new(container, endianness);

            let program_headers = ProgramHeader::parse(bytes, header.e_phoff as usize, header.e_phnum as usize, ctx)?;

            let mut bias: usize = 0;
            for ph in &program_headers {
                if ph.p_type == program_header::PT_LOAD {
                    // NB this _only_ works on the first load address, and the GOT values (usually at base + 2000) will be incorrect binary offsets...
                    // this is an overflow hack that allows us to use virtual memory addresses
                    // as though they're in the file by generating a fake load bias which is then
                    // used to overflow the values in the dynamic array, and in a few other places
                    // (see Dyn::DynamicInfo), to generate actual file offsets; you may have to
                    // marinate a bit on why this works. i am unsure whether it works in every
                    // conceivable case. i learned this trick from reading too much dynamic linker
                    // C code (a whole other class of C code) and having to deal with broken older
                    // kernels on VMs. enjoi
                    bias = match container {
                        Container::Little => (::core::u32::MAX - (ph.p_vaddr as u32)).wrapping_add(1) as usize,
                        Container::Big    => (::core::u64::MAX - ph.p_vaddr).wrapping_add(1) as usize,
                    };
                    // we must grab only the first one, otherwise the bias will be incorrect
                    break;
                }
            }

            let mut interpreter = None;
            for ph in &program_headers {
                if ph.p_type == program_header::PT_INTERP && ph.p_filesz != 0 {
                    let count = (ph.p_filesz - 1) as usize;
                    let offset = ph.p_offset as usize;
                    interpreter = Some(bytes.pread_with::<&str>(offset, ::scroll::ctx::StrCtx::Length(count))?);
                }
            }

            let section_headers = SectionHeader::parse(bytes, header.e_shoff as usize, header.e_shnum as usize, ctx)?;

            let get_strtab = |section_headers: &[SectionHeader], section_idx: usize| {
                if section_idx >= section_headers.len() {
                    // FIXME: warn! here
                    Ok(Strtab::default())
                } else {
                    let shdr = &section_headers[section_idx];
                    shdr.check_size(bytes.len())?;
                    Strtab::parse(bytes, shdr.sh_offset as usize, shdr.sh_size as usize, 0x0)
                }
            };

            let strtab_idx = header.e_shstrndx as usize;
            let shdr_strtab = get_strtab(&section_headers, strtab_idx)?;

            let mut syms = Symtab::default();
            let mut strtab = Strtab::default();
            for shdr in &section_headers {
                if shdr.sh_type as u32 == section_header::SHT_SYMTAB {
                    let size = shdr.sh_entsize;
                    let count = if size == 0 { 0 } else { shdr.sh_size / size };
                    syms = Symtab::parse(bytes, shdr.sh_offset as usize, count as usize, ctx)?;
                    strtab = get_strtab(&section_headers, shdr.sh_link as usize)?;
                }
            }

            let mut soname = None;
            let mut libraries = vec![];
            let mut dynsyms = Symtab::default();
            let mut dynrelas = vec![];
            let mut dynrels = vec![];
            let mut pltrelocs = vec![];
            let mut dynstrtab = Strtab::default();
            let dynamic = Dynamic::parse(bytes, &program_headers, bias, ctx)?;
            if let Some(ref dynamic) = dynamic {
                let dyn_info = &dynamic.info;
                dynstrtab = Strtab::parse(bytes,
                                          dyn_info.strtab,
                                          dyn_info.strsz,
                                          0x0)?;

                if dyn_info.soname != 0 {
                    // FIXME: warn! here
                    soname = match dynstrtab.get(dyn_info.soname) { Some(Ok(soname)) => Some(soname), _ => None };
                }
                if dyn_info.needed_count > 0 {
                    libraries = dynamic.get_libraries(&dynstrtab);
                }
                let num_syms = if dyn_info.syment == 0 { 0 } else { if dyn_info.strtab <= dyn_info.symtab { 0 } else { (dyn_info.strtab - dyn_info.symtab) / dyn_info.syment }};
                dynsyms = Symtab::parse(bytes, dyn_info.symtab, num_syms, ctx)?;
                // parse the dynamic relocations
                dynrelas = Reloc::parse(bytes, dyn_info.rela, dyn_info.relasz, true, ctx)?;
                dynrels = Reloc::parse(bytes, dyn_info.rel, dyn_info.relsz, false, ctx)?;
                let is_rela = dyn_info.pltrel as u64 == dyn::DT_RELA;
                pltrelocs = Reloc::parse(bytes, dyn_info.jmprel, dyn_info.pltrelsz, is_rela, ctx)?;
            }

            // iterate through shdrs again iff we're an ET_REL
            let shdr_relocs = {
                let mut relocs = vec![];
                if header.e_type == header::ET_REL {
                    for (idx, section) in section_headers.iter().enumerate() {
                        if section.sh_type == section_header::SHT_REL {
                            section.check_size(bytes.len())?;
                            let sh_relocs = Reloc::parse(bytes, section.sh_offset as usize, section.sh_size as usize, false, ctx)?;
                            relocs.push((idx, sh_relocs));
                        }
                        if section.sh_type == section_header::SHT_RELA {
                            section.check_size(bytes.len())?;
                            let sh_relocs = Reloc::parse(bytes, section.sh_offset as usize, section.sh_size as usize, true, ctx)?;
                            relocs.push((idx, sh_relocs));
                        }
                    }
                }
                relocs
            };
            Ok(Elf {
                header: header,
                program_headers: program_headers,
                section_headers: section_headers,
                shdr_strtab: shdr_strtab,
                dynamic: dynamic,
                dynsyms: dynsyms,
                dynstrtab: dynstrtab,
                syms: syms,
                strtab: strtab,
                dynrelas: dynrelas,
                dynrels: dynrels,
                pltrelocs: pltrelocs,
                shdr_relocs: shdr_relocs,
                soname: soname,
                interpreter: interpreter,
                libraries: libraries,
                is_64: is_64,
                is_lib: is_lib,
                entry: entry as u64,
                bias: bias as u64,
                little_endian: is_lsb,
                ctx,
            })
        }
    }

    impl<'a> ctx::TryFromCtx<'a, (usize, Endian)> for Elf<'a> {
        type Error = ::error::Error;
        type Size = usize;
        fn try_from_ctx(src: &'a [u8], (_, _): (usize, Endian)) -> Result<(Elf<'a>, Self::Size), Self::Error> {
            let elf = Elf::parse(src)?;
            Ok((elf, src.len()))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_crt1_64bit() {
        let crt1: Vec<u8> = include!("../../etc/crt1.rs");
        match Elf::parse(&crt1) {
            Ok (binary) => {
                assert!(binary.is_64);
                assert!(!binary.is_lib);
                assert_eq!(binary.entry, 0);
                assert_eq!(binary.bias, 0);
                assert!(binary.syms.get(1000).is_none());
                assert!(binary.syms.get(5).is_some());
                let syms = binary.syms.to_vec();
                let mut i = 0;
                assert!(binary.section_headers.len() != 0);
                for sym in &syms {
                    if i == 11 {
                        let symtab = binary.strtab;
                        println!("sym: {:?}", &sym);
                        assert_eq!(&symtab[sym.st_name], "_start");
                        break;
                    }
                    i += 1;
                }
                assert!(syms.len() != 0);
             },
            Err (err) => {
                println!("failed: {}", err);
                assert!(false)
            }
        }
    }

    #[test]
    fn parse_crt1_32bit() {
        let crt1: Vec<u8> = include!("../../etc/crt132.rs");
        match Elf::parse(&crt1) {
            Ok (binary) => {
                assert!(!binary.is_64);
                assert!(!binary.is_lib);
                assert_eq!(binary.entry, 0);
                assert_eq!(binary.bias, 0);
                assert!(binary.syms.get(1000).is_none());
                assert!(binary.syms.get(5).is_some());
                let syms = binary.syms.to_vec();
                let mut i = 0;
                assert!(binary.section_headers.len() != 0);
                for sym in &syms {
                    if i == 11 {
                        let symtab = binary.strtab;
                        println!("sym: {:?}", &sym);
                        assert_eq!(&symtab[sym.st_name], "__libc_csu_fini");
                        break;
                    }
                    i += 1;
                }
                assert!(syms.len() != 0);
             },
            Err (err) => {
                println!("failed: {}", err);
                assert!(false)
            }
        }
    }
}
