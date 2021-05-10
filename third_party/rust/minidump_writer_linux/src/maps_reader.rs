use crate::auxv_reader::AuxvType;
use crate::errors::MapsReaderError;
use crate::thread_info::Pid;
use byteorder::{NativeEndian, ReadBytesExt};
use goblin::elf;
use memmap2::{Mmap, MmapOptions};
use std::convert::TryInto;
use std::fs::File;
use std::mem::size_of;
use std::path::PathBuf;

pub const LINUX_GATE_LIBRARY_NAME: &'static str = "linux-gate.so";
pub const DELETED_SUFFIX: &'static str = " (deleted)";
pub const RESERVED_FLAGS: &'static str = "---p";

type Result<T> = std::result::Result<T, MapsReaderError>;

#[derive(Debug, PartialEq, Clone)]
pub struct SystemMappingInfo {
    pub start_address: usize,
    pub end_address: usize,
}

// One of these is produced for each mapping in the process (i.e. line in
// /proc/$x/maps).
#[derive(Debug, PartialEq, Clone)]
pub struct MappingInfo {
    // On Android, relocation packing can mean that the reported start
    // address of the mapping must be adjusted by a bias in order to
    // compensate for the compression of the relocation section. The
    // following two members hold (after LateInit) the adjusted mapping
    // range. See crbug.com/606972 for more information.
    pub start_address: usize,
    pub size: usize,
    // When Android relocation packing causes |start_addr| and |size| to
    // be modified with a load bias, we need to remember the unbiased
    // address range. The following structure holds the original mapping
    // address range as reported by the operating system.
    pub system_mapping_info: SystemMappingInfo,
    pub offset: usize,    // offset into the backed file.
    pub executable: bool, // true if the mapping has the execute bit set.
    pub name: Option<String>,
    // pub elf_obj: Option<elf::Elf>,
}

#[derive(Debug)]
pub struct MappingEntry {
    pub mapping: MappingInfo,
    pub identifier: Vec<u8>,
}

// A list of <MappingInfo, GUID>
pub type MappingList = Vec<MappingEntry>;

#[derive(Debug)]
pub enum MappingInfoParsingResult {
    SkipLine,
    Success(MappingInfo),
}

fn is_mapping_a_path(pathname: Option<&str>) -> bool {
    match pathname {
        Some(x) => x.contains("/"),
        None => false,
    }
}

