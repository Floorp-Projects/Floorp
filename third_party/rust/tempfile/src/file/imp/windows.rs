use std::fs::File;
use std::io;
use std::os::windows::ffi::OsStrExt;
use std::os::windows::io::{AsRawHandle, FromRawHandle, RawHandle};
use std::path::Path;
use std::ptr;

use winapi::shared::minwindef::DWORD;
use winapi::um::fileapi::{CreateFileW, SetFileAttributesW, CREATE_NEW};
use winapi::um::handleapi::INVALID_HANDLE_VALUE;
use winapi::um::winbase::{FILE_FLAG_DELETE_ON_CLOSE, MOVEFILE_REPLACE_EXISTING};
use winapi::um::winbase::{MoveFileExW, ReOpenFile};
use winapi::um::winnt::{FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_TEMPORARY};
use winapi::um::winnt::{FILE_GENERIC_READ, FILE_GENERIC_WRITE, HANDLE};
use winapi::um::winnt::{FILE_SHARE_DELETE, FILE_SHARE_READ, FILE_SHARE_WRITE};

use util;

#[cfg_attr(irustfmt, rustfmt_skip)]
const ACCESS: DWORD     = FILE_GENERIC_READ
                        | FILE_GENERIC_WRITE;
#[cfg_attr(irustfmt, rustfmt_skip)]
const SHARE_MODE: DWORD = FILE_SHARE_DELETE
                        | FILE_SHARE_READ
                        | FILE_SHARE_WRITE;
#[cfg_attr(irustfmt, rustfmt_skip)]
const FLAGS: DWORD      = FILE_ATTRIBUTE_HIDDEN
                        | FILE_ATTRIBUTE_TEMPORARY;

fn to_utf16(s: &Path) -> Vec<u16> {
    s.as_os_str()
        .encode_wide()
        .chain(Some(0).into_iter())
        .collect()
}

fn win_create(
    path: &Path,
    access: DWORD,
    share_mode: DWORD,
    disp: DWORD,
    flags: DWORD,
) -> io::Result<File> {
    let path = to_utf16(path);
    let handle = unsafe {
        CreateFileW(
            path.as_ptr(),
            access,
            share_mode,
            0 as *mut _,
            disp,
            flags,
            ptr::null_mut(),
        )
    };
    if handle == INVALID_HANDLE_VALUE {
        Err(io::Error::last_os_error())
    } else {
        Ok(unsafe { File::from_raw_handle(handle as RawHandle) })
    }
}

pub fn create_named(path: &Path) -> io::Result<File> {
    win_create(path, ACCESS, SHARE_MODE, CREATE_NEW, FLAGS)
}

pub fn create(dir: &Path) -> io::Result<File> {
    util::create_helper(dir, ".tmp", "", ::NUM_RAND_CHARS, |path| {
        win_create(
            &path,
            ACCESS,
            0, // Exclusive
            CREATE_NEW,
            FLAGS | FILE_FLAG_DELETE_ON_CLOSE,
        )
    })
}

pub fn reopen(file: &File, _path: &Path) -> io::Result<File> {
    let handle = file.as_raw_handle();
    unsafe {
        let handle = ReOpenFile(handle as HANDLE, ACCESS, SHARE_MODE, 0);
        if handle == INVALID_HANDLE_VALUE {
            Err(io::Error::last_os_error())
        } else {
            Ok(FromRawHandle::from_raw_handle(handle as RawHandle))
        }
    }
}

pub fn persist(old_path: &Path, new_path: &Path, overwrite: bool) -> io::Result<()> {
    // TODO: We should probably do this in one-shot using SetFileInformationByHandle but the API is
    // really painful.

    unsafe {
        let old_path_w = to_utf16(old_path);
        let new_path_w = to_utf16(new_path);

        // Don't succeed if this fails. We don't want to claim to have successfully persisted a file
        // still marked as temporary because this file won't have the same consistency guarantees.
        if SetFileAttributesW(old_path_w.as_ptr(), FILE_ATTRIBUTE_NORMAL) == 0 {
            return Err(io::Error::last_os_error());
        }

        let mut flags = 0;

        if overwrite {
            flags |= MOVEFILE_REPLACE_EXISTING;
        }

        if MoveFileExW(old_path_w.as_ptr(), new_path_w.as_ptr(), flags) == 0 {
            let e = io::Error::last_os_error();
            // If this fails, the temporary file is now un-hidden and no longer marked temporary
            // (slightly less efficient) but it will still work.
            let _ = SetFileAttributesW(old_path_w.as_ptr(), FLAGS);
            Err(e)
        } else {
            Ok(())
        }
    }
}
