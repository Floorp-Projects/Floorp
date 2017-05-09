# Write a Sanity Test

Finally, to tie everything together, let's write a sanity test that round trips
some text through compression and decompression, and then asserts that it came
back out the same as it went in. This is a little wordy using the raw FFI
bindings, but hopefully we wouldn't usually ask people to do this, we'd provide
a nice Rust-y API on top of the raw FFI bindings for them. However, since this
is for testing the bindings directly, our sanity test will use the bindings
directly.

The test data I'm round tripping are some Futurama quotes I got off the internet
and put in the `futurama-quotes.txt` file, which is read into a `&'static str`
at compile time via the `include_str!("../futurama-quotes.txt")` macro
invocation.

Without further ado, here is the test, which should be appended to the bottom of
our `src/lib.rs` file:

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use std::mem;

    #[test]
    fn round_trip_compression_decompression() {
        unsafe {
            let input = include_str!("../futurama-quotes.txt").as_bytes();
            let mut compressed_output: Vec<u8> = vec![0; input.len()];
            let mut decompressed_output: Vec<u8> = vec![0; input.len()];

            // Construct a compression stream.
            let mut stream: bz_stream = mem::zeroed();
            let result = BZ2_bzCompressInit(&mut stream as *mut _,
                                            1,   // 1 x 100000 block size
                                            4,   // verbosity (4 = most verbose)
                                            0);  // default work factor
            match result {
                r if r == (BZ_CONFIG_ERROR as _) => panic!("BZ_CONFIG_ERROR"),
                r if r == (BZ_PARAM_ERROR as _) => panic!("BZ_PARAM_ERROR"),
                r if r == (BZ_MEM_ERROR as _) => panic!("BZ_MEM_ERROR"),
                r if r == (BZ_OK as _) => {},
                r => panic!("Unknown return value = {}", r),
            }

            // Compress `input` into `compressed_output`.
            stream.next_in = input.as_ptr() as *mut _;
            stream.avail_in = input.len() as _;
            stream.next_out = compressed_output.as_mut_ptr() as *mut _;
            stream.avail_out = compressed_output.len() as _;
            let result = BZ2_bzCompress(&mut stream as *mut _, BZ_FINISH as _);
            match result {
                r if r == (BZ_RUN_OK as _) => panic!("BZ_RUN_OK"),
                r if r == (BZ_FLUSH_OK as _) => panic!("BZ_FLUSH_OK"),
                r if r == (BZ_FINISH_OK as _) => panic!("BZ_FINISH_OK"),
                r if r == (BZ_SEQUENCE_ERROR as _) => panic!("BZ_SEQUENCE_ERROR"),
                r if r == (BZ_STREAM_END as _) => {},
                r => panic!("Unknown return value = {}", r),
            }

            // Finish the compression stream.
            let result = BZ2_bzCompressEnd(&mut stream as *mut _);
            match result {
                r if r == (BZ_PARAM_ERROR as _) => panic!(BZ_PARAM_ERROR),
                r if r == (BZ_OK as _) => {},
                r => panic!("Unknown return value = {}", r),
            }

            // Construct a decompression stream.
            let mut stream: bz_stream = mem::zeroed();
            let result = BZ2_bzDecompressInit(&mut stream as *mut _,
                                              4,   // verbosity (4 = most verbose)
                                              0);  // default small factor
            match result {
                r if r == (BZ_CONFIG_ERROR as _) => panic!("BZ_CONFIG_ERROR"),
                r if r == (BZ_PARAM_ERROR as _) => panic!("BZ_PARAM_ERROR"),
                r if r == (BZ_MEM_ERROR as _) => panic!("BZ_MEM_ERROR"),
                r if r == (BZ_OK as _) => {},
                r => panic!("Unknown return value = {}", r),
            }

            // Decompress `compressed_output` into `decompressed_output`.
            stream.next_in = compressed_output.as_ptr() as *mut _;
            stream.avail_in = compressed_output.len() as _;
            stream.next_out = decompressed_output.as_mut_ptr() as *mut _;
            stream.avail_out = decompressed_output.len() as _;
            let result = BZ2_bzDecompress(&mut stream as *mut _);
            match result {
                r if r == (BZ_PARAM_ERROR as _) => panic!("BZ_PARAM_ERROR"),
                r if r == (BZ_DATA_ERROR as _) => panic!("BZ_DATA_ERROR"),
                r if r == (BZ_DATA_ERROR_MAGIC as _) => panic!("BZ_DATA_ERROR"),
                r if r == (BZ_MEM_ERROR as _) => panic!("BZ_MEM_ERROR"),
                r if r == (BZ_OK as _) => panic!("BZ_OK"),
                r if r == (BZ_STREAM_END as _) => {},
                r => panic!("Unknown return value = {}", r),
            }

            // Close the decompression stream.
            let result = BZ2_bzDecompressEnd(&mut stream as *mut _);
            match result {
                r if r == (BZ_PARAM_ERROR as _) => panic!("BZ_PARAM_ERROR"),
                r if r == (BZ_OK as _) => {},
                r => panic!("Unknown return value = {}", r),
            }

            assert_eq!(input, &decompressed_output[..]);
        }
    }
}
```

Now let's run `cargo test` again and verify that everying is linking and binding
properly!

```bash
$ cargo test
   Compiling libbindgen-tutorial-bzip2-sys v0.1.0
    Finished debug [unoptimized + debuginfo] target(s) in 0.54 secs
     Running target/debug/deps/libbindgen_tutorial_bzip2_sys-1c5626bbc4401c3a

