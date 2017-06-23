// This code is generated.

use super::*;

macro_rules! cstr {
  ($x:expr) => { concat!($x, "\0").as_bytes().as_ptr() as *const c_char }
}

#[cfg(not(feature = "dlopen"))]
mod static_fns {
    use super::*;
    use std::os::raw::{c_char, c_double, c_float, c_int, c_uint, c_void};

    #[link(name = "pulse")]
    extern "C" {
        pub fn pa_get_library_version() -> *const c_char;
        pub fn pa_channel_map_can_balance(map: *const pa_channel_map) -> c_int;
        pub fn pa_channel_map_init(m: *mut pa_channel_map) -> *mut pa_channel_map;
        pub fn pa_context_connect(c: *mut pa_context,
                                  server: *const c_char,
                                  flags: pa_context_flags_t,
                                  api: *const pa_spawn_api)
                                  -> c_int;
        pub fn pa_context_disconnect(c: *mut pa_context);
        pub fn pa_context_drain(c: *mut pa_context,
                                cb: pa_context_notify_cb_t,
                                userdata: *mut c_void)
                                -> *mut pa_operation;
        pub fn pa_context_get_server_info(c: *const pa_context,
                                          cb: pa_server_info_cb_t,
                                          userdata: *mut c_void)
                                          -> *mut pa_operation;
        pub fn pa_context_get_sink_info_by_name(c: *const pa_context,
                                                name: *const c_char,
                                                cb: pa_sink_info_cb_t,
                                                userdata: *mut c_void)
                                                -> *mut pa_operation;
        pub fn pa_context_get_sink_info_list(c: *const pa_context,
                                             cb: pa_sink_info_cb_t,
                                             userdata: *mut c_void)
                                             -> *mut pa_operation;
        pub fn pa_context_get_sink_input_info(c: *const pa_context,
                                              idx: u32,
                                              cb: pa_sink_input_info_cb_t,
                                              userdata: *mut c_void)
                                              -> *mut pa_operation;
        pub fn pa_context_get_source_info_list(c: *const pa_context,
                                               cb: pa_source_info_cb_t,
                                               userdata: *mut c_void)
                                               -> *mut pa_operation;
        pub fn pa_context_get_state(c: *const pa_context) -> pa_context_state_t;
        pub fn pa_context_new(mainloop: *mut pa_mainloop_api, name: *const c_char) -> *mut pa_context;
        pub fn pa_context_rttime_new(c: *const pa_context,
                                     usec: pa_usec_t,
                                     cb: pa_time_event_cb_t,
                                     userdata: *mut c_void)
                                     -> *mut pa_time_event;
        pub fn pa_context_set_sink_input_volume(c: *mut pa_context,
                                                idx: u32,
                                                volume: *const pa_cvolume,
                                                cb: pa_context_success_cb_t,
                                                userdata: *mut c_void)
                                                -> *mut pa_operation;
        pub fn pa_context_set_state_callback(c: *mut pa_context, cb: pa_context_notify_cb_t, userdata: *mut c_void);
        pub fn pa_context_errno(c: *mut pa_context) -> c_int;
        pub fn pa_context_set_subscribe_callback(c: *mut pa_context,
                                                 cb: pa_context_subscribe_cb_t,
                                                 userdata: *mut c_void);
        pub fn pa_context_subscribe(c: *mut pa_context,
                                    m: pa_subscription_mask_t,
                                    cb: pa_context_success_cb_t,
                                    userdata: *mut c_void)
                                    -> *mut pa_operation;
        pub fn pa_context_ref(c: *mut pa_context) -> *mut pa_context;
        pub fn pa_context_unref(c: *mut pa_context);
        pub fn pa_cvolume_set(a: *mut pa_cvolume, channels: c_uint, v: pa_volume_t) -> *mut pa_cvolume;
        pub fn pa_cvolume_set_balance(v: *mut pa_cvolume,
                                      map: *const pa_channel_map,
                                      new_balance: c_float)
                                      -> *mut pa_cvolume;
        pub fn pa_frame_size(spec: *const pa_sample_spec) -> usize;
        pub fn pa_mainloop_api_once(m: *mut pa_mainloop_api,
                                    callback: pa_mainloop_api_once_cb_t,
                                    userdata: *mut c_void);
        pub fn pa_strerror(error: pa_error_code_t) -> *const c_char;
        pub fn pa_operation_ref(o: *mut pa_operation) -> *mut pa_operation;
        pub fn pa_operation_unref(o: *mut pa_operation);
        pub fn pa_operation_cancel(o: *mut pa_operation);
        pub fn pa_operation_get_state(o: *const pa_operation) -> pa_operation_state_t;
        pub fn pa_operation_set_state_callback(o: *mut pa_operation,
                                               cb: pa_operation_notify_cb_t,
                                               userdata: *mut c_void);
        pub fn pa_proplist_gets(p: *mut pa_proplist, key: *const c_char) -> *const c_char;
        pub fn pa_rtclock_now() -> pa_usec_t;
        pub fn pa_stream_begin_write(p: *mut pa_stream, data: *mut *mut c_void, nbytes: *mut usize) -> c_int;
        pub fn pa_stream_cancel_write(p: *mut pa_stream) -> c_int;
        pub fn pa_stream_is_suspended(s: *const pa_stream) -> c_int;
        pub fn pa_stream_is_corked(s: *const pa_stream) -> c_int;
        pub fn pa_stream_connect_playback(s: *mut pa_stream,
                                          dev: *const c_char,
                                          attr: *const pa_buffer_attr,
                                          flags: pa_stream_flags_t,
                                          volume: *const pa_cvolume,
                                          sync_stream: *const pa_stream)
                                          -> c_int;
        pub fn pa_stream_connect_record(s: *mut pa_stream,
                                        dev: *const c_char,
                                        attr: *const pa_buffer_attr,
                                        flags: pa_stream_flags_t)
                                        -> c_int;
        pub fn pa_stream_cork(s: *mut pa_stream,
                              b: c_int,
                              cb: pa_stream_success_cb_t,
                              userdata: *mut c_void)
                              -> *mut pa_operation;
        pub fn pa_stream_disconnect(s: *mut pa_stream) -> c_int;
        pub fn pa_stream_drop(p: *mut pa_stream) -> c_int;
        pub fn pa_stream_get_buffer_attr(s: *const pa_stream) -> *const pa_buffer_attr;
        pub fn pa_stream_get_channel_map(s: *const pa_stream) -> *const pa_channel_map;
        pub fn pa_stream_get_device_name(s: *const pa_stream) -> *const c_char;
        pub fn pa_stream_get_index(s: *const pa_stream) -> u32;
        pub fn pa_stream_get_latency(s: *const pa_stream, r_usec: *mut pa_usec_t, negative: *mut c_int) -> c_int;
        pub fn pa_stream_get_sample_spec(s: *const pa_stream) -> *const pa_sample_spec;
        pub fn pa_stream_get_state(p: *const pa_stream) -> pa_stream_state_t;
        pub fn pa_stream_get_context(s: *const pa_stream) -> *mut pa_context;
        pub fn pa_stream_get_time(s: *const pa_stream, r_usec: *mut pa_usec_t) -> c_int;
        pub fn pa_stream_new(c: *mut pa_context,
                             name: *const c_char,
                             ss: *const pa_sample_spec,
                             map: *const pa_channel_map)
                             -> *mut pa_stream;
        pub fn pa_stream_peek(p: *mut pa_stream, data: *mut *const c_void, nbytes: *mut usize) -> c_int;
        pub fn pa_stream_readable_size(p: *const pa_stream) -> usize;
        pub fn pa_stream_set_state_callback(s: *mut pa_stream, cb: pa_stream_notify_cb_t, userdata: *mut c_void);
        pub fn pa_stream_set_write_callback(p: *mut pa_stream, cb: pa_stream_request_cb_t, userdata: *mut c_void);
        pub fn pa_stream_set_read_callback(p: *mut pa_stream, cb: pa_stream_request_cb_t, userdata: *mut c_void);
        pub fn pa_stream_ref(s: *mut pa_stream) -> *mut pa_stream;
        pub fn pa_stream_unref(s: *mut pa_stream);
        pub fn pa_stream_update_timing_info(p: *mut pa_stream,
                                            cb: pa_stream_success_cb_t,
                                            userdata: *mut c_void)
                                            -> *mut pa_operation;
        pub fn pa_stream_writable_size(p: *const pa_stream) -> usize;
        pub fn pa_stream_write(p: *mut pa_stream,
                               data: *const c_void,
                               nbytes: usize,
                               free_cb: pa_free_cb_t,
                               offset: i64,
                               seek: pa_seek_mode_t)
                               -> c_int;
        pub fn pa_sw_volume_from_linear(v: c_double) -> pa_volume_t;
        pub fn pa_threaded_mainloop_free(m: *mut pa_threaded_mainloop);
        pub fn pa_threaded_mainloop_get_api(m: *mut pa_threaded_mainloop) -> *mut pa_mainloop_api;
        pub fn pa_threaded_mainloop_in_thread(m: *mut pa_threaded_mainloop) -> c_int;
        pub fn pa_threaded_mainloop_lock(m: *mut pa_threaded_mainloop);
        pub fn pa_threaded_mainloop_new() -> *mut pa_threaded_mainloop;
        pub fn pa_threaded_mainloop_signal(m: *mut pa_threaded_mainloop, wait_for_accept: c_int);
        pub fn pa_threaded_mainloop_start(m: *mut pa_threaded_mainloop) -> c_int;
        pub fn pa_threaded_mainloop_stop(m: *mut pa_threaded_mainloop);
        pub fn pa_threaded_mainloop_unlock(m: *mut pa_threaded_mainloop);
        pub fn pa_threaded_mainloop_wait(m: *mut pa_threaded_mainloop);
        pub fn pa_usec_to_bytes(t: pa_usec_t, spec: *const pa_sample_spec) -> usize;
        pub fn pa_xrealloc(ptr: *mut c_void, size: usize) -> *mut c_void;
    }
}

