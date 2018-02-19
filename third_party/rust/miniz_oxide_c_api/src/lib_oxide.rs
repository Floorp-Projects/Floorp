//! This module mainly contains functionality replicating the miniz higher level API.

use std::{cmp, mem, ptr, slice, usize};
use std::io::Cursor;

use libc::{self, c_char, c_int, c_uint, c_ulong, c_void, size_t};

use miniz_oxide::deflate::core::{CompressionStrategy, TDEFLFlush, TDEFLStatus,
              compress, create_comp_flags_from_zip_params, deflate_flags};
use tdef::tdefl_compressor;
use miniz_oxide::inflate::TINFLStatus;
use miniz_oxide::inflate::core::{TINFL_LZ_DICT_SIZE, inflate_flags, DecompressorOxide};

use miniz_oxide::*;

const MZ_DEFLATED: c_int = 8;
const MZ_DEFAULT_WINDOW_BITS: c_int = 15;

pub mod return_status {
    use MZError::*;
    use miniz_oxide::MZStatus;
    use libc::c_int;
    pub const MZ_ERRNO: c_int = ErrNo as c_int;
    pub const MZ_STREAM_ERROR: c_int = Stream as c_int;
    pub const MZ_DATA_ERROR: c_int = Data as c_int;
    pub const MZ_BUF_ERROR: c_int = Buf as c_int;
    pub const MZ_VERSION_ERROR: c_int = Version as c_int;
    pub const MZ_PARAM_ERROR: c_int = Param as c_int;

    pub const MZ_OK: c_int = MZStatus::Ok as c_int;
    pub const MZ_STREAM_END: c_int = MZStatus::StreamEnd as c_int;
    pub const MZ_NEED_DICT: c_int = MZStatus::NeedDict as c_int;
}

pub use self::return_status::*;

/// Unused opaque pointer.
#[allow(bad_style)]
pub enum mz_internal_state {}
/// Signature of function used to allocate the compressor/decompressor structs.
#[allow(bad_style)]
pub type mz_alloc_func = unsafe extern "C" fn(*mut c_void, size_t, size_t) -> *mut c_void;
/// Signature of function used to free the compressor/decompressor structs.
#[allow(bad_style)]
pub type mz_free_func = unsafe extern "C" fn(*mut c_void, *mut c_void);

// Only used for zip stuff in miniz, so not sure if we need it for the non-C api parts.
/* pub const MZ_CRC32_INIT: c_ulong = 0;

pub fn update_crc32(crc32: c_uint, data: &[u8]) -> c_uint {
    let mut digest = crc32::Digest::new_with_initial(crc32::IEEE, crc32);
    digest.write(data);
    digest.sum32()
}*/

/// Inner stream state containing pointers to the used buffers and internal state.
#[repr(C)]
#[derive(Debug)]
#[allow(bad_style)]
pub struct mz_stream {
    /// Pointer to the current start of the input buffer.
    pub next_in: *const u8,
    /// Length of the input buffer.
    pub avail_in: c_uint,
    /// The total number of input bytes consumed so far.
    pub total_in: c_ulong,

    /// Pointer to the current start of the output buffer.
    pub next_out: *mut u8,
    /// Space in the output buffer.
    pub avail_out: c_uint,
    /// The total number of bytes output so far.
    pub total_out: c_ulong,

    pub msg: *const c_char,
    /// Unused
    pub state: *mut mz_internal_state,

    /// Allocation function to use for allocating the internal compressor/decompressor.
    /// Uses `mz_default_alloc_func` if set to `None`.
    pub zalloc: Option<mz_alloc_func>,
    /// Free function to use for allocating the internal compressor/decompressor.
    /// Uses `mz_default_free_func` if `None`.
    pub zfree: Option<mz_free_func>,
    /// Extra data to provide the allocation/deallocation functions.
    /// (Not used for the default ones)
    pub opaque: *mut c_void,

    // TODO: Not sure
    pub data_type: c_int,
    /// Adler32 checksum of the data that has been compressed or uncompressed.
    pub adler: c_ulong,
    /// Reserved
    pub reserved: c_ulong,
}

impl Default for mz_stream {
    fn default() -> mz_stream {
        mz_stream {
            next_in: ptr::null(),
            avail_in: 0,
            total_in: 0,

            next_out: ptr::null_mut(),
            avail_out: 0,
            total_out: 0,

            msg: ptr::null(),
            state: ptr::null_mut(),

            zalloc: None,
            zfree: None,
            opaque: ptr::null_mut(),

            data_type: 0,
            adler: 0,
            reserved: 0,
        }
    }
}


