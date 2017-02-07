/*!
This crate provides convenience methods for encoding and decoding numbers
in either big-endian or little-endian order.

The organization of the crate is pretty simple. A trait, `ByteOrder`, specifies
byte conversion methods for each type of number in Rust (sans numbers that have
a platform dependent size like `usize` and `isize`). Two types, `BigEndian`
and `LittleEndian` implement these methods. Finally, `ReadBytesExt` and
`WriteBytesExt` provide convenience methods available to all types that
implement `Read` and `Write`.

# Examples

Read unsigned 16 bit big-endian integers from a `Read` type:

```rust
use std::io::Cursor;
use byteorder::{BigEndian, ReadBytesExt};

let mut rdr = Cursor::new(vec![2, 5, 3, 0]);
// Note that we use type parameters to indicate which kind of byte order
// we want!
assert_eq!(517, rdr.read_u16::<BigEndian>().unwrap());
assert_eq!(768, rdr.read_u16::<BigEndian>().unwrap());
```

Write unsigned 16 bit little-endian integers to a `Write` type:

```rust
use byteorder::{LittleEndian, WriteBytesExt};

let mut wtr = vec![];
wtr.write_u16::<LittleEndian>(517).unwrap();
wtr.write_u16::<LittleEndian>(768).unwrap();
assert_eq!(wtr, vec![5, 2, 0, 3]);
```
*/

#![crate_name = "byteorder"]
#![doc(html_root_url = "http://burntsushi.net/rustdoc/byteorder")]

#![cfg_attr(not(feature = "std"), no_std)]

#![deny(missing_docs)]

#[cfg(feature = "std")]
extern crate core;

use core::mem::transmute;
use core::ptr::copy_nonoverlapping;

#[cfg(feature = "std")]
pub use new::{ReadBytesExt, WriteBytesExt};

#[cfg(feature = "std")]
mod new;

#[inline]
fn extend_sign(val: u64, nbytes: usize) -> i64 {
    let shift = (8 - nbytes) * 8;
    (val << shift) as i64 >> shift
}

#[inline]
fn unextend_sign(val: i64, nbytes: usize) -> u64 {
    let shift = (8 - nbytes) * 8;
    (val << shift) as u64 >> shift
}

#[inline]
fn pack_size(n: u64) -> usize {
    if n < 1 << 8 {
        1
    } else if n < 1 << 16 {
        2
    } else if n < 1 << 24 {
        3
    } else if n < 1 << 32 {
        4
    } else if n < 1 << 40 {
        5
    } else if n < 1 << 48 {
        6
    } else if n < 1 << 56 {
        7
    } else {
        8
    }
}

/// ByteOrder describes types that can serialize integers as bytes.
///
/// Note that `Self` does not appear anywhere in this trait's definition!
/// Therefore, in order to use it, you'll need to use syntax like
/// `T::read_u16(&[0, 1])` where `T` implements `ByteOrder`.
///
/// This crate provides two types that implement `ByteOrder`: `BigEndian`
/// and `LittleEndian`.
///
/// # Examples
///
/// Write and read `u32` numbers in little endian order:
///
/// ```rust
/// use byteorder::{ByteOrder, LittleEndian};
///
/// let mut buf = [0; 4];
/// LittleEndian::write_u32(&mut buf, 1_000_000);
/// assert_eq!(1_000_000, LittleEndian::read_u32(&buf));
/// ```
///
/// Write and read `i16` numbers in big endian order:
///
/// ```rust
/// use byteorder::{ByteOrder, BigEndian};
///
/// let mut buf = [0; 2];
/// BigEndian::write_i16(&mut buf, -50_000);
/// assert_eq!(-50_000, BigEndian::read_i16(&buf));
/// ```
pub trait ByteOrder {
    /// Reads an unsigned 16 bit integer from `buf`.
    ///
    /// Panics when `buf.len() < 2`.
    fn read_u16(buf: &[u8]) -> u16;

