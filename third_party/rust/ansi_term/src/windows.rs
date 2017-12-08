/// Enables ANSI code support on Windows 10.
///
/// This uses Windows API calls to alter the properties of the console that
/// the program is running in.
///
/// https://msdn.microsoft.com/en-us/library/windows/desktop/mt638032(v=vs.85).aspx
///
/// Returns a `Result` with the Windows error code if unsuccessful.
#[cfg(windows)]
pub fn enable_ansi_support() -> Result<(), u64> {

    #[link(name = "kernel32")]
    extern {
        fn GetStdHandle(handle: u64) -> *const i32;
        fn SetConsoleMode(handle: *const i32, mode: u32) -> bool;
        fn GetLastError() -> u64;
    }

    unsafe {
        const STD_OUT_HANDLE: u64 = -11i32 as u64;
        const ENABLE_ANSI_CODES: u32 = 7;

        // https://msdn.microsoft.com/en-us/library/windows/desktop/ms683231(v=vs.85).aspx
        let std_out_handle = GetStdHandle(STD_OUT_HANDLE);
        let error_code = GetLastError();
        if error_code != 0 { return Err(error_code); }

        // https://msdn.microsoft.com/en-us/library/windows/desktop/ms686033(v=vs.85).aspx
        SetConsoleMode(std_out_handle, ENABLE_ANSI_CODES);
        let error_code = GetLastError();
        if error_code != 0 { return Err(error_code); }
    }

    return Ok(());
}
