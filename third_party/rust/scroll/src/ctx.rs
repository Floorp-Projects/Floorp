//! Generic context-aware conversion traits, for automatic _downstream_ extension of `Pread`, et. al
//!
//! # Discussion
//!
//! Implementors of `TryFromCtx` automatically grant any client user of `pread, pwrite, gread, gwrite` the ability to parse their structure out of the source it has been implemented for, typically `&[u8]`.
//!
//! The implementor only needs to specify the error type, and the type of their size, and then implement the parsing/marshalling logic given a byte sequence, starting at the offset `pread`, et. al was called at, with the context you have implemented it for.
//!
//! Returning the size allows dynamic content (e.g., `&str`s) to be parsed alongside fixed size content (e.g., `u64`). The parsing context is any information you the implementor need to correctly parse out your datatype - this could be the endianness of the type, more offsets, or other complex data. The only requirement is that your `Ctx` be `Copy`, and hence encourages lightweight contexts (but this isn't required of course).
//!
//!
//! # Example
//!
//! Suppose we have a datatype and we want to specify how to parse or serialize this datatype out of some arbitrary
//! byte buffer. In order to do this, we need to provide a `TryFromCtx` impl for our datatype. In particular, if we
//! do this for the `[u8]` target, with a "parsing contex",  `YourCtx`, you will automatically get access to
//! calling `pread_with::<YourDatatype>(offset, your_ctx)` on arrays of bytes.
//!
//! In the example below, we implement `TryFromCtx` using the `Endian` parsing context provided by `scroll`, which is used to specifying the endianness at which numbers should be parsed, but you could provide anything, as long as it implements `Copy`.
//!
//! ```rust
//! use scroll::{self, ctx, Endian, Pread, BE};
//!
//! struct Data<'a> {
//!   name: &'a str,
//!   id: u32,
//! }
//!
//! impl<'a> ctx::TryFromCtx<'a, Endian> for Data<'a> {
//!   type Error = scroll::Error;
//!   type Size = usize;
//!   fn try_from_ctx (src: &'a [u8], ctx: Endian)
//!     -> Result<(Self, Self::Size), Self::Error> {
//!     let name = src.pread::<&str>(0)?;
//!     let id = src.pread_with(name.len() + 1, ctx)?;
//!     Ok((Data { name: name, id: id }, name.len() + 1 + 4))
//!   }
//! }
//!
//! let bytes = b"UserName\x00\x01\x02\x03\x04";
//! let data = bytes.pread_with::<Data>(0, BE).unwrap();
//! assert_eq!(data.id, 0x01020304);
//! assert_eq!(data.name.to_string(), "UserName".to_string());
//!
//! ```

use core::ptr::copy_nonoverlapping;
use core::mem::transmute;
use core::mem::size_of;
use core::str;
use core::result;

#[cfg(feature = "std")]
use std::ffi::{CStr, CString};

use error;
use endian::Endian;

/// A trait for measuring how large something is; for a byte sequence, it will be its length.
pub trait MeasureWith<Ctx> {
    type Units;
    #[inline]
    /// How large is `Self`, given the `ctx`?
    fn measure_with(&self, ctx: &Ctx) -> Self::Units;
}

impl<Ctx> MeasureWith<Ctx> for [u8] {
    type Units = usize;
    #[inline]
    fn measure_with(&self, _ctx: &Ctx) -> Self::Units {
        self.len()
    }
}

impl<Ctx, T: AsRef<[u8]>> MeasureWith<Ctx> for T {
    type Units = usize;
    #[inline]
    fn measure_with(&self, _ctx: &Ctx) -> Self::Units {
        self.as_ref().len()
    }
}

/// The parsing context for converting a byte sequence to a `&str`
///
/// `StrCtx` specifies what byte delimiter to use, and defaults to C-style null terminators. Be careful.
#[derive(Debug, Copy, Clone)]
pub enum StrCtx {
    Delimiter(u8),
    DelimiterUntil(u8, usize),
    Length(usize),
}