    /// Reads an unsigned 32 bit integer from `buf`.
    ///
    /// Panics when `buf.len() < 4`.
    fn read_u32(buf: &[u8]) -> u32;

    /// Reads an unsigned 64 bit integer from `buf`.
    ///
    /// Panics when `buf.len() < 8`.
    fn read_u64(buf: &[u8]) -> u64;

    /// Reads an unsigned n-bytes integer from `buf`.
    ///
    /// Panics when `nbytes < 1` or `nbytes > 8` or
    /// `buf.len() < nbytes`
    fn read_uint(buf: &[u8], nbytes: usize) -> u64;

    /// Writes an unsigned 16 bit integer `n` to `buf`.
    ///
    /// Panics when `buf.len() < 2`.
    fn write_u16(buf: &mut [u8], n: u16);

    /// Writes an unsigned 32 bit integer `n` to `buf`.
    ///
    /// Panics when `buf.len() < 4`.
    fn write_u32(buf: &mut [u8], n: u32);

    /// Writes an unsigned 64 bit integer `n` to `buf`.
    ///
    /// Panics when `buf.len() < 8`.
    fn write_u64(buf: &mut [u8], n: u64);

    /// Writes an unsigned integer `n` to `buf` using only `nbytes`.
    ///
    /// If `n` is not representable in `nbytes`, or if `nbytes` is `> 8`, then
    /// this method panics.
    fn write_uint(buf: &mut [u8], n: u64, nbytes: usize);

    /// Reads a signed 16 bit integer from `buf`.
    ///
    /// Panics when `buf.len() < 2`.
    #[inline]
    fn read_i16(buf: &[u8]) -> i16 {
        Self::read_u16(buf) as i16
    }

    /// Reads a signed 32 bit integer from `buf`.
    ///
    /// Panics when `buf.len() < 4`.
    #[inline]
    fn read_i32(buf: &[u8]) -> i32 {
        Self::read_u32(buf) as i32
    }

    /// Reads a signed 64 bit integer from `buf`.
    ///
    /// Panics when `buf.len() < 8`.
    #[inline]
    fn read_i64(buf: &[u8]) -> i64 {
        Self::read_u64(buf) as i64
    }

    /// Reads a signed n-bytes integer from `buf`.
    ///
    /// Panics when `nbytes < 1` or `nbytes > 8` or
    /// `buf.len() < nbytes`
    #[inline]
    fn read_int(buf: &[u8], nbytes: usize) -> i64 {
        extend_sign(Self::read_uint(buf, nbytes), nbytes)
    }

    /// Reads a IEEE754 single-precision (4 bytes) floating point number.
    ///
    /// Panics when `buf.len() < 4`.
    #[inline]
    fn read_f32(buf: &[u8]) -> f32 {
        unsafe { transmute(Self::read_u32(buf)) }
    }

    /// Reads a IEEE754 double-precision (8 bytes) floating point number.
    ///
    /// Panics when `buf.len() < 8`.
    #[inline]
    fn read_f64(buf: &[u8]) -> f64 {
        unsafe { transmute(Self::read_u64(buf)) }
    }

    /// Writes a signed 16 bit integer `n` to `buf`.
    ///
    /// Panics when `buf.len() < 2`.
    #[inline]
    fn write_i16(buf: &mut [u8], n: i16) {
        Self::write_u16(buf, n as u16)
    }

    /// Writes a signed 32 bit integer `n` to `buf`.
    ///
    /// Panics when `buf.len() < 4`.
    #[inline]
    fn write_i32(buf: &mut [u8], n: i32) {
        Self::write_u32(buf, n as u32)
    }

    /// Writes a signed 64 bit integer `n` to `buf`.
    ///
    /// Panics when `buf.len() < 8`.
    #[inline]
    fn write_i64(buf: &mut [u8], n: i64) {
        Self::write_u64(buf, n as u64)
    }

