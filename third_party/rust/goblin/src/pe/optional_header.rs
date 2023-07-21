use crate::container;
use crate::error;

use crate::pe::data_directories;

use scroll::{ctx, Endian, LE};
use scroll::{Pread, Pwrite, SizeWith};

/// standard COFF fields
#[repr(C)]
#[derive(Debug, PartialEq, Copy, Clone, Default, Pread, Pwrite, SizeWith)]
pub struct StandardFields32 {
    pub magic: u16,
    pub major_linker_version: u8,
    pub minor_linker_version: u8,
    pub size_of_code: u32,
    pub size_of_initialized_data: u32,
    pub size_of_uninitialized_data: u32,
    pub address_of_entry_point: u32,
    pub base_of_code: u32,
    /// absent in 64-bit PE32+
    pub base_of_data: u32,
}

pub const SIZEOF_STANDARD_FIELDS_32: usize = 28;

/// standard 64-bit COFF fields
#[repr(C)]
#[derive(Debug, PartialEq, Copy, Clone, Default, Pread, Pwrite, SizeWith)]
pub struct StandardFields64 {
    pub magic: u16,
    pub major_linker_version: u8,
    pub minor_linker_version: u8,
    pub size_of_code: u32,
    pub size_of_initialized_data: u32,
    pub size_of_uninitialized_data: u32,
    pub address_of_entry_point: u32,
    pub base_of_code: u32,
}

pub const SIZEOF_STANDARD_FIELDS_64: usize = 24;

/// Unified 32/64-bit COFF fields
#[derive(Debug, PartialEq, Copy, Clone, Default)]
pub struct StandardFields {
    pub magic: u16,
    pub major_linker_version: u8,
    pub minor_linker_version: u8,
    pub size_of_code: u64,
    pub size_of_initialized_data: u64,
    pub size_of_uninitialized_data: u64,
    pub address_of_entry_point: u64,
    pub base_of_code: u64,
    /// absent in 64-bit PE32+
    pub base_of_data: u32,
}

impl From<StandardFields32> for StandardFields {
    fn from(fields: StandardFields32) -> Self {
        StandardFields {
            magic: fields.magic,
            major_linker_version: fields.major_linker_version,
            minor_linker_version: fields.minor_linker_version,
            size_of_code: u64::from(fields.size_of_code),
            size_of_initialized_data: u64::from(fields.size_of_initialized_data),
            size_of_uninitialized_data: u64::from(fields.size_of_uninitialized_data),
            address_of_entry_point: u64::from(fields.address_of_entry_point),
            base_of_code: u64::from(fields.base_of_code),
            base_of_data: fields.base_of_data,
        }
    }
}

impl From<StandardFields64> for StandardFields {
    fn from(fields: StandardFields64) -> Self {
        StandardFields {
            magic: fields.magic,
            major_linker_version: fields.major_linker_version,
            minor_linker_version: fields.minor_linker_version,
            size_of_code: u64::from(fields.size_of_code),
            size_of_initialized_data: u64::from(fields.size_of_initialized_data),
            size_of_uninitialized_data: u64::from(fields.size_of_uninitialized_data),
            address_of_entry_point: u64::from(fields.address_of_entry_point),
            base_of_code: u64::from(fields.base_of_code),
            base_of_data: 0,
        }
    }
}

/// Standard fields magic number for 32-bit binary
pub const MAGIC_32: u16 = 0x10b;
/// Standard fields magic number for 64-bit binary
pub const MAGIC_64: u16 = 0x20b;

/// Windows specific fields
#[repr(C)]
#[derive(Debug, PartialEq, Copy, Clone, Default, Pread, Pwrite, SizeWith)]
pub struct WindowsFields32 {
    pub image_base: u32,
    pub section_alignment: u32,
    pub file_alignment: u32,
    pub major_operating_system_version: u16,
    pub minor_operating_system_version: u16,
    pub major_image_version: u16,
    pub minor_image_version: u16,
    pub major_subsystem_version: u16,
    pub minor_subsystem_version: u16,
    pub win32_version_value: u32,
    pub size_of_image: u32,
    pub size_of_headers: u32,
    pub check_sum: u32,
    pub subsystem: u16,
    pub dll_characteristics: u16,
    pub size_of_stack_reserve: u32,
    pub size_of_stack_commit: u32,
    pub size_of_heap_reserve: u32,
    pub size_of_heap_commit: u32,
    pub loader_flags: u32,
    pub number_of_rva_and_sizes: u32,
}

