/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use mozannotation_client::MozAnnotationNote;
use std::{
    cmp::min,
    fs::File,
    io::{BufRead, BufReader, Error},
    mem::{size_of, MaybeUninit},
    ptr::null_mut,
    slice,
};

use crate::{
    errors::{FindAnnotationsAddressError, PtraceError, ReadError, RetrievalError},
    ProcessHandle,
};

use super::ProcessReader;

use goblin::elf::{
    self,
    program_header::{PF_R, PT_NOTE},
    Elf, ProgramHeader,
};
use libc::{
    c_int, c_long, c_void, pid_t, ptrace, waitpid, EINTR, PTRACE_ATTACH, PTRACE_DETACH,
    PTRACE_PEEKDATA, __WALL,
};
use memoffset::offset_of;
use mozannotation_client::ANNOTATION_TYPE;

impl ProcessReader {
    pub fn new(process: ProcessHandle) -> Result<ProcessReader, RetrievalError> {
        let pid: pid_t = process;

        ptrace_attach(pid)?;

        let mut status: i32 = 0;

        loop {
            let res = unsafe { waitpid(pid, &mut status as *mut _, __WALL) };
            if res < 0 {
                match get_errno() {
                    EINTR => continue,
                    _ => {
                        ptrace_detach(pid)?;
                        return Err(RetrievalError::WaitPidError);
                    }
                }
            } else {
                break;
            }
        }

        Ok(ProcessReader { process: pid })
    }

    pub fn find_annotations(&self) -> Result<usize, FindAnnotationsAddressError> {
        let maps_file = File::open(format!("/proc/{}/maps", self.process))?;

        BufReader::new(maps_file)
            .lines()
            .flatten()
            .find_map(|line| self.find_annotations_in_module(&line).ok())
            .ok_or(FindAnnotationsAddressError::NotFound)
    }

    fn find_annotations_in_module(&self, line: &str) -> Result<usize, FindAnnotationsAddressError> {
        parse_proc_maps_line(line).and_then(|module_address| {
            let header_bytes = self.copy_array(module_address, size_of::<elf::Header>())?;
            let elf_header = Elf::parse_header(&header_bytes)?;

            let program_header_bytes = self.copy_array(
                module_address + (elf_header.e_phoff as usize),
                (elf_header.e_phnum as usize) * (elf_header.e_phentsize as usize),
            )?;

            let mut elf = Elf::lazy_parse(elf_header)?;
            let context = goblin::container::Ctx {
                container: elf.header.container()?,
                le: elf.header.endianness()?,
            };

            elf.program_headers = ProgramHeader::parse(
                &program_header_bytes,
                0,
                elf_header.e_phnum as usize,
                context,
            )?;

            self.find_mozannotation_note(module_address, &elf)
                .ok_or(FindAnnotationsAddressError::ProgramHeaderNotFound)
        })
    }

    // Looks through the program headers for the note contained in the
    // mozannotation_client crate. If the note is found return the address of the
    // note's desc field as well as its contents.
    fn find_mozannotation_note(&self, module_address: usize, elf: &Elf) -> Option<usize> {
        for program_header in elf.program_headers.iter() {
            // We're looking for a note in the program headers, it needs to be
            // readable and it needs to be at least as large as the
            // MozAnnotationNote structure.
            if (program_header.p_type == PT_NOTE)
                && ((program_header.p_flags & PF_R) != 0
                    && (program_header.p_memsz as usize >= size_of::<MozAnnotationNote>()))
            {
                // Iterate over the notes
                let notes_address = module_address + program_header.p_offset as usize;
                let mut notes_offset = 0;
                let notes_size = program_header.p_memsz as usize;
                while notes_offset < notes_size {
                    let note_address = notes_address + notes_offset;
                    if let Ok(note) = self.copy_object::<goblin::elf::note::Nhdr32>(note_address) {
                        if note.n_type == ANNOTATION_TYPE {
                            if let Ok(note) = self.copy_object::<MozAnnotationNote>(note_address) {
                                let desc = note.desc;
                                let ehdr = (-note.ehdr) as usize;
                                let offset = desc + ehdr
                                    - (offset_of!(MozAnnotationNote, ehdr)
                                        - offset_of!(MozAnnotationNote, desc));

                                return usize::checked_add(module_address, offset);
                            }
                        }

                        notes_offset += size_of::<goblin::elf::note::Nhdr32>()
                            + (note.n_descsz as usize)
                            + (note.n_namesz as usize);
                    } else {
                        break;
                    }
                }
            }
        }

        None
    }

    pub fn copy_object_shallow<T>(&self, src: usize) -> Result<MaybeUninit<T>, ReadError> {
        let data = self.copy_array(src, size_of::<T>())?;
        let mut object = MaybeUninit::<T>::uninit();
        let uninitialized_object = uninit_as_bytes_mut(&mut object);

        for (index, &value) in data.iter().enumerate() {
            uninitialized_object[index].write(value);
        }

        Ok(object)
    }

    pub fn copy_object<T>(&self, src: usize) -> Result<T, ReadError> {
        self.copy_object_shallow(src)
            .map(|object| unsafe { object.assume_init() })
    }