    /// Writes a signed integer `n` to `buf` using only `nbytes`.
    ///
    /// If `n` is not representable in `nbytes`, or if `nbytes` is `> 8`, then
    /// this method panics.
    #[inline]
    fn write_int(buf: &mut [u8], n: i64, nbytes: usize) {
        Self::write_uint(buf, unextend_sign(n, nbytes), nbytes)
    }

    /// Writes a IEEE754 single-precision (4 bytes) floating point number.
    ///
    /// Panics when `buf.len() < 4`.
    #[inline]
    fn write_f32(buf: &mut [u8], n: f32) {
        Self::write_u32(buf, unsafe { transmute(n) })
    }

    /// Writes a IEEE754 double-precision (8 bytes) floating point number.
    ///
    /// Panics when `buf.len() < 8`.
    #[inline]
    fn write_f64(buf: &mut [u8], n: f64) {
        Self::write_u64(buf, unsafe { transmute(n) })
    }
}

/// Defines big-endian serialization.
///
/// Note that this type has no value constructor. It is used purely at the
/// type level.
#[allow(missing_copy_implementations)] pub enum BigEndian {}

/// Defines little-endian serialization.
///
/// Note that this type has no value constructor. It is used purely at the
/// type level.
#[allow(missing_copy_implementations)] pub enum LittleEndian {}

/// Defines network byte order serialization.
///
/// Network byte order is defined by [RFC 1700][1] to be big-endian, and is
/// referred to in several protocol specifications.  This type is an alias of
/// BigEndian.
///
/// [1]: https://tools.ietf.org/html/rfc1700
///
/// Note that this type has no value constructor. It is used purely at the
/// type level.
pub type NetworkEndian = BigEndian;

/// Defines system native-endian serialization.
///
/// Note that this type has no value constructor. It is used purely at the
/// type level.
#[cfg(target_endian = "little")]
pub type NativeEndian = LittleEndian;

/// Defines system native-endian serialization.
///
/// Note that this type has no value constructor. It is used purely at the
/// type level.
#[cfg(target_endian = "big")]
pub type NativeEndian = BigEndian;

macro_rules! read_num_bytes {
    ($ty:ty, $size:expr, $src:expr, $which:ident) => ({
        assert!($size == ::core::mem::size_of::<$ty>());
        assert!($size <= $src.len());
        let mut data: $ty = 0;
        unsafe {
            copy_nonoverlapping(
                $src.as_ptr(),
                &mut data as *mut $ty as *mut u8,
                $size);
        }
        data.$which()
    });
}

macro_rules! write_num_bytes {
    ($ty:ty, $size:expr, $n:expr, $dst:expr, $which:ident) => ({
        assert!($size <= $dst.len());
        unsafe {
            // N.B. https://github.com/rust-lang/rust/issues/22776
            let bytes = transmute::<_, [u8; $size]>($n.$which());
            copy_nonoverlapping((&bytes).as_ptr(), $dst.as_mut_ptr(), $size);
        }
    });
}

impl ByteOrder for BigEndian {
    #[inline]
    fn read_u16(buf: &[u8]) -> u16 {
        read_num_bytes!(u16, 2, buf, to_be)
    }

    #[inline]
    fn read_u32(buf: &[u8]) -> u32 {
        read_num_bytes!(u32, 4, buf, to_be)
    }

    #[inline]
    fn read_u64(buf: &[u8]) -> u64 {
        read_num_bytes!(u64, 8, buf, to_be)
    }

    #[inline]
    fn read_uint(buf: &[u8], nbytes: usize) -> u64 {
        assert!(1 <= nbytes && nbytes <= 8 && nbytes <= buf.len());
        let mut out = [0u8; 8];
        let ptr_out = out.as_mut_ptr();
        unsafe {
            copy_nonoverlapping(
                buf.as_ptr(), ptr_out.offset((8 - nbytes) as isize), nbytes);
            (*(ptr_out as *const u64)).to_be()
        }
    }

