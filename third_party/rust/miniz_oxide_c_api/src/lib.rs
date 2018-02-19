extern crate crc;
extern crate libc;
extern crate miniz_oxide;

use std::{cmp, ptr, slice};
use std::panic::{catch_unwind, AssertUnwindSafe};
use crc::{Hasher32, crc32};

use libc::{c_int, c_uint, c_ulong, c_void, size_t};

use miniz_oxide::{MZError, MZResult};
#[allow(bad_style)]
pub use tdef::tdefl_compressor;

pub use miniz_oxide::mz_adler32_oxide;
use miniz_oxide::deflate::CompressionLevel;
use miniz_oxide::deflate::core::CompressionStrategy;

#[macro_use]
mod unmangle;

pub mod lib_oxide;
use lib_oxide::*;

mod tinfl;
pub use tinfl::{tinfl_decompress, tinfl_decompress_mem_to_heap,
                tinfl_decompress_mem_to_mem, tinfl_decompressor};

mod tdef;
pub use tdef::{tdefl_compress, tdefl_compress_buffer, tdefl_compress_mem_to_heap,
               tdefl_compress_mem_to_mem, tdefl_compress_mem_to_output,
               tdefl_create_comp_flags_from_zip_params, tdefl_get_prev_return_status, tdefl_init,
               tdefl_get_adler32};

pub use tdef::flush_modes::*;
pub use tdef::strategy::*;

pub fn mz_crc32_oxide(crc32: c_uint, data: &[u8]) -> c_uint {
    let mut digest = crc32::Digest::new_with_initial(crc32::IEEE, crc32);
    digest.write(data);
    digest.sum32()
}

pub const MZ_DEFLATED: c_int = 8;
pub const MZ_DEFAULT_WINDOW_BITS: c_int = 15;

pub const MZ_ADLER32_INIT: c_ulong = 1;
pub const MZ_CRC32_INIT: c_ulong = 0;

fn as_c_return_code(r: MZResult) -> c_int {
    match r {
        Err(status) => status as c_int,
        Ok(status) => status as c_int,
    }
}

unmangle!(
pub unsafe extern "C" fn miniz_def_alloc_func(
    _opaque: *mut c_void,
    items: size_t,
    size: size_t,
) -> *mut c_void {
    libc::malloc(items * size)
}

pub unsafe extern "C" fn miniz_def_free_func(_opaque: *mut c_void, address: *mut c_void) {
    libc::free(address)
}

pub unsafe extern "C" fn miniz_def_realloc_func(
    _opaque: *mut c_void,
    address: *mut c_void,
    items: size_t,
    size: size_t,
) -> *mut c_void {
    libc::realloc(address, items * size)
}

pub unsafe extern "C" fn mz_adler32(adler: c_ulong, ptr: *const u8, buf_len: usize) -> c_ulong {
    ptr.as_ref().map_or(MZ_ADLER32_INIT, |r| {
        let data = slice::from_raw_parts(r, buf_len);
        mz_adler32_oxide(adler as c_uint, data) as c_ulong
    })
}

pub unsafe extern "C" fn mz_crc32(crc: c_ulong, ptr: *const u8, buf_len: size_t) -> c_ulong {
    ptr.as_ref().map_or(MZ_CRC32_INIT, |r| {
        let data = slice::from_raw_parts(r, buf_len);
        mz_crc32_oxide(crc as c_uint, data) as c_ulong
    })
}
);

macro_rules! oxidize {
    ($mz_func:ident, $mz_func_oxide:ident; $($arg_name:ident: $type_name:ident),*) => {
        unmangle!(
        pub unsafe extern "C" fn $mz_func(stream: *mut mz_stream, $($arg_name: $type_name),*)
                                          -> c_int {
            match stream.as_mut() {
                None => MZError::Stream as c_int,
                Some(stream) => {
                    // Make sure we catch a potential panic, as
                    // this is called from C.
                    match catch_unwind(AssertUnwindSafe(|| {
                        let mut stream_oxide = StreamOxide::new(&mut *stream);
                        let status = $mz_func_oxide(&mut stream_oxide, $($arg_name),*);
                        *stream = stream_oxide.into_mz_stream();
                        as_c_return_code(status)
                    })) {
                        Ok(res) => res,
                        Err(_) => {
                            println!("FATAL ERROR: Caught panic!");
                            MZError::Stream as c_int},
                    }
                }
            }
        });
    };
}

