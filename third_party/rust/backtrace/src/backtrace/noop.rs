use std::os::raw::c_void;

#[inline(always)]
pub fn trace(_cb: &mut FnMut(&super::Frame) -> bool) {}

pub struct Frame;

impl Frame {
    pub fn ip(&self) -> *mut c_void {
        0 as *mut _
    }

    pub fn symbol_address(&self) -> *mut c_void {
        0 as *mut _
    }
}
