use gl_context::GLContextDispatcher;

pub trait NativeGLContextMethods: Sized {
    type Handle;

    fn get_proc_address(&str) -> *const ();

    // These are convenient methods to manage handles
    fn current() -> Option<Self>;
    fn current_handle() -> Option<Self::Handle>;

    fn create_shared(with: Option<&Self::Handle>) -> Result<Self, &'static str>;

    fn create_shared_with_dispatcher(with: Option<&Self::Handle>, _dispatcher: Option<Box<GLContextDispatcher>>)
        -> Result<Self, &'static str> {
        Self::create_shared(with)
    }

    fn create_headless() -> Result<Self, &'static str> {
        Self::create_shared(None)
    }

    fn handle(&self) -> Self::Handle;
    fn is_current(&self) -> bool;
    fn make_current(&self) -> Result<(), &'static str>;
    fn unbind(&self) -> Result<(), &'static str>;
}

#[cfg(target_os="linux")]
pub mod with_glx;
#[cfg(target_os="linux")]
pub use self::with_glx::{NativeGLContext, NativeGLContextHandle};

#[cfg(feature="osmesa")]
pub mod with_osmesa;
#[cfg(feature="osmesa")]
pub use self::with_osmesa::{OSMesaContext, OSMesaContextHandle};

#[cfg(any(target_os="android", all(target_os="linux", feature = "test_egl_in_linux")))]
pub mod with_egl;
#[cfg(target_os="android")]
pub use self::with_egl::{NativeGLContext, NativeGLContextHandle};

#[cfg(target_os="macos")]
pub mod with_cgl;
#[cfg(target_os="macos")]
pub use self::with_cgl::{NativeGLContext, NativeGLContextHandle};

#[cfg(target_os="windows")]
pub mod with_wgl;
#[cfg(target_os="windows")]
pub use self::with_wgl::{NativeGLContext, NativeGLContextHandle};

#[cfg(not(any(target_os="linux", target_os="android", target_os="macos", target_os="windows")))]
pub mod not_implemented;
#[cfg(not(any(target_os="linux", target_os="android", target_os="macos", target_os="windows")))]
pub use self::not_implemented::{NativeGLContext, NativeGLContextHandle};
