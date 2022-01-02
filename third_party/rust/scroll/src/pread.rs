use core::result;
use core::ops::{Index, RangeFrom};

use crate::ctx::{TryFromCtx, MeasureWith};
use crate::error;

/// A very generic, contextual pread interface in Rust. 
///
/// Like [Pwrite](trait.Pwrite.html) — but for reading!
///
/// Implementing `Pread` on a data store allows you to then read almost arbitrarily complex types
/// efficiently.
///
/// To this end the Pread trait works in conjuction with the [TryFromCtx](ctx/trait.TryFromCtx.html);
/// The `TryFromCtx` trait implemented on a type defines how to convert data to an object of that
/// type, the Pread trait implemented on a data store defines how to extract said data from that
/// store.
///
/// It should be noted though that in this context, data does not necessarily mean `&[u8]` —
/// `Pread` and `TryFromCtx` are generic over what 'data' means and could be implemented instead
/// over chunks of memory or any other indexable type — but scroll does come with a set of powerful
/// blanket implementations for data being a continous block of byte-addressable memory.
///
/// Pread provides two main groups of functions: pread and gread.
///
/// `pread` is the basic function that simply extracts a given type from a given data store - either
/// using a provided Context in the case of [pread_with](trait.Pread.html#method.pread_with) or
/// with the default context for the given type in the case of [pread](trait.Pread.html#method.pread)
///
/// `gread` does in addition to that update the offset it's currently at, allowing for a cursored
/// read — `gread_inout` expands on that and reads a number of continous types from the data store.
/// gread again comes with `_with` variants to allow using a specific context.
///
/// Since pread and friends are very generic functions their types are rather complex, but very
/// much understandable; `TryFromCtx` is generic over `Ctx` ([described
/// here](ctx/index.html#context)), `Output` and `Error`. The Error type is hopefully
/// self-explanatory, however the `Output` type is rather important; it defines what Pread extracts
/// from the data store and has to match up with what `TryFromCtx` expects as input to convert into
/// the resulting type. scroll defaults to `&[u8]` here.
///
/// Unless you need to implement your own data store — that is either can't convert to `&[u8]` or
/// have a data that is not `&[u8]` — you will probably want to implement
/// [TryFromCtx](ctx/trait.TryFromCtx.html) on your Rust types to be extracted.
///
pub trait Pread<Ctx, E> : Index<usize> + Index<RangeFrom<usize>> + MeasureWith<Ctx>
 where
       Ctx: Copy,
       E: From<error::Error>,
{
    #[inline]
    /// Reads a value from `self` at `offset` with a default `Ctx`. For the primitive numeric values, this will read at the machine's endianness.
    /// # Example
    /// ```rust
    /// use scroll::Pread;
    /// let bytes = [0x7fu8; 0x01];
    /// let byte = bytes.pread::<u8>(0).unwrap();
    fn pread<'a, N: TryFromCtx<'a, Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>>(&'a self, offset: usize) -> result::Result<N, E> where <Self as Index<RangeFrom<usize>>>::Output: 'a, Ctx: Default {
        self.pread_with(offset, Ctx::default())
    }
    #[inline]
    /// Reads a value from `self` at `offset` with the given `ctx`
    /// # Example
    /// ```rust
    /// use scroll::Pread;
    /// let bytes: [u8; 2] = [0xde, 0xad];
    /// let dead: u16 = bytes.pread_with(0, scroll::BE).unwrap();
    /// assert_eq!(dead, 0xdeadu16);
    fn pread_with<'a, N: TryFromCtx<'a, Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>>(&'a self, offset: usize, ctx: Ctx) -> result::Result<N, E> where <Self as Index<RangeFrom<usize>>>::Output: 'a {
        let len = self.measure_with(&ctx);
        if offset >= len {
            return Err(error::Error::BadOffset(offset).into())
        }
        N::try_from_ctx(&self[offset..], ctx).and_then(|(n, _)| Ok(n))
    }
    #[inline]
    /// Reads a value from `self` at `offset` with a default `Ctx`. For the primitive numeric values, this will read at the machine's endianness. Updates the offset
    /// # Example
    /// ```rust
    /// use scroll::Pread;
    /// let offset = &mut 0;
    /// let bytes = [0x7fu8; 0x01];
    /// let byte = bytes.gread::<u8>(offset).unwrap();
    /// assert_eq!(*offset, 1);
    fn gread<'a, N: TryFromCtx<'a, Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>>(&'a self, offset: &mut usize) -> result::Result<N, E> where Ctx: Default, <Self as Index<RangeFrom<usize>>>::Output: 'a {
        let ctx = Ctx::default();
        self.gread_with(offset, ctx)
    }
    /// Reads a value from `self` at `offset` with the given `ctx`, and updates the offset.
    /// # Example
    /// ```rust
    /// use scroll::Pread;
    /// let offset = &mut 0;
    /// let bytes: [u8; 2] = [0xde, 0xad];
    /// let dead: u16 = bytes.gread_with(offset, scroll::BE).unwrap();
    /// assert_eq!(dead, 0xdeadu16);
    /// assert_eq!(*offset, 2);
    #[inline]
    fn gread_with<'a, N: TryFromCtx<'a, Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>>
        (&'a self, offset: &mut usize, ctx: Ctx) ->
        result::Result<N, E>
        where <Self as Index<RangeFrom<usize>>>::Output: 'a
    {
        let o = *offset;
        // self.pread_with(o, ctx).and_then(|(n, size)| {
        //     *offset += size;
        //     Ok(n)
        // })
        let len = self.measure_with(&ctx);
        if o >= len {
            return Err(error::Error::BadOffset(o).into())
        }
        N::try_from_ctx(&self[o..], ctx).and_then(|(n, size)| {
            *offset += size;
            Ok(n)
        })
    }

    /// Trys to write `inout.len()` `N`s into `inout` from `Self` starting at `offset`, using the default context for `N`, and updates the offset.
    /// # Example
    /// ```rust
    /// use scroll::Pread;
    /// let mut bytes: Vec<u8> = vec![0, 0];
    /// let offset = &mut 0;
    /// let bytes_from: [u8; 2] = [0x48, 0x49];
    /// bytes_from.gread_inout(offset, &mut bytes).unwrap();
    /// assert_eq!(&bytes, &bytes_from);
    /// assert_eq!(*offset, 2);
    #[inline]
    fn gread_inout<'a, N>(&'a self, offset: &mut usize, inout: &mut [N]) -> result::Result<(), E>
        where
        N: TryFromCtx<'a, Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>,
    Ctx: Default,
    <Self as Index<RangeFrom<usize>>>::Output: 'a
    {
        for i in inout.iter_mut() {
            *i = self.gread(offset)?;
        }
        Ok(())
    }

    /// Trys to write `inout.len()` `N`s into `inout` from `Self` starting at `offset`, using the context `ctx`
    /// # Example
    /// ```rust
    /// use scroll::{ctx, LE, Pread};
    /// let mut bytes: Vec<u8> = vec![0, 0];
    /// let offset = &mut 0;
    /// let bytes_from: [u8; 2] = [0x48, 0x49];
    /// bytes_from.gread_inout_with(offset, &mut bytes, LE).unwrap();
    /// assert_eq!(&bytes, &bytes_from);
    /// assert_eq!(*offset, 2);
    #[inline]
    fn gread_inout_with<'a, N>(&'a self, offset: &mut usize, inout: &mut [N], ctx: Ctx) -> result::Result<(), E>
        where
        N: TryFromCtx<'a, Ctx, <Self as Index<RangeFrom<usize>>>::Output, Error = E>,
    <Self as Index<RangeFrom<usize>>>::Output: 'a
    {
        for i in inout.iter_mut() {
            *i = self.gread_with(offset, ctx)?;
        }
        Ok(())
    }
}

impl<Ctx: Copy,
     E: From<error::Error>,
     R: ?Sized + Index<usize> + Index<RangeFrom<usize>> + MeasureWith<Ctx>>
    Pread<Ctx, E> for R {}
