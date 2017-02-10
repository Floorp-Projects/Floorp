use std::fs::{File, OpenOptions};
use std::io;
use std::mem;
use std::os::windows::fs::OpenOptionsExt;
use std::os::windows::io::{
    AsRawHandle, FromRawHandle, IntoRawHandle, RawHandle,
};
use std::path::Path;

use kernel32::{GetFileInformationByHandle, GetStdHandle};
use winapi::fileapi::BY_HANDLE_FILE_INFORMATION;
use winapi::minwindef::DWORD;
use winapi::winbase::{
    FILE_FLAG_BACKUP_SEMANTICS,
    STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE,
};

// For correctness, it is critical that both file handles remain open while
// their attributes are checked for equality. In particular, the file index
// numbers on a Windows stat object are not guaranteed to remain stable over
// time.
//
// See the docs and remarks on MSDN:
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa363788(v=vs.85).aspx
//
// It gets worse. It appears that the index numbers are not always
// guaranteed to be unqiue. Namely, ReFS uses 128 bit numbers for unique
// identifiers. This requires a distinct syscall to get `FILE_ID_INFO`
// documented here:
// https://msdn.microsoft.com/en-us/library/windows/desktop/hh802691(v=vs.85).aspx
//
// It seems straight-forward enough to modify this code to use
// `FILE_ID_INFO` when available (minimum Windows Server 2012), but I don't
// have access to such Windows machines.
//
// Two notes.
//
// 1. Java's NIO uses the approach implemented here and appears to ignore
//    `FILE_ID_INFO` altogether. So Java's NIO and this code are
//    susceptible to bugs when running on a file system where
//    `nFileIndex{Low,High}` are not unique.
//
// 2. LLVM has a bug where they fetch the id of a file and continue to use
//    it even after the handle has been closed, so that uniqueness is no
//    longer guaranteed (when `nFileIndex{Low,High}` are unique).
//    bug report: http://lists.llvm.org/pipermail/llvm-bugs/2014-December/037218.html
//
// All said and done, checking whether two files are the same on Windows
// seems quite tricky. Moreover, even if the code is technically incorrect,
// it seems like the chances of actually observing incorrect behavior are
// extremely small. Nevertheless, we mitigate this by checking size too.
//
// In the case where this code is erroneous, two files will be reported
// as equivalent when they are in fact distinct. This will cause the loop
// detection code to report a false positive, which will prevent descending
// into the offending directory. As far as failure modes goes, this isn't
// that bad.

#[derive(Debug)]
pub struct Handle {
    file: Option<File>,
    // If is_std is true, then we don't drop the corresponding File since it
    // will close the handle.
    is_std: bool,
    key: Option<Key>,
}

#[derive(Debug, Eq, PartialEq)]
struct Key {
    volume: DWORD,
    idx_high: DWORD,
    idx_low: DWORD,
}

impl Drop for Handle {
    fn drop(&mut self) {
        if self.is_std {
            self.file.take().unwrap().into_raw_handle();
        }
    }
}

impl Eq for Handle {}

impl PartialEq for Handle {
    fn eq(&self, other: &Handle) -> bool {
        if self.key.is_none() || other.key.is_none() {
            return false;
        }
        self.key == other.key
    }
}

impl AsRawHandle for ::Handle {
    fn as_raw_handle(&self) -> RawHandle {
        self.0.file.as_ref().take().unwrap().as_raw_handle()
    }
}

impl IntoRawHandle for ::Handle {
    fn into_raw_handle(mut self) -> RawHandle {
        self.0.file.take().unwrap().into_raw_handle()
    }
}

impl Handle {
    pub fn from_path<P: AsRef<Path>>(p: P) -> io::Result<Handle> {
        let file = try!(OpenOptions::new()
            .read(true)
            // Necessary in order to support opening directory paths.
            .custom_flags(FILE_FLAG_BACKUP_SEMANTICS)
            .open(p));
        Handle::from_file(file)
    }

    pub fn from_file(file: File) -> io::Result<Handle> {
        file_info(&file).map(|info| Handle::from_file_info(file, false, info))
    }

    fn from_std_handle(file: File) -> io::Result<Handle> {
        match file_info(&file) {
            Ok(info) => Ok(Handle::from_file_info(file, true, info)),
            // In a Windows console, if there is no pipe attached to a STD
            // handle, then GetFileInformationByHandle will return an error.
            // We don't really care. The only thing we care about is that
            // this handle is never equivalent to any other handle, which is
            // accomplished by setting key to None.
            Err(_) => Ok(Handle { file: Some(file), is_std: true, key: None }),
        }
    }

    fn from_file_info(
        file: File,
        is_std: bool,
        info: BY_HANDLE_FILE_INFORMATION,
    ) -> Handle {
        Handle {
            file: Some(file),
            is_std: is_std,
            key: Some(Key {
                volume: info.dwVolumeSerialNumber,
                idx_high: info.nFileIndexHigh,
                idx_low: info.nFileIndexLow,
            }),
        }
    }

    pub fn stdin() -> io::Result<Handle> {
        Handle::from_std_handle(unsafe {
            File::from_raw_handle(GetStdHandle(STD_INPUT_HANDLE))
        })
    }

    pub fn stdout() -> io::Result<Handle> {
        Handle::from_std_handle(unsafe {
            File::from_raw_handle(GetStdHandle(STD_OUTPUT_HANDLE))
        })
    }

    pub fn stderr() -> io::Result<Handle> {
        Handle::from_std_handle(unsafe {
            File::from_raw_handle(GetStdHandle(STD_ERROR_HANDLE))
        })
    }

    pub fn as_file(&self) -> &File {
        self.file.as_ref().take().unwrap()
    }

    pub fn as_file_mut(&mut self) -> &mut File {
        self.file.as_mut().take().unwrap()
    }
}

fn file_info(file: &File) -> io::Result<BY_HANDLE_FILE_INFORMATION> {
    let (r, info) = unsafe {
        let mut info: BY_HANDLE_FILE_INFORMATION = mem::zeroed();
        (GetFileInformationByHandle(file.as_raw_handle(), &mut info), info)
    };
    if r == 0 {
        Err(io::Error::last_os_error())
    } else {
        Ok(info)
    }
}
