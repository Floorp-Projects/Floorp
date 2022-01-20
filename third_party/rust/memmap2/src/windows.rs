#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use std::fs::File;
use std::os::raw::c_void;
use std::os::windows::io::{AsRawHandle, RawHandle};
use std::{io, mem, ptr};

type BOOL = i32;
type WORD = u16;
type DWORD = u32;
type WCHAR = u16;
type HANDLE = *mut c_void;
type LPVOID = *mut c_void;
type LPCVOID = *const c_void;
type ULONG_PTR = usize;
type SIZE_T = ULONG_PTR;
type LPCWSTR = *const WCHAR;
type PDWORD = *mut DWORD;
type DWORD_PTR = ULONG_PTR;
type LPSECURITY_ATTRIBUTES = *mut SECURITY_ATTRIBUTES;
type LPSYSTEM_INFO = *mut SYSTEM_INFO;

const INVALID_HANDLE_VALUE: HANDLE = -1isize as HANDLE;

const STANDARD_RIGHTS_REQUIRED: DWORD = 0x000F0000;

const SECTION_QUERY: DWORD = 0x0001;
const SECTION_MAP_WRITE: DWORD = 0x0002;
const SECTION_MAP_READ: DWORD = 0x0004;
const SECTION_MAP_EXECUTE: DWORD = 0x0008;
const SECTION_EXTEND_SIZE: DWORD = 0x0010;
const SECTION_MAP_EXECUTE_EXPLICIT: DWORD = 0x0020;
const SECTION_ALL_ACCESS: DWORD = STANDARD_RIGHTS_REQUIRED
    | SECTION_QUERY
    | SECTION_MAP_WRITE
    | SECTION_MAP_READ
    | SECTION_MAP_EXECUTE
    | SECTION_EXTEND_SIZE;

const PAGE_READONLY: DWORD = 0x02;
const PAGE_READWRITE: DWORD = 0x04;
const PAGE_WRITECOPY: DWORD = 0x08;
const PAGE_EXECUTE_READ: DWORD = 0x20;
const PAGE_EXECUTE_READWRITE: DWORD = 0x40;
const PAGE_EXECUTE_WRITECOPY: DWORD = 0x80;

const FILE_MAP_WRITE: DWORD = SECTION_MAP_WRITE;
const FILE_MAP_READ: DWORD = SECTION_MAP_READ;
const FILE_MAP_ALL_ACCESS: DWORD = SECTION_ALL_ACCESS;
const FILE_MAP_EXECUTE: DWORD = SECTION_MAP_EXECUTE_EXPLICIT;
const FILE_MAP_COPY: DWORD = 0x00000001;