#[cfg(not(feature = "dlopen"))]
pub use self::static_fns::*;

#[cfg(feature = "dlopen")]
mod dynamic_fns {
    use super::*;
    use libc::{RTLD_LAZY, dlclose, dlopen, dlsym};
    use std::os::raw::{c_char, c_double, c_float, c_int, c_uint, c_void};

    #[derive(Debug)]
    pub struct LibLoader {
        _lib: *mut ::libc::c_void,
    }

    impl LibLoader {
        pub unsafe fn open() -> Option<LibLoader> {
            let h = dlopen(cstr!("libpulse.so.0"), RTLD_LAZY);
            if h.is_null() {
                return None;
            }

            PA_GET_LIBRARY_VERSION = {
                let fp = dlsym(h, cstr!("pa_get_library_version"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CHANNEL_MAP_CAN_BALANCE = {
                let fp = dlsym(h, cstr!("pa_channel_map_can_balance"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CHANNEL_MAP_INIT = {
                let fp = dlsym(h, cstr!("pa_channel_map_init"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_CONNECT = {
                let fp = dlsym(h, cstr!("pa_context_connect"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_DISCONNECT = {
                let fp = dlsym(h, cstr!("pa_context_disconnect"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_DRAIN = {
                let fp = dlsym(h, cstr!("pa_context_drain"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_GET_SERVER_INFO = {
                let fp = dlsym(h, cstr!("pa_context_get_server_info"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_GET_SINK_INFO_BY_NAME = {
                let fp = dlsym(h, cstr!("pa_context_get_sink_info_by_name"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_GET_SINK_INFO_LIST = {
                let fp = dlsym(h, cstr!("pa_context_get_sink_info_list"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_GET_SINK_INPUT_INFO = {
                let fp = dlsym(h, cstr!("pa_context_get_sink_input_info"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_GET_SOURCE_INFO_LIST = {
                let fp = dlsym(h, cstr!("pa_context_get_source_info_list"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_GET_STATE = {
                let fp = dlsym(h, cstr!("pa_context_get_state"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_NEW = {
                let fp = dlsym(h, cstr!("pa_context_new"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_RTTIME_NEW = {
                let fp = dlsym(h, cstr!("pa_context_rttime_new"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_SET_SINK_INPUT_VOLUME = {
                let fp = dlsym(h, cstr!("pa_context_set_sink_input_volume"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_SET_STATE_CALLBACK = {
                let fp = dlsym(h, cstr!("pa_context_set_state_callback"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_ERRNO = {
                let fp = dlsym(h, cstr!("pa_context_errno"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_SET_SUBSCRIBE_CALLBACK = {
                let fp = dlsym(h, cstr!("pa_context_set_subscribe_callback"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_SUBSCRIBE = {
                let fp = dlsym(h, cstr!("pa_context_subscribe"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_REF = {
                let fp = dlsym(h, cstr!("pa_context_ref"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CONTEXT_UNREF = {
                let fp = dlsym(h, cstr!("pa_context_unref"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CVOLUME_SET = {
                let fp = dlsym(h, cstr!("pa_cvolume_set"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_CVOLUME_SET_BALANCE = {
                let fp = dlsym(h, cstr!("pa_cvolume_set_balance"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_FRAME_SIZE = {
                let fp = dlsym(h, cstr!("pa_frame_size"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_MAINLOOP_API_ONCE = {
                let fp = dlsym(h, cstr!("pa_mainloop_api_once"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STRERROR = {
                let fp = dlsym(h, cstr!("pa_strerror"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_OPERATION_REF = {
                let fp = dlsym(h, cstr!("pa_operation_ref"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_OPERATION_UNREF = {
                let fp = dlsym(h, cstr!("pa_operation_unref"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_OPERATION_CANCEL = {
                let fp = dlsym(h, cstr!("pa_operation_cancel"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_OPERATION_GET_STATE = {
                let fp = dlsym(h, cstr!("pa_operation_get_state"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_OPERATION_SET_STATE_CALLBACK = {
                let fp = dlsym(h, cstr!("pa_operation_set_state_callback"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_PROPLIST_GETS = {
                let fp = dlsym(h, cstr!("pa_proplist_gets"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_RTCLOCK_NOW = {
                let fp = dlsym(h, cstr!("pa_rtclock_now"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_BEGIN_WRITE = {
                let fp = dlsym(h, cstr!("pa_stream_begin_write"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_CANCEL_WRITE = {
                let fp = dlsym(h, cstr!("pa_stream_cancel_write"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_IS_SUSPENDED = {
                let fp = dlsym(h, cstr!("pa_stream_is_suspended"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_IS_CORKED = {
                let fp = dlsym(h, cstr!("pa_stream_is_corked"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_CONNECT_PLAYBACK = {
                let fp = dlsym(h, cstr!("pa_stream_connect_playback"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_CONNECT_RECORD = {
                let fp = dlsym(h, cstr!("pa_stream_connect_record"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_CORK = {
                let fp = dlsym(h, cstr!("pa_stream_cork"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_DISCONNECT = {
                let fp = dlsym(h, cstr!("pa_stream_disconnect"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_DROP = {
                let fp = dlsym(h, cstr!("pa_stream_drop"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_BUFFER_ATTR = {
                let fp = dlsym(h, cstr!("pa_stream_get_buffer_attr"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_CHANNEL_MAP = {
                let fp = dlsym(h, cstr!("pa_stream_get_channel_map"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_DEVICE_NAME = {
                let fp = dlsym(h, cstr!("pa_stream_get_device_name"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_INDEX = {
                let fp = dlsym(h, cstr!("pa_stream_get_index"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_LATENCY = {
                let fp = dlsym(h, cstr!("pa_stream_get_latency"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_SAMPLE_SPEC = {
                let fp = dlsym(h, cstr!("pa_stream_get_sample_spec"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_STATE = {
                let fp = dlsym(h, cstr!("pa_stream_get_state"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_CONTEXT = {
                let fp = dlsym(h, cstr!("pa_stream_get_context"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_GET_TIME = {
                let fp = dlsym(h, cstr!("pa_stream_get_time"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_NEW = {
                let fp = dlsym(h, cstr!("pa_stream_new"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_PEEK = {
                let fp = dlsym(h, cstr!("pa_stream_peek"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_READABLE_SIZE = {
                let fp = dlsym(h, cstr!("pa_stream_readable_size"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_SET_STATE_CALLBACK = {
                let fp = dlsym(h, cstr!("pa_stream_set_state_callback"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_SET_WRITE_CALLBACK = {
                let fp = dlsym(h, cstr!("pa_stream_set_write_callback"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_SET_READ_CALLBACK = {
                let fp = dlsym(h, cstr!("pa_stream_set_read_callback"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_REF = {
                let fp = dlsym(h, cstr!("pa_stream_ref"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_UNREF = {
                let fp = dlsym(h, cstr!("pa_stream_unref"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_UPDATE_TIMING_INFO = {
                let fp = dlsym(h, cstr!("pa_stream_update_timing_info"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_WRITABLE_SIZE = {
                let fp = dlsym(h, cstr!("pa_stream_writable_size"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_STREAM_WRITE = {
                let fp = dlsym(h, cstr!("pa_stream_write"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_SW_VOLUME_FROM_LINEAR = {
                let fp = dlsym(h, cstr!("pa_sw_volume_from_linear"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_FREE = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_free"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_GET_API = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_get_api"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_IN_THREAD = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_in_thread"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_LOCK = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_lock"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_NEW = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_new"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_SIGNAL = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_signal"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_START = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_start"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_STOP = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_stop"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_UNLOCK = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_unlock"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_THREADED_MAINLOOP_WAIT = {
                let fp = dlsym(h, cstr!("pa_threaded_mainloop_wait"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_USEC_TO_BYTES = {
                let fp = dlsym(h, cstr!("pa_usec_to_bytes"));
                if fp.is_null() {
                    return None;
                }
                fp
            };
            PA_XREALLOC = {
                let fp = dlsym(h, cstr!("pa_xrealloc"));
                if fp.is_null() {
                    return None;
                }
                fp
            };

            Some(LibLoader {
                     _lib: h,
                 })
        }
    }

    impl ::std::ops::Drop for LibLoader {
        #[inline]
        fn drop(&mut self) {
            unsafe {
                dlclose(self._lib);
            }
        }
    }

    static mut PA_GET_LIBRARY_VERSION: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_get_library_version() -> *const c_char {
        (::std::mem::transmute::<_, extern "C" fn() -> *const c_char>(PA_GET_LIBRARY_VERSION))()
    }

    static mut PA_CHANNEL_MAP_CAN_BALANCE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_channel_map_can_balance(map: *const pa_channel_map) -> c_int {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_channel_map) -> c_int>(PA_CHANNEL_MAP_CAN_BALANCE))(map)
    }

    static mut PA_CHANNEL_MAP_INIT: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_channel_map_init(m: *mut pa_channel_map) -> *mut pa_channel_map {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_channel_map) -> *mut pa_channel_map>(PA_CHANNEL_MAP_INIT))(m)
    }

    static mut PA_CONTEXT_CONNECT: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_connect(c: *mut pa_context,
                                     server: *const c_char,
                                     flags: pa_context_flags_t,
                                     api: *const pa_spawn_api)
                                     -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_context,
                                               *const c_char,
                                               pa_context_flags_t,
                                               *const pa_spawn_api)
                                               -> c_int>(PA_CONTEXT_CONNECT))(c, server, flags, api)
    }

    static mut PA_CONTEXT_DISCONNECT: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_disconnect(c: *mut pa_context) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_context)>(PA_CONTEXT_DISCONNECT))(c)
    }

    static mut PA_CONTEXT_DRAIN: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_drain(c: *mut pa_context,
                                   cb: pa_context_notify_cb_t,
                                   userdata: *mut c_void)
                                   -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_context, pa_context_notify_cb_t, *mut c_void)
                                               -> *mut pa_operation>(PA_CONTEXT_DRAIN))(c, cb, userdata)
    }

    static mut PA_CONTEXT_GET_SERVER_INFO: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_get_server_info(c: *const pa_context,
                                             cb: pa_server_info_cb_t,
                                             userdata: *mut c_void)
                                             -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_context, pa_server_info_cb_t, *mut c_void)
                                               -> *mut pa_operation>(PA_CONTEXT_GET_SERVER_INFO))(c, cb, userdata)
    }

    static mut PA_CONTEXT_GET_SINK_INFO_BY_NAME: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_get_sink_info_by_name(c: *const pa_context,
                                                   name: *const c_char,
                                                   cb: pa_sink_info_cb_t,
                                                   userdata: *mut c_void)
                                                   -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_context,
                                               *const c_char,
                                               pa_sink_info_cb_t,
                                               *mut c_void)
                                               -> *mut pa_operation>(PA_CONTEXT_GET_SINK_INFO_BY_NAME))(c,
                                                                                                        name,
                                                                                                        cb,
                                                                                                        userdata)
    }

    static mut PA_CONTEXT_GET_SINK_INFO_LIST: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_get_sink_info_list(c: *const pa_context,
                                                cb: pa_sink_info_cb_t,
                                                userdata: *mut c_void)
                                                -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_context, pa_sink_info_cb_t, *mut c_void)
                                               -> *mut pa_operation>(PA_CONTEXT_GET_SINK_INFO_LIST))(c, cb, userdata)
    }

    static mut PA_CONTEXT_GET_SINK_INPUT_INFO: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_get_sink_input_info(c: *const pa_context,
                                                 idx: u32,
                                                 cb: pa_sink_input_info_cb_t,
                                                 userdata: *mut c_void)
                                                 -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_context, u32, pa_sink_input_info_cb_t, *mut c_void)
                                               -> *mut pa_operation>(PA_CONTEXT_GET_SINK_INPUT_INFO))(c,
                                                                                                      idx,
                                                                                                      cb,
                                                                                                      userdata)
    }

    static mut PA_CONTEXT_GET_SOURCE_INFO_LIST: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_get_source_info_list(c: *const pa_context,
                                                  cb: pa_source_info_cb_t,
                                                  userdata: *mut c_void)
                                                  -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_context, pa_source_info_cb_t, *mut c_void)
                                               -> *mut pa_operation>(PA_CONTEXT_GET_SOURCE_INFO_LIST))(c, cb, userdata)
    }

    static mut PA_CONTEXT_GET_STATE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_get_state(c: *const pa_context) -> pa_context_state_t {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_context) -> pa_context_state_t>(PA_CONTEXT_GET_STATE))(c)
    }

    static mut PA_CONTEXT_NEW: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_new(mainloop: *mut pa_mainloop_api, name: *const c_char) -> *mut pa_context {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_mainloop_api, *const c_char)
                                               -> *mut pa_context>(PA_CONTEXT_NEW))(mainloop, name)
    }

    static mut PA_CONTEXT_RTTIME_NEW: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_rttime_new(c: *const pa_context,
                                        usec: pa_usec_t,
                                        cb: pa_time_event_cb_t,
                                        userdata: *mut c_void)
                                        -> *mut pa_time_event {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_context,
                                               pa_usec_t,
                                               pa_time_event_cb_t,
                                               *mut c_void)
                                               -> *mut pa_time_event>(PA_CONTEXT_RTTIME_NEW))(c, usec, cb, userdata)
    }

    static mut PA_CONTEXT_SET_SINK_INPUT_VOLUME: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_set_sink_input_volume(c: *mut pa_context,
                                                   idx: u32,
                                                   volume: *const pa_cvolume,
                                                   cb: pa_context_success_cb_t,
                                                   userdata: *mut c_void)
                                                   -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_context,
                                               u32,
                                               *const pa_cvolume,
                                               pa_context_success_cb_t,
                                               *mut c_void)
                                               -> *mut pa_operation>(PA_CONTEXT_SET_SINK_INPUT_VOLUME))(c,
                                                                                                        idx,
                                                                                                        volume,
                                                                                                        cb,
                                                                                                        userdata)
    }

    static mut PA_CONTEXT_SET_STATE_CALLBACK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_set_state_callback(c: *mut pa_context,
                                                cb: pa_context_notify_cb_t,
                                                userdata: *mut c_void) {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_context,
                                               pa_context_notify_cb_t,
                                               *mut c_void)>(PA_CONTEXT_SET_STATE_CALLBACK))(c, cb, userdata)
    }

    static mut PA_CONTEXT_ERRNO: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_errno(c: *mut pa_context) -> c_int {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_context) -> c_int>(PA_CONTEXT_ERRNO))(c)
    }

    static mut PA_CONTEXT_SET_SUBSCRIBE_CALLBACK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_set_subscribe_callback(c: *mut pa_context,
                                                    cb: pa_context_subscribe_cb_t,
                                                    userdata: *mut c_void) {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_context,
                                               pa_context_subscribe_cb_t,
                                               *mut c_void)>(PA_CONTEXT_SET_SUBSCRIBE_CALLBACK))(c, cb, userdata)
    }

    static mut PA_CONTEXT_SUBSCRIBE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_subscribe(c: *mut pa_context,
                                       m: pa_subscription_mask_t,
                                       cb: pa_context_success_cb_t,
                                       userdata: *mut c_void)
                                       -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_context,
                                               pa_subscription_mask_t,
                                               pa_context_success_cb_t,
                                               *mut c_void)
                                               -> *mut pa_operation>(PA_CONTEXT_SUBSCRIBE))(c, m, cb, userdata)
    }

    static mut PA_CONTEXT_REF: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_ref(c: *mut pa_context) -> *mut pa_context {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_context) -> *mut pa_context>(PA_CONTEXT_REF))(c)
    }

    static mut PA_CONTEXT_UNREF: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_context_unref(c: *mut pa_context) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_context)>(PA_CONTEXT_UNREF))(c)
    }

    static mut PA_CVOLUME_SET: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_cvolume_set(a: *mut pa_cvolume, channels: c_uint, v: pa_volume_t) -> *mut pa_cvolume {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_cvolume, c_uint, pa_volume_t)
                                               -> *mut pa_cvolume>(PA_CVOLUME_SET))(a, channels, v)
    }

    static mut PA_CVOLUME_SET_BALANCE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_cvolume_set_balance(v: *mut pa_cvolume,
                                         map: *const pa_channel_map,
                                         new_balance: c_float)
                                         -> *mut pa_cvolume {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_cvolume,
                                               *const pa_channel_map,
                                               c_float)
                                               -> *mut pa_cvolume>(PA_CVOLUME_SET_BALANCE))(v, map, new_balance)
    }

    static mut PA_FRAME_SIZE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_frame_size(spec: *const pa_sample_spec) -> usize {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_sample_spec) -> usize>(PA_FRAME_SIZE))(spec)
    }

    static mut PA_MAINLOOP_API_ONCE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_mainloop_api_once(m: *mut pa_mainloop_api,
                                       callback: pa_mainloop_api_once_cb_t,
                                       userdata: *mut c_void) {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_mainloop_api,
                                               pa_mainloop_api_once_cb_t,
                                               *mut c_void)>(PA_MAINLOOP_API_ONCE))(m, callback, userdata)
    }

    static mut PA_STRERROR: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_strerror(error: pa_error_code_t) -> *const c_char {
        (::std::mem::transmute::<_, extern "C" fn(pa_error_code_t) -> *const c_char>(PA_STRERROR))(error)
    }

    static mut PA_OPERATION_REF: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_operation_ref(o: *mut pa_operation) -> *mut pa_operation {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_operation) -> *mut pa_operation>(PA_OPERATION_REF))(o)
    }

    static mut PA_OPERATION_UNREF: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_operation_unref(o: *mut pa_operation) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_operation)>(PA_OPERATION_UNREF))(o)
    }

    static mut PA_OPERATION_CANCEL: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_operation_cancel(o: *mut pa_operation) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_operation)>(PA_OPERATION_CANCEL))(o)
    }

    static mut PA_OPERATION_GET_STATE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_operation_get_state(o: *const pa_operation) -> pa_operation_state_t {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_operation)
                                               -> pa_operation_state_t>(PA_OPERATION_GET_STATE))(o)
    }

    static mut PA_OPERATION_SET_STATE_CALLBACK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_operation_set_state_callback(o: *mut pa_operation,
                                                  cb: pa_operation_notify_cb_t,
                                                  userdata: *mut c_void) {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_operation,
                                               pa_operation_notify_cb_t,
                                               *mut c_void)>(PA_OPERATION_SET_STATE_CALLBACK))(o, cb, userdata)
    }

    static mut PA_PROPLIST_GETS: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_proplist_gets(p: *mut pa_proplist, key: *const c_char) -> *const c_char {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_proplist, *const c_char)
                                               -> *const c_char>(PA_PROPLIST_GETS))(p, key)
    }

    static mut PA_RTCLOCK_NOW: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_rtclock_now() -> pa_usec_t {
        (::std::mem::transmute::<_, extern "C" fn() -> pa_usec_t>(PA_RTCLOCK_NOW))()
    }

    static mut PA_STREAM_BEGIN_WRITE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_begin_write(p: *mut pa_stream, data: *mut *mut c_void, nbytes: *mut usize) -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream,
                                               *mut *mut c_void,
                                               *mut usize)
                                               -> c_int>(PA_STREAM_BEGIN_WRITE))(p, data, nbytes)
    }

    static mut PA_STREAM_CANCEL_WRITE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_cancel_write(p: *mut pa_stream) -> c_int {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_stream) -> c_int>(PA_STREAM_CANCEL_WRITE))(p)
    }

    static mut PA_STREAM_IS_SUSPENDED: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_is_suspended(s: *const pa_stream) -> c_int {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_stream) -> c_int>(PA_STREAM_IS_SUSPENDED))(s)
    }

    static mut PA_STREAM_IS_CORKED: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_is_corked(s: *const pa_stream) -> c_int {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_stream) -> c_int>(PA_STREAM_IS_CORKED))(s)
    }

    static mut PA_STREAM_CONNECT_PLAYBACK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_connect_playback(s: *mut pa_stream,
                                             dev: *const c_char,
                                             attr: *const pa_buffer_attr,
                                             flags: pa_stream_flags_t,
                                             volume: *const pa_cvolume,
                                             sync_stream: *const pa_stream)
                                             -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream,
                                               *const c_char,
                                               *const pa_buffer_attr,
                                               pa_stream_flags_t,
                                               *const pa_cvolume,
                                               *const pa_stream)
                                               -> c_int>(PA_STREAM_CONNECT_PLAYBACK))(s,
                                                                                      dev,
                                                                                      attr,
                                                                                      flags,
                                                                                      volume,
                                                                                      sync_stream)
    }

    static mut PA_STREAM_CONNECT_RECORD: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_connect_record(s: *mut pa_stream,
                                           dev: *const c_char,
                                           attr: *const pa_buffer_attr,
                                           flags: pa_stream_flags_t)
                                           -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream,
                                               *const c_char,
                                               *const pa_buffer_attr,
                                               pa_stream_flags_t)
                                               -> c_int>(PA_STREAM_CONNECT_RECORD))(s, dev, attr, flags)
    }

    static mut PA_STREAM_CORK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_cork(s: *mut pa_stream,
                                 b: c_int,
                                 cb: pa_stream_success_cb_t,
                                 userdata: *mut c_void)
                                 -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream, c_int, pa_stream_success_cb_t, *mut c_void)
                                               -> *mut pa_operation>(PA_STREAM_CORK))(s, b, cb, userdata)
    }

    static mut PA_STREAM_DISCONNECT: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_disconnect(s: *mut pa_stream) -> c_int {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_stream) -> c_int>(PA_STREAM_DISCONNECT))(s)
    }

    static mut PA_STREAM_DROP: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_drop(p: *mut pa_stream) -> c_int {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_stream) -> c_int>(PA_STREAM_DROP))(p)
    }

    static mut PA_STREAM_GET_BUFFER_ATTR: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_buffer_attr(s: *const pa_stream) -> *const pa_buffer_attr {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_stream)
                                               -> *const pa_buffer_attr>(PA_STREAM_GET_BUFFER_ATTR))(s)
    }

    static mut PA_STREAM_GET_CHANNEL_MAP: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_channel_map(s: *const pa_stream) -> *const pa_channel_map {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_stream)
                                               -> *const pa_channel_map>(PA_STREAM_GET_CHANNEL_MAP))(s)
    }

    static mut PA_STREAM_GET_DEVICE_NAME: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_device_name(s: *const pa_stream) -> *const c_char {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_stream) -> *const c_char>(PA_STREAM_GET_DEVICE_NAME))(s)
    }

    static mut PA_STREAM_GET_INDEX: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_index(s: *const pa_stream) -> u32 {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_stream) -> u32>(PA_STREAM_GET_INDEX))(s)
    }

    static mut PA_STREAM_GET_LATENCY: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_latency(s: *const pa_stream, r_usec: *mut pa_usec_t, negative: *mut c_int) -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_stream,
                                               *mut pa_usec_t,
                                               *mut c_int)
                                               -> c_int>(PA_STREAM_GET_LATENCY))(s, r_usec, negative)
    }

    static mut PA_STREAM_GET_SAMPLE_SPEC: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_sample_spec(s: *const pa_stream) -> *const pa_sample_spec {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_stream)
                                               -> *const pa_sample_spec>(PA_STREAM_GET_SAMPLE_SPEC))(s)
    }

    static mut PA_STREAM_GET_STATE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_state(p: *const pa_stream) -> pa_stream_state_t {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_stream) -> pa_stream_state_t>(PA_STREAM_GET_STATE))(p)
    }

    static mut PA_STREAM_GET_CONTEXT: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_context(s: *const pa_stream) -> *mut pa_context {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_stream) -> *mut pa_context>(PA_STREAM_GET_CONTEXT))(s)
    }

    static mut PA_STREAM_GET_TIME: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_get_time(s: *const pa_stream, r_usec: *mut pa_usec_t) -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*const pa_stream, *mut pa_usec_t)
                                               -> c_int>(PA_STREAM_GET_TIME))(s, r_usec)
    }

    static mut PA_STREAM_NEW: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_new(c: *mut pa_context,
                                name: *const c_char,
                                ss: *const pa_sample_spec,
                                map: *const pa_channel_map)
                                -> *mut pa_stream {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_context,
                                               *const c_char,
                                               *const pa_sample_spec,
                                               *const pa_channel_map)
                                               -> *mut pa_stream>(PA_STREAM_NEW))(c, name, ss, map)
    }

    static mut PA_STREAM_PEEK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_peek(p: *mut pa_stream, data: *mut *const c_void, nbytes: *mut usize) -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream,
                                               *mut *const c_void,
                                               *mut usize)
                                               -> c_int>(PA_STREAM_PEEK))(p, data, nbytes)
    }

    static mut PA_STREAM_READABLE_SIZE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_readable_size(p: *const pa_stream) -> usize {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_stream) -> usize>(PA_STREAM_READABLE_SIZE))(p)
    }

    static mut PA_STREAM_SET_STATE_CALLBACK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_set_state_callback(s: *mut pa_stream, cb: pa_stream_notify_cb_t, userdata: *mut c_void) {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream,
                                               pa_stream_notify_cb_t,
                                               *mut c_void)>(PA_STREAM_SET_STATE_CALLBACK))(s, cb, userdata)
    }

    static mut PA_STREAM_SET_WRITE_CALLBACK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_set_write_callback(p: *mut pa_stream, cb: pa_stream_request_cb_t, userdata: *mut c_void) {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream,
                                               pa_stream_request_cb_t,
                                               *mut c_void)>(PA_STREAM_SET_WRITE_CALLBACK))(p, cb, userdata)
    }

    static mut PA_STREAM_SET_READ_CALLBACK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_set_read_callback(p: *mut pa_stream, cb: pa_stream_request_cb_t, userdata: *mut c_void) {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream,
                                               pa_stream_request_cb_t,
                                               *mut c_void)>(PA_STREAM_SET_READ_CALLBACK))(p, cb, userdata)
    }

    static mut PA_STREAM_REF: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_ref(s: *mut pa_stream) -> *mut pa_stream {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_stream) -> *mut pa_stream>(PA_STREAM_REF))(s)
    }

    static mut PA_STREAM_UNREF: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_unref(s: *mut pa_stream) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_stream)>(PA_STREAM_UNREF))(s)
    }

    static mut PA_STREAM_UPDATE_TIMING_INFO: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_update_timing_info(p: *mut pa_stream,
                                               cb: pa_stream_success_cb_t,
                                               userdata: *mut c_void)
                                               -> *mut pa_operation {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream, pa_stream_success_cb_t, *mut c_void)
                                               -> *mut pa_operation>(PA_STREAM_UPDATE_TIMING_INFO))(p, cb, userdata)
    }

    static mut PA_STREAM_WRITABLE_SIZE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_writable_size(p: *const pa_stream) -> usize {
        (::std::mem::transmute::<_, extern "C" fn(*const pa_stream) -> usize>(PA_STREAM_WRITABLE_SIZE))(p)
    }

    static mut PA_STREAM_WRITE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_stream_write(p: *mut pa_stream,
                                  data: *const c_void,
                                  nbytes: usize,
                                  free_cb: pa_free_cb_t,
                                  offset: i64,
                                  seek: pa_seek_mode_t)
                                  -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_stream,
                                               *const c_void,
                                               usize,
                                               pa_free_cb_t,
                                               i64,
                                               pa_seek_mode_t)
                                               -> c_int>(PA_STREAM_WRITE))(p, data, nbytes, free_cb, offset, seek)
    }

    static mut PA_SW_VOLUME_FROM_LINEAR: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_sw_volume_from_linear(v: c_double) -> pa_volume_t {
        (::std::mem::transmute::<_, extern "C" fn(c_double) -> pa_volume_t>(PA_SW_VOLUME_FROM_LINEAR))(v)
    }

    static mut PA_THREADED_MAINLOOP_FREE: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_free(m: *mut pa_threaded_mainloop) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_threaded_mainloop)>(PA_THREADED_MAINLOOP_FREE))(m)
    }

    static mut PA_THREADED_MAINLOOP_GET_API: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_get_api(m: *mut pa_threaded_mainloop) -> *mut pa_mainloop_api {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_threaded_mainloop)
                                               -> *mut pa_mainloop_api>(PA_THREADED_MAINLOOP_GET_API))(m)
    }

    static mut PA_THREADED_MAINLOOP_IN_THREAD: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_in_thread(m: *mut pa_threaded_mainloop) -> c_int {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_threaded_mainloop)
                                               -> c_int>(PA_THREADED_MAINLOOP_IN_THREAD))(m)
    }

    static mut PA_THREADED_MAINLOOP_LOCK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_lock(m: *mut pa_threaded_mainloop) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_threaded_mainloop)>(PA_THREADED_MAINLOOP_LOCK))(m)
    }

    static mut PA_THREADED_MAINLOOP_NEW: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_new() -> *mut pa_threaded_mainloop {
        (::std::mem::transmute::<_, extern "C" fn() -> *mut pa_threaded_mainloop>(PA_THREADED_MAINLOOP_NEW))()
    }

    static mut PA_THREADED_MAINLOOP_SIGNAL: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_signal(m: *mut pa_threaded_mainloop, wait_for_accept: c_int) {
        (::std::mem::transmute::<_,
                                 extern "C" fn(*mut pa_threaded_mainloop,
                                               c_int)>(PA_THREADED_MAINLOOP_SIGNAL))(m, wait_for_accept)
    }

    static mut PA_THREADED_MAINLOOP_START: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_start(m: *mut pa_threaded_mainloop) -> c_int {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_threaded_mainloop) -> c_int>(PA_THREADED_MAINLOOP_START))(m)
    }

    static mut PA_THREADED_MAINLOOP_STOP: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_stop(m: *mut pa_threaded_mainloop) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_threaded_mainloop)>(PA_THREADED_MAINLOOP_STOP))(m)
    }

    static mut PA_THREADED_MAINLOOP_UNLOCK: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_unlock(m: *mut pa_threaded_mainloop) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_threaded_mainloop)>(PA_THREADED_MAINLOOP_UNLOCK))(m)
    }

    static mut PA_THREADED_MAINLOOP_WAIT: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_threaded_mainloop_wait(m: *mut pa_threaded_mainloop) {
        (::std::mem::transmute::<_, extern "C" fn(*mut pa_threaded_mainloop)>(PA_THREADED_MAINLOOP_WAIT))(m)
    }

    static mut PA_USEC_TO_BYTES: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_usec_to_bytes(t: pa_usec_t, spec: *const pa_sample_spec) -> usize {
        (::std::mem::transmute::<_, extern "C" fn(pa_usec_t, *const pa_sample_spec) -> usize>(PA_USEC_TO_BYTES))(t,
                                                                                                                 spec)
    }

    static mut PA_XREALLOC: *mut ::libc::c_void = 0 as *mut _;
    #[inline]
    pub unsafe fn pa_xrealloc(ptr: *mut c_void, size: usize) -> *mut c_void {
        (::std::mem::transmute::<_, extern "C" fn(*mut c_void, usize) -> *mut c_void>(PA_XREALLOC))(ptr, size)
    }

}

#[cfg(feature = "dlopen")]
pub use self::dynamic_fns::*;
