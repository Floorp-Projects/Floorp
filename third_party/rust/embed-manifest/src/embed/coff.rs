//! Just as much COFF object file support as is needed to write a resource data segment
//! for GNU Windows targets. Inspired by the `write::Object` code from the `object` crate.
//!
//! Integers are converted from u64 to u32 and added without checking because the manifest
//! data cannot get anywhere close to overflowing unless the supplied application name or
//! number of dependencies was extremely long. If this code was used more generally or if
//! the input was less trustworthy then more checked conversions and checked arithmetic
//! would be needed.

use std::io::{self, Seek, SeekFrom, Write};
use std::time::SystemTime;

#[derive(Debug, Clone, Copy)]
pub enum MachineType {
    I386,
    X86_64,
    Aarch64,
}

impl MachineType {
    pub fn machine(&self) -> u16 {
        match self {
            Self::I386 => 0x014c,
            Self::X86_64 => 0x8664,
            Self::Aarch64 => 0xaa64,
        }
    }

    pub fn relocation_type(&self) -> u16 {
        match self {
            Self::I386 => 7,
            Self::X86_64 => 3,
            Self::Aarch64 => 2,
        }
    }
}

pub struct CoffWriter<W> {
    /// wrapped writer or buffer
    writer: W,
    /// machine type for file header
    machine_type: MachineType,
    // size in bytes of resource section data
    size_of_raw_data: u32,
    // number of relocations at the end of the section
    number_of_relocations: u16,
}

impl<W: Write + Seek> CoffWriter<W> {
    /// Create a new instance wrapping a writer.
    pub fn new(mut writer: W, machine_type: MachineType) -> io::Result<Self> {
        // Add space for file header and section table.
        writer.write_all(&[0; 60])?;
        Ok(Self {
            writer,
            machine_type,
            size_of_raw_data: 0,
            number_of_relocations: 0,
        })
    }

    /// Add data to a section and return its offset within the section.
    pub fn add_data(&mut self, data: &[u8]) -> io::Result<u32> {
        let start = self.size_of_raw_data;
        self.writer.write_all(data)?;
        self.size_of_raw_data = start + data.len() as u32;
        Ok(start)
    }

    // Pad the resource data to a multiple of `n` bytes.
    pub fn align_to(&mut self, n: u32) -> io::Result<()> {
        let offset = self.size_of_raw_data % n;
        if offset != 0 {
            let padding = n - offset;
            for _ in 0..padding {
                self.writer.write_all(&[0])?;
            }
            self.size_of_raw_data += padding;
        }
        Ok(())
    }

    /// Write a relocation for a symbol at the end of the section.
    pub fn add_relocation(&mut self, address: u32) -> io::Result<()> {
        self.number_of_relocations += 1;
        self.writer.write_all(&address.to_le_bytes())?;
        self.writer.write_all(&[0, 0, 0, 0])?;
        self.writer.write_all(&self.machine_type.relocation_type().to_le_bytes())
    }

    /// Write the object and section headers and write the symbol table.
    pub fn finish(mut self) -> io::Result<W> {
        // Get the timestamp for the header. `as` is correct here, as the low 32 bits
        // should be used.
        let timestamp = SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .map_or(0, |d| d.as_secs() as u32);

        // Copy file location of the symbol table.
        let pointer_to_symbol_table = self.writer.stream_position()? as u32;

        // Write the symbols and auxiliary data for the section.
        self.writer.write_all(b".rsrc\0\0\0")?; // name
        self.writer.write_all(&[0, 0, 0, 0])?; // address
        self.writer.write_all(&[1, 0])?; // section number (1-based)
        self.writer.write_all(&[0, 0, 3, 1])?; // type = 0, class = static, aux symbols = 1
        self.writer.write_all(&self.size_of_raw_data.to_le_bytes())?;
        self.writer.write_all(&self.number_of_relocations.to_le_bytes())?;
        self.writer.write_all(&[0; 12])?;

        // Write the empty string table.
        self.writer.write_all(&[0; 4])?;

        // Write the object header.
        let end_of_file = self.writer.seek(SeekFrom::Start(0))?;
        self.writer.write_all(&self.machine_type.machine().to_le_bytes())?;
        self.writer.write_all(&[1, 0])?; // number of sections
        self.writer.write_all(&timestamp.to_le_bytes())?;
        self.writer.write_all(&pointer_to_symbol_table.to_le_bytes())?;
        self.writer.write_all(&[2, 0, 0, 0])?; // number of symbol table entries
        self.writer.write_all(&[0; 4])?; // optional header size = 0, characteristics = 0

        // Write the section header.
        self.writer.write_all(b".rsrc\0\0\0")?;
        self.writer.write_all(&[0; 8])?; // virtual size = 0 and virtual address = 0
        self.writer.write_all(&self.size_of_raw_data.to_le_bytes())?;
        self.writer.write_all(&[60, 0, 0, 0])?; // pointer to raw data
        self.writer.write_all(&(self.size_of_raw_data + 60).to_le_bytes())?; // pointer to relocations
        self.writer.write_all(&[0; 4])?; // pointer to line numbers
        self.writer.write_all(&self.number_of_relocations.to_le_bytes())?;
        self.writer.write_all(&[0; 2])?; // number of line numbers
        self.writer.write_all(&[0x40, 0, 0x30, 0xc0])?; // characteristics

        // Return the inner writer and dispose of this object.
        self.writer.seek(SeekFrom::Start(end_of_file))?;
        Ok(self.writer)
    }
}

/// Returns the bytes for a resource directory table.
///
/// Most of the fields are set to zero, including the timestamp, to aid
/// with making builds reproducible.
///
/// ```c
/// typedef struct {
///     DWORD Characteristics,
///     DWORD TimeDateStamp,
///     WORD MajorVersion,
///     WORD MinorVersion,
///     WORD NumberOfNamedEntries,
///     WORD NumberOfIdEntries
/// } IMAGE_RESOURCE_DIRECTORY;
/// ```
pub fn resource_directory_table(number_of_id_entries: u16) -> [u8; 16] {
    let mut table = [0; 16];
    table[14..16].copy_from_slice(&number_of_id_entries.to_le_bytes());
    table
}

/// Returns the bytes for a resource directory entry for an ID.
///
/// ```c
/// typedef struct {
///     DWORD Name,
///     DWORD OffsetToData
/// } IMAGE_RESOURCE_DIRECTORY_ENTRY;
/// ```
pub fn resource_directory_id_entry(id: u32, offset: u32, subdirectory: bool) -> [u8; 8] {
    let mut entry = [0; 8];
    entry[0..4].copy_from_slice(&id.to_le_bytes());
    let flag: u32 = if subdirectory { 0x80000000 } else { 0 };
    entry[4..8].copy_from_slice(&((offset & 0x7fffffff) | flag).to_le_bytes());
    entry
}

/// Returns the bytes for a resource data entry.
///
/// ```c
/// typedef struct {
///     DWORD OffsetToData,
///     DWORD Size,
///     DWORD CodePage,
///     DWORD Reserved
/// } IMAGE_RESOURCE_DATA_ENTRY;
/// ```
pub fn resource_data_entry(rva: u32, size: u32) -> [u8; 16] {
    let mut entry = [0; 16];
    entry[0..4].copy_from_slice(&rva.to_le_bytes());
    entry[4..8].copy_from_slice(&size.to_le_bytes());
    entry
}
