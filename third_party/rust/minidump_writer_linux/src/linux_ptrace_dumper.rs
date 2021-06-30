// use libc::c_void;
#[cfg(target_os = "android")]
use crate::android::late_process_mappings;
use crate::auxv_reader::{AuxvType, ProcfsAuxvIter};
use crate::errors::{DumperError, InitError, ThreadInfoError};
use crate::maps_reader::{MappingInfo, MappingInfoParsingResult, DELETED_SUFFIX};
use crate::minidump_format::MDGUID;
use crate::thread_info::{Pid, ThreadInfo};
use crate::LINUX_GATE_LIBRARY_NAME;
use goblin::elf;
use nix::errno::Errno;
use nix::sys::{ptrace, wait};
use std::collections::HashMap;
use std::convert::TryInto;
use std::ffi::c_void;
use std::io::{BufRead, BufReader};
use std::path;
use std::result::Result;

#[derive(Debug, Clone)]
pub struct Thread {
    pub tid: Pid,
    pub name: Option<String>,
}

#[derive(Debug)]
pub struct LinuxPtraceDumper {
    pub pid: Pid,
    threads_suspended: bool,
    pub threads: Vec<Thread>,
    pub auxv: HashMap<AuxvType, AuxvType>,
    pub mappings: Vec<MappingInfo>,
}

#[cfg(target_pointer_width = "32")]
pub const AT_SYSINFO_EHDR: u32 = 33;
#[cfg(target_pointer_width = "64")]
pub const AT_SYSINFO_EHDR: u64 = 33;

impl Drop for LinuxPtraceDumper {
    fn drop(&mut self) {
        // Always try to resume all threads (e.g. in case of error)
        let _ = self.resume_threads();
    }
}

impl LinuxPtraceDumper {
    /// Constructs a dumper for extracting information of a given process
    /// with a process ID of |pid|.
    pub fn new(pid: Pid) -> Result<Self, InitError> {
        let mut dumper = LinuxPtraceDumper {
            pid,
            threads_suspended: false,
            threads: Vec::new(),
            auxv: HashMap::new(),
            mappings: Vec::new(),
        };
        dumper.init()?;
        Ok(dumper)
    }

    // TODO: late_init for chromeos and android
    pub fn init(&mut self) -> Result<(), InitError> {
        self.read_auxv()?;
        self.enumerate_threads()?;
        self.enumerate_mappings()?;
        Ok(())
    }

    pub fn late_init(&mut self) -> Result<(), InitError> {
        #[cfg(target_os = "android")]
        {
            late_process_mappings(self.pid, &mut self.mappings)?;
        }
        Ok(())
    }

    /// Copies content of |length| bytes from a given process |child|,
    /// starting from |src|, into |dest|. This method uses ptrace to extract
    /// the content from the target process. Always returns true.
    pub fn copy_from_process(
        child: Pid,
        src: *mut c_void,
        num_of_bytes: usize,
    ) -> Result<Vec<u8>, DumperError> {
        use DumperError::CopyFromProcessError as CFPE;
        let pid = nix::unistd::Pid::from_raw(child);
        let mut res = Vec::new();
        let mut idx = 0usize;
        while idx < num_of_bytes {
            let word = ptrace::read(pid, (src as usize + idx) as *mut c_void)
                .map_err(|e| CFPE(child, src as usize, idx, num_of_bytes, e))?;
            res.append(&mut word.to_ne_bytes().to_vec());
            idx += std::mem::size_of::<libc::c_long>();
        }
        Ok(res)
    }

    /// Suspends a thread by attaching to it.
    pub fn suspend_thread(child: Pid) -> Result<(), DumperError> {
        use DumperError::PtraceAttachError as AttachErr;
        use DumperError::PtraceDetachError as DetachErr;

        let pid = nix::unistd::Pid::from_raw(child);
        // This may fail if the thread has just died or debugged.
        ptrace::attach(pid).map_err(|e| AttachErr(child, e))?;
        loop {
            match wait::waitpid(pid, Some(wait::WaitPidFlag::__WALL)) {
                Ok(_) => break,
                Err(e @ nix::Error::Sys(Errno::EINTR)) => {
                    ptrace::detach(pid).map_err(|e| DetachErr(child, e))?;
                    return Err(DumperError::WaitPidError(child, e));
                }
                Err(_) => continue,
            }
        }
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        {
            // On x86, the stack pointer is NULL or -1, when executing trusted code in
            // the seccomp sandbox. Not only does this cause difficulties down the line
            // when trying to dump the thread's stack, it also results in the minidumps
            // containing information about the trusted threads. This information is
            // generally completely meaningless and just pollutes the minidumps.
            // We thus test the stack pointer and exclude any threads that are part of
            // the seccomp sandbox's trusted code.
            let skip_thread;
            let regs = ptrace::getregs(pid);
            if let Ok(regs) = regs {
                #[cfg(target_arch = "x86_64")]
                {
                    skip_thread = regs.rsp == 0;
                }
                #[cfg(target_arch = "x86")]
                {
                    skip_thread = regs.esp == 0;
                }
            } else {
                skip_thread = true;
            }
            if skip_thread {
                ptrace::detach(pid).map_err(|e| DetachErr(child, e))?;
                return Err(DumperError::DetachSkippedThread(child));
            }
        }
        Ok(())
    }