impl MappingInfo {
    pub fn parse_from_line(
        line: &str,
        linux_gate_loc: AuxvType,
        last_mapping: Option<&mut MappingInfo>,
    ) -> Result<MappingInfoParsingResult> {
        let mut last_whitespace = false;

        // There is no `line.splitn_whitespace(6)`, so we have to do it somewhat manually
        // Split at the first whitespace, trim of the rest.
        let mut splits = line
            .trim()
            .splitn(6, |c: char| {
                if c.is_whitespace() {
                    if last_whitespace {
                        return false;
                    }
                    last_whitespace = true;
                    true
                } else {
                    last_whitespace = false;
                    false
                }
            })
            .map(str::trim);

        let address = splits
            .next()
            .ok_or(MapsReaderError::MapEntryMalformed("address"))?;
        let perms = splits
            .next()
            .ok_or(MapsReaderError::MapEntryMalformed("permissions"))?;
        let mut offset = usize::from_str_radix(
            splits
                .next()
                .ok_or(MapsReaderError::MapEntryMalformed("offset"))?,
            16,
        )?;
        let _dev = splits
            .next()
            .ok_or(MapsReaderError::MapEntryMalformed("dev"))?;
        let _inode = splits
            .next()
            .ok_or(MapsReaderError::MapEntryMalformed("inode"))?;
        let mut pathname = splits.next(); // Optional

        // Due to our ugly `splitn_whitespace()` hack from above, we might have
        // only trailing whitespaces as the name, so we it might still be "Some()"
        if let Some(x) = pathname {
            if x.is_empty() {
                pathname = None;
            }
        }

        let mut addresses = address.split('-');
        let start_address = usize::from_str_radix(addresses.next().unwrap(), 16)?;
        let end_address = usize::from_str_radix(addresses.next().unwrap(), 16)?;

        let executable = perms.contains("x");

        // Only copy name if the name is a valid path name, or if
        // it's the VDSO image.
        let is_path = is_mapping_a_path(pathname);

        if !is_path && linux_gate_loc != 0 && start_address == linux_gate_loc.try_into()? {
            pathname = Some(LINUX_GATE_LIBRARY_NAME);
            offset = 0;
        }

        match (pathname, last_mapping) {
            (Some(_name), Some(module)) => {
                // Merge adjacent mappings into one module, assuming they're a single
                // library mapped by the dynamic linker.
                if (start_address == module.start_address + module.size)
                    && (pathname == module.name.as_deref())
                {
                    module.system_mapping_info.end_address = end_address;
                    module.size = end_address - module.start_address;
                    module.executable |= executable;
                    return Ok(MappingInfoParsingResult::SkipLine);
                }
            }
            (None, Some(module)) => {
                // Also merge mappings that result from address ranges that the
                // linker reserved but which a loaded library did not use. These
                // appear as an anonymous private mapping with no access flags set
                // and which directly follow an executable mapping.
                let module_end_address = module.start_address + module.size;
                if (start_address == module_end_address)
                    && module.executable
                    && is_mapping_a_path(module.name.as_deref())
                    && (offset == 0 || offset == module_end_address)
                    && perms == RESERVED_FLAGS
                {
                    module.size = end_address - module.start_address;
                    return Ok(MappingInfoParsingResult::SkipLine);
                }
            }
            _ => (),
        }

        let name = pathname.map(ToOwned::to_owned);

        let info = MappingInfo {
            start_address,
            size: end_address - start_address,
            system_mapping_info: SystemMappingInfo {
                start_address,
                end_address,
            },
            offset,
            executable,
            name,
            // elf_obj,
        };

        Ok(MappingInfoParsingResult::Success(info))
    }

    pub fn get_mmap(name: &Option<String>, offset: usize) -> Result<Mmap> {
        if !MappingInfo::is_mapped_file_safe_to_open(&name) {
            return Err(MapsReaderError::NotSafeToOpenMapping(
                name.clone().unwrap_or_default(),
            ));
        }

        // Not doing this as root_prefix is always "" at the moment
        //   if (!dumper.GetMappingAbsolutePath(mapping, filename))
        let filename = name.clone().unwrap_or(String::new());
        let mapped_file = unsafe {
            MmapOptions::new()
                .offset(offset.try_into()?) // try_into() to work for both 32 and 64 bit
                .map(&File::open(filename)?)?
        };

        if mapped_file.is_empty() || mapped_file.len() < elf::header::SELFMAG {
            return Err(MapsReaderError::MmapSanityCheckFailed);
        }
        Ok(mapped_file)
    }

    pub fn handle_deleted_file_in_mapping(path: &str, pid: Pid) -> Result<String> {
        // Check for ' (deleted)' in |path|.
        // |path| has to be at least as long as "/x (deleted)".
        if !path.ends_with(DELETED_SUFFIX) {
            return Ok(path.to_string());
        }

        // Check |path| against the /proc/pid/exe 'symlink'.
        let exe_link = format!("/proc/{}/exe", pid);
        let link_path = std::fs::read_link(&exe_link)?;

        // This is a no-op for now (until we want to support root_prefix for chroot-envs)
        // if (!GetMappingAbsolutePath(new_mapping, new_path))
        //   return false;

        if link_path != PathBuf::from(path) {
            return Err(MapsReaderError::SymlinkError(
                PathBuf::from(path),
                link_path,
            ));
        }

        // Check to see if someone actually named their executable 'foo (deleted)'.

        // This makes currently no sense, as exe_link == new_path
        // if let (Some(exe_stat), Some(new_path_stat)) = (nix::stat::stat(exe_link), nix::stat::stat(new_path)) {
        //     if exe_stat.st_dev == new_path_stat.st_dev && exe_stat.st_ino == new_path_stat.st_ino {
        //         return Err("".into());
        //     }
        // }
        return Ok(exe_link);
    }

