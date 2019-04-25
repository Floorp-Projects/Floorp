use std::io::{self, BufRead, Read, BufReader, Error, ErrorKind, Write};
use std::{cmp,mem};

use super::InflateStream;

/// Workaround for lack of copy_from_slice on pre-1.9 rust.
#[inline]
fn copy_from_slice(mut to: &mut [u8], from: &[u8]) {
    assert_eq!(to.len(), from.len());
    to.write_all(from).unwrap();
}

/// A DEFLATE decoder/decompressor.
///
/// This structure implements a `Read` interface and takes a stream
/// of compressed data that implements the `BufRead` trait as input,
/// providing the decompressed data when read from.
///
/// # Example
/// ```
/// use std::io::Read;
/// use inflate::DeflateDecoderBuf;
///
/// const TEST_STRING: &'static str = "Hello, world";
/// let encoded = vec![243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
/// let mut decoder = DeflateDecoderBuf::new(&encoded[..]);
/// let mut output = Vec::new();
/// let status = decoder.read_to_end(&mut output);
/// # let _ = status;
/// assert_eq!(String::from_utf8(output).unwrap(), TEST_STRING);
/// ```
pub struct DeflateDecoderBuf<R> {
    /// The inner reader instance
    reader: R,
    /// The raw decompressor
    decompressor: InflateStream,
    /// How many bytes of the decompressor's output buffer still need to be output.
    pending_output_bytes: usize,
    /// Total number of bytes read from the underlying reader.
    total_in: u64,
    /// Total number of bytes written in `read` calls.
    total_out: u64,
}

impl<R: BufRead> DeflateDecoderBuf<R> {
    /// Create a new `Deflatedecoderbuf` to read from a raw deflate stream.
    pub fn new(reader: R) -> DeflateDecoderBuf<R> {
        DeflateDecoderBuf {
            reader: reader,
            decompressor: InflateStream::new(),
            pending_output_bytes: 0,
            total_in: 0,
            total_out: 0,
        }
    }

    /// Create a new `DeflateDecoderbuf` that reads from a zlib wrapped deflate stream.
    pub fn from_zlib(reader: R) -> DeflateDecoderBuf<R> {
        DeflateDecoderBuf {
            reader: reader,
            decompressor: InflateStream::from_zlib(),
            pending_output_bytes: 0,
            total_in: 0,
            total_out: 0,
        }
    }

    /// Create a new `DeflateDecoderbuf` that reads from a zlib wrapped deflate stream.
    /// without calculating and validating the checksum.
    pub fn from_zlib_no_checksum(reader: R) -> DeflateDecoderBuf<R> {
        DeflateDecoderBuf {
            reader: reader,
            decompressor: InflateStream::from_zlib_no_checksum(),
            pending_output_bytes: 0,
            total_in: 0,
            total_out: 0,
        }
    }
}

impl<R> DeflateDecoderBuf<R> {
    /// Resets the decompressor, and replaces the current inner `BufRead` instance by `r`.
    /// without doing any extra reallocations.
    ///
    /// Note that this function doesn't ensure that all data has been output.
    #[inline]
    pub fn reset(&mut self, r: R) -> R {
        self.decompressor.reset();
        mem::replace(&mut self.reader, r)
    }

    /// Resets the decoder, but continue to read from the same reader.
    ///
    /// Note that this function doesn't ensure that all data has been output.
    #[inline]
    pub fn reset_data(&mut self) {
        self.decompressor.reset()
    }

    /// Returns a reference to the underlying `BufRead` instance.
    #[inline]
    pub fn get_ref(&self) -> &R {
        &self.reader
    }

    /// Returns a mutable reference to the underlying `BufRead` instance.
    ///
    /// Note that mutation of the reader may cause surprising results if the decoder is going to
    /// keep being used.
    #[inline]
    pub fn get_mut(&mut self) -> &mut R {
        &mut self.reader
    }

    /// Drops the decoder and return the inner `BufRead` instance.
    ///
    /// Note that this function doesn't ensure that all data has been output.
    #[inline]
    pub fn into_inner(self) -> R {
        self.reader
    }