pub type MZResult = Result<MZStatus, MZError>;

/// Default allocation function using `malloc`.
pub unsafe extern "C" fn def_alloc_func(
    _opaque: *mut c_void,
    items: size_t,
    size: size_t,
) -> *mut c_void {
    libc::malloc(items * size)
}

/// Default free function using `free`.
pub unsafe extern "C" fn def_free_func(_opaque: *mut c_void, address: *mut c_void) {
    libc::free(address)
}

/// Trait used for states that can be carried by BoxedState.
pub trait StateType {}
impl StateType for tdefl_compressor {}
impl StateType for inflate_state {}

/// Wrapper for a heap-allocated compressor/decompressor that frees the stucture on drop.
pub struct BoxedState<ST: StateType> {
    pub inner: *mut ST,
    pub alloc: mz_alloc_func,
    pub free: mz_free_func,
    pub opaque: *mut c_void,
}

impl<ST: StateType> Drop for BoxedState<ST> {
    fn drop(&mut self) {
        self.free_state();
    }
}

impl<ST: StateType> BoxedState<ST> {
    pub fn as_mut(&mut self) -> Option<&mut ST> {
        unsafe { self.inner.as_mut() }
    }

    pub fn new(stream: &mut mz_stream) -> Self {
        BoxedState {
            inner: stream.state as *mut ST,
            alloc: stream.zalloc.unwrap_or(def_alloc_func),
            free: stream.zfree.unwrap_or(def_free_func),
            opaque: stream.opaque,
        }
    }

    pub fn forget(mut self) -> *mut ST {
        let state = self.inner;
        self.inner = ptr::null_mut();
        state
    }

    fn alloc_state<T>(&mut self) -> MZResult {
        self.inner = unsafe { (self.alloc)(self.opaque, 1, mem::size_of::<ST>()) as *mut ST };
        if self.inner.is_null() {
            Err(MZError::Mem)
        } else {
            Ok(MZStatus::Ok)
        }
    }

    pub fn free_state(&mut self) {
        if !self.inner.is_null() {
            unsafe { (self.free)(self.opaque, self.inner as *mut c_void) }
            self.inner = ptr::null_mut();
        }
    }
}

pub struct StreamOxide<'io, ST: StateType> {
    pub next_in: Option<&'io [u8]>,
    pub total_in: c_ulong,

    pub next_out: Option<&'io mut [u8]>,
    pub total_out: c_ulong,

    pub state: BoxedState<ST>,

    pub adler: c_ulong,
}


impl<'io, ST: StateType> StreamOxide<'io, ST> {
    pub unsafe fn new(stream: &mut mz_stream) -> Self {
        let in_slice = stream.next_in.as_ref().map(|ptr| {
            slice::from_raw_parts(ptr, stream.avail_in as usize)
        });

        let out_slice = stream.next_out.as_mut().map(|ptr| {
            slice::from_raw_parts_mut(ptr, stream.avail_out as usize)
        });

        StreamOxide {
            next_in: in_slice,
            total_in: stream.total_in,
            next_out: out_slice,
            total_out: stream.total_out,
            state: BoxedState::new(stream),
            adler: stream.adler,
        }
    }

    pub fn into_mz_stream(mut self) -> mz_stream {
        mz_stream {
            next_in: self.next_in.map_or(
                ptr::null(),
                |in_slice| in_slice.as_ptr(),
            ),
            avail_in: self.next_in.map_or(0, |in_slice| in_slice.len() as c_uint),
            total_in: self.total_in,

            next_out: self.next_out.as_mut().map_or(
                ptr::null_mut(),
                |out_slice| out_slice.as_mut_ptr(),
            ),
            avail_out: self.next_out.as_mut().map_or(
                0,
                |out_slice| out_slice.len() as c_uint,
            ),
            total_out: self.total_out,

            msg: ptr::null(),

            zalloc: Some(self.state.alloc),
            zfree: Some(self.state.free),
            opaque: self.state.opaque,
            state: self.state.forget() as *mut mz_internal_state,

            data_type: 0,
            adler: self.adler,
            reserved: 0,
        }
    }
}

/// Returns true if the window_bits parameter is valid.
fn invalid_window_bits(window_bits: c_int) -> bool {
    (window_bits != MZ_DEFAULT_WINDOW_BITS) && (-window_bits != MZ_DEFAULT_WINDOW_BITS)
}

