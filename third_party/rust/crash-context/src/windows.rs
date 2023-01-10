pub mod ffi;

/// Full Windows crash context
pub struct CrashContext {
    /// The information on the exception.
    ///
    /// Note that this is a pointer into the actual memory of the crashed process,
    /// and is a pointer to an [EXCEPTION_POINTERS](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_pointers)
    pub exception_pointers: *const ffi::EXCEPTION_POINTERS,
    /// The top level exception code from the exception_pointers. This is provided
    /// so that external processes don't need to use `ReadProcessMemory` to inspect
    /// the exception code
    pub exception_code: u32,
    /// The pid of the process that crashed
    pub process_id: u32,
    /// The thread id on which the exception occurred
    pub thread_id: u32,
}
