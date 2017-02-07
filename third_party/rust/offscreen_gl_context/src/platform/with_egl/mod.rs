mod native_gl_context;
mod utils;
pub use self::native_gl_context::NativeGLContext;
pub use self::native_gl_context::NativeGLContextHandle;

// NB: The last three zeros in egl attributes after the egl::EGL_NONE
// are a workaround for workaround buggy implementations.
// Also, when we compare a createxx call with zero, it's equivalent to
// compare it with egl::EGL_NO_XX