    /// Resumes a thread by detaching from it.
    pub fn resume_thread(child: Pid) -> Result<(), DumperError> {
        use DumperError::PtraceDetachError as DetachErr;
        let pid = nix::unistd::Pid::from_raw(child);
        ptrace::detach(pid).map_err(|e| DetachErr(child, e))?;
        Ok(())
    }

    pub fn suspend_threads(&mut self) -> Result<(), DumperError> {
        // Iterate over all threads and try to suspend them.
        // If the thread either disappeared before we could attach to it, or if
        // it was part of the seccomp sandbox's trusted code, it is OK to
        // silently drop it from the minidump.
        self.threads.retain(|x| Self::suspend_thread(x.tid).is_ok());

        if self.threads.is_empty() {
            Err(DumperError::SuspendNoThreadsLeft)
        } else {
            self.threads_suspended = true;
            Ok(())
        }
    }

    pub fn resume_threads(&mut self) -> Result<(), DumperError> {
        let mut result = Ok(());
        if self.threads_suspended {
            for thread in &self.threads {
                match Self::resume_thread(thread.tid) {
                    Ok(_) => {}
                    x => {
                        result = x;
                    }
                }
            }
        }
        self.threads_suspended = false;
        result
    }

    /// Parse /proc/$pid/task to list all the threads of the process identified by
    /// pid.
    fn enumerate_threads(&mut self) -> Result<(), InitError> {
        let pid = self.pid;
        let filename = format!("/proc/{}/task", pid);
        let task_path = path::PathBuf::from(&filename);
        if task_path.is_dir() {
            std::fs::read_dir(task_path)
                .map_err(|e| InitError::IOError(filename, e))?
                .into_iter()
                .filter_map(|entry| entry.ok()) // Filter out bad entries
                .filter_map(|entry| {
                    entry
                        .file_name() // Parse name to Pid, filter out those that are unparsable
                        .to_str()
                        .and_then(|name| name.parse::<Pid>().ok())
                })
                .map(|tid| {
                    // Read the thread-name (if there is any)
                    let name = std::fs::read_to_string(format!("/proc/{}/task/{}/comm", pid, tid))
                        // NOTE: This is a bit wasteful as it does two allocations in order to trim, but leaving it for now
                        .map(|s| s.trim_end().to_string())
                        .ok();
                    (tid, name)
                })
                .for_each(|(tid, name)| self.threads.push(Thread { tid, name }))
        }
        Ok(())
    }

    fn read_auxv(&mut self) -> Result<(), InitError> {
        let filename = format!("/proc/{}/auxv", self.pid);
        let auxv_path = path::PathBuf::from(&filename);
        let auxv_file =
            std::fs::File::open(auxv_path).map_err(|e| InitError::IOError(filename, e))?;
        let input = BufReader::new(auxv_file);
        let reader = ProcfsAuxvIter::new(input);
        self.auxv = reader
            .filter_map(Result::ok)
            .map(|x| (x.key, x.value))
            .collect();

        if self.auxv.is_empty() {
            Err(InitError::NoAuxvEntryFound(self.pid))
        } else {
            Ok(())
        }
    }

