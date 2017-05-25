#![doc(html_root_url = "https://docs.rs/miniz-sys/0.1")]
#![allow(bad_style)]

extern crate libc;
use libc::*;

pub const MZ_NO_FLUSH: c_int = 0;
pub const MZ_PARTIAL_FLUSH: c_int = 1;
pub const MZ_SYNC_FLUSH: c_int = 2;
pub const MZ_FULL_FLUSH: c_int = 3;
pub const MZ_FINISH: c_int = 4;
pub const MZ_BLOCK: c_int = 5;

pub const MZ_OK: c_int = 0;
pub const MZ_STREAM_END: c_int = 1;
pub const MZ_NEED_DICT: c_int = 2;
pub const MZ_ERRNO: c_int = -1;
pub const MZ_STREAM_ERROR: c_int = -2;
pub const MZ_DATA_ERROR: c_int = -3;
pub const MZ_MEM_ERROR: c_int = -4;
pub const MZ_BUF_ERROR: c_int = -5;
pub const MZ_VERSION_ERROR: c_int = -6;
pub const MZ_PARAM_ERROR: c_int = -10000;

pub const MZ_DEFLATED: c_int = 8;
pub const MZ_DEFAULT_WINDOW_BITS: c_int = 15;
pub const MZ_DEFAULT_STRATEGY: c_int = 0;

#[repr(C)]
pub struct mz_stream {
    pub next_in: *const u8,
    pub avail_in: c_uint,
    pub total_in: c_ulong,

    pub next_out: *mut u8,
    pub avail_out: c_uint,
    pub total_out: c_ulong,

    pub msg: *const c_char,
    pub state: *mut mz_internal_state,

    pub zalloc: Option<mz_alloc_func>,
    pub zfree: Option<mz_free_func>,
    pub opaque: *mut c_void,

    pub data_type: c_int,
    pub adler: c_ulong,
    pub reserved: c_ulong,
}

pub enum mz_internal_state {}

pub type mz_alloc_func = extern fn(*mut c_void,
                                   size_t,
                                   size_t) -> *mut c_void;
pub type mz_free_func = extern fn(*mut c_void, *mut c_void);

extern {
    pub fn mz_deflateInit2(stream: *mut mz_stream,
                           level: c_int,
                           method: c_int,
                           window_bits: c_int,
                           mem_level: c_int,
                           strategy: c_int)
                           -> c_int;
    pub fn mz_deflate(stream: *mut mz_stream, flush: c_int) -> c_int;
    pub fn mz_deflateEnd(stream: *mut mz_stream) -> c_int;
    pub fn mz_deflateReset(stream: *mut mz_stream) -> c_int;

    pub fn mz_inflateInit2(stream: *mut mz_stream,
                           window_bits: c_int)
                           -> c_int;
    pub fn mz_inflate(stream: *mut mz_stream, flush: c_int) -> c_int;
    pub fn mz_inflateEnd(stream: *mut mz_stream) -> c_int;

    pub fn mz_crc32(crc: c_ulong, ptr: *const u8, len: size_t) -> c_ulong;
}
