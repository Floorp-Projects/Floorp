macro_rules! elf_compression_header {
    () => {
        use plain;
        // Declare that this is a plain type.
        unsafe impl plain::Plain for CompressionHeader {}

        impl ::core::fmt::Debug for CompressionHeader {
            fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
                write!(f,
                       "ch_type: {} ch_size: 0x{} ch_addralign: 0x{:x}",
                       self.ch_type,
                       self.ch_size,
                       self.ch_addralign)
            }
        }
    }
}

/// ZLIB/DEFLATE algorithm.
pub const ELFCOMPRESS_ZLIB: u32 = 1;
/// Start of OS-specific.
pub const ELFCOMPRESS_LOOS: u32 = 0x60000000;
/// End of OS-specific.
pub const ELFCOMPRESS_HIOS: u32 = 0x6fffffff;
/// Start of processor-specific.
pub const ELFCOMPRESS_LOPROC: u32 = 0x70000000;
/// End of processor-specific.
pub const ELFCOMPRESS_HIPROC: u32 = 0x7fffffff;

macro_rules! elf_compression_header_std_impl { ($size:ty) => {

    #[cfg(test)]
    mod test {
        use super::*;
        #[test]
        fn size_of() {
            assert_eq!(::std::mem::size_of::<CompressionHeader>(), SIZEOF_CHDR);
        }
    }

    if_alloc! {
        use elf::compression_header::CompressionHeader as ElfCompressionHeader;

        use plain::Plain;

        if_std! {
            use error::Result;

            use std::fs::File;
            use std::io::{Read, Seek};
            use std::io::SeekFrom::Start;
        }

        impl From<CompressionHeader> for ElfCompressionHeader {
            fn from(ch: CompressionHeader) -> Self {
                ElfCompressionHeader {
                    ch_type: ch.ch_type,
                    ch_size: ch.ch_size as u64,
                    ch_addralign: ch.ch_addralign as u64,
                }
            }
        }

        impl CompressionHeader {
            pub fn from_bytes(bytes: &[u8]) -> CompressionHeader {
                let mut chdr = CompressionHeader::default();
                chdr.copy_from_bytes(bytes).expect("buffer is too short for header");
                chdr
            }

            #[cfg(feature = "std")]
            pub fn from_fd(fd: &mut File, offset: u64) -> Result<CompressionHeader> {
                let mut chdr = CompressionHeader::default();
                try!(fd.seek(Start(offset)));
                unsafe {
                    try!(fd.read(plain::as_mut_bytes(&mut chdr)));
                }
                Ok(chdr)
            }
        }
    } // end if_alloc
};}


pub mod compression_header32 {
    pub use elf::compression_header::*;

    #[repr(C)]
    #[derive(Copy, Clone, Eq, PartialEq, Default)]
    #[cfg_attr(feature = "alloc", derive(Pread, Pwrite, SizeWith))]
    /// The compression header is used at the start of SHF_COMPRESSED sections
    pub struct CompressionHeader {
        /// Compression format
        pub ch_type: u32,
        /// Uncompressed data size
        pub ch_size: u32,
        /// Uncompressed data alignment
        pub ch_addralign: u32,
    }

    elf_compression_header!();

    pub const SIZEOF_CHDR: usize = 12;

    elf_compression_header_std_impl!(u32);

    if_alloc! {
        impl From<ElfCompressionHeader> for CompressionHeader {
            fn from(ch: ElfCompressionHeader) -> Self {
                CompressionHeader {
                    ch_type: ch.ch_type,
                    ch_size: ch.ch_size as u32,
                    ch_addralign: ch.ch_addralign as u32,
                }
            }
        }
    }
}


pub mod compression_header64 {
    pub use elf::compression_header::*;

    #[repr(C)]
    #[derive(Copy, Clone, Eq, PartialEq, Default)]
    #[cfg_attr(feature = "alloc", derive(Pread, Pwrite, SizeWith))]
    /// The compression header is used at the start of SHF_COMPRESSED sections
    pub struct CompressionHeader {
        /// Compression format
        pub ch_type: u32,
        pub ch_reserved: u32,
        /// Uncompressed data size
        pub ch_size: u64,
        /// Uncompressed data alignment
        pub ch_addralign: u64,
    }

    elf_compression_header!();

    pub const SIZEOF_CHDR: usize = 24;

    elf_compression_header_std_impl!(u64);