    pub fn stack_has_pointer_to_mapping(&self, stack_copy: &[u8], sp_offset: usize) -> bool {
        // Loop over all stack words that would have been on the stack in
        // the target process (i.e. are word aligned, and at addresses >=
        // the stack pointer).  Regardless of the alignment of |stack_copy|,
        // the memory starting at |stack_copy| + |offset| represents an
        // aligned word in the target process.
        let low_addr = self.system_mapping_info.start_address;
        let high_addr = self.system_mapping_info.end_address;
        let mut offset = (sp_offset + size_of::<usize>() - 1) & !(size_of::<usize>() - 1);
        while offset <= stack_copy.len() - size_of::<usize>() {
            let addr = match std::mem::size_of::<usize>() {
                4 => stack_copy[offset..]
                    .as_ref()
                    .read_u32::<NativeEndian>()
                    .map(|u| u as usize),
                8 => stack_copy[offset..]
                    .as_ref()
                    .read_u64::<NativeEndian>()
                    .map(|u| u as usize),
                x => panic!("Unexpected type width: {}", x),
            };
            if let Ok(addr) = addr {
                if low_addr <= addr && addr <= high_addr {
                    return true;
                }
                offset += size_of::<usize>();
            } else {
                break;
            }
        }
        false
    }

    pub fn is_mapped_file_safe_to_open(name: &Option<String>) -> bool {
        // It is unsafe to attempt to open a mapped file that lives under /dev,
        // because the semantics of the open may be driver-specific so we'd risk
        // hanging the crash dumper. And a file in /dev/ almost certainly has no
        // ELF file identifier anyways.
        if let Some(name) = name {
            if name.starts_with("/dev/") {
                return false;
            }
        }
        true
    }

    fn elf_file_so_name(&self) -> Result<String> {
        // Find the shared object name (SONAME) by examining the ELF information
        // for |mapping|. If the SONAME is found copy it into the passed buffer
        // |soname| and return true. The size of the buffer is |soname_size|.
        let mapped_file = MappingInfo::get_mmap(&self.name, self.offset)?;

        let elf_obj = elf::Elf::parse(&mapped_file)?;

        let soname = elf_obj.soname.ok_or(MapsReaderError::NoSoName(
            self.name.clone().unwrap_or("None".to_string()),
        ))?;
        Ok(soname.to_string())
    }

    pub fn get_mapping_effective_name_and_path(&self) -> Result<(String, String)> {
        let mut file_path = self.name.clone().unwrap_or(String::new());
        let file_name;

        // Tools such as minidump_stackwalk use the name of the module to look up
        // symbols produced by dump_syms. dump_syms will prefer to use a module's
        // DT_SONAME as the module name, if one exists, and will fall back to the
        // filesystem name of the module.

        // Just use the filesystem name if no SONAME is present.
        let file_name = match self.elf_file_so_name() {
            Ok(name) => name,
            Err(_) => {
                //   file_path := /path/to/libname.so
                //   file_name := libname.so
                let split: Vec<_> = file_path.rsplitn(2, "/").collect();
                file_name = split.first().unwrap().to_string();
                return Ok((file_path, file_name));
            }
        };

        if self.executable && self.offset != 0 {
            // If an executable is mapped from a non-zero offset, this is likely because
            // the executable was loaded directly from inside an archive file (e.g., an
            // apk on Android).
            // In this case, we append the file_name to the mapped archive path:
            //   file_name := libname.so
            //   file_path := /path/to/ARCHIVE.APK/libname.so
            file_path = format!("{}/{}", file_path, file_name);
        } else {
            // Otherwise, replace the basename with the SONAME.
            let split: Vec<_> = file_path.rsplitn(2, '/').collect();
            if split.len() == 2 {
                // NOTE: rsplitn reverses the order, so the remainder is the last item
                file_path = format!("{}/{}", split[1], file_name);
            } else {
                file_path = file_name.clone();
            }
        }

        Ok((file_path, file_name))
    }

