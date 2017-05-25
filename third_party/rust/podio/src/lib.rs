//! Additional methods for Read and Write
//!
//! The additional methods implemented allow reading and writing integers and floats
//! in the specified endianness.
//!
//! # Usage
//!
//! Basically, you need to `use` the trait WritePodExt or ReadPodExt.
//!
//! # Examples
//!
//! ## Reading
//!
//! To read some value from a reader, import ReadPodExt and the needed endianness.
//!
//! ```
//! use podio::{ReadPodExt, BigEndian};
//!
//! let slice: &[u8] = &[0x10, 0x20, 0x30, 0x40];
//! let mut reader = std::io::Cursor::new(slice);
//!
//! let value = reader.read_u32::<BigEndian>().unwrap();
//!
//! assert_eq!(value, 0x10203040);
//! ```
//!
//! ## Writing
//!
//! For writing, you need to import the trait WritePodExt.
//!
//! ```
//! use podio::{WritePodExt, LittleEndian};
//!
//! let slice: &mut [u8] = &mut [0; 2];
//! let mut writer = std::io::Cursor::new(slice);
//!
//! writer.write_u16::<LittleEndian>(0x8802).unwrap();
//!
//! assert_eq!(writer.into_inner(), &[0x02, 0x88]);
//! ```
//!
//! ## Read exact
//!
//! One additional method, not really dealing with POD, is `read_exact`.
//!
//! ```
//! use podio::ReadPodExt;
//!
//! let slice: &[u8] = &[0, 1, 2, 3];
//! let mut reader = std::io::Cursor::new(slice);
//!
//! assert_eq!(reader.read_exact(1).unwrap(), [0]);
//! assert_eq!(reader.read_exact(2).unwrap(), [1,2]);
//! assert_eq!(reader.read_exact(0).unwrap(), []);
//! assert_eq!(reader.read_exact(1).unwrap(), [3]);
//! assert!(reader.read_exact(1).is_err());

#![warn(missing_docs)]

use std::io;
use std::io::prelude::*;

/// Little endian. The number `0xABCD` is stored `[0xCD, 0xAB]`
pub enum LittleEndian {}
/// Big endian. The number `0xABCD` is stored `[0xAB, 0xCD]`
pub enum BigEndian {}

/// Trait implementing conversion methods for a specific endianness
pub trait Endianness {
    /// Converts a value from the platform type to the specified endianness
    fn int_to_target<T: EndianConvert>(val: T) -> T;
    /// Converts a value from the sepcified endianness to the platform type
    fn int_from_target<T: EndianConvert>(val: T) -> T;
}

/// Generic trait for endian conversions on integers
pub trait EndianConvert {
    /// Convert self to a big-endian value
    fn to_be(self) -> Self;
    /// Convert self to a little-endian value
    fn to_le(self) -> Self;
    /// Convert a big-endian value to the target endianness
    fn from_be(x: Self) -> Self;
    /// Convert a little-endian value to the target endiannes
    fn from_le(x: Self) -> Self;
}

/// Additional write methods for a io::Write
pub trait WritePodExt {
    /// Write a u64
    fn write_u64<T: Endianness>(&mut self, u64) -> io::Result<()>;
    /// Write a u32
    fn write_u32<T: Endianness>(&mut self, u32) -> io::Result<()>;
    /// Write a u16
    fn write_u16<T: Endianness>(&mut self, u16) -> io::Result<()>;
    /// Write a u8
    fn write_u8(&mut self, u8) -> io::Result<()>;
    /// Write a i64
    fn write_i64<T: Endianness>(&mut self, i64) -> io::Result<()>;
    /// Write a i32
    fn write_i32<T: Endianness>(&mut self, i32) -> io::Result<()>;
    /// Write a i16
    fn write_i16<T: Endianness>(&mut self, i16) -> io::Result<()>;
    /// Write a i8
    fn write_i8(&mut self, i8) -> io::Result<()>;
    /// Write a f32
    fn write_f32<T: Endianness>(&mut self, f32) -> io::Result<()>;
    /// Write a f64
    fn write_f64<T: Endianness>(&mut self, f64) -> io::Result<()>;
}

