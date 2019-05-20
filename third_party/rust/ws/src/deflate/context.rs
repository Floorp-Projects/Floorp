use std::mem;
use std::slice;

use super::ffi;
use super::libc::{c_char, c_int, c_uint};

use result::{Error, Kind, Result};

const ZLIB_VERSION: &'static str = "1.2.8\0";

trait Context {
    fn stream(&mut self) -> &mut ffi::z_stream;

    fn stream_apply<F>(&mut self, input: &[u8], output: &mut Vec<u8>, each: F) -> Result<()>
    where
        F: Fn(&mut ffi::z_stream) -> Option<Result<()>>,
    {
        debug_assert!(output.len() == 0, "Output vector is not empty.");

        let stream = self.stream();

        stream.next_in = input.as_ptr() as *mut _;
        stream.avail_in = input.len() as c_uint;

        let mut output_size;

        loop {
            output_size = output.len();

            if output_size == output.capacity() {
                output.reserve(input.len())
            }

            let out_slice = unsafe {
                slice::from_raw_parts_mut(
                    output.as_mut_ptr().offset(output_size as isize),
                    output.capacity() - output_size,
                )
            };

            stream.next_out = out_slice.as_mut_ptr();
            stream.avail_out = out_slice.len() as c_uint;

            let before = stream.total_out;
            let cont = each(stream);

            unsafe {
                output.set_len((stream.total_out - before) as usize + output_size);
            }

            if let Some(result) = cont {
                return result;
            }
        }
    }
}

pub struct Compressor {
    // Box the z_stream to ensure it isn't moved. Moving the z_stream
    // causes zlib to fail, because it maintains internal pointers.
    stream: Box<ffi::z_stream>,
}

impl Compressor {
    pub fn new(window_bits: i8) -> Compressor {
        debug_assert!(window_bits >= 9, "Received too small window size.");
        debug_assert!(window_bits <= 15, "Received too large window size.");

        unsafe {
            let mut stream: Box<ffi::z_stream> = Box::new(mem::zeroed());
            let result = ffi::deflateInit2_(
                stream.as_mut(),
                9,
                ffi::Z_DEFLATED,
                -window_bits as c_int,
                9,
                ffi::Z_DEFAULT_STRATEGY,
                ZLIB_VERSION.as_ptr() as *const c_char,
                mem::size_of::<ffi::z_stream>() as c_int,
            );
            assert!(result == ffi::Z_OK, "Failed to initialize compresser.");
            Compressor { stream: stream }
        }
    }

    pub fn compress(&mut self, input: &[u8], output: &mut Vec<u8>) -> Result<()> {
        self.stream_apply(input, output, |stream| unsafe {
            match ffi::deflate(stream, ffi::Z_SYNC_FLUSH) {
                ffi::Z_OK | ffi::Z_BUF_ERROR => {
                    if stream.avail_in == 0 && stream.avail_out > 0 {
                        Some(Ok(()))
                    } else {
                        None
                    }
                }
                code => Some(Err(Error::new(
                    Kind::Protocol,
                    format!("Failed to perform compression: {}", code),
                ))),
            }
        })
    }

    pub fn reset(&mut self) -> Result<()> {
        match unsafe { ffi::deflateReset(self.stream.as_mut()) } {
            ffi::Z_OK => Ok(()),
            code => Err(Error::new(
                Kind::Protocol,
                format!("Failed to reset compression context: {}", code),
            )),
        }
    }
}

impl Context for Compressor {
    fn stream(&mut self) -> &mut ffi::z_stream {
        self.stream.as_mut()
    }
}

impl Drop for Compressor {
    fn drop(&mut self) {
        match unsafe { ffi::deflateEnd(self.stream.as_mut()) } {
            ffi::Z_STREAM_ERROR => error!("Compression stream encountered bad state."),
            // Ignore discarded data error because we are raw
            ffi::Z_OK | ffi::Z_DATA_ERROR => trace!("Deallocated compression context."),
            code => error!("Bad zlib status encountered: {}", code),
        }
    }
}

pub struct Decompressor {
    stream: Box<ffi::z_stream>,
}

