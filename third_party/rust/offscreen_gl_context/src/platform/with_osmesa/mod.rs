use std::ffi::CString;
use std::ptr;

use osmesa_sys;
use gleam::gl;

use platform::NativeGLContextMethods;

const DUMMY_BUFFER_WIDTH: usize = 16;
const DUMMY_BUFFER_HEIGHT: usize = 16;

pub struct OSMesaContext {
    buffer: Vec<u8>,
    context: osmesa_sys::OSMesaContext,
}

pub struct OSMesaContextHandle(osmesa_sys::OSMesaContext);

unsafe impl Send for OSMesaContextHandle {}

impl OSMesaContext {
    pub fn new(share_with: Option<osmesa_sys::OSMesaContext>)
        -> Result<Self, &'static str> {
        let shared = match share_with {
            Some(ctx) => ctx,
            _ => ptr::null_mut(),
        };

        let context = unsafe {
            osmesa_sys::OSMesaCreateContext(osmesa_sys::OSMESA_RGBA, shared)
        };

        if context.is_null() {
            return Err("OSMesaCreateContext");
        }

        let buffer = vec![0u8; DUMMY_BUFFER_WIDTH * DUMMY_BUFFER_HEIGHT * 4];
        Ok(OSMesaContext {
            buffer: buffer,
            context: context,
        })
    }
}

impl NativeGLContextMethods for OSMesaContext {
    type Handle = OSMesaContextHandle;

    fn get_proc_address(addr: &str) -> *const () {
        let addr = CString::new(addr.as_bytes()).unwrap();
        let addr = addr.as_ptr();
        unsafe {
            ::std::mem::transmute(
                osmesa_sys::OSMesaGetProcAddress(addr as *const _))
        }
    }

    fn current_handle() -> Option<Self::Handle> {
        let current = unsafe { osmesa_sys::OSMesaGetCurrentContext() };
        if current.is_null() {
            None
        } else {
            Some(OSMesaContextHandle(current))
        }
    }

    fn current() -> Option<Self> {
        /* We can't access to the OSMesa buffer from here. */
        None
    }

    fn create_shared(with: Option<&Self::Handle>) -> Result<Self, &'static str> {
        Self::new(with.map(|w| w.0))
    }

    fn is_current(&self) -> bool {
        unsafe {
            osmesa_sys::OSMesaGetCurrentContext() == self.context
        }
    }

    fn handle(&self) -> Self::Handle {
        OSMesaContextHandle(self.context)
    }

    fn make_current(&self) -> Result<(), &'static str> {
        unsafe {
            if !self.is_current() &&
               osmesa_sys::OSMesaMakeCurrent(self.context,
                                             self.buffer.as_ptr() as *const _ as *mut _,
                                             gl::UNSIGNED_BYTE,
                                             DUMMY_BUFFER_WIDTH as i32,
                                             DUMMY_BUFFER_HEIGHT as i32) == 0 {
               Err("OSMesaMakeCurrent")
           } else {
               Ok(())
           }
        }
    }

    fn unbind(&self) -> Result<(), &'static str> {
        // OSMesa doesn't allow any API to unbind a context before [1], and just
        // bails out on null context, buffer, or whatever, so not much we can do
        // here. Thus, ignore the failure and just flush the context if we're
        // using an old OSMesa version.
        //
        // [1]: https://www.mail-archive.com/mesa-dev@lists.freedesktop.org/msg128408.html
        if self.is_current() {
            let ret = unsafe {
                osmesa_sys::OSMesaMakeCurrent(ptr::null_mut(),
                                              ptr::null_mut(), 0, 0, 0)
            };
            if ret == gl::FALSE {
                gl::flush();
            }
        }

        Ok(())
    }
}

impl Drop for OSMesaContext {
    fn drop(&mut self) {
        unsafe { osmesa_sys::OSMesaDestroyContext(self.context) }
    }
}