/// A C-style, null terminator based delimiter
pub const NULL: u8 = 0;
/// A space-based delimiter
pub const SPACE: u8 = 0x20;
/// A newline-based delimiter
pub const RET: u8 = 0x0a;
/// A tab-based delimiter
pub const TAB: u8 = 0x09;

impl Default for StrCtx {
    #[inline]
    fn default() -> Self {
        StrCtx::Delimiter(NULL)
    }
}

impl StrCtx {
    pub fn len(&self) -> usize {
        match *self {
            StrCtx::Delimiter(_) => 1,
            StrCtx::DelimiterUntil(_, _) => 1,
            StrCtx::Length(_) => 0,
        }
    }
}

/// Reads `Self` from `This` using the context `Ctx`; must _not_ fail
pub trait FromCtx<Ctx: Copy = (), This: ?Sized = [u8]> {
    #[inline]
    fn from_ctx(this: &This, ctx: Ctx) -> Self;
}

/// Tries to read `Self` from `This` using the context `Ctx`
pub trait TryFromCtx<'a, Ctx: Copy = (), This: ?Sized = [u8]> where Self: 'a + Sized {
    type Error;
    type Size;
    #[inline]
    fn try_from_ctx(from: &'a This, ctx: Ctx) -> Result<(Self, Self::Size), Self::Error>;
}

/// Writes `Self` into `This` using the context `Ctx`
pub trait IntoCtx<Ctx: Copy = (), This: ?Sized = [u8]>: Sized {
    fn into_ctx(self, &mut This, ctx: Ctx);
}

/// Tries to write `Self` into `This` using the context `Ctx`
pub trait TryIntoCtx<Ctx: Copy = (), This: ?Sized = [u8]>: Sized {
    type Error;
    type Size;
    fn try_into_ctx(self, &mut This, ctx: Ctx) -> Result<Self::Size, Self::Error>;
}

/// Gets the size of `Self` with a `Ctx`, and in `Self::Units`. Implementors can then call `Gread` related functions
///
/// The rationale behind this trait is to:
///
/// 1. Prevent `gread` from being used, and the offset being modified based on simply the sizeof the value, which can be a misnomer, e.g., for Leb128, etc.
/// 2. Allow a context based size, which is useful for 32/64 bit variants for various containers, etc.
pub trait SizeWith<Ctx = ()> {
    type Units;
    #[inline]
    fn size_with(ctx: &Ctx) -> Self::Units;
}

macro_rules! signed_to_unsigned {
    (i8) =>  {u8 };
    (u8) =>  {u8 };
    (i16) => {u16};
    (u16) => {u16};
    (i32) => {u32};
    (u32) => {u32};
    (i64) => {u64};
    (u64) => {u64};
    (i128) => {u128};
    (u128) => {u128};
    (f32) => {u32};
    (f64) => {u64};
}

macro_rules! write_into {
    ($typ:ty, $size:expr, $n:expr, $dst:expr, $endian:expr) => ({
        unsafe {
            assert!($dst.len() >= $size);
            let bytes = transmute::<$typ, [u8; $size]>(if $endian.is_little() { $n.to_le() } else { $n.to_be() });
            copy_nonoverlapping((&bytes).as_ptr(), $dst.as_mut_ptr(), $size);
        }
    });
}

macro_rules! into_ctx_impl {
    ($typ:tt, $size:expr) => {
        impl IntoCtx<Endian> for $typ {
            #[inline]
            fn into_ctx(self, dst: &mut [u8], le: Endian) {
                assert!(dst.len() >= $size);
                write_into!($typ, $size, self, dst, le);
            }
        }
        impl<'a> IntoCtx<Endian> for &'a $typ {
            #[inline]
            fn into_ctx(self, dst: &mut [u8], le: Endian) {
                (*self).into_ctx(dst, le)
            }
        }
        impl TryIntoCtx<Endian> for $typ where $typ: IntoCtx<Endian> {
            type Error = error::Error;
            type Size = usize;
            #[inline]
            fn try_into_ctx(self, dst: &mut [u8], le: Endian) -> error::Result<Self::Size> {
                if $size > dst.len () {
                    Err(error::Error::TooBig{size: $size, len: dst.len()})
                } else {
                    <$typ as IntoCtx<Endian>>::into_ctx(self, dst, le);
                    Ok($size)
                }
            }
        }
        impl<'a> TryIntoCtx<Endian> for &'a $typ {
            type Error = error::Error;
            type Size = usize;
            #[inline]
            fn try_into_ctx(self, dst: &mut [u8], le: Endian) -> error::Result<Self::Size> {
                (*self).try_into_ctx(dst, le)
            }
        }
    }
}

macro_rules! from_ctx_impl {
    ($typ:tt, $size:expr) => {
        impl<'a> FromCtx<Endian> for $typ {
            #[inline]
            fn from_ctx(src: &[u8], le: Endian) -> Self {
                assert!(src.len() >= $size);
                let mut data: signed_to_unsigned!($typ) = 0;
                unsafe {
                    copy_nonoverlapping(
                        src.as_ptr(),
                        &mut data as *mut signed_to_unsigned!($typ) as *mut u8,
                        $size);
                }
                (if le.is_little() { data.to_le() } else { data.to_be() }) as $typ
            }
        }

        impl<'a> TryFromCtx<'a, Endian> for $typ where $typ: FromCtx<Endian> {
            type Error = error::Error<usize>;
            type Size = usize;
            #[inline]
            fn try_from_ctx(src: &'a [u8], le: Endian) -> result::Result<(Self, Self::Size), Self::Error> {
                if $size > src.len () {
                    Err(error::Error::TooBig{size: $size, len: src.len()})
                } else {
                    Ok((FromCtx::from_ctx(&src, le), $size))
                }
            }
        }
        // as ref
        impl<'a, T> FromCtx<Endian, T> for $typ where T: AsRef<[u8]> {
            #[inline]
            fn from_ctx(src: &T, le: Endian) -> Self {
                let src = src.as_ref();
                assert!(src.len() >= $size);
                let mut data: signed_to_unsigned!($typ) = 0;
                unsafe {
                    copy_nonoverlapping(
                        src.as_ptr(),
                        &mut data as *mut signed_to_unsigned!($typ) as *mut u8,
                        $size);
                }
                (if le.is_little() { data.to_le() } else { data.to_be() }) as $typ
            }
        }

