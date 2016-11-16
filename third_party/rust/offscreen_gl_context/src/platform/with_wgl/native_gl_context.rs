use platform::NativeGLContextMethods;
use gl_context::GLContextDispatcher;
use std::ffi::CString;
use std::os::raw::c_void;
use std::ptr;
use std::sync::mpsc;

use winapi;
use user32;
use kernel32;
use super::wgl;
use super::wgl_attributes::*;
use super::utils;

// Wrappers to satisfy `Sync`.
struct HMODULEWrapper(winapi::HMODULE);
unsafe impl Sync for HMODULEWrapper {}

lazy_static! {
    static ref GL_LIB: Option<HMODULEWrapper>  = {
        let p = unsafe { kernel32::LoadLibraryA(b"opengl32.dll\0".as_ptr() as *const _) };
        if p.is_null() {
            error!("WGL: opengl32.dll not found!");
            None
        } else {
            debug!("WGL: opengl32.dll loaded!");
            Some(HMODULEWrapper(p))
        }
    };

    static ref PROC_ADDR_CTX: Option<NativeGLContext> = {
        match unsafe { utils::create_offscreen(ptr::null_mut(), &WGLAttributes::default()) } {
            Ok(ref res) => {
                let ctx = NativeGLContext {
                    render_ctx: res.0,
                    device_ctx: res.1,
                    weak: false,
                };
                Some(ctx)
            }
            Err(s) => {
                error!("Error creating GetProcAddress helper context: {}", s);
                None
            }
        }
    };
}

pub struct NativeGLContext {
    render_ctx: winapi::HGLRC,
    device_ctx: winapi::HDC,
    weak: bool,
}

impl Drop for NativeGLContext {
    fn drop(&mut self) {
        unsafe {
            if !self.weak {
                // the context to be deleted needs to be unbound
                self.unbind().unwrap();
                wgl::DeleteContext(self.render_ctx as *const _);
                let window = user32::WindowFromDC(self.device_ctx);
                debug_assert!(!window.is_null());
                user32::ReleaseDC(window, self.device_ctx);
                user32::DestroyWindow(window);
            }
        }
    }
}

unsafe impl Send for NativeGLContext {}
unsafe impl Sync for NativeGLContext {}

pub struct NativeGLContextHandle(winapi::HGLRC, winapi::HDC);
unsafe impl Send for NativeGLContextHandle {}
unsafe impl Sync for NativeGLContextHandle {}

impl NativeGLContextMethods for NativeGLContext {
    type Handle = NativeGLContextHandle;

    fn get_proc_address(addr: &str) -> *const () {
        let addr = CString::new(addr.as_bytes()).unwrap();
        let addr = addr.as_ptr();
        unsafe {
            if wgl::GetCurrentContext().is_null() {
                // wglGetProcAddress only works in the presence of a valid GL context
                // We use a dummy ctx when the caller calls this function without a valid GL context
                if let Some(ref ctx) = *PROC_ADDR_CTX {
                    if ctx.make_current().is_err() {
                        return ptr::null_mut();
                    }
                } else {
                    return ptr::null_mut();
                }
            }

            let p = wgl::GetProcAddress(addr) as *const _;
            if !p.is_null() {
                return p;
            }
            // wglGetProcAddress​ doesn't return function pointers for some legacy functions,
            // (the ones coming from OpenGL 1.1)
            // These functions are exported by the opengl32.dll itself,
            // so we have to fallback to kernel32 getProcAddress if wglGetProcAddress​ return null
            match *GL_LIB {
                Some(ref lib) => kernel32::GetProcAddress(lib.0, addr) as *const _,
                None => ptr::null_mut(),
            }
        }
    }

    fn create_shared(with: Option<&Self::Handle>) -> Result<NativeGLContext, &'static str> {
        Self::create_shared_with_dispatcher(with, None)
    }

    fn create_shared_with_dispatcher(with: Option<&Self::Handle>, dispatcher: Option<Box<GLContextDispatcher>>)
        -> Result<NativeGLContext, &'static str> {
        let (render_ctx, device_ctx) = match with {
            Some(ref handle) => (handle.0, handle.1),
            None => (ptr::null_mut(), ptr::null_mut())
        };

        if let Some(ref dispatcher) = dispatcher {
            // wglShareLists fails if the context to share is current in a different thread.
            // Additionally wglMakeCurrent cannot 'steal' a context that is current in other thread, so
            // we have to unbind the shared context in its own thread, call wglShareLists in this thread
            // and bind the original share context again when the wglShareList is completed.
            let (tx, rx) = mpsc::channel();
            dispatcher.dispatch(Box::new(move || {
                unsafe {
                    if wgl::MakeCurrent(ptr::null_mut(), ptr::null_mut()) == 0 {
                        error!("WGL MakeCurrent failed");
                    }
                }
                tx.send(()).unwrap();
            }));
            // Wait until wglMakeCurrent operation is completed in the thread of the shared context
            rx.recv().unwrap();
        }

        match unsafe { utils::create_offscreen(render_ctx, &WGLAttributes::default()) } {
            Ok(ref res) => {
                let ctx = NativeGLContext {
                    render_ctx: res.0,
                    device_ctx: res.1,
                    weak: false,
                };

                // Restore shared context
                if let Some(ref dispatcher) = dispatcher {
                    let (tx, rx) = mpsc::channel();
                    let handle = NativeGLContextHandle(render_ctx, device_ctx);
                    dispatcher.dispatch(Box::new(move || {
                        unsafe { 
                            if wgl::MakeCurrent(handle.1 as *const _, handle.0 as *const _) == 0 {
                                error!("WGL MakeCurrent failed!");
                            } 
                        };
                        tx.send(()).unwrap();
                    }));
                    // Wait until wglMakeCurrent operation is completed in the thread of the shared context
                    rx.recv().unwrap();
                }

                Ok(ctx)
            }
            Err(s) => {
                error!("WGL: {}", s);
                Err("Error creating WGL context")
            }
        }
    }

    fn is_current(&self) -> bool {
        unsafe { wgl::GetCurrentContext() == self.render_ctx as *const c_void }
    }

    fn current() -> Option<Self> {
        if let Some(handle) = Self::current_handle() {
            Some(NativeGLContext {
                render_ctx: handle.0,
                device_ctx: handle.1,
                weak: true,
            })
        } else {
            None
        }
    }

    fn current_handle() -> Option<Self::Handle> {
        let ctx = unsafe { wgl::GetCurrentContext() };
        let hdc = unsafe { wgl::GetCurrentDC() };
        if ctx.is_null() || hdc.is_null() {
            None
        } else {
            Some(NativeGLContextHandle(ctx as winapi::HGLRC, hdc as winapi::HDC))
        }
    }

    fn make_current(&self) -> Result<(), &'static str> {
        unsafe {
            if wgl::MakeCurrent(self.device_ctx as *const _, self.render_ctx as *const _) != 0 {
                Ok(())
            } else {
                Err("WGL::makeCurrent failed")
            }
        }
    }

    fn unbind(&self) -> Result<(), &'static str> {
        unsafe {
            if self.is_current() && wgl::MakeCurrent(ptr::null_mut(), ptr::null_mut()) == 0 {
                Err("WGL::MakeCurrent (on unbind)")
            } else {
                Ok(())
            }
        }
    }

    fn handle(&self) -> Self::Handle {
        NativeGLContextHandle(self.render_ctx, self.device_ctx)
    }
}
