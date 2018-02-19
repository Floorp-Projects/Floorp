#![allow(dead_code)]

use libc::*;
use std::{ptr, slice, usize};
use std::io::Cursor;
use miniz_oxide::inflate::TINFLStatus;
pub use miniz_oxide::inflate::core::{decompress, inflate_flags};
pub use miniz_oxide::inflate::core::DecompressorOxide as tinfl_decompressor;

pub const TINFL_DECOMPRESS_MEM_TO_MEM_FAILED: size_t = usize::MAX;

unmangle!(
pub unsafe extern "C" fn tinfl_decompress(
    r: *mut tinfl_decompressor,
    in_buf: *const u8,
    in_buf_size: *mut usize,
    out_buf_start: *mut u8,
    out_buf_next: *mut u8,
    out_buf_size: *mut usize,
    flags: u32,
) -> i32 {
    let next_pos = out_buf_next as usize - out_buf_start as usize;
    let out_size = *out_buf_size + next_pos;
    let mut out_cursor = Cursor::new(slice::from_raw_parts_mut(out_buf_start, out_size));
    out_cursor.set_position(next_pos as u64);
    let (status, in_consumed, out_consumed) = decompress(
        r.as_mut().expect("bad decompressor pointer"),
        slice::from_raw_parts(in_buf, *in_buf_size),
        &mut out_cursor,
        flags,
    );

    *in_buf_size = in_consumed;
    *out_buf_size = out_consumed;
    status as i32
}

pub unsafe extern "C" fn tinfl_decompress_mem_to_mem(
    p_out_buf: *mut c_void,
    out_buf_len: size_t,
    p_src_buf: *const c_void,
    src_buf_len: size_t,
    flags: c_int,
) -> size_t {
    let flags = flags as u32;
    let mut decomp = tinfl_decompressor::with_init_state_only();

    let (status, _, out_consumed) =
        decompress(
            &mut decomp,
            slice::from_raw_parts(p_src_buf as *const u8, src_buf_len),
            &mut Cursor::new(slice::from_raw_parts_mut(p_out_buf as *mut u8, out_buf_len)),
            ((flags & !inflate_flags::TINFL_FLAG_HAS_MORE_INPUT) | inflate_flags::TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF),
        );

    if status != TINFLStatus::Done {
        TINFL_DECOMPRESS_MEM_TO_MEM_FAILED as size_t
    } else {
        out_consumed
    }
}

/// Decompress data from `p_src_buf` to a continuously growing heap-allocated buffer.
///
/// Sets `p_out_len` to the length of the returned buffer.
/// Returns `ptr::null()` if decompression or allocation fails.
/// The buffer should be freed with `miniz_def_free_func`.
pub unsafe extern "C" fn tinfl_decompress_mem_to_heap(
    p_src_buf: *const c_void,
    src_buf_len: size_t,
    p_out_len: *mut size_t,
    flags: c_int,
) -> *mut c_void {
    let flags = flags as u32;
    const MIN_BUFFER_CAPACITY: size_t = 128;

    // We're not using a Vec for the buffer here to make sure the buffer is allocated and freed by
    // the same allocator.

    let mut decomp = tinfl_decompressor::with_init_state_only();
    // Pointer to the buffer to place the decompressed data into.
    let mut p_buf: *mut c_void = ptr::null_mut();
    // Capacity of the current output buffer.
    let mut out_buf_capacity = 0;

    *p_out_len = 0;
    // How far into the source buffer we have read.
    let mut src_buf_ofs = 0;
    loop {
        let mut out_cur = Cursor::new(slice::from_raw_parts_mut(
            p_buf as *mut u8,
            out_buf_capacity,
        ));
        out_cur.set_position(*p_out_len as u64);
        let (status, in_consumed, out_consumed) =
            decompress(
                &mut decomp,
                slice::from_raw_parts(
                    p_src_buf.offset(src_buf_ofs as isize) as *const u8,
                    src_buf_len - src_buf_ofs,
                ),
                &mut out_cur,
                ((flags & !inflate_flags::TINFL_FLAG_HAS_MORE_INPUT) |
                 inflate_flags::TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF),
            );

        // If decompression fails or we don't have any input, bail out.
        if (status as i32) < 0 || status == TINFLStatus::NeedsMoreInput {
            ::miniz_def_free_func(ptr::null_mut(), p_buf);
            *p_out_len = 0;
            return ptr::null_mut();
        }

        src_buf_ofs += in_consumed;
        *p_out_len += out_consumed;

        if status == TINFLStatus::Done {
            break;
        }

        // If we need more space, double the capacity of the output buffer
        // and keep going.
        let mut new_out_buf_capacity = out_buf_capacity * 2;

        // Try to get at least 128 bytes of buffer capacity.
        if new_out_buf_capacity < MIN_BUFFER_CAPACITY {
            new_out_buf_capacity = MIN_BUFFER_CAPACITY
        }

        let p_new_buf = ::miniz_def_realloc_func(ptr::null_mut(), p_buf, 1, new_out_buf_capacity);
        // Bail out if growing fails.
        if p_new_buf.is_null() {
            ::miniz_def_free_func(ptr::null_mut(), p_buf);
            *p_out_len = 0;
            return ptr::null_mut();
        }

        // Otherwise, continue using the reallocated buffer.
        p_buf = p_new_buf;
        out_buf_capacity = new_out_buf_capacity;
    }

    p_buf
}
);