pub fn mz_compress2_oxide(
    stream_oxide: &mut StreamOxide<tdefl_compressor>,
    level: c_int,
    dest_len: &mut c_ulong,
) -> MZResult {
    mz_deflate_init_oxide(stream_oxide, level)?;
    let status = mz_deflate_oxide(stream_oxide, MZFlush::Finish as c_int);
    mz_deflate_end_oxide(stream_oxide)?;

    match status {
        Ok(MZStatus::StreamEnd) => {
            *dest_len = stream_oxide.total_out;
            Ok(MZStatus::Ok)
        }
        Ok(MZStatus::Ok) => Err(MZError::Buf),
        _ => status,
    }
}


/// Initialize the wrapped compressor with the requested level (0-10) and default settings.
///
/// The compression level will be set to 6 (default) if the requested level is not available.
pub fn mz_deflate_init_oxide(
    stream_oxide: &mut StreamOxide<tdefl_compressor>,
    level: c_int,
) -> MZResult {
    mz_deflate_init2_oxide(
        stream_oxide,
        level,
        MZ_DEFLATED,
        MZ_DEFAULT_WINDOW_BITS,
        9,
        CompressionStrategy::Default as c_int,
    )
}

/// Initialize the compressor with the requested parameters.
///
/// # Params
/// stream_oxide: The stream to be initialized.
/// level: Compression level (0-10).
/// method: Compression method. Only `MZ_DEFLATED` is accepted.
/// window_bits: Number of bits used to represent the compression sliding window.
///              Only `MZ_DEFAULT_WINDOW_BITS` is currently supported.
///              A negative value, i.e `-MZ_DEFAULT_WINDOW_BITS` indicates that the stream
///              should be wrapped in a zlib wrapper.
/// mem_level: Currently unused. Only values from 1 to and including 9 are accepted.
/// strategy: Compression strategy. See `deflate::CompressionStrategy` for accepted options.
///           The default, which is used in most cases, is 0.
pub fn mz_deflate_init2_oxide(
    stream_oxide: &mut StreamOxide<tdefl_compressor>,
    level: c_int,
    method: c_int,
    window_bits: c_int,
    mem_level: c_int,
    strategy: c_int,
) -> MZResult {
    let comp_flags = deflate_flags::TDEFL_COMPUTE_ADLER32 as c_uint |
        create_comp_flags_from_zip_params(level, window_bits, strategy);

    let invalid_level = (mem_level < 1) || (mem_level > 9);
    if (method != MZ_DEFLATED) || invalid_level || invalid_window_bits(window_bits) {
        return Err(MZError::Param);
    }

    stream_oxide.adler = MZ_ADLER32_INIT;
    stream_oxide.total_in = 0;
    stream_oxide.total_out = 0;

    stream_oxide.state.alloc_state::<tdefl_compressor>()?;
    if stream_oxide.state.as_mut().is_none() {
        mz_deflate_end_oxide(stream_oxide)?;
        return Err(MZError::Param);
    }

    match stream_oxide.state.as_mut() {
        Some(state) => *state = tdefl_compressor::new(comp_flags),
        None => unreachable!(),
    }

    Ok(MZStatus::Ok)
}

pub fn mz_deflate_oxide(
    stream_oxide: &mut StreamOxide<tdefl_compressor>,
    flush: c_int,
) -> MZResult {
    let state = stream_oxide.state.as_mut().ok_or(MZError::Stream)?;
    let next_in = stream_oxide.next_in.as_mut().ok_or(MZError::Stream)?;
    let next_out = stream_oxide.next_out.as_mut().ok_or(MZError::Stream)?;

    let flush = MZFlush::new(flush)?;

    if next_out.is_empty() {
        return Err(MZError::Buf);
    }

    if state.prev_return_status() == TDEFLStatus::Done {
        return if flush == MZFlush::Finish {
            Ok(MZStatus::StreamEnd)
        } else {
            Err(MZError::Buf)
        };
    }

    let original_total_in = stream_oxide.total_in;
    let original_total_out = stream_oxide.total_out;

    loop {
        let in_bytes;
        let out_bytes;
        let defl_status = {
            let res = compress(&mut state.inner, *next_in, *next_out, TDEFLFlush::from(flush));
            in_bytes = res.1;
            out_bytes = res.2;
            res.0
        };

        *next_in = &next_in[in_bytes..];
        *next_out = &mut mem::replace(next_out, &mut [])[out_bytes..];
        stream_oxide.total_in += in_bytes as c_ulong;
        stream_oxide.total_out += out_bytes as c_ulong;
        stream_oxide.adler = state.adler32() as c_ulong;

        if defl_status == TDEFLStatus::BadParam || defl_status == TDEFLStatus::PutBufFailed {
            return Err(MZError::Stream);
        }

        if defl_status == TDEFLStatus::Done {
            return Ok(MZStatus::StreamEnd);
        }

        if next_out.is_empty() {
            return Ok(MZStatus::Ok);
        }

        if next_in.is_empty() && (flush != MZFlush::Finish) {
            let total_changed = (stream_oxide.total_in != original_total_in) ||
                (stream_oxide.total_out != original_total_out);

            return if (flush != MZFlush::None) || total_changed {
                Ok(MZStatus::Ok)
            } else {
                Err(MZError::Buf)
            };
        }
    }
}

