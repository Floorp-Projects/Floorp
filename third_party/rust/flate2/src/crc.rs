//! Simple CRC bindings backed by miniz.c

use std::io::prelude::*;
use std::io;
use libc;

use ffi;

/// The CRC calculated by a CrcReader.
pub struct Crc {
    crc: libc::c_ulong,
    amt: u32,
}

/// A wrapper around a `std::io::Read` that calculates the CRC.
pub struct CrcReader<R> {
    inner: R,
    crc: Crc,
}

impl Crc {
    /// Create a new CRC.
    pub fn new() -> Crc {
        Crc { crc: 0, amt: 0 }
    }

    /// bla
    pub fn sum(&self) -> u32 {
        self.crc as u32
    }

    /// The number of bytes that have been used to calculate the CRC.
    /// This value is only accurate if the amount is lower than 2^32.
    pub fn amount(&self) -> u32 {
        self.amt
    }

    /// Update the CRC with the bytes in `data`.
    pub fn update(&mut self, data: &[u8]) {
        self.amt = self.amt.wrapping_add(data.len() as u32);
        self.crc = unsafe {
            ffi::mz_crc32(self.crc, data.as_ptr(), data.len() as libc::size_t)
        };
    }

    /// Reset the CRC.
    pub fn reset(&mut self) {
        self.crc = 0;
        self.amt = 0;
    }

    /// Combine the CRC with the CRC for the subsequent block of bytes.
    pub fn combine(&mut self, additional_crc: &Crc) {
        self.crc = unsafe {
            ffi::mz_crc32_combine(self.crc as ::libc::c_ulong,
                                  additional_crc.crc as ::libc::c_ulong,
                                  additional_crc.amt as ::libc::off_t)
        };
        self.amt += additional_crc.amt;
    }
}

impl<R: Read> CrcReader<R> {
    /// Create a new CrcReader.
    pub fn new(r: R) -> CrcReader<R> {
        CrcReader {
            inner: r,
            crc: Crc::new(),
        }
    }

    /// Get the Crc for this CrcReader.
    pub fn crc(&self) -> &Crc {
        &self.crc
    }

    /// Get the reader that is wrapped by this CrcReader.
    pub fn into_inner(self) -> R {
        self.inner
    }

    /// Get the reader that is wrapped by this CrcReader by reference.
    pub fn get_ref(&self) -> &R {
        &self.inner
    }

    /// Get a mutable reference to the reader that is wrapped by this CrcReader.
    pub fn get_mut(&mut self) -> &mut R {
        &mut self.inner
    }

    /// Reset the Crc in this CrcReader.
    pub fn reset(&mut self) {
        self.crc.reset();
    }
}

impl<R: Read> Read for CrcReader<R> {
    fn read(&mut self, into: &mut [u8]) -> io::Result<usize> {
        let amt = try!(self.inner.read(into));
        self.crc.update(&into[..amt]);
        Ok(amt)
    }
}

impl<R: BufRead> BufRead for CrcReader<R> {
    fn fill_buf(&mut self) -> io::Result<&[u8]> {
        self.inner.fill_buf()
    }
    fn consume(&mut self, amt: usize) {
        if let Ok(data) = self.inner.fill_buf() {
            self.crc.update(&data[..amt]);
        }
        self.inner.consume(amt);
    }
}