    if_alloc! {
        impl From<ElfCompressionHeader> for CompressionHeader {
            fn from(ch: ElfCompressionHeader) -> Self {
                CompressionHeader {
                    ch_type: ch.ch_type,
                    ch_reserved: 0,
                    ch_size: ch.ch_size as u64,
                    ch_addralign: ch.ch_addralign as u64,
                }
            }
        }
    }
}

///////////////////////////////
// Std/analysis/Unified Structs
///////////////////////////////

if_alloc! {
    use error;
    use core::fmt;
    use core::result;
    use scroll::ctx;
    use container::{Container, Ctx};

    #[derive(Default, PartialEq, Clone)]
    /// A unified CompressionHeader - convertable to and from 32-bit and 64-bit variants
    pub struct CompressionHeader {
        /// Compression format
        pub ch_type: u32,
        /// Uncompressed data size
        pub ch_size: u64,
        /// Uncompressed data alignment
        pub ch_addralign: u64,
    }

    impl CompressionHeader {
        /// Return the size of the underlying compression header, given a `container`
        #[inline]
        pub fn size(ctx: &Ctx) -> usize {
            use scroll::ctx::SizeWith;
            Self::size_with(ctx)
        }
        pub fn new() -> Self {
            CompressionHeader {
                ch_type: 0,
                ch_size: 0,
                ch_addralign: 2 << 8,
            }
        }
        /// Parse a compression header from `bytes` at `offset`, using the given `ctx`
        #[cfg(feature = "endian_fd")]
        pub fn parse(bytes: &[u8], mut offset: usize, ctx: Ctx) -> error::Result<CompressionHeader> {
            use scroll::Pread;
            bytes.gread_with(&mut offset, ctx)
        }
    }

    impl fmt::Debug for CompressionHeader {
        fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
            write!(f,
                   "ch_type: {} ch_size: 0x{} ch_addralign: 0x{:x}",
                   self.ch_type,
                   self.ch_size,
                   self.ch_addralign)
        }
    }

    impl ctx::SizeWith<Ctx> for CompressionHeader {
        type Units = usize;
        fn size_with( &Ctx { container, .. }: &Ctx) -> Self::Units {
            match container {
                Container::Little => {
                    compression_header32::SIZEOF_CHDR
                },
                Container::Big => {
                    compression_header64::SIZEOF_CHDR
                },
            }
        }
    }

    impl<'a> ctx::TryFromCtx<'a, Ctx> for CompressionHeader {
        type Error = ::error::Error;
        type Size = usize;
        fn try_from_ctx(bytes: &'a [u8], Ctx {container, le}: Ctx) -> result::Result<(Self, Self::Size), Self::Error> {
            use scroll::Pread;
            let res = match container {
                Container::Little => {
                    (bytes.pread_with::<compression_header32::CompressionHeader>(0, le)?.into(), compression_header32::SIZEOF_CHDR)
                },
                Container::Big => {
                    (bytes.pread_with::<compression_header64::CompressionHeader>(0, le)?.into(), compression_header64::SIZEOF_CHDR)
                }
            };
            Ok(res)
        }
    }

    impl ctx::TryIntoCtx<Ctx> for CompressionHeader {
        type Error = ::error::Error;
        type Size = usize;
        fn try_into_ctx(self, bytes: &mut [u8], Ctx {container, le}: Ctx) -> result::Result<Self::Size, Self::Error> {
            use scroll::Pwrite;
            match container {
                Container::Little => {
                    let chdr: compression_header32::CompressionHeader = self.into();
                    Ok(bytes.pwrite_with(chdr, 0, le)?)
                },
                Container::Big => {
                    let chdr: compression_header64::CompressionHeader = self.into();
                    Ok(bytes.pwrite_with(chdr, 0, le)?)
                }
            }
        }
    }
    impl ctx::IntoCtx<Ctx> for CompressionHeader {
        fn into_ctx(self, bytes: &mut [u8], Ctx {container, le}: Ctx) {
            use scroll::Pwrite;
            match container {
                Container::Little => {
                    let chdr: compression_header32::CompressionHeader = self.into();
                    bytes.pwrite_with(chdr, 0, le).unwrap();
                },
                Container::Big => {
                    let chdr: compression_header64::CompressionHeader = self.into();
                    bytes.pwrite_with(chdr, 0, le).unwrap();
                }
            }
        }
    }
} // end if_alloc
