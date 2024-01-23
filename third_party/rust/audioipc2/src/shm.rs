// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#![allow(clippy::missing_safety_doc)]

use crate::errors::*;
use crate::PlatformHandle;
use std::{convert::TryInto, ffi::c_void, slice};

#[cfg(unix)]
pub use unix::SharedMem;
#[cfg(windows)]
pub use windows::SharedMem;

#[derive(Copy, Clone)]
struct SharedMemView {
    ptr: *mut c_void,
    size: usize,
}

unsafe impl Send for SharedMemView {}

impl SharedMemView {
    pub unsafe fn get_slice(&self, size: usize) -> Result<&[u8]> {
        let map = slice::from_raw_parts(self.ptr as _, self.size);
        if size <= self.size {
            Ok(&map[..size])
        } else {
            bail!("mmap size");
        }
    }

    pub unsafe fn get_mut_slice(&mut self, size: usize) -> Result<&mut [u8]> {
        let map = slice::from_raw_parts_mut(self.ptr as _, self.size);
        if size <= self.size {
            Ok(&mut map[..size])
        } else {
            bail!("mmap size")
        }
    }
}

#[cfg(unix)]
mod unix {
    use super::*;
    use memmap2::{MmapMut, MmapOptions};
    use std::fs::File;
    use std::os::unix::io::{AsRawFd, FromRawFd};

    #[cfg(target_os = "android")]
    fn open_shm_file(_id: &str, size: usize) -> Result<File> {
        unsafe {
            let fd = ashmem::ASharedMemory_create(std::ptr::null(), size);
            if fd >= 0 {
                // Drop PROT_EXEC
                let r = ashmem::ASharedMemory_setProt(fd, libc::PROT_READ | libc::PROT_WRITE);
                assert_eq!(r, 0);
                return Ok(File::from_raw_fd(fd.try_into().unwrap()));
            }
            Err(std::io::Error::last_os_error().into())
        }
    }

    #[cfg(not(target_os = "android"))]
    fn open_shm_file(id: &str, size: usize) -> Result<File> {
        let file = open_shm_file_impl(id)?;
        allocate_file(&file, size)?;
        Ok(file)
    }

    #[cfg(not(target_os = "android"))]
    fn open_shm_file_impl(id: &str) -> Result<File> {
        use std::env::temp_dir;
        use std::fs::{remove_file, OpenOptions};

        let id_cstring = std::ffi::CString::new(id).unwrap();

        #[cfg(target_os = "linux")]
        {
            unsafe {
                let r = libc::syscall(libc::SYS_memfd_create, id_cstring.as_ptr(), 0);
                if r >= 0 {
                    return Ok(File::from_raw_fd(r.try_into().unwrap()));
                }
            }

            let mut path = std::path::PathBuf::from("/dev/shm");
            path.push(id);

            if let Ok(file) = OpenOptions::new()
                .read(true)
                .write(true)
                .create_new(true)
                .open(&path)
            {
                let _ = remove_file(&path);
                return Ok(file);
            }
        }

        unsafe {
            let fd = libc::shm_open(
                id_cstring.as_ptr(),
                libc::O_RDWR | libc::O_CREAT | libc::O_EXCL,
                0o600,
            );
            if fd >= 0 {
                libc::shm_unlink(id_cstring.as_ptr());
                return Ok(File::from_raw_fd(fd));
            }
        }

        let mut path = temp_dir();
        path.push(id);

        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .create_new(true)
            .open(&path)?;

        let _ = remove_file(&path);
        Ok(file)
    }

    #[cfg(not(target_os = "android"))]
    fn handle_enospc(s: &str) -> Result<()> {
        let err = std::io::Error::last_os_error();
        let errno = err.raw_os_error().unwrap_or(0);
        assert_ne!(errno, 0);
        debug!("allocate_file: {} failed errno={}", s, errno);
        if errno == libc::ENOSPC {
            return Err(err.into());
        }
        Ok(())
    }

    #[cfg(not(target_os = "android"))]
    fn allocate_file(file: &File, size: usize) -> Result<()> {
        // First, set the file size.  This may create a sparse file on
        // many systems, which can fail with SIGBUS when accessed via a
        // mapping and the lazy backing allocation fails due to low disk
        // space.  To avoid this, try to force the entire file to be
        // preallocated before mapping using OS-specific approaches below.

        file.set_len(size.try_into().unwrap())?;

        let fd = file.as_raw_fd();
        let size: libc::off_t = size.try_into().unwrap();

        // Try Linux-specific fallocate.
        #[cfg(target_os = "linux")]
        {
            if unsafe { libc::fallocate(fd, 0, 0, size) } == 0 {
                return Ok(());
            }
            handle_enospc("fallocate()")?;
        }

        // Try macOS-specific fcntl.
        #[cfg(target_os = "macos")]
        {
            let params = libc::fstore_t {
                fst_flags: libc::F_ALLOCATEALL,
                fst_posmode: libc::F_PEOFPOSMODE,
                fst_offset: 0,
                fst_length: size,
                fst_bytesalloc: 0,
            };
            if unsafe { libc::fcntl(fd, libc::F_PREALLOCATE, &params) } == 0 {
                return Ok(());
            }
            handle_enospc("fcntl(F_PREALLOCATE)")?;
        }

        // Fall back to portable version, where available.
        #[cfg(any(target_os = "linux", target_os = "freebsd", target_os = "dragonfly"))]
        {
            if unsafe { libc::posix_fallocate(fd, 0, size) } == 0 {
                return Ok(());
            }
            handle_enospc("posix_fallocate()")?;
        }

        Ok(())
    }

