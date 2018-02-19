#![feature(test)]

extern crate libc;
extern crate miniz_oxide;
extern crate miniz_oxide_c_api;
extern crate test;


use test::Bencher;
use std::io::Read;
use std::{ops, ptr};
use libc::c_void;

use miniz_oxide::deflate::compress_to_vec;
use miniz_oxide::deflate::core::{create_comp_flags_from_zip_params, CompressorOxide};

use miniz_oxide_c_api::miniz_def_free_func;

/// Safe wrapper around a buffer.
pub struct HeapBuf {
    buf: *mut c_void,
}

impl ops::Drop for HeapBuf {
    fn drop(&mut self) {
        unsafe {
            ::miniz_def_free_func(ptr::null_mut(), self.buf);
        }
    }
}

/// Wrap pointer in a buffer that frees the memory on exit.
fn w(buf: *mut c_void) -> HeapBuf {
    HeapBuf { buf: buf }
}

fn get_test_file_data(name: &str) -> Vec<u8> {
    use std::fs::File;
    let mut input = Vec::new();
    let mut f = File::open(name).unwrap();

    f.read_to_end(&mut input).unwrap();
    input
}

macro_rules! decompress_bench {
    ($bench_name:ident, $decompress_func:ident, $level:expr, $path_to_data:expr) => {
        #[bench]
        fn $bench_name(b: &mut ::Bencher) {
            let input = ::get_test_file_data($path_to_data);
            let compressed = ::compress_to_vec(input.as_slice(), $level);

            let mut out_len: usize = 0;
            b.iter(|| unsafe {
                ::w($decompress_func(
                    compressed.as_ptr() as *mut ::c_void,
                    compressed.len(),
                    &mut out_len,
                    0,
                ))
            });
        }
    };
}

macro_rules! compress_bench {
    ($bench_name:ident, $compress_func:ident, $level:expr, $path_to_data:expr) => {
        #[bench]
        fn $bench_name(b: &mut ::Bencher) {
            let input = ::get_test_file_data($path_to_data);

            let mut out_len: usize = 0;
            let flags = ::create_comp_flags_from_zip_params($level, -15, 0) as i32;
            b.iter(|| unsafe {
                ::w($compress_func(
                    input.as_ptr() as *mut ::c_void,
                    input.len(),
                    &mut out_len,
                    flags,
                ))
            });
        }
    };
}

mod oxide {
    use miniz_oxide_c_api::{tdefl_compress_mem_to_heap, tinfl_decompress_mem_to_heap};

    compress_bench!(
        compress_short_lvl_1,
        tdefl_compress_mem_to_heap,
        1,
        "benches/data/short"
    );
    compress_bench!(
        compress_short_lvl_6,
        tdefl_compress_mem_to_heap,
        6,
        "benches/data/short"
    );

    compress_bench!(
        compress_bin_lvl_1,
        tdefl_compress_mem_to_heap,
        1,
        "benches/data/bin"
    );
    compress_bench!(
        compress_bin_lvl_6,
        tdefl_compress_mem_to_heap,
        6,
        "benches/data/bin"
    );
    compress_bench!(
        compress_bin_lvl_9,
        tdefl_compress_mem_to_heap,
        9,
        "benches/data/bin"
    );

    compress_bench!(
        compress_code_lvl_1,
        tdefl_compress_mem_to_heap,
        1,
        "benches/data/code"
    );
    compress_bench!(
        compress_code_lvl_6,
        tdefl_compress_mem_to_heap,
        6,
        "benches/data/code"
    );
    compress_bench!(
        compress_code_lvl_9,
        tdefl_compress_mem_to_heap,
        9,
        "benches/data/code"
    );

    compress_bench!(
        compress_compressed_lvl_1,
        tdefl_compress_mem_to_heap,
        1,
        "benches/data/compressed"
);
    compress_bench!(
        compress_compressed_lvl_6,
        tdefl_compress_mem_to_heap,
        6,
        "benches/data/compressed"
);
    compress_bench!(
        compress_compressed_lvl_9,
        tdefl_compress_mem_to_heap,
        9,
        "benches/data/compressed"
    );

    decompress_bench!(
        decompress_short_lvl_1,
        tinfl_decompress_mem_to_heap,
        1,
        "benches/data/short"
    );
    decompress_bench!(
        decompress_bin_lvl_1,
        tinfl_decompress_mem_to_heap,
        1,
        "benches/data/bin"
    );
    decompress_bench!(
        decompress_bin_lvl_6,
        tinfl_decompress_mem_to_heap,
        6,
        "benches/data/bin"
    );
    decompress_bench!(
        decompress_bin_lvl_9,
        tinfl_decompress_mem_to_heap,
        9,
        "benches/data/bin"
    );

