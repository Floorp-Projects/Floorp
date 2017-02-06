use euclid::Size2D;
use platform::NativeGLContextMethods;
use platform::with_egl::utils::{create_pixel_buffer_backed_offscreen_context};
use std::ffi::CString;
use egl;
use egl::types::{EGLint, EGLBoolean, EGLDisplay, EGLSurface, EGLConfig, EGLContext};

pub struct NativeGLContextHandle(pub EGLDisplay, pub EGLSurface);
unsafe impl Send for NativeGLContextHandle {}

pub struct NativeGLContext {
    native_display: EGLDisplay,
    native_surface: EGLSurface,
    native_context: EGLContext,
    weak: bool,
}

impl NativeGLContext {
    pub fn new(share_context: Option<&EGLContext>,
               display: EGLDisplay,
               surface: EGLSurface,
               config: EGLConfig)
        -> Result<NativeGLContext, &'static str> {

        let shared = match share_context {
            Some(ctx) => *ctx,
            None => egl::NO_CONTEXT as EGLContext,
        };

        let attributes = [
            egl::CONTEXT_CLIENT_VERSION as EGLint, 2,
            egl::NONE as EGLint, 0, 0, 0, // see mod.rs
        ];

        let ctx =  unsafe { egl::CreateContext(display, config, shared, attributes.as_ptr()) };

        // TODO: Check for every type of error possible, not just client error?
        // Note if we do it we must do it too on egl::CreatePBufferSurface, etc...
        if ctx == (egl::NO_CONTEXT as EGLContext) {
            unsafe { egl::DestroySurface(display, surface) };
            return Err("Error creating an EGL context");
        }

        Ok(NativeGLContext {
            native_display: display,
            native_surface: surface,
            native_context: ctx,
            weak: false,
        })
    }
}

impl Drop for NativeGLContext {
    fn drop(&mut self) {
        let _ = self.unbind();
        if !self.weak {
            unsafe {
                if egl::DestroySurface(self.native_display, self.native_surface) == 0 {
                    debug!("egl::DestroySurface failed");
                }
                if egl::DestroyContext(self.native_display, self.native_context) == 0 {
                    debug!("egl::DestroyContext failed");
                }
            }
        }
    }
}

impl NativeGLContextMethods for NativeGLContext {
    type Handle = NativeGLContextHandle;

    fn get_proc_address(addr: &str) -> *const () {
        unsafe {
            let addr = CString::new(addr.as_bytes()).unwrap().as_ptr();
            egl::GetProcAddress(addr as *const _) as *const ()
        }
    }

    fn create_headless() -> Result<NativeGLContext, &'static str> {
        // We create a context with a dummy size, we can't rely on a
        // default framebuffer
        create_pixel_buffer_backed_offscreen_context(Size2D::new(16, 16), None)
    }

    fn create_shared(with: Option<&Self::Handle>) -> Result<NativeGLContext, &'static str> {
        create_pixel_buffer_backed_offscreen_context(Size2D::new(16, 16), with)
    }

    fn current_handle() -> Option<Self::Handle> {
        let native_context = unsafe { egl::GetCurrentContext() };
        let native_display = unsafe { egl::GetCurrentDisplay() };

        if native_context != egl::NO_CONTEXT && native_display != egl::NO_DISPLAY {
            Some(NativeGLContextHandle(native_context, native_display))
        } else {
            None
        }
    }


    fn current() -> Option<Self> {
        if let Some(handle) = Self::current_handle() {
            let surface = unsafe { egl::GetCurrentSurface(egl::DRAW as EGLint) };

            debug_assert!(surface != egl::NO_SURFACE);

            Some(NativeGLContext {
                native_context: handle.0,
                native_display: handle.1,
                native_surface: surface,
                weak: true,
            })
        } else {
            None
        }
    }

    #[inline(always)]
    fn is_current(&self) -> bool {
        unsafe {
            egl::GetCurrentContext() == self.native_context
        }
    }

    fn make_current(&self) -> Result<(), &'static str> {
        unsafe {
            if !self.is_current() &&
                egl::MakeCurrent(self.native_display,
                                 self.native_surface,
                                 self.native_surface,
                                 self.native_context) == (egl::FALSE as EGLBoolean) {
                Err("egl::MakeCurrent")
            } else {
                Ok(())
            }
        }
    }

    fn handle(&self) -> Self::Handle {
        NativeGLContextHandle(self.native_context, self.native_display)
    }

    fn unbind(&self) -> Result<(), &'static str> {
        unsafe {
            if self.is_current() &&
               egl::MakeCurrent(self.native_display,
                                egl::NO_SURFACE as EGLSurface,
                                egl::NO_SURFACE as EGLSurface,
                                egl::NO_CONTEXT as EGLContext) == (egl::FALSE as EGLBoolean) {
                Err("egl::MakeCurrent (on unbind)")
            } else {
                Ok(())
            }
        }
    }
}
