//! Implementation for C backends.
use std::alloc::{self, Layout};
use std::cmp;
use std::convert::TryFrom;
use std::fmt;
use std::marker;
use std::ops::{Deref, DerefMut};
use std::ptr;

pub use libc::{c_int, c_uint, c_void, size_t};

use super::*;
use crate::mem::{self, FlushDecompress, Status};

pub struct StreamWrapper {
    pub inner: Box<mz_stream>,
}

impl fmt::Debug for StreamWrapper {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(f, "StreamWrapper")
    }
}

impl Default for StreamWrapper {
    fn default() -> StreamWrapper {
        StreamWrapper {
            inner: Box::new(mz_stream {
                next_in: ptr::null_mut(),
                avail_in: 0,
                total_in: 0,
                next_out: ptr::null_mut(),
                avail_out: 0,
                total_out: 0,
                msg: ptr::null_mut(),
                adler: 0,
                data_type: 0,
                reserved: 0,
                opaque: ptr::null_mut(),
                state: ptr::null_mut(),
                #[cfg(feature = "any_zlib")]
                zalloc,
                #[cfg(feature = "any_zlib")]
                zfree,
                #[cfg(not(feature = "any_zlib"))]
                zalloc: Some(zalloc),
                #[cfg(not(feature = "any_zlib"))]
                zfree: Some(zfree),
            }),
        }
    }
}

const ALIGN: usize = std::mem::align_of::<usize>();

fn align_up(size: usize, align: usize) -> usize {
    (size + align - 1) & !(align - 1)
}

extern "C" fn zalloc(_ptr: *mut c_void, items: AllocSize, item_size: AllocSize) -> *mut c_void {
    // We need to multiply `items` and `item_size` to get the actual desired
    // allocation size. Since `zfree` doesn't receive a size argument we
    // also need to allocate space for a `usize` as a header so we can store
    // how large the allocation is to deallocate later.
    let size = match items
        .checked_mul(item_size)
        .and_then(|i| usize::try_from(i).ok())
        .map(|size| align_up(size, ALIGN))
        .and_then(|i| i.checked_add(std::mem::size_of::<usize>()))
    {
        Some(i) => i,
        None => return ptr::null_mut(),
    };

    // Make sure the `size` isn't too big to fail `Layout`'s restrictions
    let layout = match Layout::from_size_align(size, ALIGN) {
        Ok(layout) => layout,
        Err(_) => return ptr::null_mut(),
    };

    unsafe {
        // Allocate the data, and if successful store the size we allocated
        // at the beginning and then return an offset pointer.
        let ptr = alloc::alloc(layout) as *mut usize;
        if ptr.is_null() {
            return ptr as *mut c_void;
        }
        *ptr = size;
        ptr.add(1) as *mut c_void
    }
}

extern "C" fn zfree(_ptr: *mut c_void, address: *mut c_void) {
    unsafe {
        // Move our address being free'd back one pointer, read the size we
        // stored in `zalloc`, and then free it using the standard Rust
        // allocator.
        let ptr = (address as *mut usize).offset(-1);
        let size = *ptr;
        let layout = Layout::from_size_align_unchecked(size, ALIGN);
        alloc::dealloc(ptr as *mut u8, layout)
    }
}

impl Deref for StreamWrapper {
    type Target = mz_stream;

    fn deref(&self) -> &Self::Target {
        &*self.inner
    }
}

impl DerefMut for StreamWrapper {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut *self.inner
    }
}

unsafe impl<D: Direction> Send for Stream<D> {}
unsafe impl<D: Direction> Sync for Stream<D> {}

/// Trait used to call the right destroy/end function on the inner
/// stream object on drop.
pub trait Direction {
    unsafe fn destroy(stream: *mut mz_stream) -> c_int;
}

#[derive(Debug)]
pub enum DirCompress {}
#[derive(Debug)]
pub enum DirDecompress {}

#[derive(Debug)]
pub struct Stream<D: Direction> {
    pub stream_wrapper: StreamWrapper,
    pub total_in: u64,
    pub total_out: u64,
    pub _marker: marker::PhantomData<D>,
}

