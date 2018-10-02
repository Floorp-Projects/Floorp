
macro_rules! elf_dyn {
    ($size:ty) => {
        #[repr(C)]
        #[derive(Copy, Clone, PartialEq, Default)]
        #[cfg_attr(feature = "alloc", derive(Pread, Pwrite, SizeWith))]
        /// An entry in the dynamic array
        pub struct Dyn {
            /// Dynamic entry type
            pub d_tag: $size,
            /// Integer value
            pub d_val: $size,
        }

        use plain;
        unsafe impl plain::Plain for Dyn {}
    }
}

// TODO: figure out what's the best, most friendly + safe API choice here - u32s or u64s
// remember that DT_TAG is "pointer sized"/used as address sometimes Original rationale: I
// decided to use u64 instead of u32 due to pattern matching use case seems safer to cast the
// elf32's d_tag from u32 -> u64 at runtime instead of casting the elf64's d_tag from u64 ->
// u32 at runtime

/// Marks end of dynamic section
pub const DT_NULL: u64 = 0;
/// Name of needed library
pub const DT_NEEDED: u64 = 1;
/// Size in bytes of PLT relocs
pub const DT_PLTRELSZ: u64 = 2;
/// Processor defined value
pub const DT_PLTGOT: u64 = 3;
/// Address of symbol hash table
pub const DT_HASH: u64 = 4;
/// Address of string table
pub const DT_STRTAB: u64 = 5;
/// Address of symbol table
pub const DT_SYMTAB: u64 = 6;
/// Address of Rela relocs
pub const DT_RELA: u64 = 7;
/// Total size of Rela relocs
pub const DT_RELASZ: u64 = 8;
/// Size of one Rela reloc
pub const DT_RELAENT: u64 = 9;
/// Size of string table
pub const DT_STRSZ: u64 = 10;
/// Size of one symbol table entry
pub const DT_SYMENT: u64 = 11;
/// Address of init function
pub const DT_INIT: u64 = 12;
/// Address of termination function
pub const DT_FINI: u64 = 13;
/// Name of shared object
pub const DT_SONAME: u64 = 14;
/// Library search path (deprecated)
pub const DT_RPATH: u64 = 15;
/// Start symbol search here
pub const DT_SYMBOLIC: u64 = 16;
/// Address of Rel relocs
pub const DT_REL: u64 = 17;
/// Total size of Rel relocs
pub const DT_RELSZ: u64 = 18;
/// Size of one Rel reloc
pub const DT_RELENT: u64 = 19;
/// Type of reloc in PLT
pub const DT_PLTREL: u64 = 20;
/// For debugging; unspecified
pub const DT_DEBUG: u64 = 21;
/// Reloc might modify .text
pub const DT_TEXTREL: u64 = 22;
/// Address of PLT relocs
pub const DT_JMPREL: u64 = 23;
/// Process relocations of object
pub const DT_BIND_NOW: u64 = 24;
/// Array with addresses of init fct
pub const DT_INIT_ARRAY: u64 = 25;
/// Array with addresses of fini fct
pub const DT_FINI_ARRAY: u64 = 26;
/// Size in bytes of DT_INIT_ARRAY
pub const DT_INIT_ARRAYSZ: u64 = 27;
/// Size in bytes of DT_FINI_ARRAY
pub const DT_FINI_ARRAYSZ: u64 = 28;
/// Library search path
pub const DT_RUNPATH: u64 = 29;
/// Flags for the object being loaded
pub const DT_FLAGS: u64 = 30;
/// Start of encoded range
pub const DT_ENCODING: u64 = 32;
/// Array with addresses of preinit fct
pub const DT_PREINIT_ARRAY: u64 = 32;
/// size in bytes of DT_PREINIT_ARRAY
pub const DT_PREINIT_ARRAYSZ: u64 = 33;
/// Number used
pub const DT_NUM: u64 = 34;
/// Start of OS-specific
pub const DT_LOOS: u64 = 0x6000000d;
/// End of OS-specific
pub const DT_HIOS: u64 = 0x6ffff000;
/// Start of processor-specific
pub const DT_LOPROC: u64 = 0x70000000;
/// End of processor-specific
pub const DT_HIPROC: u64 = 0x7fffffff;
// Most used by any processor
// pub const DT_PROCNUM: u64 = DT_MIPS_NUM;

