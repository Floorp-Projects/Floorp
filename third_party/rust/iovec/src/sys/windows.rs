use winapi::{WSABUF, DWORD};
use std::{mem, slice, u32};

pub struct IoVec {
    inner: [u8],
}

pub const MAX_LENGTH: usize = u32::MAX as usize;

impl IoVec {
    pub fn as_ref(&self) -> &[u8] {
        unsafe {
            let vec = self.wsabuf();
            slice::from_raw_parts(vec.buf as *const u8, vec.len as usize)
        }
    }

    pub fn as_mut(&mut self) -> &mut [u8] {
        unsafe {
            let vec = self.wsabuf();
            slice::from_raw_parts_mut(vec.buf as *mut u8, vec.len as usize)
        }
    }

    unsafe fn wsabuf(&self) -> WSABUF {
        mem::transmute(&self.inner)
    }
}

impl<'a> From<&'a [u8]> for &'a IoVec {
    fn from(src: &'a [u8]) -> Self {
        assert!(src.len() <= MAX_LENGTH);

        unsafe {
            mem::transmute(WSABUF {
                buf: src.as_ptr() as *mut _,
                len: src.len() as DWORD,
            })
        }
    }
}

impl<'a> From<&'a mut [u8]> for &'a mut IoVec {
    fn from(src: &'a mut [u8]) -> Self {
        assert!(src.len() <= MAX_LENGTH);

        unsafe {
            mem::transmute(WSABUF {
                buf: src.as_ptr() as *mut _,
                len: src.len() as DWORD,
            })
        }
    }
}