/// Free the inner compression state.
///
/// Currently always returns `MZStatus::Ok`.
pub fn mz_deflate_end_oxide(stream_oxide: &mut StreamOxide<tdefl_compressor>) -> MZResult {
    stream_oxide.state.free_state();
    Ok(MZStatus::Ok)
}


/// Reset the compressor, so it can be used to compress a new set of data.
///
/// Returns `MZError::Stream` if the inner stream is missing, otherwise `MZStatus::Ok`.
// TODO: probably not covered by tests
pub fn mz_deflate_reset_oxide(stream_oxide: &mut StreamOxide<tdefl_compressor>) -> MZResult {
    let state = stream_oxide.state.as_mut().ok_or(MZError::Stream)?;
    stream_oxide.total_in = 0;
    stream_oxide.total_out = 0;
    *state = tdefl_compressor::new(state.flags() as u32);
    Ok(MZStatus::Ok)
}



#[repr(C)]
#[allow(bad_style)]
pub struct inflate_state {
    pub m_decomp: DecompressorOxide,

    pub m_dict_ofs: c_uint,
    pub m_dict_avail: c_uint,
    pub m_first_call: c_uint,
    pub m_has_flushed: c_uint,

    pub m_window_bits: c_int,
    pub m_dict: [u8; TINFL_LZ_DICT_SIZE],
    pub m_last_status: TINFLStatus,
}

pub fn mz_inflate_init_oxide(stream_oxide: &mut StreamOxide<inflate_state>) -> MZResult {
    mz_inflate_init2_oxide(stream_oxide, MZ_DEFAULT_WINDOW_BITS)
}

pub fn mz_inflate_init2_oxide(
    stream_oxide: &mut StreamOxide<inflate_state>,
    window_bits: c_int,
) -> MZResult {
    if invalid_window_bits(window_bits) {
        return Err(MZError::Param);
    }

    stream_oxide.adler = 0;
    stream_oxide.total_in = 0;
    stream_oxide.total_out = 0;

    stream_oxide.state.alloc_state::<inflate_state>()?;
    let state = stream_oxide.state.as_mut().ok_or(MZError::Mem)?;
    state.m_decomp.init();
    state.m_dict_ofs = 0;
    state.m_dict_avail = 0;
    state.m_last_status = TINFLStatus::NeedsMoreInput;
    state.m_first_call = 1;
    state.m_has_flushed = 0;
    state.m_window_bits = window_bits;

    Ok(MZStatus::Ok)
}

fn push_dict_out(state: &mut inflate_state, next_out: &mut &mut [u8]) -> c_ulong {
    let n = cmp::min(state.m_dict_avail as usize, next_out.len());
    (next_out[..n]).copy_from_slice(
        &state.m_dict[state.m_dict_ofs as usize..state.m_dict_ofs as usize + n],
    );
    *next_out = &mut mem::replace(next_out, &mut [])[n..];
    state.m_dict_avail -= n as c_uint;
    state.m_dict_ofs = (state.m_dict_ofs + (n as c_uint)) &
        ((TINFL_LZ_DICT_SIZE - 1) as c_uint);
    n as c_ulong
}

