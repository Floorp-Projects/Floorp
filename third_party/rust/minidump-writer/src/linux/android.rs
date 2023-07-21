use crate::errors::AndroidError;
use crate::maps_reader::MappingInfo;
use crate::ptrace_dumper::PtraceDumper;
use crate::thread_info::Pid;
use goblin::elf;
#[cfg(target_pointer_width = "32")]
use goblin::elf::dynamic::dyn32::{Dyn, SIZEOF_DYN};
#[cfg(target_pointer_width = "64")]
use goblin::elf::dynamic::dyn64::{Dyn, SIZEOF_DYN};
#[cfg(target_pointer_width = "32")]
use goblin::elf::header::header32 as elf_header;
#[cfg(target_pointer_width = "64")]
use goblin::elf::header::header64 as elf_header;
#[cfg(target_pointer_width = "32")]
use goblin::elf::program_header::program_header32::ProgramHeader;
#[cfg(target_pointer_width = "64")]
use goblin::elf::program_header::program_header64::ProgramHeader;
use std::ffi::c_void;

type Result<T> = std::result::Result<T, AndroidError>;

// From /usr/include/elf.h of the android SDK
// #define DT_ANDROID_REL (DT_LOOS + 2)
// #define DT_ANDROID_RELSZ (DT_LOOS + 3)
// #define DT_ANDROID_RELA (DT_LOOS + 4)
// #define DT_ANDROID_RELASZ (DT_LOOS + 5)
#[cfg(target_pointer_width = "64")]
const DT_ANDROID_REL: u64 = elf::dynamic::DT_LOOS + 2;
#[cfg(target_pointer_width = "64")]
const DT_ANDROID_RELA: u64 = elf::dynamic::DT_LOOS + 4;
#[cfg(target_pointer_width = "32")]
const DT_ANDROID_REL: u32 = (elf::dynamic::DT_LOOS + 2) as u32;
#[cfg(target_pointer_width = "32")]
const DT_ANDROID_RELA: u32 = (elf::dynamic::DT_LOOS + 4) as u32;

struct DynVaddresses {
    min_vaddr: usize,
    dyn_vaddr: usize,
    dyn_count: usize,
}

fn has_android_packed_relocations(pid: Pid, load_bias: usize, vaddrs: DynVaddresses) -> Result<()> {
    let dyn_addr = load_bias + vaddrs.dyn_vaddr;
    for idx in 0..vaddrs.dyn_count {
        let addr = (dyn_addr + SIZEOF_DYN * idx) as *mut c_void;
        let dyn_data = PtraceDumper::copy_from_process(pid, addr, SIZEOF_DYN)?;
        // TODO: Couldn't find a nice way to use goblin for that, to avoid the unsafe-block
        let dyn_obj: Dyn;
        unsafe {
            dyn_obj = std::mem::transmute::<[u8; SIZEOF_DYN], Dyn>(dyn_data.as_slice().try_into()?);
        }

        if dyn_obj.d_tag == DT_ANDROID_REL || dyn_obj.d_tag == DT_ANDROID_RELA {
            return Ok(());
        }
    }
    Err(AndroidError::NoRelFound)
}

fn get_effective_load_bias(pid: Pid, ehdr: &elf_header::Header, address: usize) -> usize {
    let ph = parse_loaded_elf_program_headers(pid, ehdr, address);
    // If |min_vaddr| is non-zero and we find Android packed relocation tags,
    // return the effective load bias.

    if ph.min_vaddr != 0 {
        let load_bias = address - ph.min_vaddr;
        if has_android_packed_relocations(pid, load_bias, ph).is_ok() {
            return load_bias;
        }
    }
    // Either |min_vaddr| is zero, or it is non-zero but we did not find the
    // expected Android packed relocations tags.
    address
}

fn parse_loaded_elf_program_headers(
    pid: Pid,
    ehdr: &elf_header::Header,
    address: usize,
) -> DynVaddresses {
    let phdr_addr = address + ehdr.e_phoff as usize;
    let mut min_vaddr = usize::MAX;
    let mut dyn_vaddr = 0;
    let mut dyn_count = 0;

    let phdr_opt = PtraceDumper::copy_from_process(
        pid,
        phdr_addr as *mut c_void,
        elf_header::SIZEOF_EHDR * ehdr.e_phnum as usize,
    );
    if let Ok(ph_data) = phdr_opt {
        // TODO: The original C code doesn't have error-handling here at all.
        //       We silently ignore "not parsable" for now, but might bubble it up.
        // TODO2: `from_bytes` might panic, `parse()` would return a Result<>, so maybe better
        //        to switch to that at some point.
        for phdr in ProgramHeader::from_bytes(&ph_data, ehdr.e_phnum as usize) {
            let p_vaddr = phdr.p_vaddr as usize;
            if phdr.p_type == elf::program_header::PT_LOAD && p_vaddr < min_vaddr {
                min_vaddr = p_vaddr;
            }

            if phdr.p_type == elf::program_header::PT_DYNAMIC {
                dyn_vaddr = p_vaddr;
                dyn_count = phdr.p_memsz as usize / SIZEOF_DYN;
            }
        }
    }

    DynVaddresses {
        min_vaddr,
        dyn_vaddr,
        dyn_count,
    }
}

pub fn late_process_mappings(pid: Pid, mappings: &mut [MappingInfo]) -> Result<()> {
    // Only consider exec mappings that indicate a file path was mapped, and
    // where the ELF header indicates a mapped shared library.
    for map in mappings
        .iter_mut()
        .filter(|m| m.is_executable() && m.name_is_path())
    {
        let ehdr_opt = PtraceDumper::copy_from_process(
            pid,
            map.start_address as *mut c_void,
            elf_header::SIZEOF_EHDR,
        )
        .ok()
        .and_then(|x| elf_header::Header::parse(&x).ok());

        if let Some(ehdr) = ehdr_opt {
            if ehdr.e_type == elf_header::ET_DYN {
                // Compute the effective load bias for this mapped library, and update
                // the mapping to hold that rather than |start_addr|, at the same time
                // adjusting |size| to account for the change in |start_addr|. Where
                // the library does not contain Android packed relocations,
                // GetEffectiveLoadBias() returns |start_addr| and the mapping entry
                // is not changed.
                let load_bias = get_effective_load_bias(pid, &ehdr, map.start_address);
                map.size += map.start_address - load_bias;
                map.start_address = load_bias;
            }
        }
    }
    Ok(())
}
