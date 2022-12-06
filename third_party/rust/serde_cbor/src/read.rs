#[cfg(feature = "alloc")]
use alloc::{vec, vec::Vec};
#[cfg(feature = "std")]
use core::cmp;
use core::mem;

#[cfg(feature = "std")]
use std::io::{self, Read as StdRead};

use crate::error::{Error, ErrorCode, Result};

#[cfg(not(feature = "unsealed_read_write"))]
/// Trait used by the deserializer for iterating over input.
///
/// This trait is sealed by default, enabling the `unsealed_read_write` feature removes this bound
/// to allow objects outside of this crate to implement this trait.
pub trait Read<'de>: private::Sealed {
    #[doc(hidden)]
    /// Read n bytes from the input.
    ///
    /// Implementations that can are asked to return a slice with a Long lifetime that outlives the
    /// decoder, but others (eg. ones that need to allocate the data into a temporary buffer) can
    /// return it with a Short lifetime that just lives for the time of read's mutable borrow of
    /// the reader.
    ///
    /// This may, as a side effect, clear the reader's scratch buffer (as the provided
    /// implementation does).

    // A more appropriate lifetime setup for this (that would allow the Deserializer::convert_str
    // to stay a function) would be something like `fn read<'a, 'r: 'a>(&'a mut 'r immut self, ...) -> ...
    // EitherLifetime<'r, 'de>>`, which borrows self mutably for the duration of the function and
    // downgrates that reference to an immutable one that outlives the result (protecting the
    // scratch buffer from changes), but alas, that can't be expressed (yet?).
    fn read<'a>(&'a mut self, n: usize) -> Result<EitherLifetime<'a, 'de>> {
        self.clear_buffer();
        self.read_to_buffer(n)?;

        Ok(self.take_buffer())
    }

    #[doc(hidden)]
    fn next(&mut self) -> Result<Option<u8>>;

    #[doc(hidden)]
    fn peek(&mut self) -> Result<Option<u8>>;

    #[doc(hidden)]
    fn clear_buffer(&mut self);

    #[doc(hidden)]
    fn read_to_buffer(&mut self, n: usize) -> Result<()>;

    #[doc(hidden)]
    fn take_buffer<'a>(&'a mut self) -> EitherLifetime<'a, 'de>;

    #[doc(hidden)]
    fn read_into(&mut self, buf: &mut [u8]) -> Result<()>;

    #[doc(hidden)]
    fn discard(&mut self);

    #[doc(hidden)]
    fn offset(&self) -> u64;
}

#[cfg(feature = "unsealed_read_write")]
/// Trait used by the deserializer for iterating over input.
pub trait Read<'de> {
    /// Read n bytes from the input.
    ///
    /// Implementations that can are asked to return a slice with a Long lifetime that outlives the
    /// decoder, but others (eg. ones that need to allocate the data into a temporary buffer) can
    /// return it with a Short lifetime that just lives for the time of read's mutable borrow of
    /// the reader.
    ///
    /// This may, as a side effect, clear the reader's scratch buffer (as the provided
    /// implementation does).

    // A more appropriate lifetime setup for this (that would allow the Deserializer::convert_str
    // to stay a function) would be something like `fn read<'a, 'r: 'a>(&'a mut 'r immut self, ...) -> ...
    // EitherLifetime<'r, 'de>>`, which borrows self mutably for the duration of the function and
    // downgrates that reference to an immutable one that outlives the result (protecting the
    // scratch buffer from changes), but alas, that can't be expressed (yet?).
    fn read<'a>(&'a mut self, n: usize) -> Result<EitherLifetime<'a, 'de>> {
        self.clear_buffer();
        self.read_to_buffer(n)?;

        Ok(self.take_buffer())
    }

    /// Read the next byte from the input, if any.
    fn next(&mut self) -> Result<Option<u8>>;

    /// Peek at the next byte of the input, if any. This does not advance the reader, so the result
    /// of this function will remain the same until a read or clear occurs.
    fn peek(&mut self) -> Result<Option<u8>>;

    /// Clear the underlying scratch buffer
    fn clear_buffer(&mut self);

    /// Append n bytes from the reader to the reader's scratch buffer (without clearing it)
    fn read_to_buffer(&mut self, n: usize) -> Result<()>;

    /// Read out everything accumulated in the reader's scratch buffer. This may, as a side effect,
    /// clear it.
    fn take_buffer<'a>(&'a mut self) -> EitherLifetime<'a, 'de>;

    /// Read from the input until `buf` is full or end of input is encountered.
    fn read_into(&mut self, buf: &mut [u8]) -> Result<()>;

    /// Discard any data read by `peek`.
    fn discard(&mut self);

    /// Returns the offset from the start of the reader.
    fn offset(&self) -> u64;
}

