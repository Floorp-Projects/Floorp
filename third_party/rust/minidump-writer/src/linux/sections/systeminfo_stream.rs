use super::*;
use crate::linux::dumper_cpu_info as dci;

pub fn write(buffer: &mut DumpBuf) -> Result<MDRawDirectory, errors::SectionSystemInfoError> {
    let mut info_section = MemoryWriter::<MDRawSystemInfo>::alloc(buffer)?;
    let dirent = MDRawDirectory {
        stream_type: MDStreamType::SystemInfoStream as u32,
        location: info_section.location(),
    };

    let (platform_id, os_version) = dci::os_information();
    let os_version_loc = write_string_to_location(buffer, &os_version)?;

    // SAFETY: POD
    let mut info = unsafe { std::mem::zeroed::<MDRawSystemInfo>() };
    info.platform_id = platform_id as u32;
    info.csd_version_rva = os_version_loc.rva;

    dci::write_cpu_information(&mut info)?;

    info_section.set_value(buffer, info)?;
    Ok(dirent)
}
