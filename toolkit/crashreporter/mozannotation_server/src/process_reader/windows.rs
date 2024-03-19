/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use std::{
    convert::TryInto,
    mem::{size_of, MaybeUninit},
    ptr::null_mut,
};

use winapi::{
    shared::{
        basetsd::SIZE_T,
        minwindef::{DWORD, FALSE, HMODULE},
    },
    um::{
        memoryapi::ReadProcessMemory,
        psapi::{K32EnumProcessModules, K32GetModuleInformation, MODULEINFO},
    },
};

use crate::{
    errors::{FindAnnotationsAddressError, ReadError, RetrievalError},
    ProcessHandle,
};

use super::ProcessReader;

impl ProcessReader {
    pub fn new(process: ProcessHandle) -> Result<ProcessReader, RetrievalError> {
        Ok(ProcessReader { process })
    }

    pub fn find_annotations(&self) -> Result<usize, FindAnnotationsAddressError> {
        let modules = self.get_module_list()?;

        modules
            .iter()
            .find_map(|&module| {
                self.get_module_info(module).and_then(|info| {
                    self.find_annotations_in_module(
                        info.lpBaseOfDll as usize,
                        info.SizeOfImage as usize,
                    )
                    .ok()
                })
            })
            .ok_or(FindAnnotationsAddressError::InvalidAddress)
    }

    fn get_module_list(&self) -> Result<Vec<HMODULE>, FindAnnotationsAddressError> {
        let mut module_num: usize = 100;
        let mut required_buffer_size: DWORD = 0;
        let mut module_array = Vec::<MaybeUninit<HMODULE>>::with_capacity(module_num);

        loop {
            let buffer_size: DWORD = (module_num * size_of::<HMODULE>()).try_into()?;
            let res = unsafe {
                K32EnumProcessModules(
                    self.process,
                    module_array.as_mut_ptr() as *mut _,
                    buffer_size,
                    &mut required_buffer_size as *mut _,
                )
            };

            module_num = required_buffer_size as usize / size_of::<HMODULE>();

            if res == 0 {
                if required_buffer_size > buffer_size {
                    module_array = Vec::<MaybeUninit<HMODULE>>::with_capacity(module_num);
                } else {
                    return Err(FindAnnotationsAddressError::EnumProcessModulesError);
                }
            } else {
                break;
            }
        }

        // SAFETY: module_array has been filled by K32EnumProcessModules()
        let module_array: Vec<HMODULE> = unsafe {
            module_array.set_len(module_num);
            std::mem::transmute(module_array)
        };

        Ok(module_array)
    }

    fn get_module_info(&self, module: HMODULE) -> Option<MODULEINFO> {
        let mut info: MaybeUninit<MODULEINFO> = MaybeUninit::uninit();
        let res = unsafe {
            K32GetModuleInformation(
                self.process,
                module,
                info.as_mut_ptr(),
                size_of::<MODULEINFO>() as DWORD,
            )
        };

        if res == 0 {
            None
        } else {
            let info = unsafe { info.assume_init() };
            Some(info)
        }
    }

    fn find_annotations_in_module(
        &self,
        module_address: usize,
        size: usize,
    ) -> Result<usize, FindAnnotationsAddressError> {
        // We read only the first page from the module, this should be more than
        // enough to read the header and section list. In the future we might do
        // this incrementally but for now goblin requires an array to parse
        // so we can't do it just yet.
        let page_size = 4096;
        if size < page_size {
            // Don't try to read from the target module if it's too small
            return Err(FindAnnotationsAddressError::ReadError(
                ReadError::ReadProcessMemoryError,
            ));
        }

        let bytes = self.copy_array(module_address as _, 4096)?;
        let header = goblin::pe::header::Header::parse(&bytes)?;

        // Skip the PE header so we can parse the sections
        let optional_header_offset = header.dos_header.pe_pointer as usize
            + goblin::pe::header::SIZEOF_PE_MAGIC
            + goblin::pe::header::SIZEOF_COFF_HEADER;
        let offset =
            &mut (optional_header_offset + header.coff_header.size_of_optional_header as usize);

        let sections = header.coff_header.sections(&bytes, offset)?;

        for section in sections {
            if section.name.eq(mozannotation_client::ANNOTATION_SECTION) {
                let address = module_address.checked_add(section.virtual_address as usize);
                return address.ok_or(FindAnnotationsAddressError::InvalidAddress);
            }
        }

        Err(FindAnnotationsAddressError::SectionNotFound)
    }

    pub fn copy_object_shallow<T>(&self, src: usize) -> Result<MaybeUninit<T>, ReadError> {
        let mut object = MaybeUninit::<T>::uninit();
        let res = unsafe {
            ReadProcessMemory(
                self.process,
                src as _,
                object.as_mut_ptr() as _,
                size_of::<T>() as SIZE_T,
                null_mut(),
            )
        };

        if res != FALSE {
            Ok(object)
        } else {
            Err(ReadError::ReadProcessMemoryError)
        }
    }

    pub fn copy_object<T>(&self, src: usize) -> Result<T, ReadError> {
        let object = self.copy_object_shallow(src)?;
        Ok(unsafe { object.assume_init() })
    }

    pub fn copy_array<T>(&self, src: usize, num: usize) -> Result<Vec<T>, ReadError> {
        let num_of_bytes = num * size_of::<T>();
        let mut array: Vec<MaybeUninit<T>> = Vec::with_capacity(num);
        let res = unsafe {
            ReadProcessMemory(
                self.process,
                src as _,
                array.as_mut_ptr() as _,
                num_of_bytes as SIZE_T,
                null_mut(),
            )
        };

        if res != FALSE {
            unsafe {
                array.set_len(num);
                Ok(std::mem::transmute(array))
            }
        } else {
            Err(ReadError::ReadProcessMemoryError)
        }
    }
}
