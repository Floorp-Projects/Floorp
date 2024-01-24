// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use cubeb_core::{
    ffi, DeviceCollectionRef, DeviceRef, DeviceType, InputProcessingParams, StreamParams,
    StreamParamsRef,
};
use std::ffi::CStr;
use std::mem;
use std::os::raw::{c_char, c_int, c_void};
use {ContextOps, StreamOps};

// Helper macro for unwrapping `Result` values from rust-api calls
// while returning early with a c-api error code if the value of the
// expression is `Err`.
macro_rules! _try(
    ($e:expr) => (match $e {
        Ok(e) => e,
        Err(e) => return e.raw_code()
    })
);

macro_rules! as_opt_ref {
    ($e:expr) => {
        if $e.is_null() {
            None
        } else {
            Some(StreamParamsRef::from_ptr($e))
        }
    };
}

#[macro_export]
macro_rules! capi_new(
    ($ctx:ident, $stm:ident) => (
        Ops {
            init: Some($crate::capi::capi_init::<$ctx>),
            get_backend_id: Some($crate::capi::capi_get_backend_id::<$ctx>),
            get_max_channel_count: Some($crate::capi::capi_get_max_channel_count::<$ctx>),
            get_min_latency: Some($crate::capi::capi_get_min_latency::<$ctx>),
            get_preferred_sample_rate: Some($crate::capi::capi_get_preferred_sample_rate::<$ctx>),
            get_supported_input_processing_params:
                Some($crate::capi::capi_get_supported_input_processing_params::<$ctx>),
            enumerate_devices: Some($crate::capi::capi_enumerate_devices::<$ctx>),
            device_collection_destroy: Some($crate::capi::capi_device_collection_destroy::<$ctx>),
            destroy: Some($crate::capi::capi_destroy::<$ctx>),
            stream_init: Some($crate::capi::capi_stream_init::<$ctx>),
            stream_destroy: Some($crate::capi::capi_stream_destroy::<$stm>),
            stream_start: Some($crate::capi::capi_stream_start::<$stm>),
            stream_stop: Some($crate::capi::capi_stream_stop::<$stm>),
            stream_get_position: Some($crate::capi::capi_stream_get_position::<$stm>),
            stream_get_latency: Some($crate::capi::capi_stream_get_latency::<$stm>),
            stream_get_input_latency: Some($crate::capi::capi_stream_get_input_latency::<$stm>),
            stream_set_volume: Some($crate::capi::capi_stream_set_volume::<$stm>),
            stream_set_name: Some($crate::capi::capi_stream_set_name::<$stm>),
            stream_get_current_device: Some($crate::capi::capi_stream_get_current_device::<$stm>),
            stream_set_input_mute: Some($crate::capi::capi_stream_set_input_mute::<$stm>),
            stream_set_input_processing_params:
                Some($crate::capi::capi_stream_set_input_processing_params::<$stm>),
            stream_device_destroy: Some($crate::capi::capi_stream_device_destroy::<$stm>),
            stream_register_device_changed_callback:
                Some($crate::capi::capi_stream_register_device_changed_callback::<$stm>),
            register_device_collection_changed:
                Some($crate::capi::capi_register_device_collection_changed::<$ctx>)
        }));

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` and `context` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_init<CTX: ContextOps>(
    c: *mut *mut ffi::cubeb,
    context_name: *const c_char,
) -> c_int {
    let anchor = &();
    let context_name = opt_cstr(anchor, context_name);
    let context = _try!(CTX::init(context_name));
    *c = context.as_ptr();
    // Leaking pointer across C FFI
    mem::forget(context);
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` pointer.
/// The caller should ensure that pointer is valid.
pub unsafe extern "C" fn capi_get_backend_id<CTX: ContextOps>(c: *mut ffi::cubeb) -> *const c_char {
    let ctx = &mut *(c as *mut CTX);
    ctx.backend_id().as_ptr()
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` and `max_channels` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_get_max_channel_count<CTX: ContextOps>(
    c: *mut ffi::cubeb,
    max_channels: *mut u32,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);

    *max_channels = _try!(ctx.max_channel_count());
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` and `latency_frames` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_get_min_latency<CTX: ContextOps>(
    c: *mut ffi::cubeb,
    param: ffi::cubeb_stream_params,
    latency_frames: *mut u32,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);
    let param = StreamParams::from(param);
    *latency_frames = _try!(ctx.min_latency(param));
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` and `rate` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_get_preferred_sample_rate<CTX: ContextOps>(
    c: *mut ffi::cubeb,
    rate: *mut u32,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);

    *rate = _try!(ctx.preferred_sample_rate());
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` and `params` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_get_supported_input_processing_params<CTX: ContextOps>(
    c: *mut ffi::cubeb,
    params: *mut ffi::cubeb_input_processing_params,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);
    *params = _try!(ctx.supported_input_processing_params()).bits();
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` and `collection` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_enumerate_devices<CTX: ContextOps>(
    c: *mut ffi::cubeb,
    devtype: ffi::cubeb_device_type,
    collection: *mut ffi::cubeb_device_collection,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);
    let devtype = DeviceType::from_bits_truncate(devtype);
    let collection = DeviceCollectionRef::from_ptr(collection);
    _try!(ctx.enumerate_devices(devtype, collection));
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` and `collection` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_device_collection_destroy<CTX: ContextOps>(
    c: *mut ffi::cubeb,
    collection: *mut ffi::cubeb_device_collection,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);
    let collection = DeviceCollectionRef::from_ptr_mut(collection);
    _try!(ctx.device_collection_destroy(collection));
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c` pointer.
/// The caller should ensure that pointer is valid.
pub unsafe extern "C" fn capi_destroy<CTX>(c: *mut ffi::cubeb) {
    let _: Box<CTX> = Box::from_raw(c as *mut _);
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `c`, `s`, `stream_name`, `input_stream_params`,
/// `output_stream_params`, `data_callback`, `state_callback`, and `user_ptr` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_init<CTX: ContextOps>(
    c: *mut ffi::cubeb,
    s: *mut *mut ffi::cubeb_stream,
    stream_name: *const c_char,
    input_device: ffi::cubeb_devid,
    input_stream_params: *mut ffi::cubeb_stream_params,
    output_device: ffi::cubeb_devid,
    output_stream_params: *mut ffi::cubeb_stream_params,
    latency_frames: u32,
    data_callback: ffi::cubeb_data_callback,
    state_callback: ffi::cubeb_state_callback,
    user_ptr: *mut c_void,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);
    let anchor = &(); // for lifetime of stream_name as CStr

    let input_stream_params = as_opt_ref!(input_stream_params);
    let output_stream_params = as_opt_ref!(output_stream_params);

    let stream = _try!(ctx.stream_init(
        opt_cstr(anchor, stream_name),
        input_device,
        input_stream_params,
        output_device,
        output_stream_params,
        latency_frames,
        data_callback,
        state_callback,
        user_ptr
    ));
    *s = stream.as_ptr();
    // Leaking pointer across C FFI
    mem::forget(stream);
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` pointer.
/// The caller should ensure that pointer is valid.
pub unsafe extern "C" fn capi_stream_destroy<STM>(s: *mut ffi::cubeb_stream) {
    let _ = Box::from_raw(s as *mut STM);
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` pointer.
/// The caller should ensure that pointer is valid.
pub unsafe extern "C" fn capi_stream_start<STM: StreamOps>(s: *mut ffi::cubeb_stream) -> c_int {
    let stm = &mut *(s as *mut STM);

    _try!(stm.start());
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` pointer.
/// The caller should ensure that pointer is valid.
pub unsafe extern "C" fn capi_stream_stop<STM: StreamOps>(s: *mut ffi::cubeb_stream) -> c_int {
    let stm = &mut *(s as *mut STM);

    _try!(stm.stop());
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` and `position` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_get_position<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    position: *mut u64,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    *position = _try!(stm.position());
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` and `latency` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_get_latency<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    latency: *mut u32,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    *latency = _try!(stm.latency());
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` and `latency` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_get_input_latency<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    latency: *mut u32,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    *latency = _try!(stm.input_latency());
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` pointer.
/// The caller should ensure that pointer is valid.
pub unsafe extern "C" fn capi_stream_set_volume<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    volume: f32,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    _try!(stm.set_volume(volume));
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` and `name` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_set_name<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    name: *const c_char,
) -> c_int {
    let stm = &mut *(s as *mut STM);
    let anchor = &();
    if let Some(name) = opt_cstr(anchor, name) {
        _try!(stm.set_name(name));
        ffi::CUBEB_OK
    } else {
        ffi::CUBEB_ERROR_INVALID_PARAMETER
    }
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` and `device` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_get_current_device<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    device: *mut *mut ffi::cubeb_device,
) -> i32 {
    let stm = &mut *(s as *mut STM);

    *device = _try!(stm.current_device()).as_ptr();
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` pointer.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_set_input_mute<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    mute: c_int,
) -> c_int {
    let stm = &mut *(s as *mut STM);
    _try!(stm.set_input_mute(mute != 0));
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` pointer.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_set_input_processing_params<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    params: ffi::cubeb_input_processing_params,
) -> c_int {
    let stm = &mut *(s as *mut STM);
    _try!(stm.set_input_processing_params(InputProcessingParams::from_bits_truncate(params)));
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` and `device` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_device_destroy<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    device: *mut ffi::cubeb_device,
) -> c_int {
    let stm = &mut *(s as *mut STM);
    let device = DeviceRef::from_ptr(device);
    let _ = stm.device_destroy(device);
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s` and `device_changed_callback` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_stream_register_device_changed_callback<STM: StreamOps>(
    s: *mut ffi::cubeb_stream,
    device_changed_callback: ffi::cubeb_device_changed_callback,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    _try!(stm.register_device_changed_callback(device_changed_callback));
    ffi::CUBEB_OK
}

/// # Safety
///
/// Entry point from C code.
///
/// This function is unsafe because it dereferences the given `s`, `collection_changed_callback`, and
/// `user_ptr` pointers.
/// The caller should ensure those pointers are valid.
pub unsafe extern "C" fn capi_register_device_collection_changed<CTX: ContextOps>(
    c: *mut ffi::cubeb,
    devtype: ffi::cubeb_device_type,
    collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
    user_ptr: *mut c_void,
) -> i32 {
    let ctx = &mut *(c as *mut CTX);
    let devtype = DeviceType::from_bits_truncate(devtype);
    _try!(ctx.register_device_collection_changed(devtype, collection_changed_callback, user_ptr));
    ffi::CUBEB_OK
}

fn opt_cstr<T>(_anchor: &T, ptr: *const c_char) -> Option<&CStr> {
    if ptr.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(ptr) })
    }
}
