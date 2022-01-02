use crate::errors::SectionAppMemoryError;
use crate::linux_ptrace_dumper::LinuxPtraceDumper;
use crate::minidump_format::*;
use crate::minidump_writer::{DumpBuf, MinidumpWriter};
use crate::sections::MemoryArrayWriter;

type Result<T> = std::result::Result<T, SectionAppMemoryError>;

/// Write application-provided memory regions.
pub fn write(config: &mut MinidumpWriter, buffer: &mut DumpBuf) -> Result<()> {
    for app_memory in &config.app_memory {
        let data_copy = LinuxPtraceDumper::copy_from_process(
            config.blamed_thread,
            app_memory.ptr as *mut libc::c_void,
            app_memory.length,
        )?;

        let section = MemoryArrayWriter::<u8>::alloc_from_array(buffer, &data_copy)?;
        let desc = MDMemoryDescriptor {
            start_of_memory_range: app_memory.ptr as u64,
            memory: section.location(),
        };
        config.memory_blocks.push(desc);
    }
    Ok(())
}
