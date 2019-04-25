// This was originally based on code from: https://github.com/nwin/lzw
// Copyright (c) 2015 nwin
// which is under both Apache 2.0 and MIT

//! This module provides a bit writer
use std::io::{self, Write};

#[cfg(target_pointer_width = "64")]
#[macro_use]
mod arch_dep {
    /// The data type of the accumulator.
    /// a 64-bit value allows us to store more before
    /// each push to the vector, but is sub-optimal
    /// on 32-bit platforms.
    pub type AccType = u64;
    pub const FLUSH_AT: u8 = 48;
    /// Push pending bits to vector.
    /// Using a macro here since an inline function.
    /// didn't optimise properly.
    macro_rules! push{
        ($s:ident) => {
            let len = $s.w.len();
            $s.w.reserve(6);
            // Optimization:
            //
            // This is basically what `Vec::extend_from_slice` does, but it didn't inline
            // properly, so we do it manually for now.
            //
            // # Unsafe
            // We reserve enough space right before this, so setting the len manually and doing
            // unchecked indexing is safe here since we only, and always write to all of the the
            // uninitialized bytes of the vector.
            unsafe {
                $s.w.set_len(len + 6);
                $s.w.get_unchecked_mut(len..).copy_from_slice(&[$s.acc as u8,
                                                                ($s.acc >> 8) as u8,
                                                                ($s.acc >> 16) as u8,
                                                                ($s.acc >> 24) as u8,
                                                                ($s.acc >> 32) as u8,
                                                                ($s.acc >> 40) as u8
                ][..]);
            }

        };
    }
}
#[cfg(not(target_pointer_width = "64"))]
#[macro_use]
mod arch_dep {
    pub type AccType = u32;
    pub const FLUSH_AT: u8 = 16;
    macro_rules! push{
        ($s:ident) => {
            // Unlike the 64-bit case, using copy_from_slice seemed to worsen performance here.
            // TODO: Needs benching on a 32-bit system to see what works best.
            $s.w.push($s.acc as u8);
            $s.w.push(($s.acc >> 8) as u8);
        };
    }
}

use self::arch_dep::*;

/// Writes bits to a byte stream, LSB first.
pub struct LsbWriter {
    // Public for now so it can be replaced after initialization.
    pub w: Vec<u8>,
    bits: u8,
    acc: AccType,
}

impl LsbWriter {
    /// Creates a new bit reader
    pub fn new(writer: Vec<u8>) -> LsbWriter {
        LsbWriter {
            w: writer,
            bits: 0,
            acc: 0,
        }
    }

    pub fn pending_bits(&self) -> u8 {
        self.bits
    }

    /// Buffer n number of bits, and write them to the vec if there are enough pending bits.
    pub fn write_bits(&mut self, v: u16, n: u8) {
        // NOTE: This outputs garbage data if n is 0, but v is not 0
        self.acc |= (v as AccType) << self.bits;
        self.bits += n;
        // Waiting until we have FLUSH_AT bits and pushing them all in one batch.
        while self.bits >= FLUSH_AT {
            push!(self);
            self.acc >>= FLUSH_AT;
            self.bits -= FLUSH_AT;
        }
    }

    fn write_bits_finish(&mut self, v: u16, n: u8) {
        // NOTE: This outputs garbage data if n is 0, but v is not 0
        self.acc |= (v as AccType) << self.bits;
        self.bits += n % 8;
        while self.bits >= 8 {
            self.w.push(self.acc as u8);
            self.acc >>= 8;
            self.bits -= 8;
        }
    }

    pub fn flush_raw(&mut self) {
        let missing = FLUSH_AT - self.bits;
        // Have to test for self.bits > 0 here,
        // otherwise flush would output an extra byte when flush was called at a byte boundary
        if missing > 0 && self.bits > 0 {
            self.write_bits_finish(0, missing);
        }
    }
}

impl Write for LsbWriter {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if self.acc == 0 {
            self.w.extend_from_slice(buf)
        } else {
            for &byte in buf.iter() {
                self.write_bits(byte as u16, 8)
            }
        }
        Ok(buf.len())
    }

    fn flush(&mut self) -> io::Result<()> {
        self.flush_raw();
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::LsbWriter;

    #[test]
    fn write_bits() {
        let input = [
            (3, 3),
            (10, 8),
            (88, 7),
            (0, 2),
            (0, 5),
            (0, 0),
            (238, 8),
            (126, 8),
            (161, 8),
            (10, 8),
            (238, 8),
            (174, 8),
            (126, 8),
            (174, 8),
            (65, 8),
            (142, 8),
            (62, 8),
            (10, 8),
            (1, 8),
            (161, 8),
            (78, 8),
            (62, 8),
            (158, 8),
            (206, 8),
            (10, 8),
            (64, 7),
            (0, 0),
            (24, 5),
            (0, 0),
            (174, 8),
            (126, 8),
            (193, 8),
            (174, 8),
        ];
        let expected = [
            83,
            192,
            2,
            220,
            253,
            66,
            21,
            220,
            93,
            253,
            92,
            131,
            28,
            125,
            20,
            2,
            66,
            157,
            124,
            60,
            157,
            21,
            128,
            216,
            213,
            47,
            216,
            21,
        ];
        let mut writer = LsbWriter::new(Vec::new());
        for v in input.iter() {
            writer.write_bits(v.0, v.1);
        }
        writer.flush_raw();
        assert_eq!(writer.w, expected);
    }
}


#[cfg(all(test, feature = "benchmarks"))]
mod bench {
    use test_std::Bencher;
    use super::LsbWriter;
    #[bench]
    fn bit_writer(b: &mut Bencher) {
        let input = [
            (3, 3),
            (10, 8),
            (88, 7),
            (0, 2),
            (0, 5),
            (0, 0),
            (238, 8),
            (126, 8),
            (161, 8),
            (10, 8),
            (238, 8),
            (174, 8),
            (126, 8),
            (174, 8),
            (65, 8),
            (142, 8),
            (62, 8),
            (10, 8),
            (1, 8),
            (161, 8),
            (78, 8),
            (62, 8),
            (158, 8),
            (206, 8),
            (10, 8),
            (64, 7),
            (0, 0),
            (24, 5),
            (0, 0),
            (174, 8),
            (126, 8),
            (193, 8),
            (174, 8),
        ];
        let mut writer = LsbWriter::new(Vec::with_capacity(100));
        b.iter(|| for v in input.iter() {
            let _ = writer.write_bits(v.0, v.1);
        });
    }
}
