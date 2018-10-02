extern crate libc;

pub unsafe fn allocate(size: usize, align: usize) -> *mut u8 {
    assert!(align <= 16);
    libc::malloc(size) as *mut _
}

pub unsafe fn deallocate(ptr: *mut u8, _size: usize, _align: usize) {
    libc::free(ptr as *mut _);
}

pub unsafe fn reallocate(ptr: *mut u8, _old_size: usize, size: usize, align: usize) -> *mut u8 {
    assert!(align <= 16);
    libc::realloc(ptr as *mut _, size) as *mut _
}