    fn enumerate_mappings(&mut self) -> Result<(), InitError> {
        // linux_gate_loc is the beginning of the kernel's mapping of
        // linux-gate.so in the process.  It doesn't actually show up in the
        // maps list as a filename, but it can be found using the AT_SYSINFO_EHDR
        // aux vector entry, which gives the information necessary to special
        // case its entry when creating the list of mappings.
        // See http://www.trilithium.com/johan/2005/08/linux-gate/ for more
        // information.
        let linux_gate_loc = *self.auxv.get(&AT_SYSINFO_EHDR).unwrap_or(&0);
        // Although the initial executable is usually the first mapping, it's not
        // guaranteed (see http://crosbug.com/25355); therefore, try to use the
        // actual entry point to find the mapping.
        let at_entry;
        #[cfg(target_arch = "arm")]
        {
            at_entry = 9;
        }
        #[cfg(not(target_arch = "arm"))]
        {
            at_entry = libc::AT_ENTRY;
        }

        let entry_point_loc = *self.auxv.get(&at_entry).unwrap_or(&0);
        let filename = format!("/proc/{}/maps", self.pid);
        let errmap = |e| InitError::IOError(filename.clone(), e);
        let maps_path = path::PathBuf::from(&filename);
        let maps_file = std::fs::File::open(maps_path).map_err(errmap)?;

        for line in BufReader::new(maps_file).lines() {
            // /proc/<pid>/maps looks like this
            // 7fe34a863000-7fe34a864000 rw-p 00009000 00:31 4746408                    /usr/lib64/libogg.so.0.8.4
            let line = line.map_err(errmap)?;
            match MappingInfo::parse_from_line(&line, linux_gate_loc, self.mappings.last_mut()) {
                Ok(MappingInfoParsingResult::Success(map)) => self.mappings.push(map),
                Ok(MappingInfoParsingResult::SkipLine) => continue,
                Err(_) => continue,
            }
        }

        if entry_point_loc != 0 {
            let mut swap_idx = None;
            for (idx, module) in self.mappings.iter().enumerate() {
                // If this module contains the entry-point, and it's not already the first
                // one, then we need to make it be first.  This is because the minidump
                // format assumes the first module is the one that corresponds to the main
                // executable (as codified in
                // processor/minidump.cc:MinidumpModuleList::GetMainModule()).
                if entry_point_loc >= module.start_address.try_into().unwrap()
                    && entry_point_loc < (module.start_address + module.size).try_into().unwrap()
                {
                    swap_idx = Some(idx);
                    break;
                }
            }
            if let Some(idx) = swap_idx {
                self.mappings.swap(0, idx);
            }
        }
        Ok(())
    }

    /// Read thread info from /proc/$pid/status.
    /// Fill out the |tgid|, |ppid| and |pid| members of |info|. If unavailable,
    /// these members are set to -1. Returns true if all three members are
    /// available.
    pub fn get_thread_info_by_index(&self, index: usize) -> Result<ThreadInfo, ThreadInfoError> {
        if index > self.threads.len() {
            return Err(ThreadInfoError::IndexOutOfBounds(index, self.threads.len()));
        }

        ThreadInfo::create(self.pid, self.threads[index].tid)
    }

    // Get information about the stack, given the stack pointer. We don't try to
    // walk the stack since we might not have all the information needed to do
    // unwind. So we just grab, up to, 32k of stack.
    pub fn get_stack_info(&self, int_stack_pointer: usize) -> Result<(usize, usize), DumperError> {
        // Move the stack pointer to the bottom of the page that it's in.
        // NOTE: original code uses getpagesize(), which a) isn't there in Rust and
        //       b) shouldn't be used, as its not portable (see man getpagesize)
        let page_size = nix::unistd::sysconf(nix::unistd::SysconfVar::PAGE_SIZE)?
            .expect("page size apparently unlimited: doesn't make sense.");
        let stack_pointer = int_stack_pointer & !(page_size as usize - 1);

        // The number of bytes of stack which we try to capture.
        let stack_to_capture = 32 * 1024;

        let mapping = self
            .find_mapping(stack_pointer)
            .ok_or(DumperError::NoStackPointerMapping)?;
        let offset = stack_pointer - mapping.start_address;
        let distance_to_end = mapping.size - offset;
        let stack_len = std::cmp::min(distance_to_end, stack_to_capture);

        Ok((stack_pointer, stack_len))
    }

