#![doc(html_root_url = "https://docs.rs/libz-sys/1.0")]
#![allow(non_camel_case_types)]

extern crate libc;

use libc::{c_char, c_int, c_long, c_uchar, c_uint, c_ulong, c_void};

pub type alloc_func = unsafe extern fn (voidpf, uInt, uInt) -> voidpf;
pub type Bytef = u8;
pub type free_func = unsafe extern fn (voidpf, voidpf);
pub type gzFile = *mut gzFile_s;
pub type in_func = unsafe extern fn (*mut c_void, *mut *const c_uchar) -> c_uint;
pub type out_func = unsafe extern fn (*mut c_void, *mut c_uchar, c_uint) -> c_int;
pub type uInt = c_uint;
pub type uLong = c_ulong;
pub type uLongf = c_ulong;
pub type voidp = *mut c_void;
pub type voidpc = *const c_void;
pub type voidpf = *mut c_void;

pub enum gzFile_s {}
pub enum internal_state {}

#[cfg(unix)] pub type z_off_t = libc::off_t;
#[cfg(not(unix))] pub type z_off_t = c_long;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct gz_header {
    pub text: c_int,
    pub time: uLong,
    pub xflags: c_int,
    pub os: c_int,
    pub extra: *mut Bytef,
    pub extra_len: uInt,
    pub extra_max: uInt,
    pub name: *mut Bytef,
    pub name_max: uInt,
    pub comment: *mut Bytef,
    pub comm_max: uInt,
    pub hcrc: c_int,
    pub done: c_int,
}
pub type gz_headerp = *mut gz_header;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct z_stream {
    pub next_in: *mut Bytef,
    pub avail_in: uInt,
    pub total_in: uLong,
    pub next_out: *mut Bytef,
    pub avail_out: uInt,
    pub total_out: uLong,
    pub msg: *mut c_char,
    pub state: *mut internal_state,
    pub zalloc: alloc_func,
    pub zfree: free_func,
    pub opaque: voidpf,
    pub data_type: c_int,
    pub adler: uLong,
    pub reserved: uLong,
}
pub type z_streamp = *mut z_stream;

macro_rules! fns {
    ($($arg:tt)*) => {
        item! {
            #[cfg(all(target_env = "msvc", target_pointer_width = "32"))]
            extern { $($arg)* }
        }
        item! {
            #[cfg(not(all(target_env = "msvc", target_pointer_width = "32")))]
            extern "system" { $($arg)* }
        }
    }
}

macro_rules! item {
    ($i:item) => ($i)
}

