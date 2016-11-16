extern crate gleam;
extern crate euclid;

#[macro_use]
extern crate log;

#[cfg(feature="serde")]
extern crate serde;

#[cfg(target_os="linux")]
extern crate x11;
#[cfg(target_os="macos")]
extern crate cgl;
#[cfg(target_os="macos")]
extern crate core_foundation;
#[cfg(feature="osmesa")]
extern crate osmesa_sys;
#[cfg(target_os = "windows")]
extern crate winapi;
#[cfg(target_os = "windows")]
extern crate kernel32;
#[cfg(target_os = "windows")]
extern crate gdi32;
#[cfg(target_os = "windows")]
extern crate user32;
#[cfg(any(target_os="macos", target_os="windows"))]
#[macro_use]
extern crate lazy_static;

mod platform;
pub use platform::{NativeGLContext, NativeGLContextMethods, NativeGLContextHandle};
#[cfg(feature="osmesa")]
pub use platform::{OSMesaContext, OSMesaContextHandle};

mod gl_context;
pub use gl_context::{GLContext, GLContextDispatcher};

mod draw_buffer;
pub use draw_buffer::{DrawBuffer, ColorAttachmentType};

mod gl_context_attributes;
pub use gl_context_attributes::GLContextAttributes;

mod gl_context_capabilities;
pub use gl_context_capabilities::GLContextCapabilities;

mod gl_feature;
pub use gl_feature::GLFeature;

mod gl_formats;
pub use gl_formats::GLFormats;

mod gl_limits;
pub use gl_limits::GLLimits;

#[cfg(target_os="linux")]
#[allow(improper_ctypes)]
mod glx {
    include!(concat!(env!("OUT_DIR"), "/glx_bindings.rs"));
}

#[cfg(any(target_os="linux", target_os="android"))]
#[allow(non_camel_case_types)]
mod egl {
    use std::os::raw::{c_long, c_void};
    pub type khronos_utime_nanoseconds_t = khronos_uint64_t;
    pub type khronos_uint64_t = u64;
    pub type khronos_ssize_t = c_long;
    pub type EGLint = i32;
    pub type EGLNativeDisplayType = *const c_void;
    pub type EGLNativePixmapType = *const c_void;
    pub type EGLNativeWindowType = *const c_void;
    pub type NativeDisplayType = EGLNativeDisplayType;
    pub type NativePixmapType = EGLNativePixmapType;
    pub type NativeWindowType = EGLNativeWindowType;
    include!(concat!(env!("OUT_DIR"), "/egl_bindings.rs"));
}

#[cfg(test)]
mod tests;
