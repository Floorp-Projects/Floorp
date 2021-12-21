// A lifetime-managed wrapper for callback functions
#[derive(Debug, PartialEq)]
pub struct BoxedCallback<T>(*mut Box<dyn FnMut(&T)>);

impl<T> BoxedCallback<T> {
    pub fn new<F: FnMut(&T) + Send + 'static>(f: F) -> BoxedCallback<T> {
        BoxedCallback(Box::into_raw(Box::new(Box::new(f))))
    }

    pub fn null() -> BoxedCallback<T> {
        BoxedCallback(::std::ptr::null_mut())
    }

    pub fn raw_ptr(&mut self) -> *mut ::std::os::raw::c_void {
        self.0 as *mut ::std::os::raw::c_void
    }

    // must not be null
    pub unsafe fn call_from_raw_ptr(raw_ptr: *mut ::std::os::raw::c_void, arg: &T) {
        let callback = &mut *(raw_ptr as *mut Box<dyn FnMut(&T)>);
        callback(arg);
    }
}

unsafe impl<T> Send for BoxedCallback<T> {}

impl<T> Drop for BoxedCallback<T> {
    fn drop(&mut self) {
        if !self.0.is_null() {
            unsafe {
                let _ = Box::from_raw(self.0);
            }
        }
    }
}