/// Additional read methods for a io::Read
pub trait ReadPodExt {
    /// Read a u64
    fn read_u64<T: Endianness>(&mut self) -> io::Result<u64>;
    /// Read a u32
    fn read_u32<T: Endianness>(&mut self) -> io::Result<u32>;
    /// Read a u16
    fn read_u16<T: Endianness>(&mut self) -> io::Result<u16>;
    /// Read a u8
    fn read_u8(&mut self) -> io::Result<u8>;
    /// Read a i64
    fn read_i64<T: Endianness>(&mut self) -> io::Result<i64>;
    /// Read a i32
    fn read_i32<T: Endianness>(&mut self) -> io::Result<i32>;
    /// Read a i16
    fn read_i16<T: Endianness>(&mut self) -> io::Result<i16>;
    /// Read a i8
    fn read_i8(&mut self) -> io::Result<i8>;
    /// Read a f32
    fn read_f32<T: Endianness>(&mut self) -> io::Result<f32>;
    /// Read a f64
    fn read_f64<T: Endianness>(&mut self) -> io::Result<f64>;
    /// Read a specific number of bytes
    fn read_exact(&mut self, usize) -> io::Result<Vec<u8>>;
}

impl Endianness for LittleEndian {
    #[inline]
    fn int_to_target<T: EndianConvert>(val: T) -> T {
        val.to_le()
    }
    #[inline]
    fn int_from_target<T: EndianConvert>(val: T) -> T {
        <T as EndianConvert>::from_le(val)
    }
}

impl Endianness for BigEndian {
    #[inline]
    fn int_to_target<T: EndianConvert>(val: T) -> T {
        val.to_be()
    }
    #[inline]
    fn int_from_target<T: EndianConvert>(val: T) -> T {
        <T as EndianConvert>::from_be(val)
    }
}

macro_rules! impl_platform_convert {
    ($T:ty) => {
        impl EndianConvert for $T {
            #[inline]
            fn to_be(self) -> $T {
                self.to_be()
            }

            #[inline]
            fn to_le(self) -> $T {
                self.to_le()
            }

            #[inline]
            fn from_be(x: $T) -> $T {
                if cfg!(target_endian = "big") { x } else { x.swap_bytes() }
            }

            #[inline]
            fn from_le(x: $T) -> $T {
                if cfg!(target_endian = "little") { x } else { x.swap_bytes() }
            }
        }
    };
}

impl_platform_convert!(u8);
impl_platform_convert!(u16);
impl_platform_convert!(u32);
impl_platform_convert!(u64);

macro_rules! val_to_buf {
    ($val:ident, $T:expr) => {
        {
            let mut buf = [0; $T];
            for i in 0..buf.len() {
                buf[i] = ($val >> (i * 8)) as u8;
            }
            buf
        }
    };
}

impl<W: Write> WritePodExt for W {
    fn write_u64<T: Endianness>(&mut self, val: u64) -> io::Result<()> {
        let tval = <T as Endianness>::int_to_target(val);
        let buf = val_to_buf!(tval, 8);
        self.write_all(&buf)
    }

    fn write_u32<T: Endianness>(&mut self, val: u32) -> io::Result<()> {
        let tval = <T as Endianness>::int_to_target(val);
        let buf = val_to_buf!(tval, 4);
        self.write_all(&buf)
    }

    fn write_u16<T: Endianness>(&mut self, val: u16) -> io::Result<()> {
        let tval = <T as Endianness>::int_to_target(val);
        let buf = val_to_buf!(tval, 2);
        self.write_all(&buf)
    }

    fn write_u8(&mut self, val: u8) -> io::Result<()> {
        self.write_all(&[val])
    }

