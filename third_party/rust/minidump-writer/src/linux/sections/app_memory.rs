use super::*;

/// Write application-provided memory regions.
pub fn write(
    config: &mut MinidumpWriter,
    buffer: &mut DumpBuf,
) -> Result<(), errors::SectionAppMemoryError> {
    for app_memory in &config.app_memory {
        let data_copy = PtraceDumper::copy_from_process(
            config.blamed_thread,
            app_memory.ptr as *mut libc::c_void,
            app_memory.length,
        )?;

        let section = MemoryArrayWriter::write_bytes(buffer, &data_copy);
        let desc = MDMemoryDescriptor {
            start_of_memory_range: app_memory.ptr as u64,
            memory: section.location(),
        };
        config.memory_blocks.push(desc);
    }
    Ok(())
}
