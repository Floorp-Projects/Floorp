use libc::c_char;
use std::{ffi, ptr};

extern "C" {
    fn get_os_release() -> *const c_char;
    fn str_free(ptr: *const c_char);
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
        let rp = get_os_release();
        if rp == ptr::null() {
            None
        } else {
            let typ = ffi::CStr::from_ptr(rp);
            let res = Some(typ.to_string_lossy().into_owned());
            str_free(rp);
            res
        }
    }
}
