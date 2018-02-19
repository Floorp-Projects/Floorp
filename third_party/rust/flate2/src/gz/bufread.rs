use std::cmp;
use std::io::prelude::*;
use std::io;
use std::mem;

use super::{GzBuilder, GzHeader};
use super::{FCOMMENT, FEXTRA, FHCRC, FNAME};
use Compression;
use crc::CrcReader;
use deflate;

fn copy(into: &mut [u8], from: &[u8], pos: &mut usize) -> usize {
    let min = cmp::min(into.len(), from.len() - *pos);
    for (slot, val) in into.iter_mut().zip(from[*pos..*pos + min].iter()) {
        *slot = *val;
    }
    *pos += min;
    return min;
}
fn corrupt() -> io::Error {
    io::Error::new(
        io::ErrorKind::InvalidInput,
        "corrupt gzip stream does not have a matching checksum",
    )
}

fn bad_header() -> io::Error {
    io::Error::new(io::ErrorKind::InvalidInput, "invalid gzip header")
}

fn read_le_u16<R: Read>(r: &mut R) -> io::Result<u16> {
    let mut b = [0; 2];
    try!(r.read_exact(&mut b));
    Ok((b[0] as u16) | ((b[1] as u16) << 8))
}

fn read_gz_header<R: Read>(r: &mut R) -> io::Result<GzHeader> {
    let mut crc_reader = CrcReader::new(r);
    let mut header = [0; 10];
    try!(crc_reader.read_exact(&mut header));

    let id1 = header[0];
    let id2 = header[1];
    if id1 != 0x1f || id2 != 0x8b {
        return Err(bad_header());
    }
    let cm = header[2];
    if cm != 8 {
        return Err(bad_header());
    }

    let flg = header[3];
    let mtime = ((header[4] as u32) << 0) | ((header[5] as u32) << 8) | ((header[6] as u32) << 16) |
        ((header[7] as u32) << 24);
    let _xfl = header[8];
    let os = header[9];

    let extra = if flg & FEXTRA != 0 {
        let xlen = try!(read_le_u16(&mut crc_reader));
        let mut extra = vec![0; xlen as usize];
        try!(crc_reader.read_exact(&mut extra));
        Some(extra)
    } else {
        None
    };
    let filename = if flg & FNAME != 0 {
        // wow this is slow
        let mut b = Vec::new();
        for byte in crc_reader.by_ref().bytes() {
            let byte = try!(byte);
            if byte == 0 {
                break;
            }
            b.push(byte);
        }
        Some(b)
    } else {
        None
    };
    let comment = if flg & FCOMMENT != 0 {
        // wow this is slow
        let mut b = Vec::new();
        for byte in crc_reader.by_ref().bytes() {
            let byte = try!(byte);
            if byte == 0 {
                break;
            }
            b.push(byte);
        }
        Some(b)
    } else {
        None
    };

    if flg & FHCRC != 0 {
        let calced_crc = crc_reader.crc().sum() as u16;
        let stored_crc = try!(read_le_u16(&mut crc_reader));
        if calced_crc != stored_crc {
            return Err(corrupt());
        }
    }

    Ok(GzHeader {
        extra: extra,
        filename: filename,
        comment: comment,
        operating_system: os,
        mtime: mtime,
    })
}


/// A gzip streaming encoder
///
/// This structure exposes a [`BufRead`] interface that will read uncompressed data
/// from the underlying reader and expose the compressed version as a [`BufRead`]
/// interface.
///
/// [`BufRead`]: https://doc.rust-lang.org/std/io/trait.BufRead.html
///
/// # Examples
///
/// ```
/// use std::io::prelude::*;
/// use std::io;
/// use flate2::Compression;
/// use flate2::bufread::GzEncoder;
/// use std::fs::File;
/// use std::io::BufReader;
///
/// // Opens sample file, compresses the contents and returns a Vector or error
/// // File wrapped in a BufReader implements BufRead
///
/// fn open_hello_world() -> io::Result<Vec<u8>> {
///     let f = File::open("examples/hello_world.txt")?;
///     let b = BufReader::new(f);
///     let mut gz = GzEncoder::new(b, Compression::fast());
///     let mut buffer = Vec::new();
///     gz.read_to_end(&mut buffer)?;
///     Ok(buffer)
/// }
/// ```
#[derive(Debug)]
pub struct GzEncoder<R> {
    inner: deflate::bufread::DeflateEncoder<CrcReader<R>>,
    header: Vec<u8>,
    pos: usize,
    eof: bool,
}