        impl<'a, T> TryFromCtx<'a, Endian, T> for $typ where $typ: FromCtx<Endian, T>, T: AsRef<[u8]> {
            type Error = error::Error;
            type Size = usize;
            #[inline]
            fn try_from_ctx(src: &'a T, le: Endian) -> result::Result<(Self, Self::Size), Self::Error> {
                let src = src.as_ref();
                Self::try_from_ctx(src, le)
            }
        }
    };
}

macro_rules! ctx_impl {
    ($typ:tt, $size:expr) => {
        from_ctx_impl!($typ, $size);
     };
}

ctx_impl!(u8,  1);
ctx_impl!(i8,  1);
ctx_impl!(u16, 2);
ctx_impl!(i16, 2);
ctx_impl!(u32, 4);
ctx_impl!(i32, 4);
ctx_impl!(u64, 8);
ctx_impl!(i64, 8);
#[cfg(rust_1_26)]
ctx_impl!(u128, 16);
#[cfg(rust_1_26)]
ctx_impl!(i128, 16);

macro_rules! from_ctx_float_impl {
    ($typ:tt, $size:expr) => {
        impl<'a> FromCtx<Endian> for $typ {
            #[inline]
            fn from_ctx(src: &[u8], le: Endian) -> Self {
                assert!(src.len() >= ::core::mem::size_of::<Self>());
                let mut data: signed_to_unsigned!($typ) = 0;
                unsafe {
                    copy_nonoverlapping(
                        src.as_ptr(),
                        &mut data as *mut signed_to_unsigned!($typ) as *mut u8,
                        $size);
                    transmute(if le.is_little() { data.to_le() } else { data.to_be() })
                }
            }
        }
        impl<'a> TryFromCtx<'a, Endian> for $typ where $typ: FromCtx<Endian> {
            type Error = error::Error;
            type Size = usize;
            #[inline]
            fn try_from_ctx(src: &'a [u8], le: Endian) -> result::Result<(Self, Self::Size), Self::Error> {
                if $size > src.len () {
                    Err(error::Error::TooBig{size: $size, len: src.len()})
                } else {
                    Ok((FromCtx::from_ctx(src, le), $size))
                }
            }
        }
    }
}

