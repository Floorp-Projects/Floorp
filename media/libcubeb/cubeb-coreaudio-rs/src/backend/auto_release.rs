use std::fmt;

pub struct AutoRelease<T> {
    ptr: *mut T,
    release_func: unsafe extern "C" fn(*mut T),
}

impl<T> AutoRelease<T> {
    pub fn new(ptr: *mut T, release_func: unsafe extern "C" fn(*mut T)) -> Self {
        Self { ptr, release_func }
    }

    pub fn reset(&mut self, ptr: *mut T) {
        self.release();
        self.ptr = ptr;
    }

    pub fn as_ref(&self) -> &T {
        assert!(!self.ptr.is_null());
        unsafe { &*self.ptr }
    }

    pub fn as_mut(&mut self) -> &mut T {
        assert!(!self.ptr.is_null());
        unsafe { &mut *self.ptr }
    }

    pub fn as_ptr(&self) -> *const T {
        self.ptr
    }

    fn release(&self) {
        if !self.ptr.is_null() {
            unsafe {
                (self.release_func)(self.ptr);
            }
        }
    }
}

impl<T> Drop for AutoRelease<T> {
    fn drop(&mut self) {
        self.release();
    }
}

// Explicit Debug impl to work for the type T
// that doesn't implement Debug trait.
impl<T> fmt::Debug for AutoRelease<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("AutoRelease")
            .field("ptr", &self.ptr)
            .field("release_func", &self.release_func)
            .finish()
    }
}

#[test]
fn test_auto_release() {
    use std::mem;
    use std::ptr;

    unsafe extern "C" fn allocate() -> *mut libc::c_void {
        // println!("Allocate!");
        libc::calloc(1, mem::size_of::<u32>())
    }

    unsafe extern "C" fn deallocate(ptr: *mut libc::c_void) {
        // println!("Deallocate!");
        libc::free(ptr);
    }

    let mut auto_release = AutoRelease::new(ptr::null_mut(), deallocate);
    let ptr = unsafe { allocate() };
    auto_release.reset(ptr);
    assert_eq!(auto_release.as_ptr(), ptr);
}
