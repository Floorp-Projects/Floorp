use error::Result;
use serde;
use std::{io, slice};

/// An optional Read trait for advanced Bincode usage.
///
/// It is highly recommended to use bincode with `io::Read` or `&[u8]` before
/// implementing a custom `BincodeRead`.
pub trait BincodeRead<'storage>: io::Read {
    /// Forwards reading `length` bytes of a string on to the serde reader.
    fn forward_read_str<V>(&mut self, length: usize, visitor: V) -> Result<V::Value>
    where
        V: serde::de::Visitor<'storage>;

    /// Return the first `length` bytes of the internal byte buffer.
    fn get_byte_buffer(&mut self, length: usize) -> Result<Vec<u8>>;

    /// Forwards reading `length` bytes on to the serde reader.
    fn forward_read_bytes<V>(&mut self, length: usize, visitor: V) -> Result<V::Value>
    where
        V: serde::de::Visitor<'storage>;
}

/// A BincodeRead implementation for byte slices
/// NOT A PART OF THE STABLE PUBLIC API
#[doc(hidden)]
pub struct SliceReader<'storage> {
    slice: &'storage [u8],
}

/// A BincodeRead implementation for io::Readers
/// NOT A PART OF THE STABLE PUBLIC API
#[doc(hidden)]
pub struct IoReader<R> {
    reader: R,
    temp_buffer: Vec<u8>,
}

impl<'storage> SliceReader<'storage> {
    /// Constructs a slice reader
    pub fn new(bytes: &'storage [u8]) -> SliceReader<'storage> {
        SliceReader { slice: bytes }
    }
}

impl<R> IoReader<R> {
    /// Constructs an IoReadReader
    pub fn new(r: R) -> IoReader<R> {
        IoReader {
            reader: r,
            temp_buffer: vec![],
        }
    }
}

impl<'storage> io::Read for SliceReader<'storage> {
    #[inline(always)]
    fn read(&mut self, out: &mut [u8]) -> io::Result<usize> {
        (&mut self.slice).read(out)
    }
    #[inline(always)]
    fn read_exact(&mut self, out: &mut [u8]) -> io::Result<()> {
        (&mut self.slice).read_exact(out)
    }
}

impl<R: io::Read> io::Read for IoReader<R> {
    #[inline(always)]
    fn read(&mut self, out: &mut [u8]) -> io::Result<usize> {
        self.reader.read(out)
    }
    #[inline(always)]
    fn read_exact(&mut self, out: &mut [u8]) -> io::Result<()> {
        self.reader.read_exact(out)
    }
}

impl<'storage> SliceReader<'storage> {
    #[inline(always)]
    fn unexpected_eof() -> Box<::ErrorKind> {
        return Box::new(::ErrorKind::Io(io::Error::new(
            io::ErrorKind::UnexpectedEof,
            "",
        )));
    }
}

impl<'storage> BincodeRead<'storage> for SliceReader<'storage> {
    #[inline(always)]
    fn forward_read_str<V>(&mut self, length: usize, visitor: V) -> Result<V::Value>
    where
        V: serde::de::Visitor<'storage>,
    {
        use ErrorKind;
        if length > self.slice.len() {
            return Err(SliceReader::unexpected_eof());
        }

        let string = match ::std::str::from_utf8(&self.slice[..length]) {
            Ok(s) => s,
            Err(e) => return Err(ErrorKind::InvalidUtf8Encoding(e).into()),
        };
        let r = visitor.visit_borrowed_str(string);
        self.slice = &self.slice[length..];
        r
    }

    #[inline(always)]
    fn get_byte_buffer(&mut self, length: usize) -> Result<Vec<u8>> {
        if length > self.slice.len() {
            return Err(SliceReader::unexpected_eof());
        }

        let r = &self.slice[..length];
        self.slice = &self.slice[length..];
        Ok(r.to_vec())
    }

    #[inline(always)]
    fn forward_read_bytes<V>(&mut self, length: usize, visitor: V) -> Result<V::Value>
    where
        V: serde::de::Visitor<'storage>,
    {
        if length > self.slice.len() {
            return Err(SliceReader::unexpected_eof());
        }

        let r = visitor.visit_borrowed_bytes(&self.slice[..length]);
        self.slice = &self.slice[length..];
        r
    }
}

impl<R> IoReader<R>
where
    R: io::Read,
{
    fn fill_buffer(&mut self, length: usize) -> Result<()> {
        // We first reserve the space needed in our buffer.
        let current_length = self.temp_buffer.len();
        if length > current_length {
            self.temp_buffer.reserve_exact(length - current_length);
        }

        // Then create a slice with the length as our desired length. This is
        // safe as long as we only write (no reads) to this buffer, because
        // `reserve_exact` above has allocated this space.
        let buf = unsafe {
            slice::from_raw_parts_mut(self.temp_buffer.as_mut_ptr(), length)
        };

        // This method is assumed to properly handle slices which include
        // uninitialized bytes (as ours does). See discussion at the link below.
        // https://github.com/servo/bincode/issues/260
        self.reader.read_exact(buf)?;

        // Only after `read_exact` successfully returns do we set the buffer
        // length. By doing this after the call to `read_exact`, we can avoid
        // exposing uninitialized memory in the case of `read_exact` returning
        // an error.
        unsafe {
            self.temp_buffer.set_len(length);
        }

        Ok(())
    }
}

impl<'a, R> BincodeRead<'a> for IoReader<R>
where
    R: io::Read,
{
    fn forward_read_str<V>(&mut self, length: usize, visitor: V) -> Result<V::Value>
    where
        V: serde::de::Visitor<'a>,
    {
        self.fill_buffer(length)?;

        let string = match ::std::str::from_utf8(&self.temp_buffer[..]) {
            Ok(s) => s,
            Err(e) => return Err(::ErrorKind::InvalidUtf8Encoding(e).into()),
        };

        let r = visitor.visit_str(string);
        r
    }

    fn get_byte_buffer(&mut self, length: usize) -> Result<Vec<u8>> {
        self.fill_buffer(length)?;
        Ok(::std::mem::replace(&mut self.temp_buffer, Vec::new()))
    }

    fn forward_read_bytes<V>(&mut self, length: usize, visitor: V) -> Result<V::Value>
    where
        V: serde::de::Visitor<'a>,
    {
        self.fill_buffer(length)?;
        let r = visitor.visit_bytes(&self.temp_buffer[..]);
        r
    }
}
