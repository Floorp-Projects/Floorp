// Everybody gets basic buffer support, since it's needed for passing complex types over the FFI.
//
// See `uniffi/src/ffi/rustbuffer.rs` for documentation on these functions

#[allow(clippy::missing_safety_doc, missing_docs)]
#[doc(hidden)]
#[no_mangle]
pub extern "C" fn {{ ci.ffi_rustbuffer_alloc().name() }}(size: i32, call_status: &mut uniffi::RustCallStatus) -> uniffi::RustBuffer {
    uniffi::ffi::uniffi_rustbuffer_alloc(size, call_status)
}

#[allow(clippy::missing_safety_doc, missing_docs)]
#[doc(hidden)]
#[no_mangle]
pub unsafe extern "C" fn {{ ci.ffi_rustbuffer_from_bytes().name() }}(bytes: uniffi::ForeignBytes, call_status: &mut uniffi::RustCallStatus) -> uniffi::RustBuffer {
    uniffi::ffi::uniffi_rustbuffer_from_bytes(bytes, call_status)
}

#[allow(clippy::missing_safety_doc, missing_docs)]
#[doc(hidden)]
#[no_mangle]
pub unsafe extern "C" fn {{ ci.ffi_rustbuffer_free().name() }}(buf: uniffi::RustBuffer, call_status: &mut uniffi::RustCallStatus) {
    uniffi::ffi::uniffi_rustbuffer_free(buf, call_status);
}

#[allow(clippy::missing_safety_doc, missing_docs)]
#[doc(hidden)]
#[no_mangle]
pub unsafe extern "C" fn {{ ci.ffi_rustbuffer_reserve().name() }}(buf: uniffi::RustBuffer, additional: i32, call_status: &mut uniffi::RustCallStatus) -> uniffi::RustBuffer {
    uniffi::ffi::uniffi_rustbuffer_reserve(buf, additional, call_status)
}