impl Decompressor {
    pub fn new(window_bits: i8) -> Decompressor {
        debug_assert!(window_bits >= 8, "Received too small window size.");
        debug_assert!(window_bits <= 15, "Received too large window size.");

        unsafe {
            let mut stream: Box<ffi::z_stream> = Box::new(mem::zeroed());
            let result = ffi::inflateInit2_(
                stream.as_mut(),
                -window_bits as c_int,
                ZLIB_VERSION.as_ptr() as *const c_char,
                mem::size_of::<ffi::z_stream>() as c_int,
            );
            assert!(result == ffi::Z_OK, "Failed to initialize decompresser.");
            Decompressor { stream: stream }
        }
    }

    pub fn decompress(&mut self, input: &[u8], output: &mut Vec<u8>) -> Result<()> {
        self.stream_apply(input, output, |stream| unsafe {
            match ffi::inflate(stream, ffi::Z_SYNC_FLUSH) {
                ffi::Z_OK | ffi::Z_BUF_ERROR => {
                    if stream.avail_in == 0 && stream.avail_out > 0 {
                        Some(Ok(()))
                    } else {
                        None
                    }
                }
                code => Some(Err(Error::new(
                    Kind::Protocol,
                    format!("Failed to perform decompression: {}", code),
                ))),
            }
        })
    }

    pub fn reset(&mut self) -> Result<()> {
        match unsafe { ffi::inflateReset(self.stream.as_mut()) } {
            ffi::Z_OK => Ok(()),
            code => Err(Error::new(
                Kind::Protocol,
                format!("Failed to reset compression context: {}", code),
            )),
        }
    }
}

impl Context for Decompressor {
    fn stream(&mut self) -> &mut ffi::z_stream {
        self.stream.as_mut()
    }
}

impl Drop for Decompressor {
    fn drop(&mut self) {
        match unsafe { ffi::inflateEnd(self.stream.as_mut()) } {
            ffi::Z_STREAM_ERROR => error!("Decompression stream encountered bad state."),
            ffi::Z_OK => trace!("Deallocated decompression context."),
            code => error!("Bad zlib status encountered: {}", code),
        }
    }
}

mod test {
    #![allow(unused_imports, unused_variables, dead_code)]
    use super::*;

    fn as_hex(s: &[u8]) {
        for byte in s {
            print!("0x{:x} ", byte);
        }
        print!("\n");
    }

    #[test]
    fn round_trip() {
        for i in 9..16 {
            let data = "HI THERE THIS IS some data. これはデータだよ。".as_bytes();
            let mut compressed = Vec::with_capacity(data.len());
            let mut decompressed = Vec::with_capacity(data.len());

            let com = Compressor::new(i);
            let mut moved_com = com;

            moved_com
                .compress(&data, &mut compressed)
                .expect("Failed to compress data.");

            let dec = Decompressor::new(i);
            let mut moved_dec = dec;

            moved_dec
                .decompress(&compressed, &mut decompressed)
                .expect("Failed to decompress data.");

            assert_eq!(data, &decompressed[..]);
        }
    }

    #[test]
    fn reset() {
        let data1 = "HI THERE 直子さん".as_bytes();
        let data2 = "HI THERE 人太郎".as_bytes();
        let mut compressed1 = Vec::with_capacity(data1.len());
        let mut compressed2 = Vec::with_capacity(data2.len());
        let mut compressed2_ind = Vec::with_capacity(data2.len());

        let mut decompressed1 = Vec::with_capacity(data1.len());
        let mut decompressed2 = Vec::with_capacity(data2.len());
        let mut decompressed2_ind = Vec::with_capacity(data2.len());

        let mut com = Compressor::new(9);

        com.compress(&data1, &mut compressed1).unwrap();
        com.compress(&data2, &mut compressed2).unwrap();
        com.reset().unwrap();
        com.compress(&data2, &mut compressed2_ind).unwrap();

        let mut dec = Decompressor::new(9);

        dec.decompress(&compressed1, &mut decompressed1).unwrap();
        dec.decompress(&compressed2, &mut decompressed2).unwrap();
        dec.reset().unwrap();
        dec.decompress(&compressed2_ind, &mut decompressed2_ind)
            .unwrap();

        assert_eq!(data1, &decompressed1[..]);
        assert_eq!(data2, &decompressed2[..]);
        assert_eq!(data2, &decompressed2_ind[..]);
        assert!(compressed2 != compressed2_ind);
        assert!(compressed2.len() < compressed2_ind.len());
    }
}