oxidize!(mz_deflateInit2, mz_deflate_init2_oxide;
         level: c_int, method: c_int, window_bits: c_int, mem_level: c_int, strategy: c_int);
oxidize!(mz_deflate, mz_deflate_oxide;
         flush: c_int);
oxidize!(mz_deflateEnd, mz_deflate_end_oxide;);
oxidize!(mz_deflateReset, mz_deflate_reset_oxide;);


oxidize!(mz_inflateInit2, mz_inflate_init2_oxide;
         window_bits: c_int);
oxidize!(mz_inflate, mz_inflate_oxide;
         flush: c_int);
oxidize!(mz_inflateEnd, mz_inflate_end_oxide;);

unmangle!(
pub unsafe extern "C" fn mz_deflateInit(stream: *mut mz_stream, level: c_int) -> c_int {
    mz_deflateInit2(
        stream,
        level,
        MZ_DEFLATED,
        MZ_DEFAULT_WINDOW_BITS,
        9,
        CompressionStrategy::Default as c_int,
    )
}

pub unsafe extern "C" fn mz_compress(
    dest: *mut u8,
    dest_len: *mut c_ulong,
    source: *const u8,
    source_len: c_ulong,
) -> c_int {
    mz_compress2(
        dest,
        dest_len,
        source,
        source_len,
        CompressionLevel::DefaultCompression as c_int,
    )
}

pub unsafe extern "C" fn mz_compress2(
    dest: *mut u8,
    dest_len: *mut c_ulong,
    source: *const u8,
    source_len: c_ulong,
    level: c_int,
) -> c_int {
    dest_len.as_mut().map_or(
        MZError::Param as c_int,
        |dest_len| {
            if buffer_too_large(source_len, *dest_len) {
                return MZError::Param as c_int;
            }

            let mut stream: mz_stream = mz_stream {
                next_in: source,
                avail_in: source_len as c_uint,
                next_out: dest,
                avail_out: (*dest_len) as c_uint,
                ..Default::default()
            };

            let mut stream_oxide = StreamOxide::new(&mut stream);
            as_c_return_code(mz_compress2_oxide(&mut stream_oxide, level, dest_len))
        },
    )
}

pub extern "C" fn mz_deflateBound(_stream: *mut mz_stream, source_len: c_ulong) -> c_ulong {
    cmp::max(
        128 + (source_len * 110) / 100,
        128 + source_len + ((source_len / (31 * 1024)) + 1) * 5,
    )
}

pub unsafe extern "C" fn mz_inflateInit(stream: *mut mz_stream) -> c_int {
    mz_inflateInit2(stream, MZ_DEFAULT_WINDOW_BITS)
}

pub unsafe extern "C" fn mz_uncompress(
    dest: *mut u8,
    dest_len: *mut c_ulong,
    source: *const u8,
    source_len: c_ulong,
) -> c_int {
    dest_len.as_mut().map_or(
        MZError::Param as c_int,
        |dest_len| {
            if buffer_too_large(source_len, *dest_len) {
                return MZError::Param as c_int;
            }

            let mut stream: mz_stream = mz_stream {
                next_in: source,
                avail_in: source_len as c_uint,
                next_out: dest,
                avail_out: (*dest_len) as c_uint,
                ..Default::default()
            };

            let mut stream_oxide = StreamOxide::new(&mut stream);
            as_c_return_code(mz_uncompress2_oxide(&mut stream_oxide, dest_len))
        },
    )
}

pub extern "C" fn mz_compressBound(source_len: c_ulong) -> c_ulong {
    mz_deflateBound(ptr::null_mut(), source_len)
}

);

#[cfg(target_bit_width = "64")]
#[inline]
fn buffer_too_large(source_len: c_ulong, dest_len: c_ulong) -> bool {
    (source_len | dest_len) > 0xFFFFFFFF
}

#[cfg(not(target_bit_width = "64"))]
#[inline]
fn buffer_too_large(_source_len: c_ulong, _dest_len: c_ulong) -> bool {
    false
}