    /// Returns the total bytes read from the underlying `BufRead` instance.
    #[inline]
    pub fn total_in(&self) -> u64 {
        self.total_in
    }

    /// Returns the total number of bytes output from this decoder.
    #[inline]
    pub fn total_out(&self) -> u64 {
        self.total_out
    }

    /// Returns the calculated checksum value of the currently decoded data.
    ///
    /// Will return 0 for cases where the checksum is not validated.
    #[inline]
    pub fn current_checksum(&self) -> u32 {
        self.decompressor.current_checksum()
    }
}

impl<R: BufRead> Read for DeflateDecoderBuf<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let mut bytes_out = 0;
        // If there is still data left to ouput from the last call to `update()`, that needs to be
        // output first
        if self.pending_output_bytes != 0 {
            // Get the part of the buffer that has not been output yet.
            // The decompressor sets `pos` to 0 when it reaches the end of it's internal buffer,
            // so we have to check for that.
            let start = if self.decompressor.pos != 0 {
                self.decompressor.pos as usize - self.pending_output_bytes
            } else {
                self.decompressor.buffer.len() - self.pending_output_bytes
            };

            // Copy as much decompressed as possible to buf.
            let bytes_to_copy = cmp::min(buf.len(), self.pending_output_bytes);
            let pending_data =
                &self.decompressor.buffer[start..
                                          start + bytes_to_copy];
            copy_from_slice(&mut buf[..bytes_to_copy],pending_data);
            bytes_out += bytes_to_copy;
            // This won't underflow since `bytes_to_copy` will be at most
            // the same value as `pending_output_bytes`.
            self.pending_output_bytes -= bytes_to_copy;
            if self.pending_output_bytes != 0 {
                self.total_out += bytes_out as u64;
                // If there is still decompressed data left that didn't
                // fit in `buf`, return what we read.
                return Ok(bytes_out);
            }
        }

        // There is space in `buf` for more data, so try to read more.
        let (input_bytes_read, remaining_bytes) = {
            self.pending_output_bytes = 0;
            let input = try!(self.reader.fill_buf());
            if input.len() == 0 {
                self.total_out += bytes_out as u64;
                //If there is nothing more to read, return.
                return Ok(bytes_out);
            }
            let (input_bytes_read, data) =
                match self.decompressor.update(&input) {
                    Ok(res) => res,
                    Err(m) => return Err(Error::new(ErrorKind::Other, m))
                };

            // Space left in `buf`
            let space_left = buf.len() - bytes_out;
            let bytes_to_copy = cmp::min(space_left, data.len());

            copy_from_slice(&mut buf[bytes_out..bytes_out + bytes_to_copy], &data[..bytes_to_copy]);

            bytes_out += bytes_to_copy;

            // Can't underflow as bytes_to_copy is bounded by data.len().
            (input_bytes_read, data.len() - bytes_to_copy)

        };

        self.pending_output_bytes += remaining_bytes;
        self.total_in += input_bytes_read as u64;
        self.total_out += bytes_out as u64;
        self.reader.consume(input_bytes_read);

        Ok(bytes_out)
    }
}



/// A DEFLATE decoder/decompressor.
///
/// This structure implements a `Read` interface and takes a stream of compressed data that
/// implements the `Read` trait as input,
/// provoding the decompressed data when read from.
/// # Example
/// ```
/// use std::io::Read;
/// use inflate::DeflateDecoder;
/// const TEST_STRING: &'static str = "Hello, world";
/// let encoded = vec![243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
/// let mut decoder = DeflateDecoder::new(&encoded[..]);
/// let mut output = Vec::new();
/// let status = decoder.read_to_end(&mut output);
/// # let _ = status;
/// assert_eq!(String::from_utf8(output).unwrap(), TEST_STRING);
/// ```
pub struct DeflateDecoder<R> {
    /// Inner DeflateDecoderBuf, with R wrapped in a `BufReader`.
    inner: DeflateDecoderBuf<BufReader<R>>
}

impl<R: Read> DeflateDecoder<R> {
    /// Create a new `Deflatedecoderbuf` to read from a raw deflate stream.
    pub fn new(reader: R) -> DeflateDecoder<R> {
        DeflateDecoder {
            inner: DeflateDecoderBuf::new(BufReader::new(reader))
        }
    }

