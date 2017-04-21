// Required for dlopen, et al.
#[cfg(feature = "dlopen")]
extern crate libc;

mod ffi_types;
mod ffi_funcs;

pub use ffi_types::*;
pub use ffi_funcs::*;

#[cfg(feature = "dlopen")]
pub unsafe fn open() -> Option<LibLoader> {
    return LibLoader::open();
}