    pub fn is_contained_in(&self, user_mapping_list: &MappingList) -> bool {
        for user in user_mapping_list {
            // Ignore any mappings that are wholly contained within
            // mappings in the mapping_info_ list.
            if self.start_address >= user.mapping.start_address
                && (self.start_address + self.size)
                    <= (user.mapping.start_address + user.mapping.size)
            {
                return true;
            }
        }
        false
    }

    pub fn is_interesting(&self) -> bool {
        // only want modules with filenames.
        self.name.is_some() &&
        // Only want to include one mapping per shared lib.
        // Avoid filtering executable mappings.
        (self.offset == 0 || self.executable) &&
        // big enough to get a signature for.
        self.size >= 4096
    }

    pub fn contains_address(&self, address: usize) -> bool {
        self.system_mapping_info.start_address <= address
            && address < self.system_mapping_info.end_address
    }
}

#[cfg(test)]
#[cfg(target_pointer_width = "64")] // All addresses are 64 bit and I'm currently too lazy to adjust it to work for both
mod tests {
    use super::*;

    fn get_lines_and_loc() -> (Vec<&'static str>, u64) {
        (vec![
"5597483fc000-5597483fe000 r--p 00000000 00:31 4750073                    /usr/bin/cat",
"5597483fe000-559748402000 r-xp 00002000 00:31 4750073                    /usr/bin/cat",
"559748402000-559748404000 r--p 00006000 00:31 4750073                    /usr/bin/cat",
"559748404000-559748405000 r--p 00007000 00:31 4750073                    /usr/bin/cat",
"559748405000-559748406000 rw-p 00008000 00:31 4750073                    /usr/bin/cat",
"559749b0e000-559749b2f000 rw-p 00000000 00:00 0                          [heap]",
"7efd968d3000-7efd968f5000 rw-p 00000000 00:00 0",
"7efd968f5000-7efd9694a000 r--p 00000000 00:31 5004638                    /usr/lib/locale/en_US.utf8/LC_CTYPE",
"7efd9694a000-7efd96bc2000 r--p 00000000 00:31 5004373                    /usr/lib/locale/en_US.utf8/LC_COLLATE",
"7efd96bc2000-7efd96bc4000 rw-p 00000000 00:00 0",
"7efd96bc4000-7efd96bea000 r--p 00000000 00:31 4996104                    /lib64/libc-2.32.so",
"7efd96bea000-7efd96d39000 r-xp 00026000 00:31 4996104                    /lib64/libc-2.32.so",
"7efd96d39000-7efd96d85000 r--p 00175000 00:31 4996104                    /lib64/libc-2.32.so",
"7efd96d85000-7efd96d86000 ---p 001c1000 00:31 4996104                    /lib64/libc-2.32.so",
"7efd96d86000-7efd96d89000 r--p 001c1000 00:31 4996104                    /lib64/libc-2.32.so",
"7efd96d89000-7efd96d8c000 rw-p 001c4000 00:31 4996104                    /lib64/libc-2.32.so",
"7efd96d8c000-7efd96d92000 ---p 00000000 00:00 0",
"7efd96da0000-7efd96da1000 r--p 00000000 00:31 5004379                    /usr/lib/locale/en_US.utf8/LC_NUMERIC",
"7efd96da1000-7efd96da2000 r--p 00000000 00:31 5004382                    /usr/lib/locale/en_US.utf8/LC_TIME",
"7efd96da2000-7efd96da3000 r--p 00000000 00:31 5004377                    /usr/lib/locale/en_US.utf8/LC_MONETARY",
"7efd96da3000-7efd96da4000 r--p 00000000 00:31 5004376                    /usr/lib/locale/en_US.utf8/LC_MESSAGES/SYS_LC_MESSAGES",
"7efd96da4000-7efd96da5000 r--p 00000000 00:31 5004380                    /usr/lib/locale/en_US.utf8/LC_PAPER",
"7efd96da5000-7efd96da6000 r--p 00000000 00:31 5004378                    /usr/lib/locale/en_US.utf8/LC_NAME",
"7efd96da6000-7efd96da7000 r--p 00000000 00:31 5004372                    /usr/lib/locale/en_US.utf8/LC_ADDRESS",
"7efd96da7000-7efd96da8000 r--p 00000000 00:31 5004381                    /usr/lib/locale/en_US.utf8/LC_TELEPHONE",
"7efd96da8000-7efd96da9000 r--p 00000000 00:31 5004375                    /usr/lib/locale/en_US.utf8/LC_MEASUREMENT",
"7efd96da9000-7efd96db0000 r--s 00000000 00:31 5004639                    /usr/lib64/gconv/gconv-modules.cache",
"7efd96db0000-7efd96db1000 r--p 00000000 00:31 5004374                    /usr/lib/locale/en_US.utf8/LC_IDENTIFICATION",
"7efd96db1000-7efd96db2000 r--p 00000000 00:31 4996100                    /lib64/ld-2.32.so",
"7efd96db2000-7efd96dd3000 r-xp 00001000 00:31 4996100                    /lib64/ld-2.32.so",
"7efd96dd3000-7efd96ddc000 r--p 00022000 00:31 4996100                    /lib64/ld-2.32.so",
"7efd96ddc000-7efd96ddd000 r--p 0002a000 00:31 4996100                    /lib64/ld-2.32.so",
"7efd96ddd000-7efd96ddf000 rw-p 0002b000 00:31 4996100                    /lib64/ld-2.32.so",
"7ffc6dfda000-7ffc6dffb000 rw-p 00000000 00:00 0                          [stack]",
"7ffc6e0f3000-7ffc6e0f7000 r--p 00000000 00:00 0                          [vvar]",
"7ffc6e0f7000-7ffc6e0f9000 r-xp 00000000 00:00 0                          [vdso]",
"ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]"
        ], 0x7ffc6e0f7000)
    }

