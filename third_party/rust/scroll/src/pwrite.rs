use core::result;
use core::ops::{Index, IndexMut, RangeFrom};

use crate::ctx::{TryIntoCtx, MeasureWith};
use crate::error;

/// Writes into `Self` at an offset of type `I` using a `Ctx`
///
/// To implement writing into an arbitrary byte buffer, implement `TryIntoCtx`
/// # Example
/// ```rust
/// use scroll::{self, ctx, LE, Endian, Pwrite};
/// #[derive(Debug, PartialEq, Eq)]
/// pub struct Foo(u16);
///
/// // this will use the default `DefaultCtx = scroll::Endian` and `I = usize`...
/// impl ctx::TryIntoCtx<Endian> for Foo {
///     // you can use your own error here too, but you will then need to specify it in fn generic parameters
///     type Error = scroll::Error;
///     // you can write using your own context too... see `leb128.rs`
///     fn try_into_ctx(self, this: &mut [u8], le: Endian) -> Result<usize, Self::Error> {
///         if this.len() < 2 { return Err((scroll::Error::Custom("whatever".to_string())).into()) }
///         this.pwrite_with(self.0, 0, le)?;
///         Ok(2)
///     }
/// }
/// // now we can write a `Foo` into some buffer (in this case, a byte buffer, because that's what we implemented it for above)
///
/// let mut bytes: [u8; 4] = [0, 0, 0, 0];
/// bytes.pwrite_with(Foo(0x7f), 1, LE).unwrap();
///
pub trait Pwrite<Ctx, E> : Index<usize> + IndexMut<RangeFrom<usize>> + MeasureWith<Ctx>
 where
       Ctx: Copy,
       E: From<error::Error>,
{
    fn pwrite<N: TryIntoCtx<Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>>(&mut self, n: N, offset: usize) -> result::Result<usize, E> where Ctx: Default {
        self.pwrite_with(n, offset, Ctx::default())
    }
    /// Write `N` at offset `I` with context `Ctx`
    /// # Example
    /// ```
    /// use scroll::{Pwrite, Pread, LE};
    /// let mut bytes: [u8; 8] = [0, 0, 0, 0, 0, 0, 0, 0];
    /// bytes.pwrite_with::<u32>(0xbeefbeef, 0, LE).unwrap();
    /// assert_eq!(bytes.pread_with::<u32>(0, LE).unwrap(), 0xbeefbeef);
    fn pwrite_with<N: TryIntoCtx<Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>>(&mut self, n: N, offset: usize, ctx: Ctx) -> result::Result<usize, E> {
        let len = self.measure_with(&ctx);
        if offset >= len {
            return Err(error::Error::BadOffset(offset).into())
        }
        let dst = &mut self[offset..];
        n.try_into_ctx(dst, ctx)
    }
    /// Write `n` into `self` at `offset`, with a default `Ctx`. Updates the offset.
    #[inline]
    fn gwrite<N: TryIntoCtx<Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>>(&mut self, n: N, offset: &mut usize) -> result::Result<usize, E> where
        Ctx: Default {
        let ctx = Ctx::default();
        self.gwrite_with(n, offset, ctx)
    }
    /// Write `n` into `self` at `offset`, with the `ctx`. Updates the offset.
    #[inline]
    fn gwrite_with<N: TryIntoCtx<Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>>(&mut self, n: N, offset: &mut usize, ctx: Ctx) -> result::Result<usize, E> {
        let o = *offset;
        match self.pwrite_with(n, o, ctx) {
            Ok(size) => {
                *offset += size;
                Ok(size)
            },
            err => err
        }
    }
}

impl<Ctx: Copy,
     E: From<error::Error>,
     R: ?Sized + Index<usize> + IndexMut<RangeFrom<usize>> + MeasureWith<Ctx>>
    Pwrite<Ctx, E> for R {}