    #[inline]
    fn write_u16(buf: &mut [u8], n: u16) {
        write_num_bytes!(u16, 2, n, buf, to_be);
    }

    #[inline]
    fn write_u32(buf: &mut [u8], n: u32) {
        write_num_bytes!(u32, 4, n, buf, to_be);
    }

    #[inline]
    fn write_u64(buf: &mut [u8], n: u64) {
        write_num_bytes!(u64, 8, n, buf, to_be);
    }

    #[inline]
    fn write_uint(buf: &mut [u8], n: u64, nbytes: usize) {
        assert!(pack_size(n) <= nbytes && nbytes <= 8);
        assert!(nbytes <= buf.len());
        unsafe {
            let bytes: [u8; 8] = transmute(n.to_be());
            copy_nonoverlapping(
                bytes.as_ptr().offset((8 - nbytes) as isize),
                buf.as_mut_ptr(),
                nbytes);
        }
    }
}

impl ByteOrder for LittleEndian {
    #[inline]
    fn read_u16(buf: &[u8]) -> u16 {
        read_num_bytes!(u16, 2, buf, to_le)
    }

    #[inline]
    fn read_u32(buf: &[u8]) -> u32 {
        read_num_bytes!(u32, 4, buf, to_le)
    }

    #[inline]
    fn read_u64(buf: &[u8]) -> u64 {
        read_num_bytes!(u64, 8, buf, to_le)
    }

    #[inline]
    fn read_uint(buf: &[u8], nbytes: usize) -> u64 {
        assert!(1 <= nbytes && nbytes <= 8 && nbytes <= buf.len());
        let mut out = [0u8; 8];
        let ptr_out = out.as_mut_ptr();
        unsafe {
            copy_nonoverlapping(buf.as_ptr(), ptr_out, nbytes);
            (*(ptr_out as *const u64)).to_le()
        }
    }

    #[inline]
    fn write_u16(buf: &mut [u8], n: u16) {
        write_num_bytes!(u16, 2, n, buf, to_le);
    }

    #[inline]
    fn write_u32(buf: &mut [u8], n: u32) {
        write_num_bytes!(u32, 4, n, buf, to_le);
    }

    #[inline]
    fn write_u64(buf: &mut [u8], n: u64) {
        write_num_bytes!(u64, 8, n, buf, to_le);
    }

    #[inline]
    fn write_uint(buf: &mut [u8], n: u64, nbytes: usize) {
        assert!(pack_size(n as u64) <= nbytes && nbytes <= 8);
        assert!(nbytes <= buf.len());
        unsafe {
            let bytes: [u8; 8] = transmute(n.to_le());
            copy_nonoverlapping(bytes.as_ptr(), buf.as_mut_ptr(), nbytes);
        }
    }
}

#[cfg(test)]
mod test {
    extern crate quickcheck;
    extern crate rand;

    use test::rand::thread_rng;
    use test::quickcheck::{QuickCheck, StdGen, Testable};

    const U64_MAX: u64 = ::std::u64::MAX;
    const I64_MAX: u64 = ::std::i64::MAX as u64;

    fn qc_sized<A: Testable>(f: A, size: u64) {
        QuickCheck::new()
            .gen(StdGen::new(thread_rng(), size as usize))
            .tests(1_00)
            .max_tests(10_000)
            .quickcheck(f);
    }