/// Represents a reader that can return its current position
pub trait Offset {
    fn byte_offset(&self) -> usize;
}

/// Represents a buffer with one of two lifetimes.
pub enum EitherLifetime<'short, 'long> {
    /// The short lifetime
    Short(&'short [u8]),
    /// The long lifetime
    Long(&'long [u8]),
}

#[cfg(not(feature = "unsealed_read_write"))]
mod private {
    pub trait Sealed {}
}

/// CBOR input source that reads from a std::io input stream.
#[cfg(feature = "std")]
#[derive(Debug)]
pub struct IoRead<R>
where
    R: io::Read,
{
    reader: OffsetReader<R>,
    scratch: Vec<u8>,
    ch: Option<u8>,
}

#[cfg(feature = "std")]
impl<R> IoRead<R>
where
    R: io::Read,
{
    /// Creates a new CBOR input source to read from a std::io input stream.
    pub fn new(reader: R) -> IoRead<R> {
        IoRead {
            reader: OffsetReader { reader, offset: 0 },
            scratch: vec![],
            ch: None,
        }
    }

    #[inline]
    fn next_inner(&mut self) -> Result<Option<u8>> {
        let mut buf = [0; 1];
        loop {
            match self.reader.read(&mut buf) {
                Ok(0) => return Ok(None),
                Ok(_) => return Ok(Some(buf[0])),
                Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
                Err(e) => return Err(Error::io(e)),
            }
        }
    }
}

#[cfg(all(feature = "std", not(feature = "unsealed_read_write")))]
impl<R> private::Sealed for IoRead<R> where R: io::Read {}

#[cfg(feature = "std")]
impl<'de, R> Read<'de> for IoRead<R>
where
    R: io::Read,
{
    #[inline]
    fn next(&mut self) -> Result<Option<u8>> {
        match self.ch.take() {
            Some(ch) => Ok(Some(ch)),
            None => self.next_inner(),
        }
    }

    #[inline]
    fn peek(&mut self) -> Result<Option<u8>> {
        match self.ch {
            Some(ch) => Ok(Some(ch)),
            None => {
                self.ch = self.next_inner()?;
                Ok(self.ch)
            }
        }
    }

    fn read_to_buffer(&mut self, mut n: usize) -> Result<()> {
        // defend against malicious input pretending to be huge strings by limiting growth
        self.scratch.reserve(cmp::min(n, 16 * 1024));

        if n == 0 {
            return Ok(());
        }

        if let Some(ch) = self.ch.take() {
            self.scratch.push(ch);
            n -= 1;
        }

        // n == 0 is OK here and needs no further special treatment

        let transfer_result = {
            // Prepare for take() (which consumes its reader) by creating a reference adaptor
            // that'll only live in this block
            let reference = self.reader.by_ref();
            // Append the first n bytes of the reader to the scratch vector (or up to
            // an error or EOF indicated by a shorter read)
            let mut taken = reference.take(n as u64);
            taken.read_to_end(&mut self.scratch)
        };

        match transfer_result {
            Ok(r) if r == n => Ok(()),
            Ok(_) => Err(Error::syntax(
                ErrorCode::EofWhileParsingValue,
                self.offset(),
            )),
            Err(e) => Err(Error::io(e)),
        }
    }

    fn clear_buffer(&mut self) {
        self.scratch.clear();
    }

    fn take_buffer<'a>(&'a mut self) -> EitherLifetime<'a, 'de> {
        EitherLifetime::Short(&self.scratch)
    }

    fn read_into(&mut self, buf: &mut [u8]) -> Result<()> {
        self.reader.read_exact(buf).map_err(|e| {
            if e.kind() == io::ErrorKind::UnexpectedEof {
                Error::syntax(ErrorCode::EofWhileParsingValue, self.offset())
            } else {
                Error::io(e)
            }
        })
    }

    #[inline]
    fn discard(&mut self) {
        self.ch = None;
    }

    fn offset(&self) -> u64 {
        self.reader.offset
    }
}

#[cfg(feature = "std")]
impl<R> Offset for IoRead<R>
where
    R: std::io::Read,
{
    fn byte_offset(&self) -> usize {
        self.offset() as usize
    }
}

#[cfg(feature = "std")]
#[derive(Debug)]
struct OffsetReader<R> {
    reader: R,
    offset: u64,
}

#[cfg(feature = "std")]
impl<R> io::Read for OffsetReader<R>
where
    R: io::Read,
{
    #[inline]
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let r = self.reader.read(buf);
        if let Ok(count) = r {
            self.offset += count as u64;
        }
        r
    }
}