pub const SIZEOF_WINDOWS_FIELDS_32: usize = 68;
/// Offset of the `check_sum` field in [`WindowsFields32`]
pub const OFFSET_WINDOWS_FIELDS_32_CHECKSUM: usize = 36;

/// 64-bit Windows specific fields
#[repr(C)]
#[derive(Debug, PartialEq, Copy, Clone, Default, Pread, Pwrite, SizeWith)]
pub struct WindowsFields64 {
    pub image_base: u64,
    pub section_alignment: u32,
    pub file_alignment: u32,
    pub major_operating_system_version: u16,
    pub minor_operating_system_version: u16,
    pub major_image_version: u16,
    pub minor_image_version: u16,
    pub major_subsystem_version: u16,
    pub minor_subsystem_version: u16,
    pub win32_version_value: u32,
    pub size_of_image: u32,
    pub size_of_headers: u32,
    pub check_sum: u32,
    pub subsystem: u16,
    pub dll_characteristics: u16,
    pub size_of_stack_reserve: u64,
    pub size_of_stack_commit: u64,
    pub size_of_heap_reserve: u64,
    pub size_of_heap_commit: u64,
    pub loader_flags: u32,
    pub number_of_rva_and_sizes: u32,
}

pub const SIZEOF_WINDOWS_FIELDS_64: usize = 88;
/// Offset of the `check_sum` field in [`WindowsFields64`]
pub const OFFSET_WINDOWS_FIELDS_64_CHECKSUM: usize = 40;

// /// Generic 32/64-bit Windows specific fields
// #[derive(Debug, PartialEq, Copy, Clone, Default)]
// pub struct WindowsFields {
//     pub image_base: u64,
//     pub section_alignment: u32,
//     pub file_alignment: u32,
//     pub major_operating_system_version: u16,
//     pub minor_operating_system_version: u16,
//     pub major_image_version: u16,
//     pub minor_image_version: u16,
//     pub major_subsystem_version: u16,
//     pub minor_subsystem_version: u16,
//     pub win32_version_value: u32,
//     pub size_of_image: u32,
//     pub size_of_headers: u32,
//     pub check_sum: u32,
//     pub subsystem: u16,
//     pub dll_characteristics: u16,
//     pub size_of_stack_reserve: u64,
//     pub size_of_stack_commit:  u64,
//     pub size_of_heap_reserve:  u64,
//     pub size_of_heap_commit:   u64,
//     pub loader_flags: u32,
//     pub number_of_rva_and_sizes: u32,
// }

impl From<WindowsFields32> for WindowsFields {
    fn from(windows: WindowsFields32) -> Self {
        WindowsFields {
            image_base: u64::from(windows.image_base),
            section_alignment: windows.section_alignment,
            file_alignment: windows.file_alignment,
            major_operating_system_version: windows.major_operating_system_version,
            minor_operating_system_version: windows.minor_operating_system_version,
            major_image_version: windows.major_image_version,
            minor_image_version: windows.minor_image_version,
            major_subsystem_version: windows.major_subsystem_version,
            minor_subsystem_version: windows.minor_subsystem_version,
            win32_version_value: windows.win32_version_value,
            size_of_image: windows.size_of_image,
            size_of_headers: windows.size_of_headers,
            check_sum: windows.check_sum,
            subsystem: windows.subsystem,
            dll_characteristics: windows.dll_characteristics,
            size_of_stack_reserve: u64::from(windows.size_of_stack_reserve),
            size_of_stack_commit: u64::from(windows.size_of_stack_commit),
            size_of_heap_reserve: u64::from(windows.size_of_heap_reserve),
            size_of_heap_commit: u64::from(windows.size_of_heap_commit),
            loader_flags: windows.loader_flags,
            number_of_rva_and_sizes: windows.number_of_rva_and_sizes,
        }
    }
}

