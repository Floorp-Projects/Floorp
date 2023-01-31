use libc::{c_char, c_int};

extern "C" {
    fn get_os_release(outbuf: *const c_char, outlen: usize) -> c_int;
    fn get_build_number() -> c_int;
}

/// Get the version of the currently running kernel.
///
/// **Note**: On Windows 8 and later this will report the Windows 8 OS version value `6.2`,
/// unless the final application is build to explicitly target Windows 10.
/// See [`GetVersionEx`] for details.
///
/// [`GetVersionEx`]: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getversionexa
///
///
/// Returns `None` if an error occured.
pub fn kernel_version() -> Option<String> {
    unsafe {
        // Windows 10 should report "10.0", which is 4 bytes,
        // "unknown" is 7 bytes.
        // We need to account for the null byte.
        let size = 8;
        let mut buf = vec![0; size];
        let written = get_os_release(buf.as_mut_ptr() as _, size) as usize;
        match written {
            0 => None,
            _ => Some(String::from_utf8_lossy(&buf[0..written]).into_owned()),
        }
    }
}

/// Get the build number from Windows.
///
/// **Note**: On Windows 8 and later this may report a Windows 8 build number,
/// for example 9200, unless the final application is build to explicitly
/// target Windows 10.
/// See [`GetVersionEx`] for details.
///
/// [`GetVersionEx`]: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getversionexa
///
///
/// Returns `None` if an error occured.
pub fn windows_build_number() -> Option<i32> {
    unsafe {
        // Get windows build number
        let build_number = get_build_number();
        match build_number {
            0 => None,
            _ => Some(build_number),
        }
    }
}