/// DT_* entries which fall between DT_ADDRRNGHI & DT_ADDRRNGLO use the
/// Dyn.d_un.d_ptr field of the Elf*_Dyn structure.
///
/// If any adjustment is made to the ELF object after it has been
/// built these entries will need to be adjusted.
pub const DT_ADDRRNGLO: u64 = 0x6ffffe00;
/// GNU-style hash table
pub const DT_GNU_HASH: u64 = 0x6ffffef5;
///
pub const DT_TLSDESC_PLT: u64 = 0x6ffffef6;
///
pub const DT_TLSDESC_GOT: u64 = 0x6ffffef7;
/// Start of conflict section
pub const DT_GNU_CONFLICT: u64 = 0x6ffffef8;
/// Library list
pub const DT_GNU_LIBLIST: u64 = 0x6ffffef9;
/// Configuration information
pub const DT_CONFIG: u64 = 0x6ffffefa;
/// Dependency auditing
pub const DT_DEPAUDIT: u64 = 0x6ffffefb;
/// Object auditing
pub const DT_AUDIT: u64 = 0x6ffffefc;
/// PLT padding
pub const DT_PLTPAD: u64 = 0x6ffffefd;
/// Move table
pub const DT_MOVETAB: u64 = 0x6ffffefe;
/// Syminfo table
pub const DT_SYMINFO: u64 = 0x6ffffeff;
///
pub const DT_ADDRRNGHI: u64 = 0x6ffffeff;

//DT_ADDRTAGIDX(tag)	(DT_ADDRRNGHI - (tag))	/* Reverse order! */
pub const DT_ADDRNUM: u64 = 11;

/// The versioning entry types. The next are defined as part of the GNU extension
pub const DT_VERSYM: u64 = 0x6ffffff0;
pub const DT_RELACOUNT: u64 = 0x6ffffff9;
pub const DT_RELCOUNT: u64 = 0x6ffffffa;
/// State flags, see DF_1_* below
pub const DT_FLAGS_1: u64 = 0x6ffffffb;
/// Address of version definition table
pub const DT_VERDEF: u64 = 0x6ffffffc;
/// Number of version definitions
pub const DT_VERDEFNUM: u64 = 0x6ffffffd;
/// Address of table with needed versions
pub const DT_VERNEED: u64 = 0x6ffffffe;
/// Number of needed versions
pub const DT_VERNEEDNUM: u64 = 0x6fffffff;

