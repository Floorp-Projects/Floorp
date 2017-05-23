pub use self::imp::*;

#[cfg(feature = "zlib")]
#[allow(bad_style)]
mod imp {
    extern crate libz_sys as z;
    use std::mem;
    use std::ops::{Deref, DerefMut};
    use libc::{c_int, size_t, c_ulong, c_uint, c_char};

    pub use self::z::*;
    pub use self::z::deflateEnd as mz_deflateEnd;
    pub use self::z::inflateEnd as mz_inflateEnd;
    pub use self::z::deflateReset as mz_deflateReset;
    pub use self::z::deflate as mz_deflate;
    pub use self::z::inflate as mz_inflate;
    pub use self::z::z_stream as mz_stream;

    pub use self::z::Z_BLOCK as MZ_BLOCK;
    pub use self::z::Z_BUF_ERROR as MZ_BUF_ERROR;
    pub use self::z::Z_DATA_ERROR as MZ_DATA_ERROR;
    pub use self::z::Z_DEFAULT_STRATEGY as MZ_DEFAULT_STRATEGY;
    pub use self::z::Z_DEFLATED as MZ_DEFLATED;
    pub use self::z::Z_FINISH as MZ_FINISH;
    pub use self::z::Z_FULL_FLUSH as MZ_FULL_FLUSH;
    pub use self::z::Z_NO_FLUSH as MZ_NO_FLUSH;
    pub use self::z::Z_OK as MZ_OK;
    pub use self::z::Z_PARTIAL_FLUSH as MZ_PARTIAL_FLUSH;
    pub use self::z::Z_STREAM_END as MZ_STREAM_END;
    pub use self::z::Z_SYNC_FLUSH as MZ_SYNC_FLUSH;
    pub use self::z::Z_STREAM_ERROR as MZ_STREAM_ERROR;

    pub const MZ_DEFAULT_WINDOW_BITS: c_int = 15;

    pub unsafe extern fn mz_crc32(crc: c_ulong,
                                  ptr: *const u8,
                                  len: size_t) -> c_ulong {
        z::crc32(crc, ptr, len as c_uint)
    }

    pub unsafe extern fn mz_crc32_combine(crc1: c_ulong,
                                          crc2: c_ulong,
                                          len2: z_off_t) -> c_ulong {
          z::crc32_combine(crc1, crc2, len2)
    }

    const ZLIB_VERSION: &'static str = "1.2.8\0";

    pub unsafe extern fn mz_deflateInit2(stream: *mut mz_stream,
                                         level: c_int,
                                         method: c_int,
                                         window_bits: c_int,
                                         mem_level: c_int,
                                         strategy: c_int) -> c_int {
        z::deflateInit2_(stream, level, method, window_bits, mem_level,
                         strategy,
                         ZLIB_VERSION.as_ptr() as *const c_char,
                         mem::size_of::<mz_stream>() as c_int)
    }
    pub unsafe extern fn mz_inflateInit2(stream: *mut mz_stream,
                                         window_bits: c_int)
                                         -> c_int {
        z::inflateInit2_(stream, window_bits,
                         ZLIB_VERSION.as_ptr() as *const c_char,
                         mem::size_of::<mz_stream>() as c_int)
    }

    pub struct StreamWrapper{
        inner: Box<mz_stream>,
    }

    impl Default for StreamWrapper {
        fn default() -> StreamWrapper {
            StreamWrapper {
                inner: Box::new(unsafe{ mem::zeroed() })
            }
        }
    }

    impl Deref for StreamWrapper {
        type Target = mz_stream;

        fn deref(&self) -> &Self::Target {
            & *self.inner
        }
    }

    impl DerefMut for StreamWrapper {
        fn deref_mut(&mut self) -> &mut Self::Target {
            &mut *self.inner
        }
    }
}

#[cfg(not(feature = "zlib"))]
mod imp {
    extern crate miniz_sys;
    use std::mem;
    use std::ops::{Deref, DerefMut};

    use libc::{c_ulong, off_t};
    pub use self::miniz_sys::*;

    pub struct StreamWrapper {
        inner: mz_stream,
    }

    impl Default for StreamWrapper {
        fn default() -> StreamWrapper {
            StreamWrapper {
                inner : unsafe{ mem::zeroed() }
            }
        }
    }

    impl Deref for StreamWrapper {
        type Target = mz_stream;

        fn deref(&self) -> &Self::Target {
            &self.inner
        }
    }

    impl DerefMut for StreamWrapper {
        fn deref_mut(&mut self) -> &mut Self::Target {
            &mut self.inner
        }
    }

    pub unsafe extern fn mz_crc32_combine(crc1: c_ulong,
                                          crc2: c_ulong,
                                          len2: off_t) -> c_ulong {
          crc32_combine_(crc1, crc2, len2)
    }

    // gf2_matrix_times, gf2_matrix_square and crc32_combine_ are ported from
    // zlib.

    fn gf2_matrix_times(mat: &[c_ulong; 32], mut vec: c_ulong) -> c_ulong {
        let mut sum = 0;
        let mut mat_pos = 0;
        while vec != 0 {
            if vec & 1 == 1 {
                sum ^= mat[mat_pos];
            }
            vec >>= 1;
            mat_pos += 1;
        }
        sum
    }

    fn gf2_matrix_square(square: &mut [c_ulong; 32], mat: &[c_ulong; 32]) {
        for n in 0..32 {
            square[n] = gf2_matrix_times(mat, mat[n]);
        }
    }

    fn crc32_combine_(mut crc1: c_ulong, crc2: c_ulong, mut len2: off_t) -> c_ulong {
        let mut row;

        let mut even = [0; 32]; /* even-power-of-two zeros operator */
        let mut odd = [0; 32]; /* odd-power-of-two zeros operator */

        /* degenerate case (also disallow negative lengths) */
        if len2 <= 0 {
            return crc1;
        }

        /* put operator for one zero bit in odd */
        odd[0] = 0xedb88320;          /* CRC-32 polynomial */
        row = 1;
        for n in 1..32 {
            odd[n] = row;
            row <<= 1;
        }

        /* put operator for two zero bits in even */
        gf2_matrix_square(&mut even, &odd);

        /* put operator for four zero bits in odd */
        gf2_matrix_square(&mut odd, &even);

        /* apply len2 zeros to crc1 (first square will put the operator for one
           zero byte, eight zero bits, in even) */
        loop {
            /* apply zeros operator for this bit of len2 */
            gf2_matrix_square(&mut even, &odd);
            if len2 & 1 == 1 {
                crc1 = gf2_matrix_times(&even, crc1);
            }
            len2 >>= 1;

            /* if no more bits set, then done */
            if len2 == 0 {
                break;
            }

            /* another iteration of the loop with odd and even swapped */
            gf2_matrix_square(&mut odd, &even);
            if len2 & 1 == 1 {
                crc1 = gf2_matrix_times(&odd, crc1);
            }
            len2 >>= 1;

            /* if no more bits set, then done */
            if len2 == 0 {
                break;
            }
        }

        /* return combined crc */
        crc1 ^= crc2;
        crc1
    }
}

#[test]
fn crc32_combine() {
    let crc32 = unsafe {
        imp::mz_crc32_combine(1, 2, 3)
    };
    assert_eq!(crc32, 29518389);
}