fns! {
    pub fn adler32(adler: uLong, buf: *const Bytef, len: uInt) -> uLong;
    pub fn adler32_combine(adler1: uLong, adler2: uLong, len2: z_off_t) -> uLong;
    pub fn compress(dest: *mut Bytef, destLen: *mut uLongf,
                    source: *const Bytef, sourceLen: uLong) -> c_int;
    pub fn compress2(dest: *mut Bytef, destLen: *mut uLongf,
                     source: *const Bytef, sourceLen: uLong,
                     level: c_int) -> c_int;
    pub fn compressBound(sourceLen: uLong) -> uLong;
    pub fn crc32(crc: uLong, buf: *const Bytef, len: uInt) -> uLong;
    pub fn crc32_combine(crc1: uLong, crc2: uLong, len2: z_off_t) -> uLong;
    pub fn deflate(strm: z_streamp, flush: c_int) -> c_int;
    pub fn deflateBound(strm: z_streamp, sourceLen: uLong) -> uLong;
    pub fn deflateCopy(dest: z_streamp, source: z_streamp) -> c_int;
    pub fn deflateEnd(strm: z_streamp) -> c_int;
    pub fn deflateInit_(strm: z_streamp, level: c_int,
                        version: *const c_char,
                        stream_size: c_int) -> c_int;
    pub fn deflateInit2_(strm: z_streamp,
                         level: c_int,
                         method: c_int,
                         windowBits: c_int,
                         memLevel: c_int,
                         strategy: c_int,
                         version: *const c_char,
                         stream_size: c_int) -> c_int;
    pub fn deflateParams(strm: z_streamp,
                         level: c_int,
                         strategy: c_int) -> c_int;
    pub fn deflatePrime(strm: z_streamp, bits: c_int, value: c_int) -> c_int;
    pub fn deflateReset(strm: z_streamp) -> c_int;
    pub fn deflateSetDictionary(strm: z_streamp,
                                dictionary: *const Bytef,
                                dictLength: uInt) -> c_int;
    pub fn deflateSetHeader(strm: z_streamp, head: gz_headerp) -> c_int;
    pub fn deflateTune(strm: z_streamp,
                       good_length: c_int,
                       max_lazy: c_int,
                       nice_length: c_int,
                       max_chain: c_int) -> c_int;
    pub fn gzdirect(file: gzFile) -> c_int;
    pub fn gzdopen(fd: c_int, mode: *const c_char) -> gzFile;
    pub fn gzclearerr(file: gzFile);
    pub fn gzclose(file: gzFile) -> c_int;
    pub fn gzeof(file: gzFile) -> c_int;
    pub fn gzerror(file: gzFile, errnum: *mut c_int) -> *const c_char;
    pub fn gzflush(file: gzFile, flush: c_int) -> c_int;
    pub fn gzgetc(file: gzFile) -> c_int;
    pub fn gzgets(file: gzFile, buf: *mut c_char, len: c_int) -> *mut c_char;
    pub fn gzopen(path: *const c_char, mode: *const c_char) -> gzFile;
    pub fn gzputc(file: gzFile, c: c_int) -> c_int;
    pub fn gzputs(file: gzFile, s: *const c_char) -> c_int;
    pub fn gzread(file: gzFile, buf: voidp, len: c_uint) -> c_int;
    pub fn gzrewind(file: gzFile) -> c_int;
    pub fn gzseek(file: gzFile, offset: z_off_t, whence: c_int) -> z_off_t;
    pub fn gzsetparams(file: gzFile, level: c_int, strategy: c_int) -> c_int;
    pub fn gztell(file: gzFile) -> z_off_t;
    pub fn gzungetc(c: c_int, file: gzFile) -> c_int;
    pub fn gzwrite(file: gzFile, buf: voidpc, len: c_uint) -> c_int;
    pub fn inflate(strm: z_streamp, flush: c_int) -> c_int;
    pub fn inflateBack(strm: z_streamp,
                       _in: in_func,
                       in_desc: *mut c_void,
                       out: out_func,
                       out_desc: *mut c_void) -> c_int;
    pub fn inflateBackEnd(strm: z_streamp) -> c_int;
    pub fn inflateBackInit_(strm: z_streamp,
                            windowBits: c_int,
                            window: *mut c_uchar,
                            version: *const c_char,
                            stream_size: c_int) -> c_int;
    pub fn inflateCopy(dest: z_streamp, source: z_streamp) -> c_int;
    pub fn inflateEnd(strm: z_streamp) -> c_int;
    pub fn inflateGetHeader(strm: z_streamp, head: gz_headerp) -> c_int;
    pub fn inflateInit_(strm: z_streamp,
                        version: *const c_char,
                        stream_size: c_int) -> c_int;
    pub fn inflateInit2_(strm: z_streamp,
                         windowBits: c_int,
                         version: *const c_char,
                         stream_size: c_int) -> c_int;
    pub fn inflateMark(strm: z_streamp) -> c_long;
    pub fn inflatePrime(strm: z_streamp, bits: c_int, value: c_int) -> c_int;
    pub fn inflateReset(strm: z_streamp) -> c_int;
    pub fn inflateReset2(strm: z_streamp, windowBits: c_int) -> c_int;
    pub fn inflateSetDictionary(strm: z_streamp,
                                dictionary: *const Bytef,
                                dictLength: uInt) -> c_int;
    pub fn inflateSync(strm: z_streamp) -> c_int;
    pub fn uncompress(dest: *mut Bytef,
                      destLen: *mut uLongf,
                      source: *const Bytef,
                      sourceLen: uLong) -> c_int;
    pub fn zlibCompileFlags() -> uLong;
    pub fn zlibVersion() -> *const c_char;


// The above set of functions currently target 1.2.3.4 (what's present on Ubuntu
// 12.04, but there's some other APIs that were added later. Should figure out
// how to expose them...
//
// Added in 1.2.5.1
//
//     pub fn deflatePending(strm: z_streamp,
//                           pending: *mut c_uint,
//                           bits: *mut c_int) -> c_int;
//
// Addedin 1.2.7.1
//     pub fn inflateGetDictionary(strm: z_streamp,
//                                 dictionary: *mut Bytef,
//                                 dictLength: *mut uInt) -> c_int;
//
// Added in 1.2.3.5
//     pub fn gzbuffer(file: gzFile, size: c_uint) -> c_int;
//     pub fn gzclose_r(file: gzFile) -> c_int;
//     pub fn gzclose_w(file: gzFile) -> c_int;
//     pub fn gzoffset(file: gzFile) -> z_off_t;
}

pub const Z_NO_FLUSH: c_int = 0;
pub const Z_PARTIAL_FLUSH: c_int = 1;
pub const Z_SYNC_FLUSH: c_int = 2;
pub const Z_FULL_FLUSH: c_int = 3;
pub const Z_FINISH: c_int = 4;
pub const Z_BLOCK: c_int = 5;
pub const Z_TREES: c_int = 6;

pub const Z_OK: c_int = 0;
pub const Z_STREAM_END: c_int = 1;
pub const Z_NEED_DICT: c_int = 2;
pub const Z_ERRNO: c_int = -1;
pub const Z_STREAM_ERROR: c_int = -2;
pub const Z_DATA_ERROR: c_int = -3;
pub const Z_MEM_ERROR: c_int = -4;
pub const Z_BUF_ERROR: c_int = -5;
pub const Z_VERSION_ERROR: c_int = -6;

pub const Z_NO_COMPRESSION: c_int = 0;
pub const Z_BEST_SPEED: c_int = 1;
pub const Z_BEST_COMPRESSION: c_int = 9;
pub const Z_DEFAULT_COMPRESSION: c_int = -1;

pub const Z_FILTERED: c_int = 1;
pub const Z_HUFFMAN_ONLY: c_int = 2;
pub const Z_RLE: c_int = 3;
pub const Z_FIXED: c_int = 4;
pub const Z_DEFAULT_STRATEGY: c_int = 0;

pub const Z_BINARY: c_int = 0;
pub const Z_TEXT: c_int = 1;
pub const Z_ASCII: c_int = Z_TEXT;
pub const Z_UNKNOWN: c_int = 2;

pub const Z_DEFLATED: c_int = 8;