    fn get_all_mappings() -> Vec<MappingInfo> {
        let mut mappings: Vec<MappingInfo> = Vec::new();
        let (lines, linux_gate_loc) = get_lines_and_loc();
        // Only /usr/bin/cat and [heap]
        for line in lines {
            match MappingInfo::parse_from_line(&line, linux_gate_loc, mappings.last_mut()) {
                Ok(MappingInfoParsingResult::Success(map)) => mappings.push(map),
                Ok(MappingInfoParsingResult::SkipLine) => continue,
                Err(_) => assert!(false),
            }
        }
        assert_eq!(mappings.len(), 23);
        mappings
    }

    #[test]
    fn test_merged() {
        let mut mappings: Vec<MappingInfo> = Vec::new();
        let (lines, linux_gate_loc) = get_lines_and_loc();
        // Only /usr/bin/cat and [heap]
        for line in lines[0..=6].iter() {
            match MappingInfo::parse_from_line(&line, linux_gate_loc, mappings.last_mut()) {
                Ok(MappingInfoParsingResult::Success(map)) => mappings.push(map),
                Ok(MappingInfoParsingResult::SkipLine) => continue,
                Err(_) => assert!(false),
            }
        }

        assert_eq!(mappings.len(), 3);
        let cat_map = MappingInfo {
            start_address: 0x5597483fc000,
            size: 40960,
            system_mapping_info: SystemMappingInfo {
                start_address: 0x5597483fc000,
                end_address: 0x559748406000,
            },
            offset: 0,
            executable: true,
            name: Some("/usr/bin/cat".to_string()),
        };

        assert_eq!(mappings[0], cat_map);

        let heap_map = MappingInfo {
            start_address: 0x559749b0e000,
            size: 135168,
            system_mapping_info: SystemMappingInfo {
                start_address: 0x559749b0e000,
                end_address: 0x559749b2f000,
            },
            offset: 0,
            executable: false,
            name: Some("[heap]".to_string()),
        };

        assert_eq!(mappings[1], heap_map);

        let empty_map = MappingInfo {
            start_address: 0x7efd968d3000,
            size: 139264,
            system_mapping_info: SystemMappingInfo {
                start_address: 0x7efd968d3000,
                end_address: 0x7efd968f5000,
            },
            offset: 0,
            executable: false,
            name: None,
        };

        assert_eq!(mappings[2], empty_map);
    }

