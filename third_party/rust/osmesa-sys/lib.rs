// osmesa-rs: Off-Screen Mesa bindings for Rust.
// The OSMesa library is available under the MIT license.
// These bindings are public domain.

#![allow(non_snake_case)]

#[macro_use]
extern crate shared_library;

use std::os::raw::{
  c_char,
  c_int,
  c_uchar,
  c_uint,
  c_void,
};


//
// functions
//

#[cfg(target_os="macos")]
const LIB_NAME: &'static str = "libOSMesa.dylib";

#[cfg(not(target_os="macos"))]
const LIB_NAME: &'static str = "libOSMesa.so";

shared_library!(OsMesa, LIB_NAME,
  pub fn OSMesaColorClamp (enable: c_uchar),
  pub fn OSMesaCreateContext (format: c_uint, sharelist: OSMesaContext) -> OSMesaContext,
  pub fn OSMesaCreateContextExt (format: c_uint, depthBits: c_int, stencilBits: c_int, accumBits: c_int, sharelist: OSMesaContext) -> OSMesaContext,
  pub fn OSMesaCreateContextAttribs(attribList: *const c_int, sharelist: OSMesaContext) -> OSMesaContext,
  pub fn OSMesaDestroyContext (ctx: OSMesaContext),
  pub fn OSMesaGetColorBuffer (c: OSMesaContext, width: *mut c_int, height: *mut c_int, format: *mut c_int, buffer: *mut *mut c_void) -> c_uchar,
  pub fn OSMesaGetCurrentContext () -> OSMesaContext,
  pub fn OSMesaGetDepthBuffer (c: OSMesaContext, width: *mut c_int, height: *mut c_int, bytesPerValue: *mut c_int, buffer: *mut *mut c_void) -> c_uchar,
  pub fn OSMesaGetIntegerv (pname: c_int, value: *mut c_int),
  pub fn OSMesaGetProcAddress (funcName: *const c_char) -> OSMESAproc,
  pub fn OSMesaMakeCurrent (ctx: OSMesaContext, buffer: *mut c_void, _type: c_uint, width: c_int, height: c_int) -> c_uchar,
  pub fn OSMesaPixelStore (pname: c_int, value: c_int),
);


//
// types
//


// opaque structs
#[repr(C)] pub struct osmesa_context;

// types
pub type OSMesaContext = *mut osmesa_context;
pub type OSMESAproc = Option<unsafe extern "C" fn ()>;


//
// constants
//


// context formats
pub const OSMESA_BGRA: c_uint = 0x0001;
pub const OSMESA_ARGB: c_uint = 0x0002;
pub const OSMESA_BGR: c_uint = 0x0004;
pub const OSMESA_RGB_565: c_uint = 0x0005;
pub const OSMESA_COLOR_INDEX: c_uint = 0x1900;
pub const OSMESA_RGB: c_uint = 0x1907;
pub const OSMESA_RGBA: c_uint = 0x1908;

// OSMesaGetIntegerv
pub const OSMESA_WIDTH: c_int = 0x0020;
pub const OSMESA_HEIGHT: c_int = 0x0021;
pub const OSMESA_FORMAT: c_int = 0x0022;
pub const OSMESA_TYPE: c_int = 0x0023;
pub const OSMESA_MAX_WIDTH: c_int = 0x0024;
pub const OSMESA_MAX_HEIGHT: c_int = 0x0025;

// OSMesaPixelStore
pub const OSMESA_ROW_LENGTH: c_int = 0x0010;
pub const OSMESA_Y_UP: c_int = 0x0011;

// OSMesaCreateContextAttribs
pub const OSMESA_DEPTH_BITS: c_int = 0x30;
pub const OSMESA_STENCIL_BITS: c_int = 0x31;
pub const OSMESA_ACCUM_BITS: c_int = 0x32;
pub const OSMESA_PROFILE: c_int = 0x33;
pub const OSMESA_CORE_PROFILE: c_int = 0x34;
pub const OSMESA_COMPAT_PROFILE: c_int = 0x35;
pub const OSMESA_CONTEXT_MAJOR_VERSION: c_int = 0x36;
pub const OSMESA_CONTEXT_MINOR_VERSION: c_int = 0x37;
