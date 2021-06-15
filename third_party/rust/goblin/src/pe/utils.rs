use scroll::Pread;
use alloc::string::ToString;
use crate::error;

use super::section_table;

use core::cmp;
use crate::pe::data_directories::DataDirectory;

use log::debug;

pub fn is_in_range (rva: usize, r1: usize, r2: usize) -> bool {
    r1 <= rva && rva < r2
}

// reference: Peter Ferrie. Reliable algorithm to extract overlay of a PE. https://bit.ly/2vBX2bR
#[inline]
fn aligned_pointer_to_raw_data(pointer_to_raw_data: usize) -> usize {
    const PHYSICAL_ALIGN: usize = 0x1ff;
    pointer_to_raw_data & !PHYSICAL_ALIGN
}

#[inline]
fn section_read_size(section: &section_table::SectionTable, file_alignment: u32) -> usize {
    fn round_size(size: usize) -> usize {
        const PAGE_MASK: usize = 0xfff;
        (size + PAGE_MASK) & !PAGE_MASK
    }

    let file_alignment = file_alignment as usize;
    let size_of_raw_data = section.size_of_raw_data as usize;
    let virtual_size = section.virtual_size as usize;
    let read_size = {
        let read_size = (section.pointer_to_raw_data as usize + size_of_raw_data + file_alignment - 1) & !(file_alignment - 1);
        cmp::min(read_size, round_size(size_of_raw_data))
    };

    if virtual_size == 0 {
        read_size
    } else {
        cmp::min(read_size, round_size(virtual_size))
    }
}

fn rva2offset (rva: usize, section: &section_table::SectionTable) -> usize {
    (rva - section.virtual_address as usize) + aligned_pointer_to_raw_data(section.pointer_to_raw_data as usize)
}

fn is_in_section (rva: usize, section: &section_table::SectionTable, file_alignment: u32) -> bool {
    let section_rva = section.virtual_address as usize;
    is_in_range(rva, section_rva, section_rva + section_read_size(section, file_alignment))
}

pub fn find_offset (rva: usize, sections: &[section_table::SectionTable], file_alignment: u32) -> Option<usize> {
    for (i, section) in sections.iter().enumerate() {
        debug!("Checking {} for {:#x} ∈ {:#x}..{:#x}", section.name().unwrap_or(""), rva, section.virtual_address, section.virtual_address + section.virtual_size);
        if is_in_section(rva, &section, file_alignment) {
            let offset = rva2offset(rva, &section);
            debug!("Found in section {}({}), remapped into offset {:#x}", section.name().unwrap_or(""), i, offset);
            return Some(offset)
        }
    }
    None
}

pub fn find_offset_or (rva: usize, sections: &[section_table::SectionTable], file_alignment: u32, msg: &str) -> error::Result<usize> {
    find_offset(rva, sections, file_alignment).ok_or_else(|| error::Error::Malformed(msg.to_string()))
}

pub fn try_name<'a>(bytes: &'a [u8], rva: usize, sections: &[section_table::SectionTable], file_alignment: u32) -> error::Result<&'a str> {
    match find_offset(rva, sections, file_alignment) {
        Some(offset) => {
            Ok(bytes.pread::<&str>(offset)?)
        },
        None => {
            Err(error::Error::Malformed(format!("Cannot find name from rva {:#x} in sections: {:?}", rva, sections)))
        }
    }
}

pub fn get_data<'a, T>(bytes: &'a [u8], sections: &[section_table::SectionTable], directory: DataDirectory, file_alignment: u32) -> error::Result<T>
    where T: scroll::ctx::TryFromCtx<'a, scroll::Endian, Error = scroll::Error>  {
    let rva = directory.virtual_address as usize;
    let offset = find_offset(rva, sections, file_alignment)
        .ok_or_else(||error::Error::Malformed(directory.virtual_address.to_string()))?;
    let result: T = bytes.pread_with(offset, scroll::LE)?;
    Ok(result)
}
