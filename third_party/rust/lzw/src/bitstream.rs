//! This module provides bit readers and writers

use std::io::{self, Write};

/// Containes either the consumed bytes and reconstructed bits or
/// only the consumed bytes if the supplied buffer was not bit enough
pub enum Bits {
    /// Consumed bytes, reconstructed bits
    Some(usize, u16),
    /// Consumed bytes
    None(usize),
}

/// A bit reader.
pub trait BitReader {
    /// Returns the next `n` bits.
    fn read_bits(&mut self, buf: &[u8], n: u8) -> Bits;
}

/// A bit writer.
pub trait BitWriter: Write {
    /// Writes the next `n` bits.
    fn write_bits(&mut self, v: u16, n: u8) -> io::Result<()>;
}

macro_rules! define_bit_readers {
    {$(
        $name:ident, #[$doc:meta];
    )*} => {

$( // START Structure definitions

#[$doc]
#[derive(Debug)]
pub struct $name {
    bits: u8,
    acc: u32,
}

impl $name {

    /// Creates a new bit reader
    pub fn new() -> $name {
        $name {
            bits: 0,
            acc: 0,
        }
    }


}

)* // END Structure definitions

    }
}

define_bit_readers!{
    LsbReader, #[doc = "Reads bits from a byte stream, LSB first."];
    MsbReader, #[doc = "Reads bits from a byte stream, MSB first."];
}

impl BitReader for LsbReader {

    fn read_bits(&mut self, mut buf: &[u8], n: u8) -> Bits {
        if n > 16 {
            // This is a logic error the program should have prevented this
            // Ideally we would used bounded a integer value instead of u8
            panic!("Cannot read more than 16 bits")
        }
        let mut consumed = 0;
        while self.bits < n {
            let byte = if buf.len() > 0 {
                let byte = buf[0];
                buf = &buf[1..];
                byte
            } else {
                return Bits::None(consumed)
            };
            self.acc |= (byte as u32) << self.bits;
            self.bits += 8;
            consumed += 1;
        }
        let res = self.acc & ((1 << n) - 1);
        self.acc >>= n;
        self.bits -= n;
        Bits::Some(consumed, res as u16)
    }

}

impl BitReader for MsbReader {

    fn read_bits(&mut self, mut buf: &[u8], n: u8) -> Bits {
        if n > 16 {
            // This is a logic error the program should have prevented this
            // Ideally we would used bounded a integer value instead of u8
            panic!("Cannot read more than 16 bits")
        }
        let mut consumed = 0;
        while self.bits < n {
            let byte = if buf.len() > 0 {
                let byte = buf[0];
                buf = &buf[1..];
                byte
            } else {
                return Bits::None(consumed)
            };
            self.acc |= (byte as u32) << (24 - self.bits);
            self.bits += 8;
            consumed += 1;
        }
        let res = self.acc >> (32 - n);
        self.acc <<= n;
        self.bits -= n;
        Bits::Some(consumed, res as u16)
    }
}

macro_rules! define_bit_writers {
    {$(
        $name:ident, #[$doc:meta];
    )*} => {

$( // START Structure definitions

#[$doc]
#[allow(dead_code)]
pub struct $name<W: Write> {
    w: W,
    bits: u8,
    acc: u32,
}

impl<W: Write> $name<W> {
    /// Creates a new bit reader
    #[allow(dead_code)]
    pub fn new(writer: W) -> $name<W> {
        $name {
            w: writer,
            bits: 0,
            acc: 0,
        }
    }
}

impl<W: Write> Write for $name<W> {

    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if self.acc == 0 {
            self.w.write(buf)
        } else {
            for &byte in buf.iter() {
                try!(self.write_bits(byte as u16, 8))
            }
            Ok(buf.len())
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        let missing = 8 - self.bits;
        if missing > 0 {
            try!(self.write_bits(0, missing));
        }
        self.w.flush()
    }
}

)* // END Structure definitions

    }
}

define_bit_writers!{
    LsbWriter, #[doc = "Writes bits to a byte stream, LSB first."];
    MsbWriter, #[doc = "Writes bits to a byte stream, MSB first."];
}

impl<W: Write> BitWriter for LsbWriter<W> {

    fn write_bits(&mut self, v: u16, n: u8) -> io::Result<()> {
        self.acc |= (v as u32) << self.bits;
        self.bits += n;
        while self.bits >= 8 {
            try!(self.w.write_all(&[self.acc as u8]));
            self.acc >>= 8;
            self.bits -= 8

        }
        Ok(())
    }

}

impl<W: Write> BitWriter for MsbWriter<W> {

    fn write_bits(&mut self, v: u16, n: u8) -> io::Result<()> {
        self.acc |= (v as u32) << (32 - n - self.bits);
        self.bits += n;
        while self.bits >= 8 {
            try!(self.w.write_all(&[(self.acc >> 24) as u8]));
            self.acc <<= 8;
            self.bits -= 8

        }
        Ok(())
    }

}

#[cfg(test)]
mod test {
    use super::{BitReader, BitWriter, Bits};

    #[test]
    fn reader_writer() {
        let data = [255, 20, 40, 120, 128];
        let mut offset = 0;
        let mut expanded_data = Vec::new();
        let mut reader = super::LsbReader::new();
        while let Bits::Some(consumed, b) = reader.read_bits(&data[offset..], 10) {
            offset += consumed;
            expanded_data.push(b)
        }
        let mut compressed_data = Vec::new();
        {
            let mut writer = super::LsbWriter::new(&mut compressed_data);
            for &datum in expanded_data.iter() {
                let _  = writer.write_bits(datum, 10);
            }
        }
        assert_eq!(&data[..], &compressed_data[..])
    }
}
