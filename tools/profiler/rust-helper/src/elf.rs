/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use compact_symbol_table::CompactSymbolTable;
use goblin::elf;
use object::{ElfFile, Object, SymbolKind, Uuid};
use std::cmp;
use std::collections::HashMap;

const UUID_SIZE: usize = 16;
const PAGE_SIZE: usize = 4096;

fn get_symbol_map<'a, 'b, T>(object_file: &'b T) -> HashMap<u32, &'a str>
where
    T: Object<'a, 'b>,
{
    object_file
        .dynamic_symbols()
        .chain(object_file.symbols())
        .filter(|symbol| symbol.kind() == SymbolKind::Text)
        .filter_map(|symbol| symbol.name().map(|name| (symbol.address() as u32, name)))
        .collect()
}

pub fn get_compact_symbol_table(
    buffer: &[u8],
    breakpad_id: Option<&str>,
) -> Option<CompactSymbolTable> {
    let elf_file = ElfFile::parse(buffer).ok()?;
    let elf_id = get_elf_id(&elf_file, buffer)?;
    if !breakpad_id.map_or(true, |id| id == format!("{:X}0", elf_id.simple())) {
        return None;
    }
    return Some(CompactSymbolTable::from_map(get_symbol_map(&elf_file)));
}

fn create_elf_id(identifier: &[u8], little_endian: bool) -> Option<Uuid> {
    // Make sure that we have exactly UUID_SIZE bytes available
    let mut data = [0 as u8; UUID_SIZE];
    let len = cmp::min(identifier.len(), UUID_SIZE);
    data[0..len].copy_from_slice(&identifier[0..len]);

    if little_endian {
        // The file ELF file targets a little endian architecture. Convert to
        // network byte order (big endian) to match the Breakpad processor's
        // expectations. For big endian object files, this is not needed.
        data[0..4].reverse(); // uuid field 1
        data[4..6].reverse(); // uuid field 2
        data[6..8].reverse(); // uuid field 3
    }

    Uuid::from_bytes(&data).ok()
}

/// Tries to obtain the object identifier of an ELF object.
///
/// As opposed to Mach-O, ELF does not specify a unique ID for object files in
/// its header. Compilers and linkers usually add either `SHT_NOTE` sections or
/// `PT_NOTE` program header elements for this purpose. If one of these notes
/// is present, ElfFile's build_id() method will find it.
///
/// If neither of the above are present, this function will hash the first page
/// of the `.text` section (program code). This matches what the Breakpad
/// processor does.
///
/// If all of the above fails, this function will return `None`.
pub fn get_elf_id(elf_file: &ElfFile, data: &[u8]) -> Option<Uuid> {
    if let Some(identifier) = elf_file.build_id() {
        return create_elf_id(identifier, elf_file.elf().little_endian);
    }

    // We were not able to locate the build ID, so fall back to hashing the
    // first page of the ".text" (program code) section. This algorithm XORs
    // 16-byte chunks directly into a UUID buffer.
    if let Some(section_data) = find_text_section(elf_file.elf(), data) {
        let mut hash = [0; UUID_SIZE];
        for i in 0..cmp::min(section_data.len(), PAGE_SIZE) {
            hash[i % UUID_SIZE] ^= section_data[i];
        }

        return create_elf_id(&hash, elf_file.elf().little_endian);
    }

    None
}

/// Returns a reference to the data of the the .text section in an ELF binary.
fn find_text_section<'elf, 'data>(elf: &'elf elf::Elf, data: &'data [u8]) -> Option<&'data [u8]> {
    elf.section_headers.iter().find_map(|header| {
        match (header.sh_type, elf.shdr_strtab.get(header.sh_name)) {
            (elf::section_header::SHT_PROGBITS, Some(Ok(".text"))) => {
                Some(&data[header.sh_offset as usize..][..header.sh_size as usize])
            }
            _ => None,
        }
    })
}
