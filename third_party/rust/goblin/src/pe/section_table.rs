use scroll::{self, Pread};
use error;

#[repr(C)]
#[derive(Debug, PartialEq, Copy, Clone, Default)]
pub struct SectionTable {
    pub name: [u8; 8],
    pub virtual_size: u32,
    pub virtual_address: u32,
    pub size_of_raw_data: u32,
    pub pointer_to_raw_data: u32,
    pub pointer_to_relocations: u32,
    pub pointer_to_linenumbers: u32,
    pub number_of_relocations: u16,
    pub number_of_linenumbers: u16,
    pub characteristics: u32,
}

pub const SIZEOF_SECTION_TABLE: usize = 8 * 5;

impl SectionTable {
    pub fn parse(bytes: &[u8], offset: &mut usize) -> error::Result<Self> {
        let mut table = SectionTable::default();
        let mut name = [0u8; 8];
        for i in 0..8 {
            name[i] = bytes.gread_with(offset, scroll::LE)?;
        }
        table.name = name;
        table.virtual_size = bytes.gread_with(offset, scroll::LE)?;
        table.virtual_address = bytes.gread_with(offset, scroll::LE)?;
        table.size_of_raw_data = bytes.gread_with(offset, scroll::LE)?;
        table.pointer_to_raw_data = bytes.gread_with(offset, scroll::LE)?;
        table.pointer_to_relocations = bytes.gread_with(offset, scroll::LE)?;
        table.pointer_to_linenumbers = bytes.gread_with(offset, scroll::LE)?;
        table.number_of_relocations = bytes.gread_with(offset, scroll::LE)?;
        table.number_of_linenumbers = bytes.gread_with(offset, scroll::LE)?;
        table.characteristics = bytes.gread_with(offset, scroll::LE)?;
        Ok(table)
    }
    pub fn name(&self) -> error::Result<&str> {
        Ok(self.name.pread(0)?)
    }
}

/// The section should not be padded to the next boundary. This flag is obsolete and is replaced
/// by `IMAGE_SCN_ALIGN_1BYTES`. This is valid only for object files.
pub const IMAGE_SCN_TYPE_NO_PAD: u32 = 0x00000008;
/// The section contains executable code.
pub const IMAGE_SCN_CNT_CODE: u32 = 0x00000020;
/// The section contains initialized data.
pub const IMAGE_SCN_CNT_INITIALIZED_DATA: u32 = 0x00000040;
///  The section contains uninitialized data.
pub const IMAGE_SCN_CNT_UNINITIALIZED_DATA: u32 = 0x00000080;
pub const IMAGE_SCN_LNK_OTHER: u32 = 0x00000100;
/// The section contains comments or other information. The .drectve section has this type.
/// This is valid for object files only.
pub const IMAGE_SCN_LNK_INFO: u32 = 0x00000200;
/// The section will not become part of the image. This is valid only for object files.
pub const IMAGE_SCN_LNK_REMOVE: u32 = 0x00000800;
/// The section contains COMDAT data. This is valid only for object files.
pub const IMAGE_SCN_LNK_COMDAT: u32 = 0x00001000;
/// The section contains data referenced through the global pointer (GP).
pub const IMAGE_SCN_GPREL: u32 = 0x00008000;
pub const IMAGE_SCN_MEM_PURGEABLE: u32 = 0x00020000;
pub const IMAGE_SCN_MEM_16BIT: u32 = 0x00020000;
pub const IMAGE_SCN_MEM_LOCKED: u32 = 0x00040000;
pub const IMAGE_SCN_MEM_PRELOAD: u32 = 0x00080000;

pub const IMAGE_SCN_ALIGN_1BYTES: u32 = 0x00100000;
pub const IMAGE_SCN_ALIGN_2BYTES: u32 = 0x00200000;
pub const IMAGE_SCN_ALIGN_4BYTES: u32 = 0x00300000;
pub const IMAGE_SCN_ALIGN_8BYTES: u32 = 0x00400000;
pub const IMAGE_SCN_ALIGN_16BYTES: u32 = 0x00500000;
pub const IMAGE_SCN_ALIGN_32BYTES: u32 = 0x00600000;
pub const IMAGE_SCN_ALIGN_64BYTES: u32 = 0x00700000;
pub const IMAGE_SCN_ALIGN_128BYTES: u32 = 0x00800000;
pub const IMAGE_SCN_ALIGN_256BYTES: u32 = 0x00900000;
pub const IMAGE_SCN_ALIGN_512BYTES: u32 = 0x00A00000;
pub const IMAGE_SCN_ALIGN_1024BYTES: u32 = 0x00B00000;
pub const IMAGE_SCN_ALIGN_2048BYTES: u32 = 0x00C00000;
pub const IMAGE_SCN_ALIGN_4096BYTES: u32 = 0x00D00000;
pub const IMAGE_SCN_ALIGN_8192BYTES: u32 = 0x00E00000;
pub const IMAGE_SCN_ALIGN_MASK: u32 = 0x00F00000;

/// The section contains extended relocations.
pub const IMAGE_SCN_LNK_NRELOC_OVFL: u32 = 0x01000000;
/// The section can be discarded as needed.
pub const IMAGE_SCN_MEM_DISCARDABLE: u32 = 0x02000000;
/// The section cannot be cached.
pub const IMAGE_SCN_MEM_NOT_CACHED: u32 = 0x04000000;
/// The section is not pageable.
pub const IMAGE_SCN_MEM_NOT_PAGED: u32 = 0x08000000;
/// The section can be shared in memory.
pub const IMAGE_SCN_MEM_SHARED: u32 = 0x10000000;
/// The section can be executed as code.
pub const IMAGE_SCN_MEM_EXECUTE: u32 = 0x20000000;
/// The section can be read.
pub const IMAGE_SCN_MEM_READ: u32 = 0x40000000;
/// The section can be written to.
pub const IMAGE_SCN_MEM_WRITE: u32 = 0x80000000;
