// platform-specific aliases are unknown
// IMPORTANT: these are alises to the same level of the bindings
// the values must be defined by the user
pub type khronos_utime_nanoseconds_t = super::khronos_utime_nanoseconds_t;
pub type khronos_uint64_t = super::khronos_uint64_t;
pub type khronos_ssize_t = super::khronos_ssize_t;
pub type EGLNativeDisplayType = super::EGLNativeDisplayType;
pub type EGLNativePixmapType = super::EGLNativePixmapType;
pub type EGLNativeWindowType = super::EGLNativeWindowType;
pub type EGLint = super::EGLint;
pub type NativeDisplayType = super::NativeDisplayType;
pub type NativePixmapType = super::NativePixmapType;
pub type NativeWindowType = super::NativeWindowType;

// EGL alises
pub type Bool = EGLBoolean;  // TODO: not sure
pub type EGLBoolean = super::__gl_imports::raw::c_uint;
pub type EGLenum = super::__gl_imports::raw::c_uint;
pub type EGLAttribKHR = isize;
pub type EGLAttrib = isize;
pub type EGLConfig = *const super::__gl_imports::raw::c_void;
pub type EGLContext = *const super::__gl_imports::raw::c_void;
pub type EGLDeviceEXT = *const super::__gl_imports::raw::c_void;
pub type EGLDisplay = *const super::__gl_imports::raw::c_void;
pub type EGLSurface = *const super::__gl_imports::raw::c_void;
pub type EGLClientBuffer = *const super::__gl_imports::raw::c_void;
pub type __eglMustCastToProperFunctionPointerType = extern "system" fn() -> ();
pub type EGLImageKHR = *const super::__gl_imports::raw::c_void;
pub type EGLImage = *const super::__gl_imports::raw::c_void;
pub type EGLOutputLayerEXT = *const super::__gl_imports::raw::c_void;
pub type EGLOutputPortEXT = *const super::__gl_imports::raw::c_void;
pub type EGLSyncKHR = *const super::__gl_imports::raw::c_void;
pub type EGLSync = *const super::__gl_imports::raw::c_void;
pub type EGLTimeKHR = khronos_utime_nanoseconds_t;
pub type EGLTime = khronos_utime_nanoseconds_t;
pub type EGLSyncNV = *const super::__gl_imports::raw::c_void;
pub type EGLTimeNV = khronos_utime_nanoseconds_t;
pub type EGLuint64NV = khronos_utime_nanoseconds_t;
pub type EGLStreamKHR = *const super::__gl_imports::raw::c_void;
pub type EGLuint64KHR = khronos_uint64_t;
pub type EGLNativeFileDescriptorKHR = super::__gl_imports::raw::c_int;
pub type EGLsizeiANDROID = khronos_ssize_t;
pub type EGLSetBlobFuncANDROID = extern "system" fn(*const super::__gl_imports::raw::c_void, EGLsizeiANDROID, *const super::__gl_imports::raw::c_void, EGLsizeiANDROID) -> ();
pub type EGLGetBlobFuncANDROID = extern "system" fn(*const super::__gl_imports::raw::c_void, EGLsizeiANDROID, *mut super::__gl_imports::raw::c_void, EGLsizeiANDROID) -> EGLsizeiANDROID;

#[repr(C)]
pub struct EGLClientPixmapHI {
    pData: *const super::__gl_imports::raw::c_void,
    iWidth: EGLint,
    iHeight: EGLint,
    iStride: EGLint,
}