/// Converts a tag to its string representation.
#[inline]
pub fn tag_to_str(tag: u64) -> &'static str {
    match tag {
        DT_NULL => "DT_NULL",
        DT_NEEDED => "DT_NEEDED",
        DT_PLTRELSZ => "DT_PLTRELSZ",
        DT_PLTGOT => "DT_PLTGOT",
        DT_HASH => "DT_HASH",
        DT_STRTAB => "DT_STRTAB",
        DT_SYMTAB => "DT_SYMTAB",
        DT_RELA => "DT_RELA",
        DT_RELASZ => "DT_RELASZ",
        DT_RELAENT => "DT_RELAENT",
        DT_STRSZ => "DT_STRSZ",
        DT_SYMENT => "DT_SYMENT",
        DT_INIT => "DT_INIT",
        DT_FINI => "DT_FINI",
        DT_SONAME => "DT_SONAME",
        DT_RPATH => "DT_RPATH",
        DT_SYMBOLIC => "DT_SYMBOLIC",
        DT_REL => "DT_REL",
        DT_RELSZ => "DT_RELSZ",
        DT_RELENT => "DT_RELENT",
        DT_PLTREL => "DT_PLTREL",
        DT_DEBUG => "DT_DEBUG",
        DT_TEXTREL => "DT_TEXTREL",
        DT_JMPREL => "DT_JMPREL",
        DT_BIND_NOW => "DT_BIND_NOW",
        DT_INIT_ARRAY => "DT_INIT_ARRAY",
        DT_FINI_ARRAY => "DT_FINI_ARRAY",
        DT_INIT_ARRAYSZ => "DT_INIT_ARRAYSZ",
        DT_FINI_ARRAYSZ => "DT_FINI_ARRAYSZ",
        DT_RUNPATH => "DT_RUNPATH",
        DT_FLAGS => "DT_FLAGS",
        DT_PREINIT_ARRAY => "DT_PREINIT_ARRAY",
        DT_PREINIT_ARRAYSZ => "DT_PREINIT_ARRAYSZ",
        DT_NUM => "DT_NUM",
        DT_LOOS => "DT_LOOS",
        DT_HIOS => "DT_HIOS",
        DT_LOPROC => "DT_LOPROC",
        DT_HIPROC => "DT_HIPROC",
        DT_VERSYM => "DT_VERSYM",
        DT_RELACOUNT => "DT_RELACOUNT",
        DT_RELCOUNT => "DT_RELCOUNT",
        DT_GNU_HASH => "DT_GNU_HASH",
        DT_VERDEF => "DT_VERDEF",
        DT_VERDEFNUM => "DT_VERDEFNUM",
        DT_VERNEED => "DT_VERNEED",
        DT_VERNEEDNUM => "DT_VERNEEDNUM",
        DT_FLAGS_1 => "DT_FLAGS_1",
        _ => "UNKNOWN_TAG",
    }
}

// Values of `d_un.d_val` in the DT_FLAGS entry
/// Object may use DF_ORIGIN.
pub const DF_ORIGIN: u64 = 0x00000001;
/// Symbol resolutions starts here.
pub const DF_SYMBOLIC: u64 = 0x00000002;
/// Object contains text relocations.
pub const DF_TEXTREL: u64 = 0x00000004;
/// No lazy binding for this object.
pub const DF_BIND_NOW: u64 = 0x00000008;
/// Module uses the static TLS model.
pub const DF_STATIC_TLS: u64 = 0x00000010;

/// === State flags ===
/// selectable in the `d_un.d_val` element of the DT_FLAGS_1 entry in the dynamic section.
///
/// Set RTLD_NOW for this object.
pub const DF_1_NOW: u64 = 0x00000001;
/// Set RTLD_GLOBAL for this object.
pub const DF_1_GLOBAL: u64 = 0x00000002;
/// Set RTLD_GROUP for this object.
pub const DF_1_GROUP: u64 = 0x00000004;
/// Set RTLD_NODELETE for this object.
pub const DF_1_NODELETE: u64 = 0x00000008;
/// Trigger filtee loading at runtime.
pub const DF_1_LOADFLTR: u64 = 0x00000010;
/// Set RTLD_INITFIRST for this object.
pub const DF_1_INITFIRST: u64 = 0x00000020;
/// Set RTLD_NOOPEN for this object.
pub const DF_1_NOOPEN: u64 = 0x00000040;
/// $ORIGIN must be handled.
pub const DF_1_ORIGIN: u64 = 0x00000080;
/// Direct binding enabled.
pub const DF_1_DIRECT: u64 = 0x00000100;
pub const DF_1_TRANS: u64 = 0x00000200;
/// Object is used to interpose.
pub const DF_1_INTERPOSE: u64 = 0x00000400;
/// Ignore default lib search path.
pub const DF_1_NODEFLIB: u64 = 0x00000800;
/// Object can't be dldump'ed.
pub const DF_1_NODUMP: u64 = 0x00001000;
/// Configuration alternative created.
pub const DF_1_CONFALT: u64 = 0x00002000;
/// Filtee terminates filters search.
pub const DF_1_ENDFILTEE: u64 = 0x00004000;
/// Disp reloc applied at build time.
pub const DF_1_DISPRELDNE: u64 = 0x00008000;
/// Disp reloc applied at run-time.
pub const DF_1_DISPRELPND: u64 = 0x00010000;
/// Object has no-direct binding.
pub const DF_1_NODIRECT: u64 = 0x00020000;
pub const DF_1_IGNMULDEF: u64 = 0x00040000;
pub const DF_1_NOKSYMS: u64 = 0x00080000;
pub const DF_1_NOHDR: u64 = 0x00100000;
/// Object is modified after built.
pub const DF_1_EDITED: u64 = 0x00200000;
pub const DF_1_NORELOC: u64 = 0x00400000;
/// Object has individual interposers.
pub const DF_1_SYMINTPOSE: u64 = 0x00800000;
/// Global auditing required.
pub const DF_1_GLOBAUDIT: u64 = 0x01000000;
/// Singleton dyn are used.
pub const DF_1_SINGLETON: u64 = 0x02000000;

