use crate::linux_ptrace_dumper::LinuxPtraceDumper;
use crate::maps_reader::MappingInfo;
use crate::minidump_format::*;
use crate::minidump_writer::{DumpBuf, MinidumpWriter};
use crate::sections::{write_string_to_location, MemoryArrayWriter, MemoryWriter};
use crate::Result;

/// Write information about the mappings in effect. Because we are using the
/// minidump format, the information about the mappings is pretty limited.
/// Because of this, we also include the full, unparsed, /proc/$x/maps file in
/// another stream in the file.
pub fn write(
    config: &mut MinidumpWriter,
    buffer: &mut DumpBuf,
    dumper: &mut LinuxPtraceDumper,
) -> Result<MDRawDirectory> {
    let mut num_output_mappings = config.user_mapping_list.len();

    for mapping in &dumper.mappings {
        // If the mapping is uninteresting, or if
        // there is caller-provided information about this mapping
        // in the user_mapping_list list, skip it
        if mapping.is_interesting() && !mapping.is_contained_in(&config.user_mapping_list) {
            num_output_mappings += 1;
        }
    }

    let list_header = MemoryWriter::<u32>::alloc_with_val(buffer, num_output_mappings as u32)?;

    let mut dirent = MDRawDirectory {
        stream_type: MDStreamType::ModuleListStream as u32,
        location: list_header.location(),
    };

    // In case of num_output_mappings == 0, this call doesn't allocate any memory in the buffer
    let mut mapping_list =
        MemoryArrayWriter::<MDRawModule>::alloc_array(buffer, num_output_mappings)?;
    dirent.location.data_size += mapping_list.location().data_size;

    // First write all the mappings from the dumper
    let mut idx = 0;
    for map_idx in 0..dumper.mappings.len() {
        if !dumper.mappings[map_idx].is_interesting()
            || dumper.mappings[map_idx].is_contained_in(&config.user_mapping_list)
        {
            continue;
        }
        // Note: elf_identifier_for_mapping_index() can manipulate the |mapping.name|.
        let identifier = dumper
            .elf_identifier_for_mapping_index(map_idx)
            .unwrap_or(Default::default());
        let module = fill_raw_module(buffer, &dumper.mappings[map_idx], &identifier)?;
        mapping_list.set_value_at(buffer, module, idx)?;
        idx += 1;
    }

    // Next write all the mappings provided by the caller
    for user in &config.user_mapping_list {
        // GUID was provided by caller.
        let module = fill_raw_module(buffer, &user.mapping, &user.identifier)?;
        mapping_list.set_value_at(buffer, module, idx)?;
        idx += 1;
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

    let (file_path, _) = mapping.get_mapping_effective_name_and_path()?;
    let name_header = write_string_to_location(buffer, &file_path)?;

    Ok(MDRawModule {
        base_of_image: mapping.start_address as u64,
        size_of_image: mapping.size as u32,
        cv_record,
        module_name_rva: name_header.rva,
        ..Default::default()
    })
}
