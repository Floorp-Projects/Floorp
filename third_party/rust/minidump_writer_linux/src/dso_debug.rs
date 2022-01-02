use crate::auxv_reader::AuxvType;
use crate::errors::SectionDsoDebugError;
use crate::linux_ptrace_dumper::LinuxPtraceDumper;
use crate::minidump_format::*;
use crate::sections::{write_string_to_location, MemoryArrayWriter, MemoryWriter};
use libc;
use std::collections::HashMap;
use std::io::Cursor;

type Result<T> = std::result::Result<T, SectionDsoDebugError>;

#[cfg(all(target_pointer_width = "32"))]
use goblin::elf::program_header::program_header32::SIZEOF_PHDR;
#[cfg(all(target_pointer_width = "64"))]
use goblin::elf::program_header::program_header64::SIZEOF_PHDR;

#[cfg(all(target_pointer_width = "64", target_arch = "arm"))]
type ElfAddr = u64;
#[cfg(all(target_pointer_width = "64", not(target_arch = "arm")))]
type ElfAddr = libc::Elf64_Addr;
#[cfg(all(target_pointer_width = "32", target_arch = "arm"))]
type ElfAddr = u32;
#[cfg(all(target_pointer_width = "32", not(target_arch = "arm")))]
type ElfAddr = libc::Elf32_Addr;

// COPY from <link.h>
#[derive(Debug, Clone, Default)]
#[repr(C)]
pub struct LinkMap {
    /* These first few members are part of the protocol with the debugger.
    This is the same format used in SVR4.  */
    l_addr: ElfAddr, /* Difference between the address in the ELF
                     file and the addresses in memory.  */
    l_name: usize, /* Absolute file name object was found in. WAS: `char*`  */
    l_ld: usize,   /* Dynamic section of the shared object.  WAS: `ElfW(Dyn) *` */
    l_next: usize, /* Chain of loaded objects. WAS: `struct link_map *` */
    l_prev: usize, /* Chain of loaded objects. WAS: `struct link_map *` */
}

// COPY from <link.h>
#[derive(Debug, Clone)]
#[allow(non_camel_case_types, unused)]
#[repr(C)]
enum RState {
    /* This state value describes the mapping change taking place when
    the `r_brk' address is called.  */
    RT_CONSISTENT, /* Mapping change is complete.  */
    RT_ADD,        /* Beginning to add a new object.  */
    RT_DELETE,     /* Beginning to remove an object mapping.  */
}
impl Default for RState {
    fn default() -> Self {
        RState::RT_CONSISTENT // RStates are not used anyway
    }
}
// COPY from <link.h>
#[derive(Debug, Clone, Default)]
#[repr(C)]
pub struct RDebug {
    r_version: libc::c_int, /* Version number for this protocol.  */
    r_map: usize,           /* Head of the chain of loaded objects. WAS: `struct link_map *` */

    /* This is the address of a function internal to the run-time linker,
    that will always be called when the linker begins to map in a
    library or unmap it, and again when the mapping change is complete.
    The debugger can set a breakpoint at this address if it wants to
    notice shared object mapping changes.  */
    r_brk: ElfAddr,
    r_state: RState,
    r_ldbase: ElfAddr, /* Base address the linker is loaded at.  */
}