if_alloc! {
    use core::fmt;
    use scroll::ctx;
    use core::result;
    use container::{Ctx, Container};
    use strtab::Strtab;
    use self::dyn32::{DynamicInfo};
    use alloc::vec::Vec;

    #[derive(Default, PartialEq, Clone)]
    pub struct Dyn {
        pub d_tag: u64,
        pub d_val: u64,
    }

    impl Dyn {
        #[inline]
        pub fn size(container: Container) -> usize {
            use scroll::ctx::SizeWith;
            Self::size_with(&Ctx::from(container))
        }
    }

    impl fmt::Debug for Dyn {
        fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
            write!(f,
                   "d_tag: {} d_val: 0x{:x}",
                   tag_to_str(self.d_tag as u64),
                   self.d_val)
        }
    }

    impl ctx::SizeWith<Ctx> for Dyn {
        type Units = usize;
        fn size_with(&Ctx { container, .. }: &Ctx) -> usize {
            match container {
                Container::Little => {
                    dyn32::SIZEOF_DYN
                },
                Container::Big => {
                    dyn64::SIZEOF_DYN
                },
            }
        }
    }

    impl<'a> ctx::TryFromCtx<'a, Ctx> for Dyn {
        type Error = ::error::Error;
        type Size = usize;
        fn try_from_ctx(bytes: &'a [u8], Ctx { container, le}: Ctx) -> result::Result<(Self, Self::Size), Self::Error> {
            use scroll::Pread;
            let dyn = match container {
                Container::Little => {
                    (bytes.pread_with::<dyn32::Dyn>(0, le)?.into(), dyn32::SIZEOF_DYN)
                },
                Container::Big => {
                    (bytes.pread_with::<dyn64::Dyn>(0, le)?.into(), dyn64::SIZEOF_DYN)
                }
            };
            Ok(dyn)
        }
    }

    impl ctx::TryIntoCtx<Ctx> for Dyn {
        type Error = ::error::Error;
        type Size = usize;
        fn try_into_ctx(self, bytes: &mut [u8], Ctx { container, le}: Ctx) -> result::Result<Self::Size, Self::Error> {
            use scroll::Pwrite;
            match container {
                Container::Little => {
                    let dyn: dyn32::Dyn = self.into();
                    Ok(bytes.pwrite_with(dyn, 0, le)?)
                },
                Container::Big => {
                    let dyn: dyn64::Dyn = self.into();
                    Ok(bytes.pwrite_with(dyn, 0, le)?)
                }
            }
        }
    }

    #[derive(Debug)]
    pub struct Dynamic {
        pub dyns: Vec<Dyn>,
        pub info: DynamicInfo,
        count: usize,
    }

    impl Dynamic {
        #[cfg(feature = "endian_fd")]
        /// Returns a vector of dynamic entries from the underlying byte `bytes`, with `endianness`, using the provided `phdrs`
        pub fn parse(bytes: &[u8], phdrs: &[::elf::program_header::ProgramHeader], bias: usize, ctx: Ctx) -> ::error::Result<Option<Self>> {
            use scroll::ctx::SizeWith;
            use scroll::Pread;
            use elf::program_header;
            for phdr in phdrs {
                if phdr.p_type == program_header::PT_DYNAMIC {
                    let filesz = phdr.p_filesz as usize;
                    let size = Dyn::size_with(&ctx);
                    let count = filesz / size;
                    let mut dyns = Vec::with_capacity(count);
                    let mut offset = phdr.p_offset as usize;
                    for _ in 0..count {
                        let dyn = bytes.gread_with::<Dyn>(&mut offset, ctx)?;
                        let tag = dyn.d_tag;
                        dyns.push(dyn);
                        if tag == DT_NULL { break }
                    }
                    let mut info = DynamicInfo::default();
                    for dyn in &dyns {
                        let dyn: dyn32::Dyn = dyn.clone().into();
                        info.update(bias, &dyn);
                    }
                    let count = dyns.len();
                    return Ok(Some(Dynamic { dyns: dyns, info: info, count: count }));
                }
            }
            Ok(None)
        }

        pub fn get_libraries<'a>(&self, strtab: &Strtab<'a>) -> Vec<&'a str> {
            let count = self.info.needed_count;
            let mut needed = Vec::with_capacity(count);
            for dyn in &self.dyns {
                if dyn.d_tag as u64 == DT_NEEDED {
                    match strtab.get(dyn.d_val as usize) {
                        Some(Ok(lib)) => needed.push(lib),
                        // FIXME: warn! here
                        _ => (),
                    }
                }
            }
            needed
        }
    }
}

