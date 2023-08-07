#![cfg_attr(not(any(target_arch = "wasm32")), no_std)]

extern crate alloc;

use alloc::alloc::Layout;

#[cfg(target_arch = "wasm32")]
mod wasm_glue;
#[cfg(target_arch = "wasm32")]
pub use wasm_glue::{console_log, console_trace, console_warn};

mod writeable;
pub use writeable::DiplomatWriteable;

mod result;
pub use result::DiplomatResult;

/// Allocates a buffer of a given size in Rust's memory.
///
/// # Safety
/// - The allocated buffer must be freed with [`diplomat_free()`].
#[no_mangle]
pub unsafe extern "C" fn diplomat_alloc(size: usize, align: usize) -> *mut u8 {
    alloc::alloc::alloc(Layout::from_size_align(size, align).unwrap())
}

/// Frees a buffer that was allocated in Rust's memory.
/// # Safety
/// - `ptr` must be a pointer to a valid buffer allocated by [`diplomat_alloc()`].
#[no_mangle]
pub unsafe extern "C" fn diplomat_free(ptr: *mut u8, size: usize, align: usize) {
    alloc::alloc::dealloc(ptr, Layout::from_size_align(size, align).unwrap())
}