    decompress_bench!(
        decompress_code_lvl_1,
        tinfl_decompress_mem_to_heap,
        1,
        "benches/data/code"
    );
    decompress_bench!(
        decompress_code_lvl_6,
        tinfl_decompress_mem_to_heap,
        6,
        "benches/data/code"
    );
    decompress_bench!(
        decompress_code_lvl_9,
        tinfl_decompress_mem_to_heap,
        9,
        "benches/data/code"
    );

    decompress_bench!(
        decompress_compressed_lvl_1,
        tinfl_decompress_mem_to_heap,
        1,
        "benches/data/compressed"
    );
    decompress_bench!(
        decompress_compressed_lvl_6,
        tinfl_decompress_mem_to_heap,
        6,
        "benches/data/compressed"
    );
    decompress_bench!(
        decompress_compressed_lvl_9,
        tinfl_decompress_mem_to_heap,
        9,
        "benches/data/compressed"
    );
}

mod miniz {
    use libc::{c_void, c_int};

    /// Functions from miniz
    /// We add the link attribute to make sure
    /// these are linked to the miniz ones rather than
    /// picking up the rust versions (as they may be exported).
    #[link(name = "miniz", kind = "static")]
    extern "C" {
        fn tinfl_decompress_mem_to_heap(
            src_buf: *const c_void,
            src_buf_len: usize,
            out_len: *mut usize,
            flags: c_int,
        ) -> *mut c_void;

        fn tdefl_compress_mem_to_heap(
            src_buf: *const c_void,
            src_buf_len: usize,
            out_len: *mut usize,
            flags: c_int,
        ) -> *mut c_void;
    }

    compress_bench!(
        compress_short_lvl_1,
        tdefl_compress_mem_to_heap,
        1,
        "benches/data/short"
    );
    compress_bench!(
        compress_short_lvl_6,
        tdefl_compress_mem_to_heap,
        6,
        "benches/data/short"
    );

    compress_bench!(
        compress_bin_lvl_1,
        tdefl_compress_mem_to_heap,
        1,
        "benches/data/bin"
    );
        compress_bench!(
        compress_bin_lvl_6,
        tdefl_compress_mem_to_heap,
        6,
        "benches/data/bin"
    );
        compress_bench!(
        compress_bin_lvl_9,
        tdefl_compress_mem_to_heap,
        9,
        "benches/data/bin"
    );

    compress_bench!(
        compress_code_lvl_1,
        tdefl_compress_mem_to_heap,
        1,
        "benches/data/code"
    );
    compress_bench!(
        compress_code_lvl_6,
        tdefl_compress_mem_to_heap,
        6,
        "benches/data/code"
    );
    compress_bench!(
        compress_code_lvl_9,
        tdefl_compress_mem_to_heap,
        9,
        "benches/data/code"
    );

    compress_bench!(
        compress_compressed_lvl_1,
        tdefl_compress_mem_to_heap,
        1,
        "benches/data/compressed"
    );
    compress_bench!(
        compress_compressed_lvl_6,
        tdefl_compress_mem_to_heap,
        6,
        "benches/data/compressed"
    );
    compress_bench!(
        compress_compressed_lvl_9,
        tdefl_compress_mem_to_heap,
        9,
        "benches/data/compressed"
    );

    decompress_bench!(
        decompress_short_lvl_1,
        tinfl_decompress_mem_to_heap,
        1,
        "benches/data/short"
    );

    decompress_bench!(
        decompress_bin_lvl_1,
        tinfl_decompress_mem_to_heap,
        1,
        "benches/data/bin"
    );
    decompress_bench!(
        decompress_bin_lvl_6,
        tinfl_decompress_mem_to_heap,
        6,
        "benches/data/bin"
    );
    decompress_bench!(
        decompress_bin_lvl_9,
        tinfl_decompress_mem_to_heap,
        9,
        "benches/data/bin"
    );

    decompress_bench!(
        decompress_code_lvl_1,
        tinfl_decompress_mem_to_heap,
        1,
        "benches/data/code"
    );
    decompress_bench!(
        decompress_code_lvl_6,
        tinfl_decompress_mem_to_heap,
        6,
        "benches/data/code"
    );
    decompress_bench!(
        decompress_code_lvl_9,
        tinfl_decompress_mem_to_heap,
        9,
        "benches/data/code"
    );

    decompress_bench!(
        decompress_compressed_lvl_1,
        tinfl_decompress_mem_to_heap,
        1,
        "benches/data/compressed"
    );
    decompress_bench!(
        decompress_compressed_lvl_6,
        tinfl_decompress_mem_to_heap,
        6,
        "benches/data/compressed"
    );
    decompress_bench!(
        decompress_compressed_lvl_9,
        tinfl_decompress_mem_to_heap,
        9,
        "benches/data/compressed"
    );
}

#[bench]
fn create_compressor(b: &mut Bencher) {
    let flags = create_comp_flags_from_zip_params(6, true as i32, 0);
    b.iter(|| CompressorOxide::new(flags));
}
