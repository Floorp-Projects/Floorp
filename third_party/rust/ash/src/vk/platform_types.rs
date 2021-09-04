use std::os::raw::*;
pub type RROutput = c_ulong;
pub type VisualID = c_uint;
pub type Display = *const c_void;
pub type Window = c_ulong;
#[allow(non_camel_case_types)]
pub type xcb_connection_t = c_void;
#[allow(non_camel_case_types)]
pub type xcb_window_t = u32;
#[allow(non_camel_case_types)]
pub type xcb_visualid_t = u32;
pub type MirConnection = *const c_void;
pub type MirSurface = *const c_void;
pub type HINSTANCE = *const c_void;
pub type HWND = *const c_void;
#[allow(non_camel_case_types)]
pub type wl_display = c_void;
#[allow(non_camel_case_types)]
pub type wl_surface = c_void;
pub type HANDLE = *mut c_void;
pub type HMONITOR = HANDLE;
pub type DWORD = c_ulong;
pub type LPCWSTR = *const u16;
#[allow(non_camel_case_types)]
pub type zx_handle_t = u32;
#[allow(non_camel_case_types)]
pub type _screen_context = c_void;
#[allow(non_camel_case_types)]
pub type _screen_window = c_void;
#[allow(non_camel_case_types)]
pub type SECURITY_ATTRIBUTES = c_void;
// Opaque types
pub type ANativeWindow = c_void;
pub type AHardwareBuffer = c_void;
pub type CAMetalLayer = c_void;
// This definition is behind an NDA with a best effort guess from
// https://github.com/google/gapid/commit/22aafebec4638c6aaa77667096bca30f6e842d95#diff-ab3ab4a7d89b4fc8a344ff4e9332865f268ea1669ee379c1b516a954ecc2e7a6R20-R21
pub type GgpStreamDescriptor = u32;
pub type GgpFrameToken = u64;
pub type IDirectFB = c_void;
pub type IDirectFBSurface = c_void;