/// A CBOR input source that reads from a slice of bytes.
#[cfg(any(feature = "std", feature = "alloc"))]
#[derive(Debug)]
pub struct SliceRead<'a> {
    slice: &'a [u8],
    scratch: Vec<u8>,
    index: usize,
}

#[cfg(any(feature = "std", feature = "alloc"))]
impl<'a> SliceRead<'a> {
    /// Creates a CBOR input source to read from a slice of bytes.
    pub fn new(slice: &'a [u8]) -> SliceRead<'a> {
        SliceRead {
            slice,
            scratch: vec![],
            index: 0,
        }
    }

    fn end(&self, n: usize) -> Result<usize> {
        match self.index.checked_add(n) {
            Some(end) if end <= self.slice.len() => Ok(end),
            _ => Err(Error::syntax(
                ErrorCode::EofWhileParsingValue,
                self.slice.len() as u64,
            )),
        }
    }
}

#[cfg(any(feature = "std", feature = "alloc"))]
impl<'a> Offset for SliceRead<'a> {
    #[inline]
    fn byte_offset(&self) -> usize {
        self.index
    }
}

#[cfg(all(
    any(feature = "std", feature = "alloc"),
    not(feature = "unsealed_read_write")
))]
impl<'a> private::Sealed for SliceRead<'a> {}

#[cfg(any(feature = "std", feature = "alloc"))]
impl<'a> Read<'a> for SliceRead<'a> {
    #[inline]
    fn next(&mut self) -> Result<Option<u8>> {
        Ok(if self.index < self.slice.len() {
            let ch = self.slice[self.index];
            self.index += 1;
            Some(ch)
        } else {
            None
        })
    }

    #[inline]
    fn peek(&mut self) -> Result<Option<u8>> {
        Ok(if self.index < self.slice.len() {
            Some(self.slice[self.index])
        } else {
            None
        })
    }

    fn clear_buffer(&mut self) {
        self.scratch.clear();
    }

    fn read_to_buffer(&mut self, n: usize) -> Result<()> {
        let end = self.end(n)?;
        let slice = &self.slice[self.index..end];
        self.scratch.extend_from_slice(slice);
        self.index = end;

        Ok(())
    }

    #[inline]
    fn read<'b>(&'b mut self, n: usize) -> Result<EitherLifetime<'b, 'a>> {
        let end = self.end(n)?;
        let slice = &self.slice[self.index..end];
        self.index = end;
        Ok(EitherLifetime::Long(slice))
    }

    fn take_buffer<'b>(&'b mut self) -> EitherLifetime<'b, 'a> {
        EitherLifetime::Short(&self.scratch)
    }

    #[inline]
    fn read_into(&mut self, buf: &mut [u8]) -> Result<()> {
        let end = self.end(buf.len())?;
        buf.copy_from_slice(&self.slice[self.index..end]);
        self.index = end;
        Ok(())
    }

    #[inline]
    fn discard(&mut self) {
        self.index += 1;
    }

    fn offset(&self) -> u64 {
        self.index as u64
    }
}