pub fn write_dso_debug_stream(
    buffer: &mut Cursor<Vec<u8>>,
    blamed_thread: i32,
    auxv: &HashMap<AuxvType, AuxvType>,
) -> Result<MDRawDirectory> {
    let at_phnum;
    let at_phdr;
    #[cfg(target_arch = "arm")]
    {
        at_phdr = 3;
        at_phnum = 5;
    }
    #[cfg(not(target_arch = "arm"))]
    {
        at_phdr = libc::AT_PHDR;
        at_phnum = libc::AT_PHNUM;
    }
    let phnum_max = *auxv
        .get(&at_phnum)
        .ok_or(SectionDsoDebugError::CouldNotFind("AT_PHNUM in auxv"))?
        as usize;
    let phdr = *auxv
        .get(&at_phdr)
        .ok_or(SectionDsoDebugError::CouldNotFind("AT_PHDR in auxv"))? as usize;

    let ph = LinuxPtraceDumper::copy_from_process(
        blamed_thread,
        phdr as *mut libc::c_void,
        SIZEOF_PHDR * phnum_max,
    )?;
    let program_headers;
    #[cfg(target_pointer_width = "64")]
    {
        program_headers = goblin::elf::program_header::program_header64::ProgramHeader::from_bytes(
            &ph, phnum_max,
        );
    }
    #[cfg(target_pointer_width = "32")]
    {
        program_headers = goblin::elf::program_header::program_header32::ProgramHeader::from_bytes(
            &ph, phnum_max,
        );
    };

    // Assume the program base is at the beginning of the same page as the PHDR
    let mut base = phdr & !0xfff;
    let mut dyn_addr = 0 as ElfAddr;
    // Search for the program PT_DYNAMIC segment
    for ph in program_headers {
        // Adjust base address with the virtual address of the PT_LOAD segment
        // corresponding to offset 0
        if ph.p_type == goblin::elf::program_header::PT_LOAD && ph.p_offset == 0 {
            base -= ph.p_vaddr as usize;
        }
        if ph.p_type == goblin::elf::program_header::PT_DYNAMIC {
            dyn_addr = ph.p_vaddr;
        }
    }

    if dyn_addr == 0 {
        return Err(SectionDsoDebugError::CouldNotFind(
            "dyn_addr in program headers",
        ));
    }

    dyn_addr += base as ElfAddr;

    let dyn_size = std::mem::size_of::<goblin::elf::Dyn>();
    let mut r_debug = 0usize;
    let mut dynamic_length = 0usize;

    // The dynamic linker makes information available that helps gdb find all
    // DSOs loaded into the program. If this information is indeed available,
    // dump it to a MD_LINUX_DSO_DEBUG stream.
    loop {
        let dyn_data = LinuxPtraceDumper::copy_from_process(
            blamed_thread,
            (dyn_addr as usize + dynamic_length) as *mut libc::c_void,
            dyn_size,
        )?;
        dynamic_length += dyn_size;

        // goblin::elf::Dyn doesn't have padding bytes
        let (head, body, _tail) = unsafe { dyn_data.align_to::<goblin::elf::Dyn>() };
        assert!(head.is_empty(), "Data was not aligned");
        let dyn_struct = &body[0];

        // #ifdef __mips__
        //       const int32_t debug_tag = DT_MIPS_RLD_MAP;
        // #else
        //       const int32_t debug_tag = DT_DEBUG;
        // #endif
        let debug_tag = goblin::elf::dynamic::DT_DEBUG;
        if dyn_struct.d_tag == debug_tag {
            r_debug = dyn_struct.d_val as usize;
        } else if dyn_struct.d_tag == goblin::elf::dynamic::DT_NULL {
            break;
        }
    }

    // The "r_map" field of that r_debug struct contains a linked list of all
    // loaded DSOs.
    // Our list of DSOs potentially is different from the ones in the crashing
    // process. So, we have to be careful to never dereference pointers
    // directly. Instead, we use CopyFromProcess() everywhere.
    // See <link.h> for a more detailed discussion of the how the dynamic
    // loader communicates with debuggers.

    let debug_entry_data = LinuxPtraceDumper::copy_from_process(
        blamed_thread,
        r_debug as *mut libc::c_void,
        std::mem::size_of::<RDebug>(),
    )?;

    // goblin::elf::Dyn doesn't have padding bytes
    let (head, body, _tail) = unsafe { debug_entry_data.align_to::<RDebug>() };
    assert!(head.is_empty(), "Data was not aligned");
    let debug_entry = &body[0];

    // Count the number of loaded DSOs
    let mut dso_vec = Vec::new();
    let mut curr_map = debug_entry.r_map;
    while curr_map != 0 {
        let link_map_data = LinuxPtraceDumper::copy_from_process(
            blamed_thread,
            curr_map as *mut libc::c_void,
            std::mem::size_of::<LinkMap>(),
        )?;

        // LinkMap is repr(C) and doesn't have padding bytes, so this should be safe
        let (head, body, _tail) = unsafe { link_map_data.align_to::<LinkMap>() };
        assert!(head.is_empty(), "Data was not aligned");
        let map = &body[0];

        curr_map = map.l_next;
        dso_vec.push(map.clone());
    }

    let mut linkmap_rva = u32::MAX;
    if dso_vec.len() > 0 {
        // If we have at least one DSO, create an array of MDRawLinkMap
        // entries in the minidump file.
        let mut linkmap = MemoryArrayWriter::<MDRawLinkMap>::alloc_array(buffer, dso_vec.len())?;
        linkmap_rva = linkmap.location().rva;

        // Iterate over DSOs and write their information to mini dump
        for (idx, map) in dso_vec.iter().enumerate() {
            let mut filename = String::new();
            if map.l_name > 0 {
                let filename_data = LinuxPtraceDumper::copy_from_process(
                    blamed_thread,
                    map.l_name as *mut libc::c_void,
                    256,
                )?;

                // C - string is NULL-terminated
                if let Some(name) = filename_data.splitn(2, |x| *x == b'\0').next() {
                    filename = String::from_utf8(name.to_vec())?;
                }
            }
            let location = write_string_to_location(buffer, &filename)?;
            let entry = MDRawLinkMap {
                addr: map.l_addr,
                name: location.rva,
                ld: map.l_ld as ElfAddr,
            };

            linkmap.set_value_at(buffer, entry, idx)?;
        }
    }

    // Write MD_LINUX_DSO_DEBUG record
    let debug = MDRawDebug {
        version: debug_entry.r_version as u32,
        map: linkmap_rva,
        dso_count: dso_vec.len() as u32,
        brk: debug_entry.r_brk,
        ldbase: debug_entry.r_ldbase,
        dynamic: dyn_addr,
    };
    let debug_loc = MemoryWriter::<MDRawDebug>::alloc_with_val(buffer, debug)?;

    let mut dirent = MDRawDirectory {
        stream_type: MDStreamType::LinuxDsoDebug as u32,
        location: debug_loc.location(),
    };

    dirent.location.data_size += dynamic_length as u32;
    let dso_debug_data = LinuxPtraceDumper::copy_from_process(
        blamed_thread,
        dyn_addr as *mut libc::c_void,
        dynamic_length,
    )?;
    MemoryArrayWriter::<u8>::alloc_from_array(buffer, &dso_debug_data)?;

    Ok(dirent)
}