macro_rules! elf_dyn_std_impl {
    ($size:ident, $phdr:ty) => {

        #[cfg(test)]
        mod test {
            use super::*;
            #[test]
            fn size_of() {
                assert_eq!(::std::mem::size_of::<Dyn>(), SIZEOF_DYN);
            }
        }

        if_alloc! {
            use core::fmt;
            use core::slice;
            use alloc::vec::Vec;

            use elf::program_header::{PT_DYNAMIC};
            use strtab::Strtab;

            use elf::dyn::Dyn as ElfDyn;

            if_std! {
                use std::fs::File;
                use std::io::{Read, Seek};
                use std::io::SeekFrom::Start;
                use error::Result;
            }

            impl From<ElfDyn> for Dyn {
                fn from(dyn: ElfDyn) -> Self {
                    Dyn {
                        d_tag: dyn.d_tag as $size,
                        d_val: dyn.d_val as $size,
                    }
                }
            }
            impl From<Dyn> for ElfDyn {
                fn from(dyn: Dyn) -> Self {
                    ElfDyn {
                        d_tag: dyn.d_tag as u64,
                        d_val: dyn.d_val as u64,
                    }
                }
            }

            impl fmt::Debug for Dyn {
                fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                    write!(f,
                           "d_tag: {} d_val: 0x{:x}",
                           tag_to_str(self.d_tag as u64),
                           self.d_val)
                }
            }

            impl fmt::Debug for DynamicInfo {
                fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                    let gnu_hash = if let Some(addr) = self.gnu_hash { addr } else { 0 };
                    let hash = if let Some(addr) = self.hash { addr } else { 0 };
                    let pltgot = if let Some(addr) = self.pltgot { addr } else { 0 };
                    write!(f, "rela: 0x{:x} relasz: {} relaent: {} relacount: {} gnu_hash: 0x{:x} hash: 0x{:x} strtab: 0x{:x} strsz: {} symtab: 0x{:x} syment: {} pltgot: 0x{:x} pltrelsz: {} pltrel: {} jmprel: 0x{:x} verneed: 0x{:x} verneednum: {} versym: 0x{:x} init: 0x{:x} fini: 0x{:x} needed_count: {}",
                           self.rela,
                           self.relasz,
                           self.relaent,
                           self.relacount,
                           gnu_hash,
                           hash,
                           self.strtab,
                           self.strsz,
                           self.symtab,
                           self.syment,
                           pltgot,
                           self.pltrelsz,
                           self.pltrel,
                           self.jmprel,
                           self.verneed,
                           self.verneednum,
                           self.versym,
                           self.init,
                           self.fini,
                           self.needed_count,
                    )
                }
            }

            /// Returns a vector of dynamic entries from the given fd and program headers
            #[cfg(feature = "std")]
            pub fn from_fd(mut fd: &File, phdrs: &[$phdr]) -> Result<Option<Vec<Dyn>>> {
                for phdr in phdrs {
                    if phdr.p_type == PT_DYNAMIC {
                        let filesz = phdr.p_filesz as usize;
                        let dync = filesz / SIZEOF_DYN;
                        let mut dyns = vec![Dyn::default(); dync];
                        try!(fd.seek(Start(phdr.p_offset as u64)));
                        unsafe {
                            try!(fd.read(plain::as_mut_bytes(&mut *dyns)));
                        }
                        dyns.dedup();
                        return Ok(Some(dyns));
                    }
                }
                Ok(None)
            }

            /// Given a bias and a memory address (typically for a _correctly_ mmap'd binary in memory), returns the `_DYNAMIC` array as a slice of that memory
            pub unsafe fn from_raw<'a>(bias: usize, vaddr: usize) -> &'a [Dyn] {
                let dynp = vaddr.wrapping_add(bias) as *const Dyn;
                let mut idx = 0;
                while (*dynp.offset(idx)).d_tag as u64 != DT_NULL {
                    idx += 1;
                }
                slice::from_raw_parts(dynp, idx as usize)
            }

            // TODO: these bare functions have always seemed awkward, but not sure where they should go...
            /// Maybe gets and returns the dynamic array with the same lifetime as the [phdrs], using the provided bias with wrapping addition.
            /// If the bias is wrong, it will either segfault or give you incorrect values, beware
            pub unsafe fn from_phdrs(bias: usize, phdrs: &[$phdr]) -> Option<&[Dyn]> {
                for phdr in phdrs {
                    // FIXME: change to casting to u64 similar to DT_*?
                    if phdr.p_type as u32 == PT_DYNAMIC {
                        return Some(from_raw(bias, phdr.p_vaddr as usize));
                    }
                }
                None
            }

            /// Gets the needed libraries from the `_DYNAMIC` array, with the str slices lifetime tied to the dynamic array/strtab's lifetime(s)
            pub unsafe fn get_needed<'a>(dyns: &[Dyn], strtab: *const Strtab<'a>, count: usize) -> Vec<&'a str> {
                let mut needed = Vec::with_capacity(count);
                for dyn in dyns {
                    if dyn.d_tag as u64 == DT_NEEDED {
                        let lib = &(*strtab)[dyn.d_val as usize];
                        needed.push(lib);
                    }
                }
                needed
            }
        }

        /// Important dynamic linking info generated via a single pass through the `_DYNAMIC` array
        #[derive(Default)]
        pub struct DynamicInfo {
            pub rela: usize,
            pub relasz: usize,
            pub relaent: $size,
            pub relacount: usize,
            pub rel: usize,
            pub relsz: usize,
            pub relent: $size,
            pub relcount: usize,
            pub gnu_hash: Option<$size>,
            pub hash: Option<$size>,
            pub strtab: usize,
            pub strsz: usize,
            pub symtab: usize,
            pub syment: usize,
            pub pltgot: Option<$size>,
            pub pltrelsz: usize,
            pub pltrel: $size,
            pub jmprel: usize,
            pub verneed: $size,
            pub verneednum: $size,
            pub versym: $size,
            pub init: $size,
            pub fini: $size,
            pub init_array: $size,
            pub init_arraysz: usize,
            pub fini_array: $size,
            pub fini_arraysz: usize,
            pub needed_count: usize,
            pub flags: $size,
            pub flags_1: $size,
            pub soname: usize,
            pub textrel: bool,
        }

        impl DynamicInfo {
            #[inline]
            pub fn update(&mut self, bias: usize, dyn: &Dyn) {
                match dyn.d_tag as u64 {
                    DT_RELA => self.rela = dyn.d_val.wrapping_add(bias as _) as usize, // .rela.dyn
                    DT_RELASZ => self.relasz = dyn.d_val as usize,
                    DT_RELAENT => self.relaent = dyn.d_val as _,
                    DT_RELACOUNT => self.relacount = dyn.d_val as usize,
                    DT_REL => self.rel = dyn.d_val.wrapping_add(bias as _) as usize, // .rel.dyn
                    DT_RELSZ => self.relsz = dyn.d_val as usize,
                    DT_RELENT => self.relent = dyn.d_val as _,
                    DT_RELCOUNT => self.relcount = dyn.d_val as usize,
                    DT_GNU_HASH => self.gnu_hash = Some(dyn.d_val.wrapping_add(bias as _)),
                    DT_HASH => self.hash = Some(dyn.d_val.wrapping_add(bias as _)) as _,
                    DT_STRTAB => self.strtab = dyn.d_val.wrapping_add(bias as _) as usize,
                    DT_STRSZ => self.strsz = dyn.d_val as usize,
                    DT_SYMTAB => self.symtab = dyn.d_val.wrapping_add(bias as _) as usize,
                    DT_SYMENT => self.syment = dyn.d_val as usize,
                    DT_PLTGOT => self.pltgot = Some(dyn.d_val.wrapping_add(bias as _)) as _,
                    DT_PLTRELSZ => self.pltrelsz = dyn.d_val as usize,
                    DT_PLTREL => self.pltrel = dyn.d_val as _,
                    DT_JMPREL => self.jmprel = dyn.d_val.wrapping_add(bias as _) as usize, // .rela.plt
                    DT_VERNEED => self.verneed = dyn.d_val.wrapping_add(bias as _) as _,
                    DT_VERNEEDNUM => self.verneednum = dyn.d_val as _,
                    DT_VERSYM => self.versym = dyn.d_val.wrapping_add(bias as _) as _,
                    DT_INIT => self.init = dyn.d_val.wrapping_add(bias as _) as _,
                    DT_FINI => self.fini = dyn.d_val.wrapping_add(bias as _) as _,
                    DT_INIT_ARRAY => self.init_array = dyn.d_val.wrapping_add(bias as _) as _,
                    DT_INIT_ARRAYSZ => self.init_arraysz = dyn.d_val as _,
                    DT_FINI_ARRAY => self.fini_array = dyn.d_val.wrapping_add(bias as _) as _,
                    DT_FINI_ARRAYSZ => self.fini_arraysz = dyn.d_val as _,
                    DT_NEEDED => self.needed_count += 1,
                    DT_FLAGS => self.flags = dyn.d_val as _,
                    DT_FLAGS_1 => self.flags_1 = dyn.d_val as _,
                    DT_SONAME => self.soname = dyn.d_val as _,
                    DT_TEXTREL => self.textrel = true,
                    _ => (),
                }
            }
            pub fn new(dynamic: &[Dyn], bias: usize) -> DynamicInfo {
                let mut info = DynamicInfo::default();
                for dyn in dynamic {
                    info.update(bias, &dyn);
                }
                info
            }
        } // end if_std
    };
}


pub mod dyn32 {
    pub use elf::dyn::*;

    elf_dyn!(u32);

    pub const SIZEOF_DYN: usize = 8;

    elf_dyn_std_impl!(u32, ::elf32::program_header::ProgramHeader);

}


pub mod dyn64 {
    pub use elf::dyn::*;

    elf_dyn!(u64);

    pub const SIZEOF_DYN: usize = 16;

    elf_dyn_std_impl!(u64, ::elf64::program_header::ProgramHeader);
}
