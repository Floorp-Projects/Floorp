use glx;
use x11::xlib::*;
use glx::types::GLXDrawable;
use std::os::raw::*;
use euclid::Size2D;

use NativeGLContext;
use NativeGLContextHandle;

pub struct ScopedXFree<T> {
    ptr: *mut T
}

impl<T> ScopedXFree<T> {
    #[inline(always)]
    pub fn new(ptr: *mut T) -> ScopedXFree<T> {
        ScopedXFree {
            ptr: ptr
        }
    }

    #[inline(always)]
    pub fn as_ptr(&self) -> *mut T {
        self.ptr
    }
}

impl<T> Drop for ScopedXFree<T> {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { XFree(self.ptr as *mut _); };
        }
    }
}

unsafe fn get_visual_and_depth(s: *mut Screen, id: VisualID) -> Result<(*mut Visual, c_int), &'static str> {
    for d in 0..((*s).ndepths as isize) {
        let depth_info : *mut Depth = (*s).depths.offset(d);
        for v in 0..((*depth_info).nvisuals as isize) {
            let visual : *mut Visual = (*depth_info).visuals.offset(v);
            if (*visual).visualid == id {
                return Ok((visual, (*depth_info).depth));
            }
        }
    }

    Err("Visual not on screen")
}

// Almost directly ported from
// https://dxr.mozilla.org/mozilla-central/source/gfx/gl/GLContextProviderGLX.cpp
pub fn create_offscreen_pixmap_backed_context(size: Size2D<i32>, shared_with: Option<&NativeGLContextHandle>) -> Result<NativeGLContext, &'static str> {
    let (shared_with, dpy) = match shared_with {
        Some(handle) => (Some(&handle.0), handle.1),
        None => {
            let dpy = unsafe { XOpenDisplay(0 as *mut c_char) as *mut glx::types::Display };

            if dpy.is_null() {
                return Err("glx::XOpenDisplay");
            }

            (None, dpy)
        }
    };


    // We try to get possible framebuffer configurations which
    // can be pixmap-backed and renderable
    let mut attributes = [
        glx::DRAWABLE_TYPE as c_int, glx::PIXMAP_BIT as c_int,
        glx::X_RENDERABLE as c_int, 1,
        0 as c_int
    ];

    let mut config_count : c_int = 0;

    let configs = ScopedXFree::new(unsafe {
        glx::ChooseFBConfig(dpy,
                            XDefaultScreen(dpy as *mut Display),
                            attributes.as_mut_ptr(),
                            &mut config_count)
    });

    if configs.as_ptr().is_null() {
        return Err("glx::ChooseFBConfig");
    }

    debug_assert!(config_count > 0);

    let mut config_index = 0;
    let mut visual_id = glx::NONE as c_int;
    for i in 0..(config_count as isize) {
        unsafe {
            let config = *configs.as_ptr().offset(i);
            let mut drawable_type : c_int = 0;

            // NOTE: glx's `Success` is unreachable from bindings, but it's defined to 0
            // TODO: Check if this conditional is neccesary:
            //   Actually this gets the drawable type and checks if
            //   contains PIXMAP_BIT, which should be true due to the attributes
            //   in glx::ChooseFBConfig
            //
            //   It's in Gecko's code, so may there be an implementation which returns bad
            //   configurations?
            if glx::GetFBConfigAttrib(dpy, config, glx::DRAWABLE_TYPE as c_int, &mut drawable_type) != 0
                || (drawable_type & (glx::PIXMAP_BIT as c_int) == 0) {
                continue;
            }

            if glx::GetFBConfigAttrib(dpy, config, glx::VISUAL_ID as c_int, &mut visual_id) != 0
                || visual_id == 0 {
                continue;
            }
        }

        config_index = i;
        break;
    }

    if visual_id == 0 {
        return Err("We don't have any config with visuals");
    }

    unsafe {
        let screen = XDefaultScreenOfDisplay(dpy as *mut _);

        let (_, depth) = try!(get_visual_and_depth(screen, visual_id as VisualID));

        let pixmap = XCreatePixmap(dpy as *mut _,
                                   XRootWindowOfScreen(screen),
                                   size.width as c_uint,
                                   size.height as c_uint,
                                   depth as c_uint);

        if pixmap == 0 {
            return Err("XCreatePixMap");
        }

        let glx_pixmap = glx::CreatePixmap(dpy,
                                           *configs.as_ptr().offset(config_index),
                                           pixmap,
                                           0 as *const c_int);

        if glx_pixmap == 0 {
            XFreePixmap(dpy as *mut _, pixmap);
            return Err("glx::createPixmap");
        }

        let chosen_config = *configs.as_ptr().offset(config_index);

        NativeGLContext::new(shared_with, dpy, glx_pixmap as GLXDrawable, chosen_config)
    }
}