/// A CBOR input source that reads from a slice of bytes using a fixed size scratch buffer.
///
/// [`SliceRead`](struct.SliceRead.html) and [`MutSliceRead`](struct.MutSliceRead.html) are usually
/// preferred over this, as they can handle indefinite length items.
#[derive(Debug)]
pub struct SliceReadFixed<'a, 'b> {
    slice: &'a [u8],
    scratch: &'b mut [u8],
    index: usize,
    scratch_index: usize,
}

impl<'a, 'b> SliceReadFixed<'a, 'b> {
    /// Creates a CBOR input source to read from a slice of bytes, backed by a scratch buffer.
    pub fn new(slice: &'a [u8], scratch: &'b mut [u8]) -> SliceReadFixed<'a, 'b> {
        SliceReadFixed {
            slice,
            scratch,
            index: 0,
            scratch_index: 0,
        }
    }

    fn end(&self, n: usize) -> Result<usize> {
        match self.index.checked_add(n) {
            Some(end) if end <= self.slice.len() => Ok(end),
            _ => Err(Error::syntax(
                ErrorCode::EofWhileParsingValue,
                self.slice.len() as u64,
            )),
        }
    }

    fn scratch_end(&self, n: usize) -> Result<usize> {
        match self.scratch_index.checked_add(n) {
            Some(end) if end <= self.scratch.len() => Ok(end),
            _ => Err(Error::scratch_too_small(self.index as u64)),
        }
    }
}

#[cfg(not(feature = "unsealed_read_write"))]
impl<'a, 'b> private::Sealed for SliceReadFixed<'a, 'b> {}

impl<'a, 'b> Read<'a> for SliceReadFixed<'a, 'b> {
    #[inline]
    fn next(&mut self) -> Result<Option<u8>> {
        Ok(if self.index < self.slice.len() {
            let ch = self.slice[self.index];
            self.index += 1;
            Some(ch)
        } else {
            None
        })
    }

    #[inline]
    fn peek(&mut self) -> Result<Option<u8>> {
        Ok(if self.index < self.slice.len() {
            Some(self.slice[self.index])
        } else {
            None
        })
    }

    fn clear_buffer(&mut self) {
        self.scratch_index = 0;
    }

    fn read_to_buffer(&mut self, n: usize) -> Result<()> {
        let end = self.end(n)?;
        let scratch_end = self.scratch_end(n)?;
        let slice = &self.slice[self.index..end];
        self.scratch[self.scratch_index..scratch_end].copy_from_slice(&slice);
        self.index = end;
        self.scratch_index = scratch_end;

        Ok(())
    }

    fn read<'c>(&'c mut self, n: usize) -> Result<EitherLifetime<'c, 'a>> {
        let end = self.end(n)?;
        let slice = &self.slice[self.index..end];
        self.index = end;
        Ok(EitherLifetime::Long(slice))
    }

    fn take_buffer<'c>(&'c mut self) -> EitherLifetime<'c, 'a> {
        EitherLifetime::Short(&self.scratch[0..self.scratch_index])
    }

    #[inline]
    fn read_into(&mut self, buf: &mut [u8]) -> Result<()> {
        let end = self.end(buf.len())?;
        buf.copy_from_slice(&self.slice[self.index..end]);
        self.index = end;
        Ok(())
    }

    #[inline]
    fn discard(&mut self) {
        self.index += 1;
    }

    fn offset(&self) -> u64 {
        self.index as u64
    }
}