from_ctx_float_impl!(f32, 4);
from_ctx_float_impl!(f64, 8);

into_ctx_impl!(u8,  1);
into_ctx_impl!(i8,  1);
into_ctx_impl!(u16, 2);
into_ctx_impl!(i16, 2);
into_ctx_impl!(u32, 4);
into_ctx_impl!(i32, 4);
into_ctx_impl!(u64, 8);
into_ctx_impl!(i64, 8);
#[cfg(rust_1_26)]
into_ctx_impl!(u128, 16);
#[cfg(rust_1_26)]
into_ctx_impl!(i128, 16);

macro_rules! into_ctx_float_impl {
    ($typ:tt, $size:expr) => {
        impl IntoCtx<Endian> for $typ {
            #[inline]
            fn into_ctx(self, dst: &mut [u8], le: Endian) {
                assert!(dst.len() >= $size);
                write_into!(signed_to_unsigned!($typ), $size, transmute::<$typ, signed_to_unsigned!($typ)>(self), dst, le);
            }
        }
        impl<'a> IntoCtx<Endian> for &'a $typ {
            #[inline]
            fn into_ctx(self, dst: &mut [u8], le: Endian) {
                (*self).into_ctx(dst, le)
            }
        }
        impl TryIntoCtx<Endian> for $typ where $typ: IntoCtx<Endian> {
            type Error = error::Error;
            type Size = usize;
            #[inline]
            fn try_into_ctx(self, dst: &mut [u8], le: Endian) -> error::Result<Self::Size> {
                if $size > dst.len () {
                    Err(error::Error::TooBig{size: $size, len: dst.len()})
                } else {
                    <$typ as IntoCtx<Endian>>::into_ctx(self, dst, le);
                    Ok($size)
                }
            }
        }
        impl<'a> TryIntoCtx<Endian> for &'a $typ {
            type Error = error::Error;
            type Size = usize;
            #[inline]
            fn try_into_ctx(self, dst: &mut [u8], le: Endian) -> error::Result<Self::Size> {
                (*self).try_into_ctx(dst, le)
            }
        }
    }
}

into_ctx_float_impl!(f32, 4);
into_ctx_float_impl!(f64, 8);

impl<'a> TryFromCtx<'a, StrCtx> for &'a str {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    /// Read a `&str` from `src` using `delimiter`
    fn try_from_ctx(src: &'a [u8], ctx: StrCtx) -> Result<(Self, Self::Size), Self::Error> {
        let len = match ctx {
            StrCtx::Length(len) => len,
            StrCtx::Delimiter(delimiter) => src.iter().take_while(|c| **c != delimiter).count(),
            StrCtx::DelimiterUntil(delimiter, len) => {
                if len > src.len() {
                    return Err(error::Error::TooBig{size: len, len: src.len()});
                };
                src
                    .iter()
                    .take_while(|c| **c != delimiter)
                    .take(len)
                    .count()
            }
        };

        if len > src.len() {
            return Err(error::Error::TooBig{size: len, len: src.len()});
        };

        match str::from_utf8(&src[..len]) {
            Ok(res) => Ok((res, len + ctx.len())),
            Err(_) => Err(error::Error::BadInput{size: src.len(), msg: "invalid utf8"})
        }
    }
}

