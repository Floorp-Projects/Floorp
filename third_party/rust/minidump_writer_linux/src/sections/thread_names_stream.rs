use crate::errors::SectionThreadNamesError;
use crate::linux_ptrace_dumper::LinuxPtraceDumper;
use crate::minidump_format::*;
use crate::minidump_writer::DumpBuf;
use crate::sections::write_string_to_location;
use crate::sections::{MemoryArrayWriter, MemoryWriter};
use std::convert::TryInto;

type Result<T> = std::result::Result<T, SectionThreadNamesError>;

pub fn write(buffer: &mut DumpBuf, dumper: &LinuxPtraceDumper) -> Result<MDRawDirectory> {
    // Only count threads that have a name
    let num_threads = dumper.threads.iter().filter(|t| t.name.is_some()).count();
    // Memory looks like this:
    // <num_threads><thread_1><thread_2>...

    let list_header = MemoryWriter::<u32>::alloc_with_val(buffer, num_threads as u32)?;

    let mut dirent = MDRawDirectory {
        stream_type: MDStreamType::ThreadNamesStream as u32,
        location: list_header.location(),
    };

    let mut thread_list = MemoryArrayWriter::<MDRawThreadName>::alloc_array(buffer, num_threads)?;
    dirent.location.data_size += thread_list.location().data_size;

    for (idx, item) in dumper.threads.iter().enumerate() {
        if let Some(name) = &item.name {
            let pos = write_string_to_location(buffer, &name)?;
            let thread = MDRawThreadName {
                thread_id: item.tid.try_into()?,
                thread_name_rva: pos.rva.into(),
            };
            thread_list.set_value_at(buffer, thread, idx)?;
        }
    }
    Ok(dirent)
}
