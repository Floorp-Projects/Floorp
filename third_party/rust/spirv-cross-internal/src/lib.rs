#[cfg(target_arch = "wasm32")]
macro_rules! check {
    ($check:expr) => {{
        $check
    }};
}

#[cfg(not(target_arch = "wasm32"))]
macro_rules! check {
    ($check:expr) => {{
        let result = $check;
        if br::ScInternalResult::Success != result {
            if br::ScInternalResult::CompilationError == result {
                let mut message_ptr = ptr::null();

                if br::ScInternalResult::Success
                    != br::sc_internal_get_latest_exception_message(&mut message_ptr)
                {
                    return Err(ErrorCode::Unhandled);
                }

                let message = match std::ffi::CStr::from_ptr(message_ptr)
                    .to_owned()
                    .into_string()
                {
                    Err(_) => return Err(ErrorCode::Unhandled),
                    Ok(v) => v,
                };

                if br::ScInternalResult::Success
                    != br::sc_internal_free_pointer(message_ptr as *mut std::os::raw::c_void)
                {
                    return Err(ErrorCode::Unhandled);
                }

                return Err(ErrorCode::CompilationError(message));
            }

            return Err(ErrorCode::Unhandled);
        }
    }};
}

mod compiler;

#[cfg(feature = "glsl")]
pub mod glsl;
#[cfg(all(feature = "hlsl", not(target_arch = "wasm32")))]
pub mod hlsl;
#[cfg(all(feature = "msl", not(target_arch = "wasm32")))]
pub mod msl;

pub mod spirv;

#[cfg(target_arch = "wasm32")]
pub(crate) mod emscripten;
pub(crate) mod ptr_util;

#[cfg(target_arch = "wasm32")]
mod bindings_wasm_functions;

#[cfg(target_arch = "wasm32")]
mod bindings {
    #![allow(dead_code)]
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    include!(concat!("bindings_wasm.rs"));
    pub use crate::bindings_wasm_functions::*;
    pub use root::*;
}

#[cfg(not(target_arch = "wasm32"))]
mod bindings {
    #![allow(dead_code)]
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    include!(concat!("bindings_native.rs"));
    pub use root::*;
}

#[derive(Clone, Debug, Hash, Eq, PartialEq)]
pub enum ErrorCode {
    Unhandled,
    CompilationError(String),
}

impl std::fmt::Display for ErrorCode {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl std::error::Error for ErrorCode {}
