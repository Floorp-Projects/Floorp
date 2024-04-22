use super::*;
use crate::linux::maps_reader::MappingInfo;

/// Write information about the mappings in effect. Because we are using the
/// minidump format, the information about the mappings is pretty limited.
/// Because of this, we also include the full, unparsed, /proc/$x/maps file in
/// another stream in the file.
pub fn write(
    config: &mut MinidumpWriter,
    buffer: &mut DumpBuf,
    dumper: &mut PtraceDumper,
) -> Result<MDRawDirectory, errors::SectionMappingsError> {
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
        modules.push(module);
    }

    let list_header = MemoryWriter::<u32>::alloc_with_val(buffer, modules.len() as u32)?;

    let mut dirent = MDRawDirectory {
        stream_type: MDStreamType::ModuleListStream as u32,
        location: list_header.location(),
    };

    if !modules.is_empty() {
        let mapping_list = MemoryArrayWriter::<MDRawModule>::alloc_from_iter(buffer, modules)?;
        dirent.location.data_size += mapping_list.location().data_size;
    }

    Ok(dirent)
}

fn fill_raw_module(
    buffer: &mut DumpBuf,
    mapping: &MappingInfo,
    identifier: &[u8],
) -> Result<MDRawModule, errors::SectionMappingsError> {
    let cv_record = if identifier.is_empty() {
        // Just zeroes
        Default::default()
    } else {
        let cv_signature = crate::minidump_format::format::CvSignature::Elf as u32;
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
        sig_section.location()
    };

    let (file_path, _, so_version) = mapping
        .get_mapping_effective_path_name_and_version()
        .map_err(|e| errors::SectionMappingsError::GetEffectivePathError(mapping.clone(), e))?;
    let name_header = write_string_to_location(buffer, file_path.to_string_lossy().as_ref())?;

    let version_info = so_version.map_or(Default::default(), |sov| format::VS_FIXEDFILEINFO {
        signature: format::VS_FFI_SIGNATURE,
        struct_version: format::VS_FFI_STRUCVERSION,
        file_version_hi: sov.major,
        file_version_lo: sov.minor,
        product_version_hi: sov.patch,
        product_version_lo: sov.prerelease,
        ..Default::default()
    });

    let raw_module = MDRawModule {
        base_of_image: mapping.start_address as u64,
        size_of_image: mapping.size as u32,
        cv_record,
        module_name_rva: name_header.rva,
        version_info,
        ..Default::default()
    };

    Ok(raw_module)
}