    /// Create a new `DeflateDecoderbuf` that reads from a zlib wrapped deflate stream.
    pub fn from_zlib(reader: R) -> DeflateDecoder<R> {
        DeflateDecoder {
            inner: DeflateDecoderBuf::from_zlib(BufReader::new(reader))
        }
    }

    /// Create a new `DeflateDecoderbuf` that reads from a zlib wrapped deflate stream.
    /// without calculating and validating the checksum.
    pub fn from_zlib_no_checksum(reader: R) -> DeflateDecoder<R> {
        DeflateDecoder {
            inner: DeflateDecoderBuf::from_zlib_no_checksum(BufReader::new(reader))
        }
    }

    /// Resets the decompressor, and replaces the current inner `BufRead` instance by `r`.
    /// without doing any extra reallocations.
    ///
    /// Note that this function doesn't ensure that all data has been output.
    #[inline]
    pub fn reset(&mut self, r: R) -> R {
        self.inner.reset(BufReader::new(r)).into_inner()
    }

    /// Returns a reference to the underlying reader.
    #[inline]
    pub fn get_ref(&self) -> &R {
        self.inner.get_ref().get_ref()
    }

    /// Returns a mutable reference to the underlying reader.
    ///
    /// Note that mutation of the reader may cause surprising results if the decoder is going to
    /// keep being used.
    #[inline]
    pub fn get_mut(&mut self) -> &mut R {
        self.inner.get_mut().get_mut()
    }

    /// Returns the total number of bytes output from this decoder.
    #[inline]
    pub fn into_inner(self) -> R {
        self.inner.into_inner().into_inner()
    }
}

impl<R> DeflateDecoder<R> {
    /// Resets the decoder, but continue to read from the same reader.
    ///
    /// Note that this function doesn't ensure that all data has been output.
    #[inline]
    pub fn reset_data(&mut self) {
        self.inner.reset_data()
    }

    /// Returns the total bytes read from the underlying reader.
    #[inline]
    pub fn total_in(&self) -> u64 {
        self.inner.total_in
    }

    /// Returns the total number of bytes output from this decoder.
    #[inline]
    pub fn total_out(&self) -> u64 {
        self.inner.total_out
    }

    /// Returns the calculated checksum value of the currently decoded data.
    ///
    /// Will return 0 for cases where the checksum is not validated.
    #[inline]
    pub fn current_checksum(&self) -> u32 {
        self.inner.current_checksum()
    }
}

impl<R: Read> Read for DeflateDecoder<R> {
    #[inline]
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.inner.read(buf)
    }
}

#[cfg(test)]
mod test {
    use super::{DeflateDecoder};
    use std::io::Read;

    #[test]
    fn deflate_reader() {
        const TEST_STRING: &'static str = "Hello, world";
        let encoded = vec![243, 72, 205, 201, 201, 215, 81, 40, 207, 47, 202, 73, 1, 0];
        let mut decoder = DeflateDecoder::new(&encoded[..]);
        let mut output = Vec::new();
        decoder.read_to_end(&mut output).unwrap();
        assert_eq!(String::from_utf8(output).unwrap(), TEST_STRING);
        assert_eq!(decoder.total_in(), encoded.len() as u64);
        assert_eq!(decoder.total_out(), TEST_STRING.len() as u64);
    }

    #[test]
    fn zlib_reader() {
        const TEST_STRING: &'static str = "Hello, zlib!";
        let encoded = vec![120, 156, 243, 72, 205, 201, 201, 215, 81, 168, 202, 201,
                       76, 82, 4, 0, 27, 101, 4, 19];
        let mut decoder = DeflateDecoder::from_zlib(&encoded[..]);
        let mut output = Vec::new();
        decoder.read_to_end(&mut output).unwrap();
        assert_eq!(String::from_utf8(output).unwrap(), TEST_STRING);
        assert_eq!(decoder.total_in(), encoded.len() as u64);
        assert_eq!(decoder.total_out(), TEST_STRING.len() as u64);
    }
}
