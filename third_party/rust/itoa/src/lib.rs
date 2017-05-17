// Copyright 2016 Itoa Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{io, mem, ptr, slice};

#[inline]
pub fn write<W: io::Write, V: Integer>(wr: W, value: V) -> io::Result<usize> {
    value.write(wr)
}

pub trait Integer {
    fn write<W: io::Write>(self, W) -> io::Result<usize>;
}

const DEC_DIGITS_LUT: &'static[u8] =
    b"0001020304050607080910111213141516171819\
      2021222324252627282930313233343536373839\
      4041424344454647484950515253545556575859\
      6061626364656667686970717273747576777879\
      8081828384858687888990919293949596979899";

// Adaptation of the original implementation at
// https://github.com/rust-lang/rust/blob/b8214dc6c6fc20d0a660fb5700dca9ebf51ebe89/src/libcore/fmt/num.rs#L188-L266
macro_rules! impl_Integer {
    ($($t:ident),* as $conv_fn:ident) => ($(
    impl Integer for $t {
        #[allow(unused_comparisons)]
        fn write<W: io::Write>(self, mut wr: W) -> io::Result<usize> {
            let is_nonnegative = self >= 0;
            let mut n = if is_nonnegative {
                self as $conv_fn
            } else {
                try!(wr.write_all(b"-"));
                // convert the negative num to positive by summing 1 to it's 2 complement
                (!(self as $conv_fn)).wrapping_add(1)
            };
            let mut buf: [u8; 20] = unsafe { mem::uninitialized() };
            let mut curr = buf.len() as isize;
            let buf_ptr = buf.as_mut_ptr();
            let lut_ptr = DEC_DIGITS_LUT.as_ptr();

            unsafe {
                // eagerly decode 4 characters at a time
                if <$t>::max_value() as u64 >= 10000 {
                    while n >= 10000 {
                        let rem = (n % 10000) as isize;
                        n /= 10000;

                        let d1 = (rem / 100) << 1;
                        let d2 = (rem % 100) << 1;
                        curr -= 4;
                        ptr::copy_nonoverlapping(lut_ptr.offset(d1), buf_ptr.offset(curr), 2);
                        ptr::copy_nonoverlapping(lut_ptr.offset(d2), buf_ptr.offset(curr + 2), 2);
                    }
                }

                // if we reach here numbers are <= 9999, so at most 4 chars long
                let mut n = n as isize; // possibly reduce 64bit math

                // decode 2 more chars, if > 2 chars
                if n >= 100 {
                    let d1 = (n % 100) << 1;
                    n /= 100;
                    curr -= 2;
                    ptr::copy_nonoverlapping(lut_ptr.offset(d1), buf_ptr.offset(curr), 2);
                }

                // decode last 1 or 2 chars
                if n < 10 {
                    curr -= 1;
                    *buf_ptr.offset(curr) = (n as u8) + 48;
                } else {
                    let d1 = n << 1;
                    curr -= 2;
                    ptr::copy_nonoverlapping(lut_ptr.offset(d1), buf_ptr.offset(curr), 2);
                }
            }

            let mut len = buf.len() - curr as usize;
            try!(wr.write_all(unsafe { slice::from_raw_parts(buf_ptr.offset(curr), len) }));
            if !is_nonnegative {
                len += 1;
            }
            Ok(len)
        }
    })*);
}

impl_Integer!(i8, u8, i16, u16, i32, u32 as u32);
impl_Integer!(i64, u64 as u64);
#[cfg(target_pointer_width = "16")]
impl_Integer!(isize, usize as u16);
#[cfg(target_pointer_width = "32")]
impl_Integer!(isize, usize as u32);
#[cfg(target_pointer_width = "64")]
impl_Integer!(isize, usize as u64);
