use scroll::{Pread};
use alloc::string::ToString;
use error;

use super::section_table;

pub fn is_in_range (rva: usize, r1: usize, r2: usize) -> bool {
    r1 <= rva && rva < r2
}

fn rva2offset (rva: usize, section: &section_table::SectionTable) -> usize {
    (rva - section.virtual_address as usize) + section.pointer_to_raw_data as usize
}

fn is_in_section (rva: usize, section: &section_table::SectionTable) -> bool {
    section.virtual_address as usize <= rva && rva < (section.virtual_address + section.virtual_size) as usize
}

pub fn find_offset (rva: usize, sections: &[section_table::SectionTable]) -> Option<usize> {
    for (i, section) in sections.iter().enumerate() {
        debug!("Checking {} for {:#x} ∈ {:#x}..{:#x}", section.name().unwrap_or(""), rva, section.virtual_address, section.virtual_address + section.virtual_size);
        if is_in_section(rva, &section) {
            let offset = rva2offset(rva, &section);
            debug!("Found in section {}({}), remapped into offset {:#x}", section.name().unwrap_or(""), i, offset);
            return Some(offset)
        }
    }
    None
}

pub fn find_offset_or (rva: usize, sections: &[section_table::SectionTable], msg: &str) -> error::Result<usize> {
    find_offset(rva, sections).ok_or(error::Error::Malformed(msg.to_string()))
}

pub fn try_name<'a>(bytes: &'a [u8], rva: usize, sections: &[section_table::SectionTable]) -> error::Result<&'a str> {
    match find_offset(rva, sections) {
        Some(offset) => {
            Ok(bytes.pread::<&str>(offset)?)
        },
        None => {
            Err(error::Error::Malformed(format!("Cannot find name from rva {:#x} in sections: {:?}", rva, sections)))
        }
    }
}
