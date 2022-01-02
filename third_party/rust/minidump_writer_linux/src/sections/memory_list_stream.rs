use crate::errors::SectionMemListError;
use crate::minidump_format::*;
use crate::minidump_writer::{DumpBuf, MinidumpWriter};
use crate::sections::{MemoryArrayWriter, MemoryWriter};

type Result<T> = std::result::Result<T, SectionMemListError>;

pub fn write(config: &mut MinidumpWriter, buffer: &mut DumpBuf) -> Result<MDRawDirectory> {
    let list_header =
        MemoryWriter::<u32>::alloc_with_val(buffer, config.memory_blocks.len() as u32)?;

    let mut dirent = MDRawDirectory {
        stream_type: MDStreamType::MemoryListStream as u32,
        location: list_header.location(),
    };

    let block_list =
        MemoryArrayWriter::<MDMemoryDescriptor>::alloc_from_array(buffer, &config.memory_blocks)?;

    dirent.location.data_size += block_list.location().data_size;

    Ok(dirent)
}