pub fn gz_encoder<R: BufRead>(header: Vec<u8>, r: R, lvl: Compression)
    -> GzEncoder<R>
{
    let crc = CrcReader::new(r);
    GzEncoder {
        inner: deflate::bufread::DeflateEncoder::new(crc, lvl),
        header: header,
        pos: 0,
        eof: false,
    }
}

impl<R: BufRead> GzEncoder<R> {
    /// Creates a new encoder which will use the given compression level.
    ///
    /// The encoder is not configured specially for the emitted header. For
    /// header configuration, see the `GzBuilder` type.
    ///
    /// The data read from the stream `r` will be compressed and available
    /// through the returned reader.
    pub fn new(r: R, level: Compression) -> GzEncoder<R> {
        GzBuilder::new().buf_read(r, level)
    }

    fn read_footer(&mut self, into: &mut [u8]) -> io::Result<usize> {
        if self.pos == 8 {
            return Ok(0);
        }
        let crc = self.inner.get_ref().crc();
        let ref arr = [
            (crc.sum() >> 0) as u8,
            (crc.sum() >> 8) as u8,
            (crc.sum() >> 16) as u8,
            (crc.sum() >> 24) as u8,
            (crc.amount() >> 0) as u8,
            (crc.amount() >> 8) as u8,
            (crc.amount() >> 16) as u8,
            (crc.amount() >> 24) as u8,
        ];
        Ok(copy(into, arr, &mut self.pos))
    }
}

impl<R> GzEncoder<R> {
    /// Acquires a reference to the underlying reader.
    pub fn get_ref(&self) -> &R {
        self.inner.get_ref().get_ref()
    }

    /// Acquires a mutable reference to the underlying reader.
    ///
    /// Note that mutation of the reader may result in surprising results if
    /// this encoder is continued to be used.
    pub fn get_mut(&mut self) -> &mut R {
        self.inner.get_mut().get_mut()
    }

    /// Returns the underlying stream, consuming this encoder
    pub fn into_inner(self) -> R {
        self.inner.into_inner().into_inner()
    }
}

impl<R: BufRead> Read for GzEncoder<R> {
    fn read(&mut self, mut into: &mut [u8]) -> io::Result<usize> {
        let mut amt = 0;
        if self.eof {
            return self.read_footer(into);
        } else if self.pos < self.header.len() {
            amt += copy(into, &self.header, &mut self.pos);
            if amt == into.len() {
                return Ok(amt);
            }
            let tmp = into;
            into = &mut tmp[amt..];
        }
        match try!(self.inner.read(into)) {
            0 => {
                self.eof = true;
                self.pos = 0;
                self.read_footer(into)
            }
            n => Ok(amt + n),
        }
    }
}

impl<R: BufRead + Write> Write for GzEncoder<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}


/// A gzip streaming decoder
///
/// This structure exposes a [`ReadBuf`] interface that will consume compressed
/// data from the underlying reader and emit uncompressed data.
///
/// [`ReadBuf`]: https://doc.rust-lang.org/std/io/trait.BufRead.html
///
/// # Examples
///
/// ```
/// use std::io::prelude::*;
/// use std::io;
/// # use flate2::Compression;
/// # use flate2::write::GzEncoder;
/// use flate2::bufread::GzDecoder;
///
/// # fn main() {
/// #   let mut e = GzEncoder::new(Vec::new(), Compression::default());
/// #   e.write(b"Hello World").unwrap();
/// #   let bytes = e.finish().unwrap();
/// #   println!("{}", decode_reader(bytes).unwrap());
/// # }
/// #
/// // Uncompresses a Gz Encoded vector of bytes and returns a string or error
/// // Here &[u8] implements BufRead
///
/// fn decode_reader(bytes: Vec<u8>) -> io::Result<String> {
///    let mut gz = GzDecoder::new(&bytes[..]);
///    let mut s = String::new();
///    gz.read_to_string(&mut s)?;
///    Ok(s)
/// }
/// ```
#[derive(Debug)]
pub struct GzDecoder<R> {
    inner: CrcReader<deflate::bufread::DeflateDecoder<R>>,
    header: io::Result<GzHeader>,
    finished: bool,
}


