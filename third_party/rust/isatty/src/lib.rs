// Based on:
//  - https://github.com/rust-lang/cargo/blob/099ad28104fe319f493dc42e0c694d468c65767d/src/cargo/lib.rs#L154-L178
//  - https://github.com/BurntSushi/ripgrep/issues/94#issuecomment-261761687

pub fn stdout_isatty() -> bool {
    isatty(stream::Stream::Stdout)
}

pub fn stderr_isatty() -> bool {
    isatty(stream::Stream::Stderr)
}

mod stream {
    pub enum Stream {
        Stdout,
        Stderr,
    }
}

#[cfg(unix)]
use unix::isatty;
#[cfg(unix)]
mod unix {
    use stream::Stream;

    pub fn isatty(stream: Stream) -> bool {
        extern crate libc;

        let fd = match stream {
            Stream::Stdout => libc::STDOUT_FILENO,
            Stream::Stderr => libc::STDERR_FILENO,
        };

        unsafe { libc::isatty(fd) != 0 }
    }
}

#[cfg(windows)]
use windows::isatty;
#[cfg(windows)]
mod windows {
    extern crate kernel32;
    extern crate winapi;

    use stream::Stream;

    pub fn isatty(stream: Stream) -> bool {
        let handle = match stream {
            Stream::Stdout => winapi::winbase::STD_OUTPUT_HANDLE,
            Stream::Stderr => winapi::winbase::STD_ERROR_HANDLE,
        };

        unsafe {
            let handle = kernel32::GetStdHandle(handle);

            // check for msys/cygwin
            if is_cygwin_pty(handle) {
                return true;
            }

            let mut out = 0;
            kernel32::GetConsoleMode(handle, &mut out) != 0
        }
    }

    /// Returns true if there is an MSYS/cygwin tty on the given handle.
    fn is_cygwin_pty(handle: winapi::HANDLE) -> bool {
        use std::ffi::OsString;
        use std::mem;
        use std::os::raw::c_void;
        use std::os::windows::ffi::OsStringExt;
        use std::slice;

        use self::kernel32::GetFileInformationByHandleEx;
        use self::winapi::fileapi::FILE_NAME_INFO;
        use self::winapi::minwinbase::FileNameInfo;
        use self::winapi::minwindef::MAX_PATH;

        unsafe {
            let size = mem::size_of::<FILE_NAME_INFO>();
            let mut name_info_bytes = vec![0u8; size + MAX_PATH];
            let res = GetFileInformationByHandleEx(handle,
                                                FileNameInfo,
                                                &mut *name_info_bytes as *mut _ as *mut c_void,
                                                name_info_bytes.len() as u32);
            if res == 0 {
                return true;
            }
            let name_info: FILE_NAME_INFO = *(name_info_bytes[0..size]
                .as_ptr() as *const FILE_NAME_INFO);
            let name_bytes = &name_info_bytes[size..size + name_info.FileNameLength as usize];
            let name_u16 = slice::from_raw_parts(name_bytes.as_ptr() as *const u16,
                                                name_bytes.len() / 2);
            let name = OsString::from_wide(name_u16)
                .as_os_str()
                .to_string_lossy()
                .into_owned();
            name.contains("msys-") || name.contains("-pty")
        }
    }
}