#[repr(C)]
#[derive(Clone, Copy)]
struct SECURITY_ATTRIBUTES {
    nLength: DWORD,
    lpSecurityDescriptor: LPVOID,
    bInheritHandle: BOOL,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SYSTEM_INFO_u_s {
    wProcessorArchitecture: WORD,
    wReserved: WORD,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct SYSTEM_INFO_u([u32; 1]);

#[repr(C)]
#[derive(Clone, Copy)]
struct SYSTEM_INFO {
    u: SYSTEM_INFO_u,
    dwPageSize: DWORD,
    lpMinimumApplicationAddress: LPVOID,
    lpMaximumApplicationAddress: LPVOID,
    dwActiveProcessorMask: DWORD_PTR,
    dwNumberOfProcessors: DWORD,
    dwProcessorType: DWORD,
    dwAllocationGranularity: DWORD,
    wProcessorLevel: WORD,
    wProcessorRevision: WORD,
}

extern "system" {
    fn CloseHandle(hObject: HANDLE) -> BOOL;

    fn CreateFileMappingW(
        hFile: HANDLE,
        lpFileMappingAttributes: LPSECURITY_ATTRIBUTES,
        flProtect: DWORD,
        dwMaximumSizeHigh: DWORD,
        dwMaximumSizeLow: DWORD,
        lpName: LPCWSTR,
    ) -> HANDLE;

    fn FlushViewOfFile(lpBaseAddress: LPCVOID, dwNumberOfBytesToFlush: SIZE_T) -> BOOL;

    fn UnmapViewOfFile(lpBaseAddress: LPCVOID) -> BOOL;

    fn MapViewOfFile(
        hFileMappingObject: HANDLE,
        dwDesiredAccess: DWORD,
        dwFileOffsetHigh: DWORD,
        dwFileOffsetLow: DWORD,
        dwNumberOfBytesToMap: SIZE_T,
    ) -> LPVOID;

    fn VirtualProtect(
        lpAddress: LPVOID,
        dwSize: SIZE_T,
        flNewProtect: DWORD,
        lpflOldProtect: PDWORD,
    ) -> BOOL;

    fn GetSystemInfo(lpSystemInfo: LPSYSTEM_INFO);
}

pub struct MmapInner {
    file: Option<File>,
    ptr: *mut c_void,
    len: usize,
    copy: bool,
}

impl MmapInner {
    /// Creates a new `MmapInner`.
    ///
    /// This is a thin wrapper around the `CreateFileMappingW` and `MapViewOfFile` system calls.
    pub fn new(
        file: &File,
        protect: DWORD,
        access: DWORD,
        offset: u64,
        len: usize,
        copy: bool,
    ) -> io::Result<MmapInner> {
        let alignment = offset % allocation_granularity() as u64;
        let aligned_offset = offset - alignment as u64;
        let aligned_len = len + alignment as usize;

        unsafe {
            let handle = CreateFileMappingW(
                file.as_raw_handle(),
                ptr::null_mut(),
                protect,
                0,
                0,
                ptr::null(),
            );
            if handle.is_null() {
                return Err(io::Error::last_os_error());
            }

            let ptr = MapViewOfFile(
                handle,
                access,
                (aligned_offset >> 16 >> 16) as DWORD,
                (aligned_offset & 0xffffffff) as DWORD,
                aligned_len as SIZE_T,
            );
            CloseHandle(handle);

            if ptr.is_null() {
                Err(io::Error::last_os_error())
            } else {
                Ok(MmapInner {
                    file: Some(file.try_clone()?),
                    ptr: ptr.offset(alignment as isize),
                    len: len as usize,
                    copy,
                })
            }
        }
    }

    pub fn map(len: usize, file: &File, offset: u64, _populate: bool) -> io::Result<MmapInner> {
        let write = protection_supported(file.as_raw_handle(), PAGE_READWRITE);
        let exec = protection_supported(file.as_raw_handle(), PAGE_EXECUTE_READ);
        let mut access = FILE_MAP_READ;
        let protection = match (write, exec) {
            (true, true) => {
                access |= FILE_MAP_WRITE | FILE_MAP_EXECUTE;
                PAGE_EXECUTE_READWRITE
            }
            (true, false) => {
                access |= FILE_MAP_WRITE;
                PAGE_READWRITE
            }
            (false, true) => {
                access |= FILE_MAP_EXECUTE;
                PAGE_EXECUTE_READ
            }
            (false, false) => PAGE_READONLY,
        };

        let mut inner = MmapInner::new(file, protection, access, offset, len, false)?;
        if write || exec {
            inner.make_read_only()?;
        }
        Ok(inner)
    }

    pub fn map_exec(
        len: usize,
        file: &File,
        offset: u64,
        _populate: bool,
    ) -> io::Result<MmapInner> {
        let write = protection_supported(file.as_raw_handle(), PAGE_READWRITE);
        let mut access = FILE_MAP_READ | FILE_MAP_EXECUTE;
        let protection = if write {
            access |= FILE_MAP_WRITE;
            PAGE_EXECUTE_READWRITE
        } else {
            PAGE_EXECUTE_READ
        };

        let mut inner = MmapInner::new(file, protection, access, offset, len, false)?;
        if write {
            inner.make_exec()?;
        }
        Ok(inner)
    }

    pub fn map_mut(len: usize, file: &File, offset: u64, _populate: bool) -> io::Result<MmapInner> {
        let exec = protection_supported(file.as_raw_handle(), PAGE_EXECUTE_READ);
        let mut access = FILE_MAP_READ | FILE_MAP_WRITE;
        let protection = if exec {
            access |= FILE_MAP_EXECUTE;
            PAGE_EXECUTE_READWRITE
        } else {
            PAGE_READWRITE
        };

        let mut inner = MmapInner::new(file, protection, access, offset, len, false)?;
        if exec {
            inner.make_mut()?;
        }
        Ok(inner)
    }

    pub fn map_copy(
        len: usize,
        file: &File,
        offset: u64,
        _populate: bool,
    ) -> io::Result<MmapInner> {
        let exec = protection_supported(file.as_raw_handle(), PAGE_EXECUTE_READWRITE);
        let mut access = FILE_MAP_COPY;
        let protection = if exec {
            access |= FILE_MAP_EXECUTE;
            PAGE_EXECUTE_WRITECOPY
        } else {
            PAGE_WRITECOPY
        };

        let mut inner = MmapInner::new(file, protection, access, offset, len, true)?;
        if exec {
            inner.make_mut()?;
        }
        Ok(inner)
    }

    pub fn map_copy_read_only(
        len: usize,
        file: &File,
        offset: u64,
        _populate: bool,
    ) -> io::Result<MmapInner> {
        let write = protection_supported(file.as_raw_handle(), PAGE_READWRITE);
        let exec = protection_supported(file.as_raw_handle(), PAGE_EXECUTE_READ);
        let mut access = FILE_MAP_COPY;
        let protection = if exec {
            access |= FILE_MAP_EXECUTE;
            PAGE_EXECUTE_WRITECOPY
        } else {
            PAGE_WRITECOPY
        };

        let mut inner = MmapInner::new(file, protection, access, offset, len, true)?;
        if write || exec {
            inner.make_read_only()?;
        }
        Ok(inner)
    }

    pub fn map_anon(len: usize, _stack: bool) -> io::Result<MmapInner> {
        unsafe {
            // Create a mapping and view with maximum access permissions, then use `VirtualProtect`
            // to set the actual `Protection`. This way, we can set more permissive protection later
            // on.
            // Also see https://msdn.microsoft.com/en-us/library/windows/desktop/aa366537.aspx

            let handle = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                ptr::null_mut(),
                PAGE_EXECUTE_READWRITE,
                (len >> 16 >> 16) as DWORD,
                (len & 0xffffffff) as DWORD,
                ptr::null(),
            );
            if handle.is_null() {
                return Err(io::Error::last_os_error());
            }
            let access = FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE;
            let ptr = MapViewOfFile(handle, access, 0, 0, len as SIZE_T);
            CloseHandle(handle);

            if ptr.is_null() {
                return Err(io::Error::last_os_error());
            }

            let mut old = 0;
            let result = VirtualProtect(ptr, len as SIZE_T, PAGE_READWRITE, &mut old);
            if result != 0 {
                Ok(MmapInner {
                    file: None,
                    ptr,
                    len: len as usize,
                    copy: false,
                })
            } else {
                Err(io::Error::last_os_error())
            }
        }
    }

    pub fn flush(&self, offset: usize, len: usize) -> io::Result<()> {
        self.flush_async(offset, len)?;
        if let Some(ref file) = self.file {
            file.sync_data()?;
        }
        Ok(())
    }

    pub fn flush_async(&self, offset: usize, len: usize) -> io::Result<()> {
        let result = unsafe { FlushViewOfFile(self.ptr.add(offset), len as SIZE_T) };
        if result != 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }

    fn virtual_protect(&mut self, protect: DWORD) -> io::Result<()> {
        unsafe {
            let alignment = self.ptr as usize % allocation_granularity();
            let ptr = self.ptr.offset(-(alignment as isize));
            let aligned_len = self.len as SIZE_T + alignment as SIZE_T;

            let mut old = 0;
            let result = VirtualProtect(ptr, aligned_len, protect, &mut old);

            if result != 0 {
                Ok(())
            } else {
                Err(io::Error::last_os_error())
            }
        }
    }

    pub fn make_read_only(&mut self) -> io::Result<()> {
        self.virtual_protect(PAGE_READONLY)
    }

    pub fn make_exec(&mut self) -> io::Result<()> {
        if self.copy {
            self.virtual_protect(PAGE_EXECUTE_WRITECOPY)
        } else {
            self.virtual_protect(PAGE_EXECUTE_READ)
        }
    }

    pub fn make_mut(&mut self) -> io::Result<()> {
        if self.copy {
            self.virtual_protect(PAGE_WRITECOPY)
        } else {
            self.virtual_protect(PAGE_READWRITE)
        }
    }

    #[inline]
    pub fn ptr(&self) -> *const u8 {
        self.ptr as *const u8
    }

    #[inline]
    pub fn mut_ptr(&mut self) -> *mut u8 {
        self.ptr as *mut u8
    }

    #[inline]
    pub fn len(&self) -> usize {
        self.len
    }
}

impl Drop for MmapInner {
    fn drop(&mut self) {
        let alignment = self.ptr as usize % allocation_granularity();
        unsafe {
            let ptr = self.ptr.offset(-(alignment as isize));
            assert!(
                UnmapViewOfFile(ptr) != 0,
                "unable to unmap mmap: {}",
                io::Error::last_os_error()
            );
        }
    }
}

unsafe impl Sync for MmapInner {}
unsafe impl Send for MmapInner {}

fn protection_supported(handle: RawHandle, protection: DWORD) -> bool {
    unsafe {
        let handle = CreateFileMappingW(handle, ptr::null_mut(), protection, 0, 0, ptr::null());
        if handle.is_null() {
            return false;
        }
        CloseHandle(handle);
        true
    }
}

fn allocation_granularity() -> usize {
    unsafe {
        let mut info = mem::zeroed();
        GetSystemInfo(&mut info);
        info.dwAllocationGranularity as usize
    }
}

pub fn file_len(file: &File) -> io::Result<u64> {
    Ok(file.metadata()?.len())
}
