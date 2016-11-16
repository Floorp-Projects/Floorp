use cgl::*;
use std::mem;

use core_foundation::bundle::{CFBundleGetBundleWithIdentifier, CFBundleGetFunctionPointerForName};
use core_foundation::base::TCFType;
use core_foundation::string::CFString;
use std::str::FromStr;
use std::sync::Mutex;

use platform::NativeGLContextMethods;

lazy_static! {
    static ref CHOOSE_PIXEL_FORMAT_MUTEX: Mutex<()> = Mutex::new(());
}

pub struct NativeGLContextHandle(CGLContextObj);

unsafe impl Send for NativeGLContextHandle {}

pub struct NativeGLContext {
    native_context: CGLContextObj,
    weak: bool,
}

impl NativeGLContext {
    pub fn new(share_context: Option<&CGLContextObj>,
               pixel_format: &CGLPixelFormatObj)
        -> Result<NativeGLContext, &'static str> {

        let shared = match share_context {
            Some(ctx) => *ctx,
            None => 0 as CGLContextObj
        };

        let mut native: CGLContextObj = unsafe { mem::uninitialized() };

        unsafe {
            if CGLCreateContext(*pixel_format, shared, &mut native) != 0 {
                return Err("CGLCreateContext");
            }
        }

        debug_assert!(native != 0 as CGLContextObj);

        Ok(NativeGLContext {
            native_context: native,
            weak: false,
        })
    }
}

impl Drop for NativeGLContext {
    fn drop(&mut self) {
        let _ = self.unbind();
        if !self.weak {
            unsafe {
                if CGLDestroyContext(self.native_context) != 0 {
                    debug!("CGLDestroyContext returned an error");
                }
            }
        }
    }
}

impl NativeGLContextMethods for NativeGLContext {
    type Handle = NativeGLContextHandle;

    fn get_proc_address(addr: &str) -> *const () {
        let symbol_name: CFString = FromStr::from_str(addr).unwrap();
        let framework_name: CFString = FromStr::from_str("com.apple.opengl").unwrap();
        let framework = unsafe {
            CFBundleGetBundleWithIdentifier(framework_name.as_concrete_TypeRef())
        };
        let symbol = unsafe {
            CFBundleGetFunctionPointerForName(framework, symbol_name.as_concrete_TypeRef())
        };
        symbol as *const ()
    }

    fn current() -> Option<Self> {
        if let Some(handle) = Self::current_handle() {
            Some(NativeGLContext {
                native_context: handle.0,
                weak: true,
            })
        } else {
            None
        }

    }

    fn current_handle() -> Option<Self::Handle> {
        let current = unsafe { CGLGetCurrentContext() };
        if current != 0 as CGLContextObj {
            Some(NativeGLContextHandle(current))
        } else {
            None
        }
    }

    fn create_shared(with: Option<&Self::Handle>) -> Result<NativeGLContext, &'static str> {
        // CGLChoosePixelFormat fails if multiple threads try to open a display connection
        // simultaneously. The following error is returned by CGLChoosePixelFormat: 
        // kCGLBadConnection - Invalid connection to Core Graphics.
        // We use a static mutex guard to fix this issue
        let _guard = CHOOSE_PIXEL_FORMAT_MUTEX.lock().unwrap();

        let mut attributes = [
            0
        ];

        let mut pixel_format : CGLPixelFormatObj = unsafe { mem::uninitialized() };
        let mut pix_count = 0;

        unsafe {
            if CGLChoosePixelFormat(attributes.as_mut_ptr(), &mut pixel_format, &mut pix_count) != 0 {
                return Err("CGLChoosePixelFormat");
            }

            if pix_count == 0 {
                return Err("No pixel formats available");
            }
        }

        let result = NativeGLContext::new(with.map(|handle| &handle.0), &pixel_format);

        unsafe {
            if CGLDestroyPixelFormat(pixel_format) != 0 {
                debug!("CGLDestroyPixelformat errored");
            }
        }

        result
    }

    fn handle(&self) -> Self::Handle {
        NativeGLContextHandle(self.native_context)
    }

    #[inline(always)]
    fn is_current(&self) -> bool {
        unsafe {
            CGLGetCurrentContext() == self.native_context
        }
    }

    fn make_current(&self) -> Result<(), &'static str> {
        unsafe {
            if !self.is_current() &&
               CGLSetCurrentContext(self.native_context) != 0 {
                    Err("CGLSetCurrentContext")
            } else {
                Ok(())
            }
        }
    }

    fn unbind(&self) -> Result<(), &'static str> {
        unsafe {
            if self.is_current() &&
               CGLSetCurrentContext(0 as CGLContextObj) != 0 {
                Err("CGLSetCurrentContext (on unbind)")
            } else {
                Ok(())
            }
        }
    }
}