    macro_rules! qc_byte_order {
        ($name:ident, $ty_int:ident, $max:expr,
         $bytes:expr, $read:ident, $write:ident) => (
            mod $name {
                use {BigEndian, ByteOrder, NativeEndian, LittleEndian};
                use super::qc_sized;

                #[test]
                fn big_endian() {
                    let max = ($max - 1) >> (8 * (8 - $bytes));
                    fn prop(n: $ty_int) -> bool {
                        let mut buf = [0; 8];
                        BigEndian::$write(&mut buf, n, $bytes);
                        n == BigEndian::$read(&mut buf[..$bytes], $bytes)
                    }
                    qc_sized(prop as fn($ty_int) -> bool, max);
                }

                #[test]
                fn little_endian() {
                    let max = ($max - 1) >> (8 * (8 - $bytes));
                    fn prop(n: $ty_int) -> bool {
                        let mut buf = [0; 8];
                        LittleEndian::$write(&mut buf, n, $bytes);
                        n == LittleEndian::$read(&mut buf[..$bytes], $bytes)
                    }
                    qc_sized(prop as fn($ty_int) -> bool, max);
                }

                #[test]
                fn native_endian() {
                    let max = ($max - 1) >> (8 * (8 - $bytes));
                    fn prop(n: $ty_int) -> bool {
                        let mut buf = [0; 8];
                        NativeEndian::$write(&mut buf, n, $bytes);
                        n == NativeEndian::$read(&mut buf[..$bytes], $bytes)
                    }
                    qc_sized(prop as fn($ty_int) -> bool, max);
                }
            }
        );
        ($name:ident, $ty_int:ident, $max:expr,
         $read:ident, $write:ident) => (
            mod $name {
                use std::mem::size_of;
                use {BigEndian, ByteOrder, NativeEndian, LittleEndian};
                use super::qc_sized;

                #[test]
                fn big_endian() {
                    fn prop(n: $ty_int) -> bool {
                        let bytes = size_of::<$ty_int>();
                        let mut buf = [0; 8];
                        BigEndian::$write(&mut buf[8 - bytes..], n);
                        n == BigEndian::$read(&mut buf[8 - bytes..])
                    }
                    qc_sized(prop as fn($ty_int) -> bool, $max - 1);
                }

                #[test]
                fn little_endian() {
                    fn prop(n: $ty_int) -> bool {
                        let bytes = size_of::<$ty_int>();
                        let mut buf = [0; 8];
                        LittleEndian::$write(&mut buf[..bytes], n);
                        n == LittleEndian::$read(&mut buf[..bytes])
                    }
                    qc_sized(prop as fn($ty_int) -> bool, $max - 1);
                }

                #[test]
                fn native_endian() {
                    fn prop(n: $ty_int) -> bool {
                        let bytes = size_of::<$ty_int>();
                        let mut buf = [0; 8];
                        NativeEndian::$write(&mut buf[..bytes], n);
                        n == NativeEndian::$read(&mut buf[..bytes])
                    }
                    qc_sized(prop as fn($ty_int) -> bool, $max - 1);
                }
            }
        );
    }

    qc_byte_order!(prop_u16, u16, ::std::u16::MAX as u64, read_u16, write_u16);
    qc_byte_order!(prop_i16, i16, ::std::i16::MAX as u64, read_i16, write_i16);
    qc_byte_order!(prop_u32, u32, ::std::u32::MAX as u64, read_u32, write_u32);
    qc_byte_order!(prop_i32, i32, ::std::i32::MAX as u64, read_i32, write_i32);
    qc_byte_order!(prop_u64, u64, ::std::u64::MAX as u64, read_u64, write_u64);
    qc_byte_order!(prop_i64, i64, ::std::i64::MAX as u64, read_i64, write_i64);
    qc_byte_order!(prop_f32, f32, ::std::u64::MAX as u64, read_f32, write_f32);
    qc_byte_order!(prop_f64, f64, ::std::i64::MAX as u64, read_f64, write_f64);