// impl From<WindowsFields32> for WindowsFields {
//     fn from(windows: WindowsFields32) -> Self {
//         WindowsFields {
//             image_base: windows.image_base,
//             section_alignment: windows.section_alignment,
//             file_alignment: windows.file_alignment,
//             major_operating_system_version: windows.major_operating_system_version,
//             minor_operating_system_version: windows.minor_operating_system_version,
//             major_image_version: windows.major_image_version,
//             minor_image_version: windows.minor_image_version,
//             major_subsystem_version: windows.major_subsystem_version,
//             minor_subsystem_version: windows.minor_subsystem_version,
//             win32_version_value: windows.win32_version_value,
//             size_of_image: windows.size_of_image,
//             size_of_headers: windows.size_of_headers,
//             check_sum: windows.check_sum,
//             subsystem: windows.subsystem,
//             dll_characteristics: windows.dll_characteristics,
//             size_of_stack_reserve: windows.size_of_stack_reserve,
//             size_of_stack_commit: windows.size_of_stack_commit,
//             size_of_heap_reserve: windows.size_of_heap_reserve,
//             size_of_heap_commit: windows.size_of_heap_commit,
//             loader_flags: windows.loader_flags,
//             number_of_rva_and_sizes: windows.number_of_rva_and_sizes,
//         }
//     }
// }

pub type WindowsFields = WindowsFields64;

#[derive(Debug, PartialEq, Copy, Clone)]
pub struct OptionalHeader {
    pub standard_fields: StandardFields,
    pub windows_fields: WindowsFields,
    pub data_directories: data_directories::DataDirectories,
}

impl OptionalHeader {
    pub fn container(&self) -> error::Result<container::Container> {
        match self.standard_fields.magic {
            MAGIC_32 => Ok(container::Container::Little),
            MAGIC_64 => Ok(container::Container::Big),
            magic => Err(error::Error::BadMagic(u64::from(magic))),
        }
    }
}

impl<'a> ctx::TryFromCtx<'a, Endian> for OptionalHeader {
    type Error = crate::error::Error;
    fn try_from_ctx(bytes: &'a [u8], _: Endian) -> error::Result<(Self, usize)> {
        let magic = bytes.pread_with::<u16>(0, LE)?;
        let offset = &mut 0;
        let (standard_fields, windows_fields): (StandardFields, WindowsFields) = match magic {
            MAGIC_32 => {
                let standard_fields = bytes.gread_with::<StandardFields32>(offset, LE)?.into();
                let windows_fields = bytes.gread_with::<WindowsFields32>(offset, LE)?.into();
                (standard_fields, windows_fields)
            }
            MAGIC_64 => {
                let standard_fields = bytes.gread_with::<StandardFields64>(offset, LE)?.into();
                let windows_fields = bytes.gread_with::<WindowsFields64>(offset, LE)?;
                (standard_fields, windows_fields)
            }
            _ => return Err(error::Error::BadMagic(u64::from(magic))),
        };
        let data_directories = data_directories::DataDirectories::parse(
            &bytes,
            windows_fields.number_of_rva_and_sizes as usize,
            offset,
        )?;
        Ok((
            OptionalHeader {
                standard_fields,
                windows_fields,
                data_directories,
            },
            0,
        )) // TODO: FIXME
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn sizeof_standards32() {
        assert_eq!(
            ::std::mem::size_of::<StandardFields32>(),
            SIZEOF_STANDARD_FIELDS_32
        );
    }
    #[test]
    fn sizeof_windows32() {
        assert_eq!(
            ::std::mem::size_of::<WindowsFields32>(),
            SIZEOF_WINDOWS_FIELDS_32
        );
    }
    #[test]
    fn sizeof_standards64() {
        assert_eq!(
            ::std::mem::size_of::<StandardFields64>(),
            SIZEOF_STANDARD_FIELDS_64
        );
    }
    #[test]
    fn sizeof_windows64() {
        assert_eq!(
            ::std::mem::size_of::<WindowsFields64>(),
            SIZEOF_WINDOWS_FIELDS_64
        );
    }
}
