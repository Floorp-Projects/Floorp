#[derive(Debug)]
/// Pointer to a host allocated memory.
pub struct Ptr(*mut std::ffi::c_void);
impl Ptr {
    /// Get the inner ptr
    pub fn as_raw_ptr(&self) -> *mut std::ffi::c_void {
        self.0
    }
}
impl<T> From<*mut T> for Ptr {
    fn from(ptr: *mut T) -> Self {
        Self(unsafe { std::mem::transmute(ptr) })
    }
}
impl std::ops::Deref for Ptr {
    type Target = *mut std::ffi::c_void;
    fn deref(&self) -> &Self::Target {
        &self.0
    }
}
