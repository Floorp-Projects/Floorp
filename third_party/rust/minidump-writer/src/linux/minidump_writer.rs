use crate::{
    dir_section::{DirSection, DumpBuf},
    linux::{
        app_memory::AppMemoryList,
        crash_context::CrashContext,
        dso_debug,
        errors::{InitError, WriterError},
        maps_reader::{MappingInfo, MappingList},
        ptrace_dumper::PtraceDumper,
        sections::*,
        thread_info::Pid,
    },
    mem_writer::{Buffer, MemoryArrayWriter, MemoryWriter, MemoryWriterError},
    minidump_format::*,
};
use std::io::{Seek, Write};

pub enum CrashingThreadContext {
    None,
    CrashContext(MDLocationDescriptor),
    CrashContextPlusAddress((MDLocationDescriptor, usize)),
}

pub struct MinidumpWriter {
    pub process_id: Pid,
    pub blamed_thread: Pid,
    pub minidump_size_limit: Option<u64>,
    pub skip_stacks_if_mapping_unreferenced: bool,
    pub principal_mapping_address: Option<usize>,
    pub user_mapping_list: MappingList,
    pub app_memory: AppMemoryList,
    pub memory_blocks: Vec<MDMemoryDescriptor>,
    pub principal_mapping: Option<MappingInfo>,
    pub sanitize_stack: bool,
    pub crash_context: Option<CrashContext>,
    pub crashing_thread_context: CrashingThreadContext,
}

// This doesn't work yet:
// https://github.com/rust-lang/rust/issues/43408
// fn write<T: Sized, P: AsRef<Path>>(path: P, value: T) -> Result<()> {
//     let mut file = std::fs::File::open(path)?;
//     let bytes: [u8; size_of::<T>()] = unsafe { transmute(value) };
//     file.write_all(&bytes)?;
//     Ok(())
// }

type Result<T> = std::result::Result<T, WriterError>;

impl MinidumpWriter {
    pub fn new(process: Pid, blamed_thread: Pid) -> Self {
        Self {
            process_id: process,
            blamed_thread,
            minidump_size_limit: None,
            skip_stacks_if_mapping_unreferenced: false,
            principal_mapping_address: None,
            user_mapping_list: MappingList::new(),
            app_memory: AppMemoryList::new(),
            memory_blocks: Vec::new(),
            principal_mapping: None,
            sanitize_stack: false,
            crash_context: None,
            crashing_thread_context: CrashingThreadContext::None,
        }
    }

    pub fn set_minidump_size_limit(&mut self, limit: u64) -> &mut Self {
        self.minidump_size_limit = Some(limit);
        self
    }

    pub fn set_user_mapping_list(&mut self, user_mapping_list: MappingList) -> &mut Self {
        self.user_mapping_list = user_mapping_list;
        self
    }

    pub fn set_principal_mapping_address(&mut self, principal_mapping_address: usize) -> &mut Self {
        self.principal_mapping_address = Some(principal_mapping_address);
        self
    }

    pub fn set_app_memory(&mut self, app_memory: AppMemoryList) -> &mut Self {
        self.app_memory = app_memory;
        self
    }

    pub fn set_crash_context(&mut self, crash_context: CrashContext) -> &mut Self {
        self.crash_context = Some(crash_context);
        self
    }

    pub fn skip_stacks_if_mapping_unreferenced(&mut self) -> &mut Self {
        self.skip_stacks_if_mapping_unreferenced = true; // Off by default
        self
    }

    pub fn sanitize_stack(&mut self) -> &mut Self {
        self.sanitize_stack = true; // Off by default
        self
    }

    /// Generates a minidump and writes to the destination provided. Returns the in-memory
    /// version of the minidump as well.
    pub fn dump(&mut self, destination: &mut (impl Write + Seek)) -> Result<Vec<u8>> {
        let mut dumper = PtraceDumper::new(self.process_id)?;
        dumper.suspend_threads()?;
        dumper.late_init()?;

        if self.skip_stacks_if_mapping_unreferenced {
            if let Some(address) = self.principal_mapping_address {
                self.principal_mapping = dumper.find_mapping_no_bias(address).cloned();
            }

            if !self.crash_thread_references_principal_mapping(&dumper) {
                return Err(InitError::PrincipalMappingNotReferenced.into());
            }
        }

        let mut buffer = Buffer::with_capacity(0);
        self.generate_dump(&mut buffer, &mut dumper, destination)?;

        // dumper would resume threads in drop() automatically,
        // but in case there is an error, we want to catch it
        dumper.resume_threads()?;

        Ok(buffer.into())
    }