#[cfg(any(feature = "std", feature = "alloc"))]
impl<'a, 'b> Offset for SliceReadFixed<'a, 'b> {
    #[inline]
    fn byte_offset(&self) -> usize {
        self.index
    }
}

/// A CBOR input source that reads from a slice of bytes, and can move data around internally to
/// reassemble indefinite strings without the need of an allocated scratch buffer.
#[derive(Debug)]
pub struct MutSliceRead<'a> {
    /// A complete view of the reader's data. It is promised that bytes before buffer_end are not
    /// mutated any more.
    slice: &'a mut [u8],
    /// Read cursor position in slice
    index: usize,
    /// Number of bytes already discarded from the slice
    before: usize,
    /// End of the buffer area that contains all bytes read_into_buffer. This is always <= index.
    buffer_end: usize,
}

impl<'a> MutSliceRead<'a> {
    /// Creates a CBOR input source to read from a slice of bytes.
    pub fn new(slice: &'a mut [u8]) -> MutSliceRead<'a> {
        MutSliceRead {
            slice,
            index: 0,
            before: 0,
            buffer_end: 0,
        }
    }

    fn end(&self, n: usize) -> Result<usize> {
        match self.index.checked_add(n) {
            Some(end) if end <= self.slice.len() => Ok(end),
            _ => Err(Error::syntax(
                ErrorCode::EofWhileParsingValue,
                self.slice.len() as u64,
            )),
        }
    }
}

#[cfg(not(feature = "unsealed_read_write"))]
impl<'a> private::Sealed for MutSliceRead<'a> {}

impl<'a> Read<'a> for MutSliceRead<'a> {
    #[inline]
    fn next(&mut self) -> Result<Option<u8>> {
        // This is duplicated from SliceRead, can that be eased?
        Ok(if self.index < self.slice.len() {
            let ch = self.slice[self.index];
            self.index += 1;
            Some(ch)
        } else {
            None
        })
    }

    #[inline]
    fn peek(&mut self) -> Result<Option<u8>> {
        // This is duplicated from SliceRead, can that be eased?
        Ok(if self.index < self.slice.len() {
            Some(self.slice[self.index])
        } else {
            None
        })
    }

    fn clear_buffer(&mut self) {
        self.slice = &mut mem::replace(&mut self.slice, &mut [])[self.index..];
        self.before += self.index;
        self.index = 0;
        self.buffer_end = 0;
    }

    fn read_to_buffer(&mut self, n: usize) -> Result<()> {
        let end = self.end(n)?;
        debug_assert!(
            self.buffer_end <= self.index,
            "MutSliceRead invariant violated: scratch buffer exceeds index"
        );
        self.slice[self.buffer_end..end].rotate_left(self.index - self.buffer_end);
        self.buffer_end += n;
        self.index = end;

        Ok(())
    }

    fn take_buffer<'b>(&'b mut self) -> EitherLifetime<'b, 'a> {
        let (left, right) = mem::replace(&mut self.slice, &mut []).split_at_mut(self.index);
        self.slice = right;
        self.before += self.index;
        self.index = 0;

        let left = &left[..self.buffer_end];
        self.buffer_end = 0;

        EitherLifetime::Long(left)
    }

    #[inline]
    fn read_into(&mut self, buf: &mut [u8]) -> Result<()> {
        // This is duplicated from SliceRead, can that be eased?
        let end = self.end(buf.len())?;
        buf.copy_from_slice(&self.slice[self.index..end]);
        self.index = end;
        Ok(())
    }

    #[inline]
    fn discard(&mut self) {
        self.index += 1;
    }

    fn offset(&self) -> u64 {
        (self.before + self.index) as u64
    }
}
