//! Pointer utilities to abstract over native pointer access (i.e. `*const T`)
//! and Emscripten pointer access (i.e. `u32` offset into the Emscripten heap).

use crate::ErrorCode;
use std::ffi::CStr;
use std::slice;

#[cfg(target_arch = "wasm32")]
use crate::emscripten;

pub unsafe fn read_string_from_ptr(ptr: *const std::os::raw::c_char) -> Result<String, ErrorCode> {
    #[cfg(not(target_arch = "wasm32"))]
    let string = CStr::from_ptr(ptr)
        .to_owned()
        .into_string()
        .map_err(|_| ErrorCode::Unhandled);
    #[cfg(target_arch = "wasm32")]
    let string = {
        let bytes = emscripten::get_module().read_bytes_into_vec_while(
            emscripten::Pointer::from_offset(ptr as u32),
            |byte, _| 0 != byte,
            false,
        );
        String::from_utf8(bytes).map_err(|_| ErrorCode::Unhandled)
    };
    string
}

pub unsafe fn read_from_ptr<T>(ptr: *const T) -> T {
    #[cfg(not(target_arch = "wasm32"))]
    let value = ptr.read();
    #[cfg(target_arch = "wasm32")]
    let value = {
        let num_bytes_to_read = std::mem::size_of::<T>();
        let mut t_val: T = std::mem::uninitialized();
        let t_ptr = &mut t_val as *mut T as *mut u8;
        let bytes = emscripten::get_module().read_bytes_into_vec_while(
            emscripten::Pointer::from_offset(ptr as u32),
            |_, bytes_read| bytes_read < num_bytes_to_read,
            false,
        );
        for (offset, byte) in bytes.iter().enumerate() {
            *t_ptr.offset(offset as isize) = *byte;
        }
        t_val
    };
    value
}

pub unsafe fn read_into_vec_from_ptr<T: Clone>(ptr: *const T, size: usize) -> Vec<T> {
    #[cfg(not(target_arch = "wasm32"))]
    let values = slice::from_raw_parts(ptr, size).to_vec();
    #[cfg(target_arch = "wasm32")]
    let values = (0..size)
        .map(|offset| read_from_ptr(ptr.add(offset)))
        .collect();
    values
}