    fn write_i64<T: Endianness>(&mut self, val: i64) -> io::Result<()> {
        self.write_u64::<T>(val as u64)
    }

    fn write_i32<T: Endianness>(&mut self, val: i32) -> io::Result<()> {
        self.write_u32::<T>(val as u32)
    }

    fn write_i16<T: Endianness>(&mut self, val: i16) -> io::Result<()> {
        self.write_u16::<T>(val as u16)
    }

    fn write_i8(&mut self, val: i8) -> io::Result<()> {
        self.write_u8(val as u8)
    }

    fn write_f32<T: Endianness>(&mut self, val: f32) -> io::Result<()> {
        let tval: u32 = unsafe { std::mem::transmute::<f32, u32>(val) };
        self.write_u32::<T>(tval)
    }

    fn write_f64<T: Endianness>(&mut self, val: f64) -> io::Result<()> {
        let tval: u64 = unsafe { std::mem::transmute::<f64, u64>(val) };
        self.write_u64::<T>(tval)
    }
}

#[inline]
fn fill_buf<R: Read>(reader: &mut R, buf: &mut [u8]) -> io::Result<()> {
    let mut idx = 0;
    while idx != buf.len() {
        match reader.read(&mut buf[idx..]) {
            Ok(0) => return Err(io::Error::new(io::ErrorKind::Other, "Could not read enough bytes")),
            Ok(v) => { idx += v; }
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
            Err(e) => return Err(e),
        }
    }
    Ok(())
}

macro_rules! buf_to_val {
    ($buf:ident, $T:ty) => {
        {
            let mut val: $T = 0;
            for i in 0..$buf.len() {
                val |= ($buf[i] as $T) << (i * 8);
            }
            val
        }
    };
}

impl<R: Read> ReadPodExt for R {
    fn read_u64<T: Endianness>(&mut self) -> io::Result<u64> {
        let buf = &mut [0u8; 8];
        try!(fill_buf(self, buf));
        let tval = buf_to_val!(buf, u64);
        Ok(<T as Endianness>::int_from_target(tval))
    }

    fn read_u32<T: Endianness>(&mut self) -> io::Result<u32> {
        let buf = &mut [0u8; 4];
        try!(fill_buf(self, buf));
        let tval = buf_to_val!(buf, u32);
        Ok(<T as Endianness>::int_from_target(tval))
    }

    fn read_u16<T: Endianness>(&mut self) -> io::Result<u16> {
        let buf = &mut [0u8; 2];
        try!(fill_buf(self, buf));
        let tval = buf_to_val!(buf, u16);
        Ok(<T as Endianness>::int_from_target(tval))
    }

    fn read_u8(&mut self) -> io::Result<u8> {
        let buf = &mut [0u8; 1];
        try!(fill_buf(self, buf));
        Ok(buf[0])
    }

    fn read_i64<T: Endianness>(&mut self) -> io::Result<i64> {
        self.read_u64::<T>().map(|v| v as i64)
    }

    fn read_i32<T: Endianness>(&mut self) -> io::Result<i32> {
        self.read_u32::<T>().map(|v| v as i32)
    }

    fn read_i16<T: Endianness>(&mut self) -> io::Result<i16> {
        self.read_u16::<T>().map(|v| v as i16)
    }

    fn read_i8(&mut self) -> io::Result<i8> {
        self.read_u8().map(|v| v as i8)
    }

    fn read_f64<T: Endianness>(&mut self) -> io::Result<f64> {
        self.read_u64::<T>().map(|v| unsafe { std::mem::transmute::<u64, f64>(v) })
    }

    fn read_f32<T: Endianness>(&mut self) -> io::Result<f32> {
        self.read_u32::<T>().map(|v| unsafe { std::mem::transmute::<u32, f32>(v) })
    }

    fn read_exact(&mut self, len: usize) -> io::Result<Vec<u8>> {
        let mut res = vec![0; len];
        try!(fill_buf(self, &mut res));
        Ok(res)
    }
}