impl<D: Direction> Drop for Stream<D> {
    fn drop(&mut self) {
        unsafe {
            let _ = D::destroy(&mut *self.stream_wrapper);
        }
    }
}

impl Direction for DirCompress {
    unsafe fn destroy(stream: *mut mz_stream) -> c_int {
        mz_deflateEnd(stream)
    }
}
impl Direction for DirDecompress {
    unsafe fn destroy(stream: *mut mz_stream) -> c_int {
        mz_inflateEnd(stream)
    }
}

#[derive(Debug)]
pub struct Inflate {
    pub inner: Stream<DirDecompress>,
}

impl InflateBackend for Inflate {
    fn make(zlib_header: bool, window_bits: u8) -> Self {
        unsafe {
            let mut state = StreamWrapper::default();
            let ret = mz_inflateInit2(
                &mut *state,
                if zlib_header {
                    window_bits as c_int
                } else {
                    -(window_bits as c_int)
                },
            );
            assert_eq!(ret, 0);
            Inflate {
                inner: Stream {
                    stream_wrapper: state,
                    total_in: 0,
                    total_out: 0,
                    _marker: marker::PhantomData,
                },
            }
        }
    }

    fn decompress(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushDecompress,
    ) -> Result<Status, DecompressError> {
        let raw = &mut *self.inner.stream_wrapper;
        raw.next_in = input.as_ptr() as *mut u8;
        raw.avail_in = cmp::min(input.len(), c_uint::max_value() as usize) as c_uint;
        raw.next_out = output.as_mut_ptr();
        raw.avail_out = cmp::min(output.len(), c_uint::max_value() as usize) as c_uint;

        let rc = unsafe { mz_inflate(raw, flush as c_int) };

        // Unfortunately the total counters provided by zlib might be only
        // 32 bits wide and overflow while processing large amounts of data.
        self.inner.total_in += (raw.next_in as usize - input.as_ptr() as usize) as u64;
        self.inner.total_out += (raw.next_out as usize - output.as_ptr() as usize) as u64;

        match rc {
            MZ_DATA_ERROR | MZ_STREAM_ERROR => mem::decompress_failed(),
            MZ_OK => Ok(Status::Ok),
            MZ_BUF_ERROR => Ok(Status::BufError),
            MZ_STREAM_END => Ok(Status::StreamEnd),
            MZ_NEED_DICT => mem::decompress_need_dict(raw.adler as u32),
            c => panic!("unknown return code: {}", c),
        }
    }

    #[cfg(feature = "any_zlib")]
    fn reset(&mut self, zlib_header: bool) {
        let bits = if zlib_header {
            MZ_DEFAULT_WINDOW_BITS
        } else {
            -MZ_DEFAULT_WINDOW_BITS
        };
        unsafe {
            inflateReset2(&mut *self.inner.stream_wrapper, bits);
        }
        self.inner.total_out = 0;
        self.inner.total_in = 0;
    }

    #[cfg(not(feature = "any_zlib"))]
    fn reset(&mut self, zlib_header: bool) {
        *self = Self::make(zlib_header, MZ_DEFAULT_WINDOW_BITS as u8);
    }
}

impl Backend for Inflate {
    #[inline]
    fn total_in(&self) -> u64 {
        self.inner.total_in
    }

    #[inline]
    fn total_out(&self) -> u64 {
        self.inner.total_out
    }
}

#[derive(Debug)]
pub struct Deflate {
    pub inner: Stream<DirCompress>,
}