    qc_byte_order!(prop_uint_1, u64, super::U64_MAX, 1, read_uint, write_uint);
    qc_byte_order!(prop_uint_2, u64, super::U64_MAX, 2, read_uint, write_uint);
    qc_byte_order!(prop_uint_3, u64, super::U64_MAX, 3, read_uint, write_uint);
    qc_byte_order!(prop_uint_4, u64, super::U64_MAX, 4, read_uint, write_uint);
    qc_byte_order!(prop_uint_5, u64, super::U64_MAX, 5, read_uint, write_uint);
    qc_byte_order!(prop_uint_6, u64, super::U64_MAX, 6, read_uint, write_uint);
    qc_byte_order!(prop_uint_7, u64, super::U64_MAX, 7, read_uint, write_uint);
    qc_byte_order!(prop_uint_8, u64, super::U64_MAX, 8, read_uint, write_uint);

    qc_byte_order!(prop_int_1, i64, super::I64_MAX, 1, read_int, write_int);
    qc_byte_order!(prop_int_2, i64, super::I64_MAX, 2, read_int, write_int);
    qc_byte_order!(prop_int_3, i64, super::I64_MAX, 3, read_int, write_int);
    qc_byte_order!(prop_int_4, i64, super::I64_MAX, 4, read_int, write_int);
    qc_byte_order!(prop_int_5, i64, super::I64_MAX, 5, read_int, write_int);
    qc_byte_order!(prop_int_6, i64, super::I64_MAX, 6, read_int, write_int);
    qc_byte_order!(prop_int_7, i64, super::I64_MAX, 7, read_int, write_int);
    qc_byte_order!(prop_int_8, i64, super::I64_MAX, 8, read_int, write_int);

    macro_rules! qc_bytes_ext {
        ($name:ident, $ty_int:ident, $max:expr,
         $bytes:expr, $read:ident, $write:ident) => (
            mod $name {
                use std::io::Cursor;
                use {
                    ReadBytesExt, WriteBytesExt,
                    BigEndian, NativeEndian, LittleEndian,
                };
                use super::qc_sized;

                #[test]
                fn big_endian() {
                    let max = ($max - 1) >> (8 * (8 - $bytes));
                    fn prop(n: $ty_int) -> bool {
                        let mut wtr = vec![];
                        wtr.$write::<BigEndian>(n).unwrap();
                        let mut rdr = Vec::new();
                        rdr.extend(wtr[8 - $bytes..].iter().map(|&x|x));
                        let mut rdr = Cursor::new(rdr);
                        n == rdr.$read::<BigEndian>($bytes).unwrap()
                    }
                    qc_sized(prop as fn($ty_int) -> bool, max);
                }

                #[test]
                fn little_endian() {
                    let max = ($max - 1) >> (8 * (8 - $bytes));
                    fn prop(n: $ty_int) -> bool {
                        let mut wtr = vec![];
                        wtr.$write::<LittleEndian>(n).unwrap();
                        let mut rdr = Cursor::new(wtr);
                        n == rdr.$read::<LittleEndian>($bytes).unwrap()
                    }
                    qc_sized(prop as fn($ty_int) -> bool, max);
                }

                #[test]
                fn native_endian() {
                    let max = ($max - 1) >> (8 * (8 - $bytes));
                    fn prop(n: $ty_int) -> bool {
                        let mut wtr = vec![];
                        wtr.$write::<NativeEndian>(n).unwrap();
                        let mut rdr = Cursor::new(wtr);
                        n == rdr.$read::<NativeEndian>($bytes).unwrap()
                    }
                    qc_sized(prop as fn($ty_int) -> bool, max);
                }
            }
        );
        ($name:ident, $ty_int:ident, $max:expr, $read:ident, $write:ident) => (
            mod $name {
                use std::io::Cursor;
                use {
                    ReadBytesExt, WriteBytesExt,
                    BigEndian, NativeEndian, LittleEndian,
                };
                use super::qc_sized;

                #[test]
                fn big_endian() {
                    fn prop(n: $ty_int) -> bool {
                        let mut wtr = vec![];
                        wtr.$write::<BigEndian>(n).unwrap();
                        let mut rdr = Cursor::new(wtr);
                        n == rdr.$read::<BigEndian>().unwrap()
                    }
                    qc_sized(prop as fn($ty_int) -> bool, $max - 1);
                }

                #[test]
                fn little_endian() {
                    fn prop(n: $ty_int) -> bool {
                        let mut wtr = vec![];
                        wtr.$write::<LittleEndian>(n).unwrap();
                        let mut rdr = Cursor::new(wtr);
                        n == rdr.$read::<LittleEndian>().unwrap()
                    }
                    qc_sized(prop as fn($ty_int) -> bool, $max - 1);
                }

                #[test]
                fn native_endian() {
                    fn prop(n: $ty_int) -> bool {
                        let mut wtr = vec![];
                        wtr.$write::<NativeEndian>(n).unwrap();
                        let mut rdr = Cursor::new(wtr);
                        n == rdr.$read::<NativeEndian>().unwrap()
                    }
                    qc_sized(prop as fn($ty_int) -> bool, $max - 1);
                }
            }
        );
    }