    pub fn copy_array<T>(&self, src: usize, num: usize) -> Result<Vec<T>, ReadError> {
        let mut array = Vec::<MaybeUninit<T>>::with_capacity(num);
        let num_bytes = num * size_of::<T>();
        let mut array_buffer = array.as_mut_ptr() as *mut u8;
        let mut index = 0;

        while index < num_bytes {
            let word = ptrace_read(self.process, src + index)?;
            let len = min(size_of::<c_long>(), num_bytes - index);
            let word_as_bytes = word.to_ne_bytes();
            for &byte in word_as_bytes.iter().take(len) {
                unsafe {
                    array_buffer.write(byte);
                    array_buffer = array_buffer.add(1);
                }
            }

            index += size_of::<c_long>();
        }

        unsafe {
            array.set_len(num);
            Ok(std::mem::transmute(array))
        }
    }
}

impl Drop for ProcessReader {
    fn drop(&mut self) {
        let _ignored = ptrace_detach(self.process);
    }
}

fn parse_proc_maps_line(line: &str) -> Result<usize, FindAnnotationsAddressError> {
    let mut splits = line.trim().splitn(6, ' ');
    let address_str = splits
        .next()
        .ok_or(FindAnnotationsAddressError::ProcMapsParseError)?;
    let _perms_str = splits
        .next()
        .ok_or(FindAnnotationsAddressError::ProcMapsParseError)?;
    let _offset_str = splits
        .next()
        .ok_or(FindAnnotationsAddressError::ProcMapsParseError)?;
    let _dev_str = splits
        .next()
        .ok_or(FindAnnotationsAddressError::ProcMapsParseError)?;
    let _inode_str = splits
        .next()
        .ok_or(FindAnnotationsAddressError::ProcMapsParseError)?;
    let _path_str = splits
        .next()
        .ok_or(FindAnnotationsAddressError::ProcMapsParseError)?;

    let address = get_proc_maps_address(address_str)?;

    Ok(address)
}

fn get_proc_maps_address(addresses: &str) -> Result<usize, FindAnnotationsAddressError> {
    let begin = addresses
        .split('-')
        .next()
        .ok_or(FindAnnotationsAddressError::ProcMapsParseError)?;
    usize::from_str_radix(begin, 16).map_err(FindAnnotationsAddressError::from)
}

fn uninit_as_bytes_mut<T>(elem: &mut MaybeUninit<T>) -> &mut [MaybeUninit<u8>] {
    // SAFETY: MaybeUninit<u8> is always valid, even for padding bytes
    unsafe { slice::from_raw_parts_mut(elem.as_mut_ptr() as *mut MaybeUninit<u8>, size_of::<T>()) }
}

/***********************************************************************
 ***** libc helpers                                                *****
 ***********************************************************************/

fn get_errno() -> c_int {
    #[cfg(target_os = "linux")]
    unsafe {
        *libc::__errno_location()
    }
    #[cfg(target_os = "android")]
    unsafe {
        *libc::__errno()
    }
}

fn clear_errno() {
    #[cfg(target_os = "linux")]
    unsafe {
        *libc::__errno_location() = 0;
    }
    #[cfg(target_os = "android")]
    unsafe {
        *libc::__errno() = 0;
    }
}

#[derive(Clone, Copy)]
enum PTraceOperation {
    Attach,
    Detach,
    PeekData,
}

#[cfg(target_os = "linux")]
type PTraceOperationNative = libc::c_uint;
#[cfg(target_os = "android")]
type PTraceOperationNative = c_int;

impl From<PTraceOperation> for PTraceOperationNative {
    fn from(val: PTraceOperation) -> Self {
        match val {
            PTraceOperation::Attach => PTRACE_ATTACH,
            PTraceOperation::Detach => PTRACE_DETACH,
            PTraceOperation::PeekData => PTRACE_PEEKDATA,
        }
    }
}

fn ptrace_attach(pid: pid_t) -> Result<(), PtraceError> {
    ptrace_helper(pid, PTraceOperation::Attach, 0).map(|_r| ())
}

fn ptrace_detach(pid: pid_t) -> Result<(), PtraceError> {
    ptrace_helper(pid, PTraceOperation::Detach, 0).map(|_r| ())
}

fn ptrace_read(pid: libc::pid_t, addr: usize) -> Result<c_long, PtraceError> {
    ptrace_helper(pid, PTraceOperation::PeekData, addr)
}

fn ptrace_helper(pid: pid_t, op: PTraceOperation, addr: usize) -> Result<c_long, PtraceError> {
    clear_errno();
    let result = unsafe { ptrace(op.into(), pid, addr, null_mut::<c_void>()) };

    if result == -1 {
        let errno = get_errno();
        if errno != 0 {
            let error = match op {
                PTraceOperation::Attach => PtraceError::TraceError(Error::from_raw_os_error(errno)),
                PTraceOperation::Detach => PtraceError::TraceError(Error::from_raw_os_error(errno)),
                PTraceOperation::PeekData => {
                    PtraceError::ReadError(Error::from_raw_os_error(errno))
                }
            };
            Err(error)
        } else {
            Ok(result)
        }
    } else {
        Ok(result)
    }
}