pub fn mz_inflate_oxide(stream_oxide: &mut StreamOxide<inflate_state>, flush: c_int) -> MZResult {
    let state = stream_oxide.state.as_mut().ok_or(MZError::Stream)?;
    let next_in = stream_oxide.next_in.as_mut().ok_or(MZError::Stream)?;
    let next_out = stream_oxide.next_out.as_mut().ok_or(MZError::Stream)?;

    let flush = MZFlush::new(flush)?;
    if flush == MZFlush::Full {
        return Err(MZError::Stream);
    }

    let mut decomp_flags = inflate_flags::TINFL_FLAG_COMPUTE_ADLER32;
    if state.m_window_bits > 0 {
        decomp_flags |= inflate_flags::TINFL_FLAG_PARSE_ZLIB_HEADER;
    }

    let first_call = state.m_first_call;
    state.m_first_call = 0;
    if (state.m_last_status as i32) < 0 {
        return Err(MZError::Data);
    }

    if (state.m_has_flushed != 0) && (flush != MZFlush::Finish) {
        return Err(MZError::Stream);
    }
    state.m_has_flushed |= (flush == MZFlush::Finish) as c_uint;

    let orig_avail_in = next_in.len() as size_t;

    if (flush == MZFlush::Finish) && (first_call != 0) {
        decomp_flags |= inflate_flags::TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
        let status = inflate::core::decompress(
            &mut state.m_decomp,
            *next_in,
            &mut Cursor::new(*next_out),
            decomp_flags,
        );
        let in_bytes = status.1;
        let out_bytes = status.2;
        let status = status.0;

        state.m_last_status = status;

        *next_in = &next_in[in_bytes..];
        *next_out = &mut mem::replace(next_out, &mut [])[out_bytes..];
        stream_oxide.total_in += in_bytes as c_ulong;
        stream_oxide.total_out += out_bytes as c_ulong;
        // Simply set this to 0 if it doesn't exist.
        stream_oxide.adler = state.m_decomp.adler32().unwrap_or(0).into();

        if (status as i32) < 0 {
            return Err(MZError::Data);
        } else if status != TINFLStatus::Done {
            state.m_last_status = TINFLStatus::Failed;
            return Err(MZError::Buf);
        }
        return Ok(MZStatus::StreamEnd);
    }

    if flush != MZFlush::Finish {
        decomp_flags |= inflate_flags::TINFL_FLAG_HAS_MORE_INPUT;
    }

    if state.m_dict_avail != 0 {
        stream_oxide.total_out += push_dict_out(state, next_out);
        return if (state.m_last_status == TINFLStatus::Done) &&
            (state.m_dict_avail == 0)
            {
                Ok(MZStatus::StreamEnd)
            } else {
            Ok(MZStatus::Ok)
        };
    }

    loop {
        let status = {
            let mut out_cursor = Cursor::new(&mut state.m_dict[..]);
            out_cursor.set_position(state.m_dict_ofs as u64);
            inflate::core::decompress(&mut state.m_decomp, *next_in, &mut out_cursor, decomp_flags)
        };

        let in_bytes = status.1;
        let out_bytes = status.2;
        let status = status.0;

        state.m_last_status = status;

        *next_in = &next_in[in_bytes..];
        stream_oxide.total_in += in_bytes as c_ulong;

        state.m_dict_avail = out_bytes as c_uint;
        stream_oxide.total_out += push_dict_out(state, next_out);
        stream_oxide.adler = state.m_decomp.adler32().unwrap_or(0).into();

        if (status as i32) < 0 {
            return Err(MZError::Data);
        }

        if (status == TINFLStatus::NeedsMoreInput) && (orig_avail_in == 0) {
            return Err(MZError::Buf);
        }

        if flush == MZFlush::Finish {
            if status == TINFLStatus::Done {
                return if state.m_dict_avail != 0 {
                    Err(MZError::Buf)
                } else {
                    Ok(MZStatus::StreamEnd)
                };
            } else if next_out.is_empty() {
                return Err(MZError::Buf);
            }
        } else {
            let empty_buf = next_in.is_empty() || next_out.is_empty();
            if (status == TINFLStatus::Done) || empty_buf || (state.m_dict_avail != 0) {
                return if (status == TINFLStatus::Done) && (state.m_dict_avail == 0) {
                    Ok(MZStatus::StreamEnd)
                } else {
                    Ok(MZStatus::Ok)
                };
            }
        }
    }
}

pub fn mz_uncompress2_oxide(
    stream_oxide: &mut StreamOxide<inflate_state>,
    dest_len: &mut c_ulong,
) -> MZResult {
    mz_inflate_init_oxide(stream_oxide)?;
    let status = mz_inflate_oxide(stream_oxide, MZFlush::Finish as c_int);
    mz_inflate_end_oxide(stream_oxide)?;

    let empty_in = stream_oxide.next_in.map_or(
        true,
        |next_in| next_in.is_empty(),
    );
    match (status, empty_in) {
        (Ok(MZStatus::StreamEnd), _) => {
            *dest_len = stream_oxide.total_out;
            Ok(MZStatus::Ok)
        }
        (Err(MZError::Buf), true) => Err(MZError::Data),
        (status, _) => status,
    }
}

pub fn mz_inflate_end_oxide(stream_oxide: &mut StreamOxide<inflate_state>) -> MZResult {
    stream_oxide.state.free_state();
    Ok(MZStatus::Ok)
}