    qc_bytes_ext!(prop_ext_u16, u16, ::std::u16::MAX as u64, read_u16, write_u16);
    qc_bytes_ext!(prop_ext_i16, i16, ::std::i16::MAX as u64, read_i16, write_i16);
    qc_bytes_ext!(prop_ext_u32, u32, ::std::u32::MAX as u64, read_u32, write_u32);
    qc_bytes_ext!(prop_ext_i32, i32, ::std::i32::MAX as u64, read_i32, write_i32);
    qc_bytes_ext!(prop_ext_u64, u64, ::std::u64::MAX as u64, read_u64, write_u64);
    qc_bytes_ext!(prop_ext_i64, i64, ::std::i64::MAX as u64, read_i64, write_i64);
    qc_bytes_ext!(prop_ext_f32, f32, ::std::u64::MAX as u64, read_f32, write_f32);
    qc_bytes_ext!(prop_ext_f64, f64, ::std::i64::MAX as u64, read_f64, write_f64);

    qc_bytes_ext!(prop_ext_uint_1, u64, super::U64_MAX, 1, read_uint, write_u64);
    qc_bytes_ext!(prop_ext_uint_2, u64, super::U64_MAX, 2, read_uint, write_u64);
    qc_bytes_ext!(prop_ext_uint_3, u64, super::U64_MAX, 3, read_uint, write_u64);
    qc_bytes_ext!(prop_ext_uint_4, u64, super::U64_MAX, 4, read_uint, write_u64);
    qc_bytes_ext!(prop_ext_uint_5, u64, super::U64_MAX, 5, read_uint, write_u64);
    qc_bytes_ext!(prop_ext_uint_6, u64, super::U64_MAX, 6, read_uint, write_u64);
    qc_bytes_ext!(prop_ext_uint_7, u64, super::U64_MAX, 7, read_uint, write_u64);
    qc_bytes_ext!(prop_ext_uint_8, u64, super::U64_MAX, 8, read_uint, write_u64);

    qc_bytes_ext!(prop_ext_int_1, i64, super::I64_MAX, 1, read_int, write_i64);
    qc_bytes_ext!(prop_ext_int_2, i64, super::I64_MAX, 2, read_int, write_i64);
    qc_bytes_ext!(prop_ext_int_3, i64, super::I64_MAX, 3, read_int, write_i64);
    qc_bytes_ext!(prop_ext_int_4, i64, super::I64_MAX, 4, read_int, write_i64);
    qc_bytes_ext!(prop_ext_int_5, i64, super::I64_MAX, 5, read_int, write_i64);
    qc_bytes_ext!(prop_ext_int_6, i64, super::I64_MAX, 6, read_int, write_i64);
    qc_bytes_ext!(prop_ext_int_7, i64, super::I64_MAX, 7, read_int, write_i64);
    qc_bytes_ext!(prop_ext_int_8, i64, super::I64_MAX, 8, read_int, write_i64);