    fn crash_thread_references_principal_mapping(&self, dumper: &PtraceDumper) -> bool {
        if self.crash_context.is_none() || self.principal_mapping.is_none() {
            return false;
        }

        let low_addr = self
            .principal_mapping
            .as_ref()
            .unwrap()
            .system_mapping_info
            .start_address;
        let high_addr = self
            .principal_mapping
            .as_ref()
            .unwrap()
            .system_mapping_info
            .end_address;

        let pc = self
            .crash_context
            .as_ref()
            .unwrap()
            .get_instruction_pointer();
        let stack_pointer = self.crash_context.as_ref().unwrap().get_stack_pointer();

        if pc >= low_addr && pc < high_addr {
            return true;
        }

        let (valid_stack_pointer, stack_len) = match dumper.get_stack_info(stack_pointer) {
            Ok(x) => x,
            Err(_) => {
                return false;
            }
        };

        let stack_copy = match PtraceDumper::copy_from_process(
            self.blamed_thread,
            valid_stack_pointer as *mut libc::c_void,
            stack_len,
        ) {
            Ok(x) => x,
            Err(_) => {
                return false;
            }
        };

        let sp_offset = stack_pointer.saturating_sub(valid_stack_pointer);
        self.principal_mapping
            .as_ref()
            .unwrap()
            .stack_has_pointer_to_mapping(&stack_copy, sp_offset)
    }

    fn generate_dump(
        &mut self,
        buffer: &mut DumpBuf,
        dumper: &mut PtraceDumper,
        destination: &mut (impl Write + Seek),
    ) -> Result<()> {
        // A minidump file contains a number of tagged streams. This is the number
        // of streams which we write.
        let num_writers = 15u32;

        let mut header_section = MemoryWriter::<MDRawHeader>::alloc(buffer)?;

        let mut dir_section = DirSection::new(buffer, num_writers, destination)?;

        let header = MDRawHeader {
            signature: MD_HEADER_SIGNATURE,
            version: MD_HEADER_VERSION,
            stream_count: num_writers,
            //   header.get()->stream_directory_rva = dir.position();
            stream_directory_rva: dir_section.position(),
            checksum: 0, /* Can be 0.  In fact, that's all that's
                          * been found in minidump files. */
            time_date_stamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)?
                .as_secs() as u32, // TODO: This is not Y2038 safe, but thats how its currently defined as
            flags: 0,
        };
        header_section.set_value(buffer, header)?;

        // Ensure the header gets flushed. If we crash somewhere below,
        // we should have a mostly-intact dump
        dir_section.write_to_file(buffer, None)?;

        let dirent = thread_list_stream::write(self, buffer, dumper)?;
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = mappings::write(self, buffer, dumper)?;
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        app_memory::write(self, buffer)?;
        // Write section to file
        dir_section.write_to_file(buffer, None)?;

        let dirent = memory_list_stream::write(self, buffer)?;
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = exception_stream::write(self, buffer)?;
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = systeminfo_stream::write(buffer)?;
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = memory_info_list_stream::write(self, buffer)?;
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, "/proc/cpuinfo") {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxCpuInfo as u32,
                location,
            },
            Err(_) => Default::default(),
        };
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/status", self.blamed_thread))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxProcStatus as u32,
                location,
            },
            Err(_) => Default::default(),
        };
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self
            .write_file(buffer, "/etc/lsb-release")
            .or_else(|_| self.write_file(buffer, "/etc/os-release"))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxLsbRelease as u32,
                location,
            },
            Err(_) => Default::default(),
        };
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/cmdline", self.blamed_thread))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxCmdLine as u32,
                location,
            },
            Err(_) => Default::default(),
        };
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/environ", self.blamed_thread))
        {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxEnviron as u32,
                location,
            },
            Err(_) => Default::default(),
        };
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/auxv", self.blamed_thread)) {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxAuxv as u32,
                location,
            },
            Err(_) => Default::default(),
        };
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = match self.write_file(buffer, &format!("/proc/{}/maps", self.blamed_thread)) {
            Ok(location) => MDRawDirectory {
                stream_type: MDStreamType::LinuxMaps as u32,
                location,
            },
            Err(_) => Default::default(),
        };
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = dso_debug::write_dso_debug_stream(buffer, self.process_id, &dumper.auxv)
            .unwrap_or_default();
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        let dirent = thread_names_stream::write(buffer, dumper)?;
        // Write section to file
        dir_section.write_to_file(buffer, Some(dirent))?;

        // If you add more directory entries, don't forget to update num_writers, above.
        Ok(())
    }

    #[allow(clippy::unused_self)]
    fn write_file(
        &self,
        buffer: &mut DumpBuf,
        filename: &str,
    ) -> std::result::Result<MDLocationDescriptor, MemoryWriterError> {
        let content = std::fs::read(filename)?;

        let section = MemoryArrayWriter::write_bytes(buffer, &content);
        Ok(section.location())
    }
}
