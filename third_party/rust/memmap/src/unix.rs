extern crate libc;

use std::{io, ptr};
use std::fs::File;
use std::os::unix::io::AsRawFd;

use ::Protection;
use ::MmapOptions;

impl Protection {

    /// Returns the `Protection` value as a POSIX protection flag.
    fn as_prot(self) -> libc::c_int {
        match self {
            Protection::Read => libc::PROT_READ,
            Protection::ReadWrite => libc::PROT_READ | libc::PROT_WRITE,
            Protection::ReadCopy => libc::PROT_READ | libc::PROT_WRITE,
            Protection::ReadExecute => libc::PROT_READ | libc::PROT_EXEC,
        }
    }

    fn as_flag(self) -> libc::c_int {
        match self {
            Protection::Read => libc::MAP_SHARED,
            Protection::ReadWrite => libc::MAP_SHARED,
            Protection::ReadCopy => libc::MAP_PRIVATE,
            Protection::ReadExecute => libc::MAP_SHARED,
        }
    }
}

#[cfg(any(all(target_os = "linux", not(target_arch="mips")),
          target_os = "freebsd",
          target_os = "android"))]
const MAP_STACK: libc::c_int = libc::MAP_STACK;

#[cfg(not(any(all(target_os = "linux", not(target_arch="mips")),
              target_os = "freebsd",
              target_os = "android")))]
const MAP_STACK: libc::c_int = 0;

impl MmapOptions {
    fn as_flag(self) -> libc::c_int {
        let mut flag = 0;
        if self.stack { flag |= MAP_STACK }
        flag
    }
}

pub struct MmapInner {
    ptr: *mut libc::c_void,
    len: usize,
}

impl MmapInner {

    pub fn open(file: &File, prot: Protection, offset: usize, len: usize) -> io::Result<MmapInner> {
        let alignment = offset % page_size();
        let aligned_offset = offset - alignment;
        let aligned_len = len + alignment;
        if aligned_len == 0 {
            // Normally the OS would catch this, but it segfaults under QEMU.
            return Err(io::Error::new(io::ErrorKind::InvalidInput,
                                      "memory map must have a non-zero length"));
        }

        unsafe {
            let ptr = libc::mmap(ptr::null_mut(),
                                 aligned_len as libc::size_t,
                                 prot.as_prot(),
                                 prot.as_flag(),
                                 file.as_raw_fd(),
                                 aligned_offset as libc::off_t);

            if ptr == libc::MAP_FAILED {
                Err(io::Error::last_os_error())
            } else {
                Ok(MmapInner {
                    ptr: ptr.offset(alignment as isize),
                    len: len,
                })
            }
        }
    }

    /// Open an anonymous memory map.
    pub fn anonymous(len: usize, prot: Protection, options: MmapOptions) -> io::Result<MmapInner> {
        let ptr = unsafe {
            libc::mmap(ptr::null_mut(),
                       len as libc::size_t,
                       prot.as_prot(),
                       options.as_flag() | prot.as_flag() | libc::MAP_ANON,
                       -1,
                       0)
        };

        if ptr == libc::MAP_FAILED {
            Err(io::Error::last_os_error())
        } else {
            Ok(MmapInner {
                ptr: ptr,
                len: len as usize,
            })
        }
    }

    pub fn flush(&self, offset: usize, len: usize) -> io::Result<()> {
        let alignment = (self.ptr as usize + offset) % page_size();
        let offset = offset as isize - alignment as isize;
        let len = len + alignment;
        let result = unsafe { libc::msync(self.ptr.offset(offset),
                                          len as libc::size_t,
                                          libc::MS_SYNC) };
        if result == 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }

    pub fn flush_async(&self, offset: usize, len: usize) -> io::Result<()> {
        let alignment = offset % page_size();
        let aligned_offset = offset - alignment;
        let aligned_len = len + alignment;
        let result = unsafe { libc::msync(self.ptr.offset(aligned_offset as isize),
                                          aligned_len as libc::size_t,
                                          libc::MS_ASYNC) };
        if result == 0 {
            Ok(())
        } else {
            Err(io::Error::last_os_error())
        }
    }

    pub fn set_protection(&mut self, prot: Protection) -> io::Result<()> {
        unsafe {
            let alignment = self.ptr as usize % page_size();
            let ptr = self.ptr.offset(- (alignment as isize));
            let len = self.len + alignment;
            let result = libc::mprotect(ptr,
                                        len,
                                        prot.as_prot());
            if result == 0 {
                Ok(())
            } else {
                Err(io::Error::last_os_error())
            }
        }
    }

    pub fn ptr(&self) -> *const u8 {
        self.ptr as *const u8
    }

    pub fn mut_ptr(&mut self) -> *mut u8 {
        self.ptr as *mut u8
    }

    pub fn len(&self) -> usize {
        self.len
    }
}

impl Drop for MmapInner {
    fn drop(&mut self) {
        let alignment = self.ptr as usize % page_size();
        unsafe {
            assert!(libc::munmap(self.ptr.offset(- (alignment as isize)),
                                 (self.len + alignment) as libc::size_t) == 0,
                    "unable to unmap mmap: {}", io::Error::last_os_error());
        }
    }
}

unsafe impl Sync for MmapInner { }
unsafe impl Send for MmapInner { }

fn page_size() -> usize {
    unsafe {
        libc::sysconf(libc::_SC_PAGESIZE) as usize
    }
}