#[cfg(test)]
mod test {
    use miniz_oxide::inflate::core::inflate_flags::{
        TINFL_FLAG_COMPUTE_ADLER32,
        TINFL_FLAG_PARSE_ZLIB_HEADER,
    };

    use super::*;
    use libc::c_void;
    use std::{ops, slice};
    /// Safe wrapper for `tinfl_decompress_mem_to_mem` using slices.
    ///
    /// Could maybe make this public later.
    fn tinfl_decompress_mem_to_mem_wrapper(
        source: &mut [u8],
        dest: &mut [u8],
        flags: i32,
    ) -> Option<usize> {
        let status = unsafe {
            let source_len = source.len();
            let dest_len = dest.len();
            tinfl_decompress_mem_to_mem(
                dest.as_mut_ptr() as *mut c_void,
                dest_len,
                source.as_mut_ptr() as *const c_void,
                source_len,
                flags,
            )
        };
        if status != TINFL_DECOMPRESS_MEM_TO_MEM_FAILED {
            Some(status)
        } else {
            None
        }
    }

    /// Safe wrapper around a buffer allocated with the miniz_def functions.
    pub struct TinflHeapBuf {
        buf: *mut c_void,
        len: size_t,
    }

    impl TinflHeapBuf {
        fn as_slice(&self) -> &[u8] {
            unsafe { slice::from_raw_parts(self.buf as *const u8, self.len) }
        }
    }

    impl ops::Drop for TinflHeapBuf {
        fn drop(&mut self) {
            unsafe {
                ::miniz_def_free_func(ptr::null_mut(), self.buf);
            }
        }
    }

    /// Safe wrapper for `tinfl_decompress_mem_to_heap` using slices.
    ///
    /// Could maybe make something like this public later.
    fn tinfl_decompress_mem_to_heap_wrapper(source: &mut [u8], flags: i32) -> Option<TinflHeapBuf> {
        let source_len = source.len();
        let mut out_len = 0;
        unsafe {
            let buf_ptr = tinfl_decompress_mem_to_heap(
                source.as_ptr() as *const c_void,
                source_len,
                &mut out_len,
                flags,
            );
            if !buf_ptr.is_null() {
                Some(TinflHeapBuf {
                    buf: buf_ptr,
                    len: out_len,
                })
            } else {
                None
            }
        }
    }

    #[test]
    fn mem_to_mem() {
        let mut encoded = [
            120, 156, 243, 72, 205, 201, 201, 215, 81, 168,
            202, 201,  76, 82,   4,   0,  27, 101,  4,  19,
        ];
        let mut out_buf = vec![0; 50];
        let flags = TINFL_FLAG_COMPUTE_ADLER32 | TINFL_FLAG_PARSE_ZLIB_HEADER;
        let size = tinfl_decompress_mem_to_mem_wrapper(
            &mut encoded[..],
            out_buf.as_mut_slice(),
            flags as i32,
        ).unwrap();
        assert_eq!(&out_buf[..size], &b"Hello, zlib!"[..]);
    }

    #[test]
    fn mem_to_heap() {
        let mut encoded = [
            120, 156, 243, 72, 205, 201, 201, 215, 81, 168,
            202, 201,  76, 82,   4,   0,  27, 101,  4,  19,
        ];
        let flags = TINFL_FLAG_COMPUTE_ADLER32 | TINFL_FLAG_PARSE_ZLIB_HEADER;
        let out_buf = tinfl_decompress_mem_to_heap_wrapper(&mut encoded[..], flags as i32).unwrap();
        assert_eq!(out_buf.as_slice(), &b"Hello, zlib!"[..]);
    }
}
