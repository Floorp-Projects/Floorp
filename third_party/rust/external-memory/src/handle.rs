#[derive(Debug)]
/// Windows handle.
pub struct Handle(*mut std::ffi::c_void);
impl From<*mut std::ffi::c_void> for Handle {
    fn from(ptr: *mut std::ffi::c_void) -> Self {
        Self(ptr)
    }
}
#[cfg(windows)]
impl std::os::windows::io::AsRawHandle for Handle {
    fn as_raw_handle(&self) -> std::os::windows::raw::HANDLE {
        self.0
    }
}
impl std::ops::Deref for Handle {
    type Target = *mut std::ffi::c_void;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
