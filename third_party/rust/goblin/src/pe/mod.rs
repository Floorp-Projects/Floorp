//! A PE32 and PE32+ parser
//!

// TODO: panics with unwrap on None for apisetschema.dll, fhuxgraphics.dll and some others

use alloc::vec::Vec;

pub mod header;
pub mod optional_header;
pub mod characteristic;
pub mod section_table;
pub mod data_directories;
pub mod export;
pub mod import;
pub mod debug;
pub mod exception;
pub mod symbol;
pub mod relocation;
pub mod utils;

use crate::error;
use crate::container;
use crate::strtab;

use log::debug;

#[derive(Debug)]
/// An analyzed PE32/PE32+ binary
pub struct PE<'a> {
    /// The PE header
    pub header: header::Header,
    /// A list of the sections in this PE binary
    pub sections: Vec<section_table::SectionTable>,
    /// The size of the binary
    pub size: usize,
    /// The name of this `dll`, if it has one
    pub name: Option<&'a str>,
    /// Whether this is a `dll` or not
    pub is_lib: bool,
    /// Whether the binary is 64-bit (PE32+)
    pub is_64: bool,
    /// the entry point of the binary
    pub entry: usize,
    /// The binary's RVA, or image base - useful for computing virtual addreses
    pub image_base: usize,
    /// Data about any exported symbols in this binary (e.g., if it's a `dll`)
    pub export_data: Option<export::ExportData<'a>>,
    /// Data for any imported symbols, and from which `dll`, etc., in this binary
    pub import_data: Option<import::ImportData<'a>>,
    /// The list of exported symbols in this binary, contains synthetic information for easier analysis
    pub exports: Vec<export::Export<'a>>,
    /// The list symbols imported by this binary from other `dll`s
    pub imports: Vec<import::Import<'a>>,
    /// The list of libraries which this binary imports symbols from
    pub libraries: Vec<&'a str>,
    /// Debug information, if any, contained in the PE header
    pub debug_data: Option<debug::DebugData<'a>>,
    /// Exception handling and stack unwind information, if any, contained in the PE header
    pub exception_data: Option<exception::ExceptionData<'a>>,
}

impl<'a> PE<'a> {
    /// Reads a PE binary from the underlying `bytes`
    pub fn parse(bytes: &'a [u8]) -> error::Result<Self> {
        let header = header::Header::parse(bytes)?;
        debug!("{:#?}", header);
        let offset = &mut (header.dos_header.pe_pointer as usize + header::SIZEOF_PE_MAGIC + header::SIZEOF_COFF_HEADER + header.coff_header.size_of_optional_header as usize);
        let sections = header.coff_header.sections(bytes, offset)?;
        let is_lib = characteristic::is_dll(header.coff_header.characteristics);
        let mut entry = 0;
        let mut image_base = 0;
        let mut exports = vec![];
        let mut export_data = None;
        let mut name = None;
        let mut imports = vec![];
        let mut import_data = None;
        let mut libraries = vec![];
        let mut debug_data = None;
        let mut exception_data = None;
        let mut is_64 = false;
        if let Some(optional_header) = header.optional_header {
            entry = optional_header.standard_fields.address_of_entry_point as usize;
            image_base = optional_header.windows_fields.image_base as usize;
            is_64 = optional_header.container()? == container::Container::Big;
            debug!("entry {:#x} image_base {:#x} is_64: {}", entry, image_base, is_64);
            let file_alignment = optional_header.windows_fields.file_alignment;
            if let Some(export_table) = *optional_header.data_directories.get_export_table() {
                if let Ok(ed) = export::ExportData::parse(bytes, export_table, &sections, file_alignment) {
                    debug!("export data {:#?}", ed);
                    exports = export::Export::parse(bytes, &ed, &sections, file_alignment)?;
                    name = ed.name;
                    debug!("name: {:#?}", name);
                    export_data = Some(ed);
                }
            }
            debug!("exports: {:#?}", exports);
            if let Some(import_table) = *optional_header.data_directories.get_import_table() {
                let id = if is_64 {
                    import::ImportData::parse::<u64>(bytes, import_table, &sections, file_alignment)?
                } else {
                    import::ImportData::parse::<u32>(bytes, import_table, &sections, file_alignment)?
                };
                debug!("import data {:#?}", id);
                if is_64 {
                    imports = import::Import::parse::<u64>(bytes, &id, &sections)?
                } else {
                    imports = import::Import::parse::<u32>(bytes, &id, &sections)?
                }
                libraries = id.import_data.iter().map( | data | { data.name }).collect::<Vec<&'a str>>();
                libraries.sort();
                libraries.dedup();
                import_data = Some(id);
            }
            debug!("imports: {:#?}", imports);
            if let Some(debug_table) = *optional_header.data_directories.get_debug_table() {
                debug_data = Some(debug::DebugData::parse(bytes, debug_table, &sections, file_alignment)?);
            }

            if header.coff_header.machine == header::COFF_MACHINE_X86_64 {
                // currently only x86_64 is supported
                debug!("exception data: {:#?}", exception_data);
                if let Some(exception_table) = *optional_header.data_directories.get_exception_table() {
                    exception_data = Some(exception::ExceptionData::parse(bytes, exception_table, &sections, file_alignment)?);
                }
            }
        }
        Ok( PE {
            header,
            sections,
            size: 0,
            name,
            is_lib,
            is_64,
            entry,
            image_base,
            export_data,
            import_data,
            exports,
            imports,
            libraries,
            debug_data,
            exception_data,
        })
    }
}

/// An analyzed COFF object
#[derive(Debug)]
pub struct Coff<'a> {
    /// The COFF header
    pub header: header::CoffHeader,
    /// A list of the sections in this COFF binary
    pub sections: Vec<section_table::SectionTable>,
    /// The COFF symbol table.
    pub symbols: symbol::SymbolTable<'a>,
    /// The string table.
    pub strings: strtab::Strtab<'a>,
}

impl<'a> Coff<'a> {
    /// Reads a COFF object from the underlying `bytes`
    pub fn parse(bytes: &'a [u8]) -> error::Result<Self> {
        let offset = &mut 0;
        let header = header::CoffHeader::parse(bytes, offset)?;
        debug!("{:#?}", header);
        // TODO: maybe parse optional header, but it isn't present for Windows.
        *offset += header.size_of_optional_header as usize;
        let sections = header.sections(bytes, offset)?;
        let symbols = header.symbols(bytes)?;
        let strings = header.strings(bytes)?;
        Ok(Coff { header, sections, symbols, strings })
    }
}