running 15 tests
test bindgen_test_layout___darwin_pthread_handler_rec ... ok
test bindgen_test_layout___sFILE ... ok
test bindgen_test_layout___sbuf ... ok
test bindgen_test_layout__bindgen_ty_1 ... ok
test bindgen_test_layout__bindgen_ty_2 ... ok
test bindgen_test_layout__opaque_pthread_attr_t ... ok
test bindgen_test_layout__opaque_pthread_cond_t ... ok
test bindgen_test_layout__opaque_pthread_condattr_t ... ok
test bindgen_test_layout__opaque_pthread_mutex_t ... ok
test bindgen_test_layout__opaque_pthread_mutexattr_t ... ok
test bindgen_test_layout__opaque_pthread_once_t ... ok
test bindgen_test_layout__opaque_pthread_rwlock_t ... ok
test bindgen_test_layout__opaque_pthread_rwlockattr_t ... ok
test bindgen_test_layout__opaque_pthread_t ... ok
    block 1: crc = 0x47bfca17, combined CRC = 0x47bfca17, size = 2857
        bucket sorting ...
        depth      1 has   2849 unresolved strings
        depth      2 has   2702 unresolved strings
        depth      4 has   1508 unresolved strings
        depth      8 has    538 unresolved strings
        depth     16 has    148 unresolved strings
        depth     32 has      0 unresolved strings
        reconstructing block ...
      2857 in block, 2221 after MTF & 1-2 coding, 61+2 syms in use
      initial group 5, [0 .. 1], has 570 syms (25.7%)
      initial group 4, [2 .. 2], has 256 syms (11.5%)
      initial group 3, [3 .. 6], has 554 syms (24.9%)
      initial group 2, [7 .. 12], has 372 syms (16.7%)
      initial group 1, [13 .. 62], has 469 syms (21.1%)
      pass 1: size is 2743, grp uses are 13 6 15 0 11
      pass 2: size is 1216, grp uses are 13 7 15 0 10
      pass 3: size is 1214, grp uses are 13 8 14 0 10
      pass 4: size is 1213, grp uses are 13 9 13 0 10
      bytes: mapping 19, selectors 17, code lengths 79, codes 1213
    final combined CRC = 0x47bfca17

    [1: huff+mtf rt+rld {0x47bfca17, 0x47bfca17}]
    combined CRCs: stored = 0x47bfca17, computed = 0x47bfca17
test tests::round_trip_compression_decompression ... ok

test result: ok. 15 passed; 0 failed; 0 ignored; 0 measured

   Doc-tests libbindgen-tutorial-bzip2-sys

running 0 tests

test result: ok. 0 passed; 0 failed; 0 ignored; 0 measured
```