    #[test]
    fn test_linux_gate_parsing() {
        let mappings = get_all_mappings();

        let gate_map = MappingInfo {
            start_address: 0x7ffc6e0f7000,
            size: 8192,
            system_mapping_info: SystemMappingInfo {
                start_address: 0x7ffc6e0f7000,
                end_address: 0x7ffc6e0f9000,
            },
            offset: 0,
            executable: true,
            name: Some("linux-gate.so".to_string()),
        };

        assert_eq!(mappings[21], gate_map);
    }

    #[test]
    fn test_reading_all() {
        let mappings = get_all_mappings();

        let found_items = vec![
            Some("/usr/bin/cat".to_string()),
            Some("[heap]".to_string()),
            None,
            Some("/usr/lib/locale/en_US.utf8/LC_CTYPE".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_COLLATE".to_string()),
            None,
            Some("/lib64/libc-2.32.so".to_string()),
            // The original shows a None here, but this is an address ranges that the
            // linker reserved but which a loaded library did not use. These
            // appear as an anonymous private mapping with no access flags set
            // and which directly follow an executable mapping.
            Some("/usr/lib/locale/en_US.utf8/LC_NUMERIC".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_TIME".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_MONETARY".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_MESSAGES/SYS_LC_MESSAGES".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_PAPER".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_NAME".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_ADDRESS".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_TELEPHONE".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_MEASUREMENT".to_string()),
            Some("/usr/lib64/gconv/gconv-modules.cache".to_string()),
            Some("/usr/lib/locale/en_US.utf8/LC_IDENTIFICATION".to_string()),
            Some("/lib64/ld-2.32.so".to_string()),
            Some("[stack]".to_string()),
            Some("[vvar]".to_string()),
            // This is rewritten from [vdso] to linux-gate.so
            Some("linux-gate.so".to_string()),
            Some("[vsyscall]".to_string()),
        ];

        assert_eq!(
            mappings.iter().map(|x| x.name.clone()).collect::<Vec<_>>(),
            found_items
        );
    }

    #[test]
    fn test_merged_reserved_mappings() {
        let mappings = get_all_mappings();

        let gate_map = MappingInfo {
            start_address: 0x7efd96bc4000,
            size: 1892352, // Merged the anonymous area after in this mapping, so its bigger..
            system_mapping_info: SystemMappingInfo {
                start_address: 0x7efd96bc4000,
                end_address: 0x7efd96d8c000, // ..but this is not visible here
            },
            offset: 0,
            executable: true,
            name: Some("/lib64/libc-2.32.so".to_string()),
        };

        assert_eq!(mappings[6], gate_map);
    }

    #[test]
    fn test_get_mapping_effective_name() {
        let lines = vec![
"7f0b97b6f000-7f0b97b70000 r--p 00000000 00:3e 27136458                   /home/martin/Documents/mozilla/devel/mozilla-central/obj/widget/gtk/mozgtk/gtk3/libmozgtk.so",
"7f0b97b70000-7f0b97b71000 r-xp 00000000 00:3e 27136458                   /home/martin/Documents/mozilla/devel/mozilla-central/obj/widget/gtk/mozgtk/gtk3/libmozgtk.so",
"7f0b97b71000-7f0b97b73000 r--p 00000000 00:3e 27136458                   /home/martin/Documents/mozilla/devel/mozilla-central/obj/widget/gtk/mozgtk/gtk3/libmozgtk.so",
"7f0b97b73000-7f0b97b74000 rw-p 00001000 00:3e 27136458                   /home/martin/Documents/mozilla/devel/mozilla-central/obj/widget/gtk/mozgtk/gtk3/libmozgtk.so",
        ];
        let linux_gate_loc = 0x7ffe091bf000;
        let mut mappings: Vec<MappingInfo> = Vec::new();
        for line in lines {
            match MappingInfo::parse_from_line(&line, linux_gate_loc, mappings.last_mut()) {
                Ok(MappingInfoParsingResult::Success(map)) => mappings.push(map),
                Ok(MappingInfoParsingResult::SkipLine) => continue,
                Err(_) => assert!(false),
            }
        }
        assert_eq!(mappings.len(), 1);

        let (file_path, file_name) = mappings[0]
            .get_mapping_effective_name_and_path()
            .expect("Couldn't get effective name for mapping");
        assert_eq!(file_name, "libmozgtk.so");
        assert_eq!(file_path, "/home/martin/Documents/mozilla/devel/mozilla-central/obj/widget/gtk/mozgtk/gtk3/libmozgtk.so");
    }

    #[test]
    fn test_whitespaces_in_maps() {
        let lines = vec![
"   7f0b97b6f000-7f0b97b70000 r--p 00000000 00:3e 27136458                   libmozgtk.so",
"7f0b97b70000-7f0b97b71000 r-xp 00000000 00:3e 27136458                   libmozgtk.so    ",
"7f0b97b71000-7f0b97b73000     r--p 00000000 00:3e 27136458\t\t\tlibmozgtk.so",
        ];
        let linux_gate_loc = 0x7ffe091bf000;
        let mut mappings: Vec<MappingInfo> = Vec::new();
        for line in lines {
            match MappingInfo::parse_from_line(&line, linux_gate_loc, mappings.last_mut()) {
                Ok(MappingInfoParsingResult::Success(map)) => mappings.push(map),
                Ok(MappingInfoParsingResult::SkipLine) => continue,
                Err(x) => panic!(format!("{:?}", x)),
            }
        }
        assert_eq!(mappings.len(), 1);

        let expected_map = MappingInfo {
            start_address: 0x7f0b97b6f000,
            size: 16384,
            system_mapping_info: SystemMappingInfo {
                start_address: 0x7f0b97b6f000,
                end_address: 0x7f0b97b73000,
            },
            offset: 0,
            executable: true,
            name: Some("libmozgtk.so".to_string()),
        };

        assert_eq!(expected_map, mappings[0]);
    }

    #[test]
    fn test_whitespaces_in_name() {
        let lines = vec![
"10000000-20000000 r--p 00000000 00:3e 27136458                   libmoz    gtk.so",
"20000000-30000000 r--p 00000000 00:3e 27136458                   libmozgtk.so (deleted)",
"30000000-40000000 r--p 00000000 00:3e 27136458                   \"libmoz     gtk.so (deleted)\"",
"30000000-40000000 r--p 00000000 00:3e 27136458                   ",
        ];
        let linux_gate_loc = 0x7ffe091bf000;
        let mut mappings: Vec<MappingInfo> = Vec::new();
        for line in lines {
            match MappingInfo::parse_from_line(&line, linux_gate_loc, mappings.last_mut()) {
                Ok(MappingInfoParsingResult::Success(map)) => mappings.push(map),
                Ok(MappingInfoParsingResult::SkipLine) => continue,
                Err(_) => assert!(false),
            }
        }
        assert_eq!(mappings.len(), 4);
        assert_eq!(mappings[0].name, Some("libmoz    gtk.so".to_string()));
        assert_eq!(mappings[1].name, Some("libmozgtk.so (deleted)".to_string()));
        assert_eq!(
            mappings[2].name,
            Some("\"libmoz     gtk.so (deleted)\"".to_string())
        );
        assert_eq!(mappings[3].name, None);
    }
}
