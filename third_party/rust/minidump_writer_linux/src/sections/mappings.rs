use crate::errors::SectionMappingsError;
use crate::linux_ptrace_dumper::LinuxPtraceDumper;
use crate::maps_reader::MappingInfo;
use crate::minidump_format::*;
use crate::minidump_writer::{DumpBuf, MinidumpWriter};
use crate::sections::{write_string_to_location, MemoryArrayWriter, MemoryWriter};

type Result<T> = std::result::Result<T, SectionMappingsError>;

/// Write information about the mappings in effect. Because we are using the
/// minidump format, the information about the mappings is pretty limited.
/// Because of this, we also include the full, unparsed, /proc/$x/maps file in
/// another stream in the file.
pub fn write(
    config: &mut MinidumpWriter,
    buffer: &mut DumpBuf,
    dumper: &mut LinuxPtraceDumper,
) -> Result<MDRawDirectory> {
    let mut modules = Vec::new();

    // First write all the mappings from the dumper
    for map_idx in 0..dumper.mappings.len() {
        // If the mapping is uninteresting, or if
        // there is caller-provided information about this mapping
        // in the user_mapping_list list, skip it

        if !dumper.mappings[map_idx].is_interesting()
            || dumper.mappings[map_idx].is_contained_in(&config.user_mapping_list)
        {
            continue;
        }
        // Note: elf_identifier_for_mapping_index() can manipulate the |mapping.name|.
        let identifier = dumper
            .elf_identifier_for_mapping_index(map_idx)
            .unwrap_or_default();

        // If the identifier is all 0, its an uninteresting mapping (bmc#1676109)
        if identifier.is_empty() || identifier.iter().all(|&x| x == 0) {
            continue;
        }

        let module = fill_raw_module(buffer, &dumper.mappings[map_idx], &identifier)?;
        modules.push(module);
    }

    // Next write all the mappings provided by the caller
    for user in &config.user_mapping_list {
        // GUID was provided by caller.
        let module = fill_raw_module(buffer, &user.mapping, &user.identifier)?;
        modules.push(module)
    }

    let list_header = MemoryWriter::<u32>::alloc_with_val(buffer, modules.len() as u32)?;

    let mut dirent = MDRawDirectory {
        stream_type: MDStreamType::ModuleListStream as u32,
        location: list_header.location(),
    };

    if !modules.is_empty() {
        let mapping_list = MemoryArrayWriter::<MDRawModule>::alloc_from_array(buffer, &modules)?;
        dirent.location.data_size += mapping_list.location().data_size;
    }

    Ok(dirent)
}

fn fill_raw_module(
    buffer: &mut DumpBuf,
    mapping: &MappingInfo,
    identifier: &[u8],
) -> Result<MDRawModule> {
    let cv_record: MDLocationDescriptor;
    if identifier.is_empty() {
        // Just zeroes
        cv_record = Default::default();
    } else {
        let cv_signature = MD_CVINFOELF_SIGNATURE;
        let array_size = std::mem::size_of_val(&cv_signature) + identifier.len();

        let mut sig_section = MemoryArrayWriter::<u8>::alloc_array(buffer, array_size)?;
        for (index, val) in cv_signature
            .to_ne_bytes()
            .iter()
            .chain(identifier.iter())
            .enumerate()
        {
            sig_section.set_value_at(buffer, *val, index)?;
        }
        cv_record = sig_section.location();
    }

    let (file_path, _) = mapping
        .get_mapping_effective_name_and_path()
        .map_err(|e| SectionMappingsError::GetEffectivePathError(mapping.clone(), e))?;
    let name_header = write_string_to_location(buffer, &file_path)?;

    Ok(MDRawModule {
        base_of_image: mapping.start_address as u64,
        size_of_image: mapping.size as u32,
        cv_record,
        module_name_rva: name_header.rva,
        ..Default::default()
    })
}