impl<R: BufRead> GzDecoder<R> {
    /// Creates a new decoder from the given reader, immediately parsing the
    /// gzip header.
    pub fn new(mut r: R) -> GzDecoder<R> {
        let header = read_gz_header(&mut r);

        let flate = deflate::bufread::DeflateDecoder::new(r);
        GzDecoder {
            inner: CrcReader::new(flate),
            header: header,
            finished: false,
        }
    }

    fn finish(&mut self) -> io::Result<()> {
        if self.finished {
            return Ok(());
        }
        let ref mut buf = [0u8; 8];
        {
            let mut len = 0;

            while len < buf.len() {
                match try!(self.inner.get_mut().get_mut().read(&mut buf[len..])) {
                    0 => return Err(corrupt()),
                    n => len += n,
                }
            }
        }

        let crc = ((buf[0] as u32) << 0) | ((buf[1] as u32) << 8) | ((buf[2] as u32) << 16) |
            ((buf[3] as u32) << 24);
        let amt = ((buf[4] as u32) << 0) | ((buf[5] as u32) << 8) | ((buf[6] as u32) << 16) |
            ((buf[7] as u32) << 24);
        if crc != self.inner.crc().sum() as u32 {
            return Err(corrupt());
        }
        if amt != self.inner.crc().amount() {
            return Err(corrupt());
        }
        self.finished = true;
        Ok(())
    }
}

impl<R> GzDecoder<R> {
    /// Returns the header associated with this stream, if it was valid
    pub fn header(&self) -> Option<&GzHeader> {
        self.header.as_ref().ok()
    }

    /// Acquires a reference to the underlying reader.
    pub fn get_ref(&self) -> &R {
        self.inner.get_ref().get_ref()
    }

    /// Acquires a mutable reference to the underlying stream.
    ///
    /// Note that mutation of the stream may result in surprising results if
    /// this encoder is continued to be used.
    pub fn get_mut(&mut self) -> &mut R {
        self.inner.get_mut().get_mut()
    }

    /// Consumes this decoder, returning the underlying reader.
    pub fn into_inner(self) -> R {
        self.inner.into_inner().into_inner()
    }
}

impl<R: BufRead> Read for GzDecoder<R> {
    fn read(&mut self, into: &mut [u8]) -> io::Result<usize> {
        if let Err(ref mut e) = self.header {
            let another_error = io::ErrorKind::Other.into();
            return Err(mem::replace(e, another_error))
        }
        match try!(self.inner.read(into)) {
            0 => {
                try!(self.finish());
                Ok(0)
            }
            n => Ok(n),
        }
    }
}

impl<R: BufRead + Write> Write for GzDecoder<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}



/// A gzip streaming decoder that decodes all members of a multistream
///
/// A gzip member consists of a header, compressed data and a trailer. The [gzip
/// specification](https://tools.ietf.org/html/rfc1952), however, allows multiple
/// gzip members to be joined in a single stream. `MultiGzDecoder` will
/// decode all consecutive members while `GzDecoder` will only decompress
/// the first gzip member. The multistream format is commonly used in
/// bioinformatics, for example when using the BGZF compressed data.
///
/// This structure exposes a [`BufRead`] interface that will consume all gzip members
/// from the underlying reader and emit uncompressed data.
///
/// [`BufRead`]: https://doc.rust-lang.org/std/io/trait.BufRead.html
///
/// # Examples
///
/// ```
/// use std::io::prelude::*;
/// use std::io;
/// # use flate2::Compression;
/// # use flate2::write::GzEncoder;
/// use flate2::bufread::MultiGzDecoder;
///
/// # fn main() {
/// #   let mut e = GzEncoder::new(Vec::new(), Compression::default());
/// #   e.write(b"Hello World").unwrap();
/// #   let bytes = e.finish().unwrap();
/// #   println!("{}", decode_reader(bytes).unwrap());
/// # }
/// #
/// // Uncompresses a Gz Encoded vector of bytes and returns a string or error
/// // Here &[u8] implements BufRead
///
/// fn decode_reader(bytes: Vec<u8>) -> io::Result<String> {
///    let mut gz = MultiGzDecoder::new(&bytes[..]);
///    let mut s = String::new();
///    gz.read_to_string(&mut s)?;
///    Ok(s)
/// }
/// ```
#[derive(Debug)]
pub struct MultiGzDecoder<R> {
    inner: CrcReader<deflate::bufread::DeflateDecoder<R>>,
    header: io::Result<GzHeader>,
    finished: bool,
}