    // Test that all of the byte conversion functions panic when given a
    // buffer that is too small.
    //
    // These tests are critical to ensure safety, otherwise we might end up
    // with a buffer overflow.
    macro_rules! too_small {
        ($name:ident, $maximally_small:expr, $zero:expr,
         $read:ident, $write:ident) => (
            mod $name {
                use {BigEndian, ByteOrder, NativeEndian, LittleEndian};

                #[test]
                #[should_panic]
                fn read_big_endian() {
                    let buf = [0; $maximally_small];
                    BigEndian::$read(&buf);
                }

                #[test]
                #[should_panic]
                fn read_little_endian() {
                    let buf = [0; $maximally_small];
                    LittleEndian::$read(&buf);
                }

                #[test]
                #[should_panic]
                fn read_native_endian() {
                    let buf = [0; $maximally_small];
                    NativeEndian::$read(&buf);
                }

                #[test]
                #[should_panic]
                fn write_big_endian() {
                    let mut buf = [0; $maximally_small];
                    BigEndian::$write(&mut buf, $zero);
                }

                #[test]
                #[should_panic]
                fn write_little_endian() {
                    let mut buf = [0; $maximally_small];
                    LittleEndian::$write(&mut buf, $zero);
                }

                #[test]
                #[should_panic]
                fn write_native_endian() {
                    let mut buf = [0; $maximally_small];
                    NativeEndian::$write(&mut buf, $zero);
                }
            }
        );
        ($name:ident, $maximally_small:expr, $read:ident) => (
            mod $name {
                use {BigEndian, ByteOrder, NativeEndian, LittleEndian};

                #[test]
                #[should_panic]
                fn read_big_endian() {
                    let buf = [0; $maximally_small];
                    BigEndian::$read(&buf, $maximally_small + 1);
                }

                #[test]
                #[should_panic]
                fn read_little_endian() {
                    let buf = [0; $maximally_small];
                    LittleEndian::$read(&buf, $maximally_small + 1);
                }

                #[test]
                #[should_panic]
                fn read_native_endian() {
                    let buf = [0; $maximally_small];
                    NativeEndian::$read(&buf, $maximally_small + 1);
                }
            }
        );
    }

    too_small!(small_u16, 1, 0, read_u16, write_u16);
    too_small!(small_i16, 1, 0, read_i16, write_i16);
    too_small!(small_u32, 3, 0, read_u32, write_u32);
    too_small!(small_i32, 3, 0, read_i32, write_i32);
    too_small!(small_u64, 7, 0, read_u64, write_u64);
    too_small!(small_i64, 7, 0, read_i64, write_i64);
    too_small!(small_f32, 3, 0.0, read_f32, write_f32);
    too_small!(small_f64, 7, 0.0, read_f64, write_f64);

    too_small!(small_uint_1, 1, read_uint);
    too_small!(small_uint_2, 2, read_uint);
    too_small!(small_uint_3, 3, read_uint);
    too_small!(small_uint_4, 4, read_uint);
    too_small!(small_uint_5, 5, read_uint);
    too_small!(small_uint_6, 6, read_uint);
    too_small!(small_uint_7, 7, read_uint);

    too_small!(small_int_1, 1, read_int);
    too_small!(small_int_2, 2, read_int);
    too_small!(small_int_3, 3, read_int);
    too_small!(small_int_4, 4, read_int);
    too_small!(small_int_5, 5, read_int);
    too_small!(small_int_6, 6, read_int);
    too_small!(small_int_7, 7, read_int);

    #[test]
    fn uint_bigger_buffer() {
        use {ByteOrder, LittleEndian};
        let n = LittleEndian::read_uint(&[1, 2, 3, 4, 5, 6, 7, 8], 5);
        assert_eq!(n, 0x0504030201);
    }
}
