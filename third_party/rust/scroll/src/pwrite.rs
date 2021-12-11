use core::result;
use core::ops::{Index, IndexMut, RangeFrom};

use crate::ctx::{TryIntoCtx, MeasureWith};
use crate::error;

/// A very generic, contextual pwrite interface in Rust.
///
/// Like [Pread](trait.Pread.html) — but for writing!
///
/// Implementing `Pwrite` on a data store allows you to then write almost arbitrarily complex types
/// efficiently.
///
/// To this end the Pwrite trait works in conjuction with the [TryIntoCtx](ctx/trait.TryIntoCtx.html);
/// The `TryIntoCtx` trait implemented on a type defines how to convert said type into data that
/// an implementation of Pwrite can … well … write.
///
/// As with [Pread](trait.Pread.html) 'data' does not necessarily mean `&[u8]` but can be any
/// indexable type. In fact much of the documentation of `Pread` applies to `Pwrite` as well just
/// with 'read' switched for 'write' and 'From' switched with 'Into' so if you haven't yet you
/// should read the documentation of `Pread` first.
///
/// Unless you need to implement your own data store — that is either can't convert to `&[u8]` or
/// have a data that is not `&[u8]` — you will probably want to implement
/// [TryIntoCtx](ctx/trait.TryIntoCtx.html) on your Rust types to be written.
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