    pub fn sanitize_stack_copy(
        &self,
        stack_copy: &mut [u8],
        stack_pointer: usize,
        sp_offset: usize,
    ) -> Result<(), DumperError> {
        // We optimize the search for containing mappings in three ways:
        // 1) We expect that pointers into the stack mapping will be common, so
        //    we cache that address range.
        // 2) The last referenced mapping is a reasonable predictor for the next
        //    referenced mapping, so we test that first.
        // 3) We precompute a bitfield based upon bits 32:32-n of the start and
        //    stop addresses, and use that to short circuit any values that can
        //    not be pointers. (n=11)
        let defaced;
        #[cfg(target_pointer_width = "64")]
        {
            defaced = 0x0defaced0defacedusize.to_ne_bytes()
        }
        #[cfg(target_pointer_width = "32")]
        {
            defaced = 0x0defacedusize.to_ne_bytes()
        };
        // the bitfield length is 2^test_bits long.
        let test_bits = 11;
        // byte length of the corresponding array.
        let array_size = 1 << (test_bits - 3);
        let array_mask = array_size - 1;
        // The amount to right shift pointers by. This captures the top bits
        // on 32 bit architectures. On 64 bit architectures this would be
        // uninformative so we take the same range of bits.
        let shift = 32 - 11;
        // let MappingInfo* last_hit_mapping = nullptr;
        // let MappingInfo* hit_mapping = nullptr;
        let stack_mapping = self.find_mapping_no_bias(stack_pointer);
        let mut last_hit_mapping: Option<&MappingInfo> = None;
        // The magnitude below which integers are considered to be to be
        // 'small', and not constitute a PII risk. These are included to
        // avoid eliding useful register values.
        let small_int_magnitude: isize = 4096;

        let mut could_hit_mapping = vec![0; array_size];
        // Initialize the bitfield such that if the (pointer >> shift)'th
        // bit, modulo the bitfield size, is not set then there does not
        // exist a mapping in mappings that would contain that pointer.
        for mapping in &self.mappings {
            if !mapping.executable {
                continue;
            }
            // For each mapping, work out the (unmodulo'ed) range of bits to
            // set.
            let mut start = mapping.start_address;
            let mut end = start + mapping.size;
            start >>= shift;
            end >>= shift;
            for bit in start..=end {
                // Set each bit in the range, applying the modulus.
                could_hit_mapping[(bit >> 3) & array_mask] |= 1 << (bit & 7);
            }
        }

        // Zero memory that is below the current stack pointer.
        let offset =
            (sp_offset + std::mem::size_of::<usize>() - 1) & !(std::mem::size_of::<usize>() - 1);
        for x in &mut stack_copy[0..offset] {
            *x = 0;
        }
        let mut chunks = stack_copy[offset..].chunks_exact_mut(std::mem::size_of::<usize>());

        // Apply sanitization to each complete pointer-aligned word in the
        // stack.
        for sp in &mut chunks {
            let addr = usize::from_ne_bytes(sp.to_vec().as_slice().try_into()?);
            let addr_signed = isize::from_ne_bytes(sp.to_vec().as_slice().try_into()?);

            if addr <= small_int_magnitude as usize && addr_signed >= -small_int_magnitude {
                continue;
            }

            if let Some(stack_map) = stack_mapping {
                if stack_map.contains_address(addr) {
                    continue;
                }
            }
            if let Some(last_hit) = last_hit_mapping {
                if last_hit.contains_address(addr) {
                    continue;
                }
            }

            let test = addr >> shift;
            if could_hit_mapping[(test >> 3) & array_mask] & (1 << (test & 7)) != 0 {
                if let Some(hit_mapping) = self.find_mapping_no_bias(addr) {
                    if hit_mapping.executable {
                        last_hit_mapping = Some(hit_mapping);
                        continue;
                    }
                }
            }
            sp.copy_from_slice(&defaced);
        }
        // Zero any partial word at the top of the stack, if alignment is
        // such that that is required.
        for sp in chunks.into_remainder() {
            *sp = 0;
        }
        Ok(())
    }

    // Find the mapping which the given memory address falls in.
    pub fn find_mapping(&self, address: usize) -> Option<&MappingInfo> {
        for map in &self.mappings {
            if address >= map.start_address && address - map.start_address < map.size {
                return Some(&map);
            }
        }
        None
    }

    // Find the mapping which the given memory address falls in. Uses the
    // unadjusted mapping address range from the kernel, rather than the
    // biased range.
    pub fn find_mapping_no_bias(&self, address: usize) -> Option<&MappingInfo> {
        for map in &self.mappings {
            if address >= map.system_mapping_info.start_address
                && address < map.system_mapping_info.end_address
            {
                return Some(&map);
            }
        }
        None
    }