impl<R: BufRead> MultiGzDecoder<R> {
    /// Creates a new decoder from the given reader, immediately parsing the
    /// (first) gzip header. If the gzip stream contains multiple members all will
    /// be decoded.
    pub fn new(mut r: R) -> MultiGzDecoder<R> {
        let header = read_gz_header(&mut r);

        let flate = deflate::bufread::DeflateDecoder::new(r);
        MultiGzDecoder {
            inner: CrcReader::new(flate),
            header: header,
            finished: false,
        }
    }

    fn finish_member(&mut self) -> io::Result<usize> {
        if self.finished {
            return Ok(0);
        }
        let ref mut buf = [0u8; 8];
        {
            let mut len = 0;

            while len < buf.len() {
                match try!(self.inner.get_mut().get_mut().read(&mut buf[len..])) {
                    0 => return Err(corrupt()),
                    n => len += n,
                }
            }
        }

        let crc = ((buf[0] as u32) << 0) | ((buf[1] as u32) << 8) | ((buf[2] as u32) << 16) |
            ((buf[3] as u32) << 24);
        let amt = ((buf[4] as u32) << 0) | ((buf[5] as u32) << 8) | ((buf[6] as u32) << 16) |
            ((buf[7] as u32) << 24);
        if crc != self.inner.crc().sum() as u32 {
            return Err(corrupt());
        }
        if amt != self.inner.crc().amount() {
            return Err(corrupt());
        }
        let remaining = match self.inner.get_mut().get_mut().fill_buf() {
            Ok(b) => if b.is_empty() {
                self.finished = true;
                return Ok(0);
            } else {
                b.len()
            },
            Err(e) => return Err(e),
        };

        let next_header = read_gz_header(self.inner.get_mut().get_mut());
        drop(mem::replace(&mut self.header, next_header));
        self.inner.reset();
        self.inner.get_mut().reset_data();

        Ok(remaining)
    }
}

impl<R> MultiGzDecoder<R> {
    /// Returns the current header associated with this stream, if it's valid
    pub fn header(&self) -> Option<&GzHeader> {
        self.header.as_ref().ok()
    }

    /// Acquires a reference to the underlying reader.
    pub fn get_ref(&self) -> &R {
        self.inner.get_ref().get_ref()
    }

    /// Acquires a mutable reference to the underlying stream.
    ///
    /// Note that mutation of the stream may result in surprising results if
    /// this encoder is continued to be used.
    pub fn get_mut(&mut self) -> &mut R {
        self.inner.get_mut().get_mut()
    }

    /// Consumes this decoder, returning the underlying reader.
    pub fn into_inner(self) -> R {
        self.inner.into_inner().into_inner()
    }
}

impl<R: BufRead> Read for MultiGzDecoder<R> {
    fn read(&mut self, into: &mut [u8]) -> io::Result<usize> {
        if let Err(ref mut e) = self.header {
            let another_error = io::ErrorKind::Other.into();
            return Err(mem::replace(e, another_error))
        }
        match try!(self.inner.read(into)) {
            0 => match self.finish_member() {
                Ok(0) => Ok(0),
                Ok(_) => self.read(into),
                Err(e) => Err(e),
            },
            n => Ok(n),
        }
    }
}

impl<R: BufRead + Write> Write for MultiGzDecoder<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}