impl<'a, T> TryFromCtx<'a, StrCtx, T> for &'a str where T: AsRef<[u8]> {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_from_ctx(src: &'a T, ctx: StrCtx) -> result::Result<(Self, Self::Size), Self::Error> {
        let src = src.as_ref();
        TryFromCtx::try_from_ctx(src, ctx)
    }
}

impl<'a> TryIntoCtx for &'a [u8] {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_into_ctx(self, dst: &mut [u8], _ctx: ()) -> error::Result<Self::Size> {
        let src_len = self.len() as isize;
        let dst_len = dst.len() as isize;
        // if src_len < 0 || dst_len < 0 || offset < 0 {
        //     return Err(error::Error::BadOffset(format!("requested operation has negative casts: src len: {} dst len: {} offset: {}", src_len, dst_len, offset)).into())
        // }
        if src_len > dst_len {
            Err(error::Error::TooBig{ size: self.len(), len: dst.len()})
        } else {
            unsafe { copy_nonoverlapping(self.as_ptr(), dst.as_mut_ptr(), src_len as usize) };
            Ok(self.len())
        }
    }
}

// TODO: make TryIntoCtx use StrCtx for awesomeness
impl<'a> TryIntoCtx for &'a str {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_into_ctx(self, dst: &mut [u8], ctx: ()) -> error::Result<Self::Size> {
        let bytes = self.as_bytes();
        TryIntoCtx::try_into_ctx(bytes, dst, ctx)
    }
}

// TODO: we can make this compile time without size_of call, but compiler probably does that anyway
macro_rules! sizeof_impl {
    ($ty:ty) => {
        impl SizeWith<Endian> for $ty {
            type Units = usize;
            #[inline]
            fn size_with(_ctx: &Endian) -> usize {
                size_of::<$ty>()
            }
        }
    }
}

sizeof_impl!(u8);
sizeof_impl!(i8);
sizeof_impl!(u16);
sizeof_impl!(i16);
sizeof_impl!(u32);
sizeof_impl!(i32);
sizeof_impl!(u64);
sizeof_impl!(i64);
#[cfg(rust_1_26)]
sizeof_impl!(u128);
#[cfg(rust_1_26)]
sizeof_impl!(i128);
sizeof_impl!(f32);
sizeof_impl!(f64);
sizeof_impl!(usize);
sizeof_impl!(isize);

impl FromCtx<Endian> for usize {
    #[inline]
    fn from_ctx(src: &[u8], le: Endian) -> Self {
        let size = ::core::mem::size_of::<Self>();
        assert!(src.len() >= size);
        let mut data: usize = 0;
        unsafe {
            copy_nonoverlapping(
                src.as_ptr(),
                &mut data as *mut usize as *mut u8,
                size);
            transmute(if le.is_little() { data.to_le() } else { data.to_be() })
        }
    }
}

impl<'a> TryFromCtx<'a, Endian> for usize where usize: FromCtx<Endian> {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_from_ctx(src: &'a [u8], le: Endian) -> result::Result<(Self, Self::Size), Self::Error> {
        let size = ::core::mem::size_of::<usize>();
        if size > src.len () {
            Err(error::Error::TooBig{size: size, len: src.len()})
        } else {
            Ok((FromCtx::from_ctx(src, le), size))
        }
    }
}

impl<'a> TryFromCtx<'a, usize> for &'a[u8] {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_from_ctx(src: &'a [u8], size: usize) -> result::Result<(Self, Self::Size), Self::Error> {
        if size > src.len () {
            Err(error::Error::TooBig{size: size, len: src.len()})
        } else {
            Ok((&src[..size], size))
        }
    }
}

impl IntoCtx<Endian> for usize {
    #[inline]
    fn into_ctx(self, dst: &mut [u8], le: Endian) {
        let size = ::core::mem::size_of::<Self>();
        assert!(dst.len() >= size);
        let mut data = if le.is_little() { self.to_le() } else { self.to_be() };
        let data = &mut data as *mut usize as *mut u8;
        unsafe {
            copy_nonoverlapping(data, dst.as_mut_ptr(), size);
        }
    }
}