impl DeflateBackend for Deflate {
    fn make(level: Compression, zlib_header: bool, window_bits: u8) -> Self {
        unsafe {
            let mut state = StreamWrapper::default();
            let ret = mz_deflateInit2(
                &mut *state,
                level.0 as c_int,
                MZ_DEFLATED,
                if zlib_header {
                    window_bits as c_int
                } else {
                    -(window_bits as c_int)
                },
                8,
                MZ_DEFAULT_STRATEGY,
            );
            assert_eq!(ret, 0);
            Deflate {
                inner: Stream {
                    stream_wrapper: state,
                    total_in: 0,
                    total_out: 0,
                    _marker: marker::PhantomData,
                },
            }
        }
    }
    fn compress(
        &mut self,
        input: &[u8],
        output: &mut [u8],
        flush: FlushCompress,
    ) -> Result<Status, CompressError> {
        let raw = &mut *self.inner.stream_wrapper;
        raw.next_in = input.as_ptr() as *mut _;
        raw.avail_in = cmp::min(input.len(), c_uint::max_value() as usize) as c_uint;
        raw.next_out = output.as_mut_ptr();
        raw.avail_out = cmp::min(output.len(), c_uint::max_value() as usize) as c_uint;

        let rc = unsafe { mz_deflate(raw, flush as c_int) };

        // Unfortunately the total counters provided by zlib might be only
        // 32 bits wide and overflow while processing large amounts of data.
        self.inner.total_in += (raw.next_in as usize - input.as_ptr() as usize) as u64;
        self.inner.total_out += (raw.next_out as usize - output.as_ptr() as usize) as u64;

        match rc {
            MZ_OK => Ok(Status::Ok),
            MZ_BUF_ERROR => Ok(Status::BufError),
            MZ_STREAM_END => Ok(Status::StreamEnd),
            MZ_STREAM_ERROR => Err(CompressError(())),
            c => panic!("unknown return code: {}", c),
        }
    }

    fn reset(&mut self) {
        self.inner.total_in = 0;
        self.inner.total_out = 0;
        let rc = unsafe { mz_deflateReset(&mut *self.inner.stream_wrapper) };
        assert_eq!(rc, MZ_OK);
    }
}

impl Backend for Deflate {
    #[inline]
    fn total_in(&self) -> u64 {
        self.inner.total_in
    }

    #[inline]
    fn total_out(&self) -> u64 {
        self.inner.total_out
    }
}

pub use self::c_backend::*;

/// Miniz specific
#[cfg(not(feature = "any_zlib"))]
mod c_backend {
    pub use miniz_sys::*;
    pub type AllocSize = libc::size_t;
}

/// Zlib specific
#[cfg(any(
    feature = "zlib-ng-compat",
    all(feature = "zlib", not(feature = "cloudflare_zlib"))
))]
#[allow(bad_style)]
mod c_backend {
    use libc::{c_char, c_int};
    use std::mem;

    pub use libz_sys::deflate as mz_deflate;
    pub use libz_sys::deflateEnd as mz_deflateEnd;
    pub use libz_sys::deflateReset as mz_deflateReset;
    pub use libz_sys::inflate as mz_inflate;
    pub use libz_sys::inflateEnd as mz_inflateEnd;
    pub use libz_sys::z_stream as mz_stream;
    pub use libz_sys::*;

    pub use libz_sys::Z_BLOCK as MZ_BLOCK;
    pub use libz_sys::Z_BUF_ERROR as MZ_BUF_ERROR;
    pub use libz_sys::Z_DATA_ERROR as MZ_DATA_ERROR;
    pub use libz_sys::Z_DEFAULT_STRATEGY as MZ_DEFAULT_STRATEGY;
    pub use libz_sys::Z_DEFLATED as MZ_DEFLATED;
    pub use libz_sys::Z_FINISH as MZ_FINISH;
    pub use libz_sys::Z_FULL_FLUSH as MZ_FULL_FLUSH;
    pub use libz_sys::Z_NEED_DICT as MZ_NEED_DICT;
    pub use libz_sys::Z_NO_FLUSH as MZ_NO_FLUSH;
    pub use libz_sys::Z_OK as MZ_OK;
    pub use libz_sys::Z_PARTIAL_FLUSH as MZ_PARTIAL_FLUSH;
    pub use libz_sys::Z_STREAM_END as MZ_STREAM_END;
    pub use libz_sys::Z_STREAM_ERROR as MZ_STREAM_ERROR;
    pub use libz_sys::Z_SYNC_FLUSH as MZ_SYNC_FLUSH;
    pub type AllocSize = libz_sys::uInt;

    pub const MZ_DEFAULT_WINDOW_BITS: c_int = 15;

    const ZLIB_VERSION: &'static str = "1.2.8\0";