    fn parse_build_id<'data>(
        elf_obj: &elf::Elf<'data>,
        mem_slice: &'data [u8],
    ) -> Option<&'data [u8]> {
        if let Some(mut notes) = elf_obj.iter_note_headers(mem_slice) {
            while let Some(Ok(note)) = notes.next() {
                if note.n_type == elf::note::NT_GNU_BUILD_ID {
                    return Some(note.desc);
                }
            }
        }
        if let Some(mut notes) = elf_obj.iter_note_sections(mem_slice, Some(".note.gnu.build-id")) {
            while let Some(Ok(note)) = notes.next() {
                if note.n_type == elf::note::NT_GNU_BUILD_ID {
                    return Some(note.desc);
                }
            }
        }
        None
    }

    pub fn elf_file_identifier_from_mapped_file(mem_slice: &[u8]) -> Result<Vec<u8>, DumperError> {
        let elf_obj = elf::Elf::parse(mem_slice)?;
        match Self::parse_build_id(&elf_obj, mem_slice) {
            // Look for a build id note first.
            Some(build_id) => Ok(build_id.to_vec()),
            // Fall back on hashing the first page of the text section.
            None => {
                // Attempt to locate the .text section of an ELF binary and generate
                // a simple hash by XORing the first page worth of bytes into |result|.
                for section in elf_obj.section_headers {
                    if section.sh_type != elf::section_header::SHT_PROGBITS {
                        continue;
                    }
                    if section.sh_flags & u64::from(elf::section_header::SHF_ALLOC) != 0 {
                        if section.sh_flags & u64::from(elf::section_header::SHF_EXECINSTR) != 0 {
                            let text_section = &mem_slice[section.sh_offset as usize..]
                                [..section.sh_size as usize];
                            // Only provide mem::size_of(MDGUID) bytes to keep identifiers produced by this
                            // function backwards-compatible.
                            let max_len = std::cmp::min(text_section.len(), 4096);
                            let mut result = vec![0u8; std::mem::size_of::<MDGUID>()];
                            let mut offset = 0;
                            while offset < max_len {
                                for idx in 0..std::mem::size_of::<MDGUID>() {
                                    if offset + idx >= text_section.len() {
                                        break;
                                    }
                                    result[idx] ^= text_section[offset + idx];
                                }
                                offset += std::mem::size_of::<MDGUID>();
                            }
                            return Ok(result);
                        }
                    }
                }
                Err(DumperError::NoBuildIDFound)
            }
        }
    }

    pub fn elf_identifier_for_mapping_index(&mut self, idx: usize) -> Result<Vec<u8>, DumperError> {
        assert!(idx < self.mappings.len());

        Self::elf_identifier_for_mapping(&mut self.mappings[idx], self.pid)
    }

    pub fn elf_identifier_for_mapping(
        mapping: &mut MappingInfo,
        pid: Pid,
    ) -> Result<Vec<u8>, DumperError> {
        if !MappingInfo::is_mapped_file_safe_to_open(&mapping.name) {
            return Err(DumperError::NotSafeToOpenMapping(
                mapping.name.clone().unwrap_or_default(),
            ));
        }

        // Special-case linux-gate because it's not a real file.
        if mapping.name.as_deref() == Some(LINUX_GATE_LIBRARY_NAME) {
            if pid == std::process::id().try_into()? {
                let mem_slice = unsafe {
                    std::slice::from_raw_parts(mapping.start_address as *const u8, mapping.size)
                };
                return Self::elf_file_identifier_from_mapped_file(mem_slice);
            } else {
                let mem_slice = Self::copy_from_process(
                    pid,
                    mapping.start_address as *mut libc::c_void,
                    mapping.size,
                )?;
                return Self::elf_file_identifier_from_mapped_file(&mem_slice);
            }
        }
        let new_name = MappingInfo::handle_deleted_file_in_mapping(
            &mapping.name.as_ref().unwrap_or(&String::new()),
            pid,
        )?;

        let mem_slice = MappingInfo::get_mmap(&Some(new_name.clone()), mapping.offset)?;
        let build_id = Self::elf_file_identifier_from_mapped_file(&mem_slice)?;

        // This means we switched from "/my/binary" to "/proc/1234/exe", because /my/binary
        // was deleted and thus has a "/my/binary (deleted)" entry. We found the mapping anyway
        // so we remove the "(deleted)".
        if let Some(old_name) = &mapping.name {
            if &new_name != old_name {
                mapping.name = Some(
                    old_name
                        .trim_end_matches(DELETED_SUFFIX)
                        .trim_end()
                        .to_string(),
                );
            }
        }
        Ok(build_id)
    }
}