impl TryIntoCtx<Endian> for usize where usize: IntoCtx<Endian> {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_into_ctx(self, dst: &mut [u8], le: Endian) -> error::Result<Self::Size> {
        let size = ::core::mem::size_of::<usize>();
        if size > dst.len() {
            Err(error::Error::TooBig{size: size, len: dst.len()})
        } else {
            <usize as IntoCtx<Endian>>::into_ctx(self, dst, le);
            Ok(size)
        }
    }
}

#[cfg(feature = "std")]
impl<'a> TryFromCtx<'a> for &'a CStr {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_from_ctx(src: &'a [u8], _ctx: ()) -> result::Result<(Self, Self::Size), Self::Error> {
        let null_byte = match src.iter().position(|b| *b == 0) {
            Some(ix) => ix,
            None => return Err(error::Error::BadInput {
                size: 0,
                msg: "The input doesn't contain a null byte",
            })
        };

        let cstr = unsafe { CStr::from_bytes_with_nul_unchecked(&src[..null_byte+1]) };
        Ok((cstr, null_byte+1))
    }
}

#[cfg(feature = "std")]
impl<'a> TryFromCtx<'a> for CString {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_from_ctx(src: &'a [u8], _ctx: ()) -> result::Result<(Self, Self::Size), Self::Error> {
        let (raw, bytes_read) = <&CStr as TryFromCtx>::try_from_ctx(src, _ctx)?;
        Ok((raw.to_owned(), bytes_read))
    }
}

#[cfg(feature = "std")]
impl<'a> TryIntoCtx for &'a CStr {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_into_ctx(self, dst: &mut [u8], _ctx: ()) -> error::Result<Self::Size> {
        let data = self.to_bytes_with_nul();

        if dst.len() < data.len() {
            Err(error::Error::TooBig {
                size: dst.len(),
                len: data.len(),
            })
        } else {
            unsafe {
                copy_nonoverlapping(data.as_ptr(), dst.as_mut_ptr(), data.len());
            }

            Ok(data.len())
        }
    }
}

#[cfg(feature = "std")]
impl TryIntoCtx for CString {
    type Error = error::Error;
    type Size = usize;
    #[inline]
    fn try_into_ctx(self, dst: &mut [u8], _ctx: ()) -> error::Result<Self::Size> {
        self.as_c_str().try_into_ctx(dst, _ctx)
    }
}


// example of marshalling to bytes, let's wait until const is an option
// impl FromCtx for [u8; 10] {
//     fn from_ctx(bytes: &[u8], _ctx: Endian) -> Self {
//         let mut dst: Self = [0; 10];
//         assert!(bytes.len() >= dst.len());
//         unsafe {
//             copy_nonoverlapping(bytes.as_ptr(), dst.as_mut_ptr(), dst.len());
//         }
//         dst
//     }
// }

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[cfg(feature = "std")]
    fn parse_a_cstr() {
        let src = CString::new("Hello World").unwrap();
        let as_bytes = src.as_bytes_with_nul();

        let (got, bytes_read) = <&CStr as TryFromCtx>::try_from_ctx(as_bytes, ()).unwrap();

        assert_eq!(bytes_read, as_bytes.len());
        assert_eq!(got, src.as_c_str());
    }

    #[test]
    #[cfg(feature = "std")]
    fn round_trip_a_c_str() {
        let src = CString::new("Hello World").unwrap();
        let src = src.as_c_str();
        let as_bytes = src.to_bytes_with_nul();

        let mut buffer = vec![0; as_bytes.len()];
        let bytes_written = src.try_into_ctx(&mut buffer, ()).unwrap();
        assert_eq!(bytes_written, as_bytes.len());

        let (got, bytes_read) = <&CStr as TryFromCtx>::try_from_ctx(&buffer, ()).unwrap();

        assert_eq!(bytes_read, as_bytes.len());
        assert_eq!(got, src);
    }
}