    pub struct SharedMem {
        file: File,
        _mmap: MmapMut,
        view: SharedMemView,
    }

    impl SharedMem {
        pub fn new(id: &str, size: usize) -> Result<SharedMem> {
            let file = open_shm_file(id, size)?;
            let mut mmap = unsafe { MmapOptions::new().len(size).map_mut(&file)? };
            assert_eq!(mmap.len(), size);
            let view = SharedMemView {
                ptr: mmap.as_mut_ptr() as _,
                size,
            };
            Ok(SharedMem {
                file,
                _mmap: mmap,
                view,
            })
        }

        pub unsafe fn make_handle(&self) -> Result<PlatformHandle> {
            PlatformHandle::duplicate(self.file.as_raw_fd()).map_err(|e| e.into())
        }

        pub unsafe fn from(handle: PlatformHandle, size: usize) -> Result<SharedMem> {
            let file = File::from_raw_fd(handle.into_raw());
            let mut mmap = MmapOptions::new().len(size).map_mut(&file)?;
            assert_eq!(mmap.len(), size);
            let view = SharedMemView {
                ptr: mmap.as_mut_ptr() as _,
                size,
            };
            Ok(SharedMem {
                file,
                _mmap: mmap,
                view,
            })
        }

        pub unsafe fn get_slice(&self, size: usize) -> Result<&[u8]> {
            self.view.get_slice(size)
        }

        pub unsafe fn get_mut_slice(&mut self, size: usize) -> Result<&mut [u8]> {
            self.view.get_mut_slice(size)
        }

        pub fn get_size(&self) -> usize {
            self.view.size
        }
    }
}

#[cfg(windows)]
mod windows {
    use std::ptr;

    use windows_sys::Win32::{
        Foundation::{CloseHandle, FALSE, HANDLE, INVALID_HANDLE_VALUE},
        System::Memory::{
            CreateFileMappingA, MapViewOfFile, UnmapViewOfFile, FILE_MAP_ALL_ACCESS,
            MEMORY_MAPPED_VIEW_ADDRESS, PAGE_READWRITE,
        },
    };

    use crate::valid_handle;

    use super::*;

    pub struct SharedMem {
        handle: HANDLE,
        view: SharedMemView,
    }

    unsafe impl Send for SharedMem {}

    impl Drop for SharedMem {
        fn drop(&mut self) {
            unsafe {
                let ok = UnmapViewOfFile(MEMORY_MAPPED_VIEW_ADDRESS {
                    Value: self.view.ptr,
                });
                assert_ne!(ok, FALSE);
                let ok = CloseHandle(self.handle);
                assert_ne!(ok, FALSE);
            }
        }
    }

    impl SharedMem {
        pub fn new(_id: &str, size: usize) -> Result<SharedMem> {
            unsafe {
                let handle = CreateFileMappingA(
                    INVALID_HANDLE_VALUE,
                    ptr::null_mut(),
                    PAGE_READWRITE,
                    (size as u64 >> 32).try_into().unwrap(),
                    (size as u64 & (u32::MAX as u64)).try_into().unwrap(),
                    ptr::null(),
                );
                if !valid_handle(handle as _) {
                    return Err(std::io::Error::last_os_error().into());
                }

                let ptr = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
                if !valid_handle(ptr.Value) {
                    return Err(std::io::Error::last_os_error().into());
                }

                Ok(SharedMem {
                    handle,
                    view: SharedMemView {
                        ptr: ptr.Value,
                        size,
                    },
                })
            }
        }

        pub unsafe fn make_handle(&self) -> Result<PlatformHandle> {
            PlatformHandle::duplicate(self.handle as _).map_err(|e| e.into())
        }

        pub unsafe fn from(handle: PlatformHandle, size: usize) -> Result<SharedMem> {
            let handle = handle.into_raw();
            let ptr = MapViewOfFile(handle as _, FILE_MAP_ALL_ACCESS, 0, 0, size);
            if !valid_handle(ptr.Value) {
                return Err(std::io::Error::last_os_error().into());
            }
            Ok(SharedMem {
                handle: handle as _,
                view: SharedMemView {
                    ptr: ptr.Value,
                    size,
                },
            })
        }

        pub unsafe fn get_slice(&self, size: usize) -> Result<&[u8]> {
            self.view.get_slice(size)
        }

        pub unsafe fn get_mut_slice(&mut self, size: usize) -> Result<&mut [u8]> {
            self.view.get_mut_slice(size)
        }

        pub fn get_size(&self) -> usize {
            self.view.size
        }
    }
}