    pub unsafe extern "C" fn mz_deflateInit2(
        stream: *mut mz_stream,
        level: c_int,
        method: c_int,
        window_bits: c_int,
        mem_level: c_int,
        strategy: c_int,
    ) -> c_int {
        libz_sys::deflateInit2_(
            stream,
            level,
            method,
            window_bits,
            mem_level,
            strategy,
            ZLIB_VERSION.as_ptr() as *const c_char,
            mem::size_of::<mz_stream>() as c_int,
        )
    }
    pub unsafe extern "C" fn mz_inflateInit2(stream: *mut mz_stream, window_bits: c_int) -> c_int {
        libz_sys::inflateInit2_(
            stream,
            window_bits,
            ZLIB_VERSION.as_ptr() as *const c_char,
            mem::size_of::<mz_stream>() as c_int,
        )
    }
}

/// Cloudflare optimized Zlib specific
#[cfg(all(feature = "cloudflare_zlib", not(feature = "zlib-ng-compat")))]
#[allow(bad_style)]
mod c_backend {
    use libc::{c_char, c_int};
    use std::mem;

    pub use cloudflare_zlib_sys::deflate as mz_deflate;
    pub use cloudflare_zlib_sys::deflateEnd as mz_deflateEnd;
    pub use cloudflare_zlib_sys::deflateReset as mz_deflateReset;
    pub use cloudflare_zlib_sys::inflate as mz_inflate;
    pub use cloudflare_zlib_sys::inflateEnd as mz_inflateEnd;
    pub use cloudflare_zlib_sys::z_stream as mz_stream;
    pub use cloudflare_zlib_sys::*;

    pub use cloudflare_zlib_sys::Z_BLOCK as MZ_BLOCK;
    pub use cloudflare_zlib_sys::Z_BUF_ERROR as MZ_BUF_ERROR;
    pub use cloudflare_zlib_sys::Z_DATA_ERROR as MZ_DATA_ERROR;
    pub use cloudflare_zlib_sys::Z_DEFAULT_STRATEGY as MZ_DEFAULT_STRATEGY;
    pub use cloudflare_zlib_sys::Z_DEFLATED as MZ_DEFLATED;
    pub use cloudflare_zlib_sys::Z_FINISH as MZ_FINISH;
    pub use cloudflare_zlib_sys::Z_FULL_FLUSH as MZ_FULL_FLUSH;
    pub use cloudflare_zlib_sys::Z_NEED_DICT as MZ_NEED_DICT;
    pub use cloudflare_zlib_sys::Z_NO_FLUSH as MZ_NO_FLUSH;
    pub use cloudflare_zlib_sys::Z_OK as MZ_OK;
    pub use cloudflare_zlib_sys::Z_PARTIAL_FLUSH as MZ_PARTIAL_FLUSH;
    pub use cloudflare_zlib_sys::Z_STREAM_END as MZ_STREAM_END;
    pub use cloudflare_zlib_sys::Z_STREAM_ERROR as MZ_STREAM_ERROR;
    pub use cloudflare_zlib_sys::Z_SYNC_FLUSH as MZ_SYNC_FLUSH;
    pub type AllocSize = cloudflare_zlib_sys::uInt;

    pub const MZ_DEFAULT_WINDOW_BITS: c_int = 15;

    const ZLIB_VERSION: &'static str = "1.2.8\0";

    pub unsafe extern "C" fn mz_deflateInit2(
        stream: *mut mz_stream,
        level: c_int,
        method: c_int,
        window_bits: c_int,
        mem_level: c_int,
        strategy: c_int,
    ) -> c_int {
        cloudflare_zlib_sys::deflateInit2_(
            stream,
            level,
            method,
            window_bits,
            mem_level,
            strategy,
            ZLIB_VERSION.as_ptr() as *const c_char,
            mem::size_of::<mz_stream>() as c_int,
        )
    }
    pub unsafe extern "C" fn mz_inflateInit2(stream: *mut mz_stream, window_bits: c_int) -> c_int {
        cloudflare_zlib_sys::inflateInit2_(
            stream,
            window_bits,
            ZLIB_VERSION.as_ptr() as *const c_char,
            mem::size_of::<mz_stream>() as c_int,
        )
    }
}
