//! gzip compression/decompression
//!
//! [1]: http://www.gzip.org/zlib/rfc-gzip.html

use std::cmp;
use std::env;
use std::ffi::CString;
use std::io::prelude::*;
use std::io;
use std::mem;

#[cfg(feature = "tokio")]
use futures::Poll;
#[cfg(feature = "tokio")]
use tokio_io::{AsyncRead, AsyncWrite};

use {Compression, Compress};
use bufreader::BufReader;
use crc::{CrcReader, Crc};
use deflate;
use zio;

static FHCRC: u8 = 1 << 1;
static FEXTRA: u8 = 1 << 2;
static FNAME: u8 = 1 << 3;
static FCOMMENT: u8 = 1 << 4;

/// A gzip streaming encoder
///
/// This structure exposes a `Write` interface that will emit compressed data
/// to the underlying writer `W`.
pub struct EncoderWriter<W: Write> {
    inner: zio::Writer<W, Compress>,
    crc: Crc,
    crc_bytes_written: usize,
    header: Vec<u8>,
}

/// A gzip streaming encoder
///
/// This structure exposes a `Read` interface that will read uncompressed data
/// from the underlying reader and expose the compressed version as a `Read`
/// interface.
pub struct EncoderReader<R: Read> {
    inner: EncoderReaderBuf<BufReader<R>>,
}

/// A gzip streaming encoder
///
/// This structure exposes a `Read` interface that will read uncompressed data
/// from the underlying reader and expose the compressed version as a `Read`
/// interface.
pub struct EncoderReaderBuf<R: BufRead> {
    inner: deflate::EncoderReaderBuf<CrcReader<R>>,
    header: Vec<u8>,
    pos: usize,
    eof: bool,
}

/// A builder structure to create a new gzip Encoder.
///
/// This structure controls header configuration options such as the filename.
pub struct Builder {
    extra: Option<Vec<u8>>,
    filename: Option<CString>,
    comment: Option<CString>,
    mtime: u32,
}

/// A gzip streaming decoder
///
/// This structure exposes a `Read` interface that will consume compressed
/// data from the underlying reader and emit uncompressed data.
pub struct DecoderReader<R: Read> {
    inner: DecoderReaderBuf<BufReader<R>>,
}

/// A gzip streaming decoder that decodes all members of a multistream
///
/// A gzip member consists of a header, compressed data and a trailer. The [gzip
/// specification](https://tools.ietf.org/html/rfc1952), however, allows multiple
/// gzip members to be joined in a single stream.  `MultiDecoderReader` will
/// decode all consecutive members while `DecoderReader` will only decompress the
/// first gzip member. The multistream format is commonly used in bioinformatics,
/// for example when using the BGZF compressed data.
///
/// This structure exposes a `Read` interface that will consume all gzip members
/// from the underlying reader and emit uncompressed data.
pub struct MultiDecoderReader<R: Read> {
    inner: MultiDecoderReaderBuf<BufReader<R>>,
}

/// A gzip streaming decoder
///
/// This structure exposes a `Read` interface that will consume compressed
/// data from the underlying reader and emit uncompressed data.
pub struct DecoderReaderBuf<R: BufRead> {
    inner: CrcReader<deflate::DecoderReaderBuf<R>>,
    header: Header,
    finished: bool,
}

/// A gzip streaming decoder that decodes all members of a multistream
///
/// A gzip member consists of a header, compressed data and a trailer. The [gzip
/// specification](https://tools.ietf.org/html/rfc1952), however, allows multiple
/// gzip members to be joined in a single stream. `MultiDecoderReaderBuf` will
/// decode all consecutive members while `DecoderReaderBuf` will only decompress
/// the first gzip member. The multistream format is commonly used in
/// bioinformatics, for example when using the BGZF compressed data.
///
/// This structure exposes a `Read` interface that will consume all gzip members
/// from the underlying reader and emit uncompressed data.
pub struct MultiDecoderReaderBuf<R: BufRead> {
    inner: CrcReader<deflate::DecoderReaderBuf<R>>,
    header: Header,
    finished: bool,
}

/// A structure representing the header of a gzip stream.
///
/// The header can contain metadata about the file that was compressed, if
/// present.
pub struct Header {
    extra: Option<Vec<u8>>,
    filename: Option<Vec<u8>>,
    comment: Option<Vec<u8>>,
    mtime: u32,
}

impl Builder {
    /// Create a new blank builder with no header by default.
    pub fn new() -> Builder {
        Builder {
            extra: None,
            filename: None,
            comment: None,
            mtime: 0,
        }
    }

    /// Configure the `mtime` field in the gzip header.
    pub fn mtime(mut self, mtime: u32) -> Builder {
        self.mtime = mtime;
        self
    }

    /// Configure the `extra` field in the gzip header.
    pub fn extra(mut self, extra: Vec<u8>) -> Builder {
        self.extra = Some(extra);
        self
    }

    /// Configure the `filename` field in the gzip header.
    pub fn filename(mut self, filename: &[u8]) -> Builder {
        self.filename = Some(CString::new(filename).unwrap());
        self
    }

    /// Configure the `comment` field in the gzip header.
    pub fn comment(mut self, comment: &[u8]) -> Builder {
        self.comment = Some(CString::new(comment).unwrap());
        self
    }

    /// Consume this builder, creating a writer encoder in the process.
    ///
    /// The data written to the returned encoder will be compressed and then
    /// written out to the supplied parameter `w`.
    pub fn write<W: Write>(self, w: W, lvl: Compression) -> EncoderWriter<W> {
        EncoderWriter {
            inner: zio::Writer::new(w, Compress::new(lvl, false)),
            crc: Crc::new(),
            header: self.into_header(lvl),
            crc_bytes_written: 0,
        }
    }

    /// Consume this builder, creating a reader encoder in the process.
    ///
    /// Data read from the returned encoder will be the compressed version of
    /// the data read from the given reader.
    pub fn read<R: Read>(self, r: R, lvl: Compression) -> EncoderReader<R> {
        EncoderReader {
            inner: self.buf_read(BufReader::new(r), lvl),
        }
    }

    /// Consume this builder, creating a reader encoder in the process.
    ///
    /// Data read from the returned encoder will be the compressed version of
    /// the data read from the given reader.
    pub fn buf_read<R>(self, r: R, lvl: Compression) -> EncoderReaderBuf<R>
        where R: BufRead
    {
        let crc = CrcReader::new(r);
        EncoderReaderBuf {
            inner: deflate::EncoderReaderBuf::new(crc, lvl),
            header: self.into_header(lvl),
            pos: 0,
            eof: false,
        }
    }

    fn into_header(self, lvl: Compression) -> Vec<u8> {
        let Builder { extra, filename, comment, mtime } = self;
        let mut flg = 0;
        let mut header = vec![0u8; 10];
        match extra {
            Some(v) => {
                flg |= FEXTRA;
                header.push((v.len() >> 0) as u8);
                header.push((v.len() >> 8) as u8);
                header.extend(v);
            }
            None => {}
        }
        match filename {
            Some(filename) => {
                flg |= FNAME;
                header.extend(filename.as_bytes_with_nul().iter().map(|x| *x));
            }
            None => {}
        }
        match comment {
            Some(comment) => {
                flg |= FCOMMENT;
                header.extend(comment.as_bytes_with_nul().iter().map(|x| *x));
            }
            None => {}
        }
        header[0] = 0x1f;
        header[1] = 0x8b;
        header[2] = 8;
        header[3] = flg;
        header[4] = (mtime >> 0) as u8;
        header[5] = (mtime >> 8) as u8;
        header[6] = (mtime >> 16) as u8;
        header[7] = (mtime >> 24) as u8;
        header[8] = match lvl {
            Compression::Best => 2,
            Compression::Fast => 4,
            _ => 0,
        };
        header[9] = match env::consts::OS {
            "linux" => 3,
            "macos" => 7,
            "win32" => 0,
            _ => 255,
        };
        return header;
    }
}

impl<W: Write> EncoderWriter<W> {
    /// Creates a new encoder which will use the given compression level.
    ///
    /// The encoder is not configured specially for the emitted header. For
    /// header configuration, see the `Builder` type.
    ///
    /// The data written to the returned encoder will be compressed and then
    /// written to the stream `w`.
    pub fn new(w: W, level: Compression) -> EncoderWriter<W> {
        Builder::new().write(w, level)
    }

    /// Acquires a reference to the underlying writer.
    pub fn get_ref(&self) -> &W {
        self.inner.get_ref()
    }

    /// Acquires a mutable reference to the underlying writer.
    ///
    /// Note that mutation of the writer may result in surprising results if
    /// this encoder is continued to be used.
    pub fn get_mut(&mut self) -> &mut W {
        self.inner.get_mut()
    }

    /// Attempt to finish this output stream, writing out final chunks of data.
    ///
    /// Note that this function can only be used once data has finished being
    /// written to the output stream. After this function is called then further
    /// calls to `write` may result in a panic.
    ///
    /// # Panics
    ///
    /// Attempts to write data to this stream may result in a panic after this
    /// function is called.
    pub fn try_finish(&mut self) -> io::Result<()> {
        try!(self.write_header());
        try!(self.inner.finish());

        while self.crc_bytes_written < 8 {
            let (sum, amt) = (self.crc.sum() as u32, self.crc.amount());
            let buf = [(sum >> 0) as u8,
                       (sum >> 8) as u8,
                       (sum >> 16) as u8,
                       (sum >> 24) as u8,
                       (amt >> 0) as u8,
                       (amt >> 8) as u8,
                       (amt >> 16) as u8,
                       (amt >> 24) as u8];
            let mut inner = self.inner.get_mut();
            let n = try!(inner.write(&buf[self.crc_bytes_written..]));
            self.crc_bytes_written += n;
        }
        Ok(())
    }

    /// Finish encoding this stream, returning the underlying writer once the
    /// encoding is done.
    ///
    /// Note that this function may not be suitable to call in a situation where
    /// the underlying stream is an asynchronous I/O stream. To finish a stream
    /// the `try_finish` (or `shutdown`) method should be used instead. To
    /// re-acquire ownership of a stream it is safe to call this method after
    /// `try_finish` or `shutdown` has returned `Ok`.
    pub fn finish(mut self) -> io::Result<W> {
        try!(self.try_finish());
        Ok(self.inner.take_inner())
    }

    fn write_header(&mut self) -> io::Result<()> {
        while self.header.len() > 0 {
            let n = try!(self.inner.get_mut().write(&self.header));
            self.header.drain(..n);
        }
        Ok(())
    }
}

impl<W: Write> Write for EncoderWriter<W> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        assert_eq!(self.crc_bytes_written, 0);
        try!(self.write_header());
        let n = try!(self.inner.write(buf));
        self.crc.update(&buf[..n]);
        Ok(n)
    }

    fn flush(&mut self) -> io::Result<()> {
        assert_eq!(self.crc_bytes_written, 0);
        self.inner.flush()
    }
}

#[cfg(feature = "tokio")]
impl<W: AsyncWrite> AsyncWrite for EncoderWriter<W> {
    fn shutdown(&mut self) -> Poll<(), io::Error> {
        try_nb!(self.try_finish());
        self.get_mut().shutdown()
    }
}

impl<R: Read + Write> Read for EncoderWriter<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.get_mut().read(buf)
    }
}

#[cfg(feature = "tokio")]
impl<R: AsyncRead + AsyncWrite> AsyncRead for EncoderWriter<R> {
}

impl<W: Write> Drop for EncoderWriter<W> {
    fn drop(&mut self) {
        if self.inner.is_present() {
            let _ = self.try_finish();
        }
    }
}

impl<R: Read> EncoderReader<R> {
    /// Creates a new encoder which will use the given compression level.
    ///
    /// The encoder is not configured specially for the emitted header. For
    /// header configuration, see the `Builder` type.
    ///
    /// The data read from the stream `r` will be compressed and available
    /// through the returned reader.
    pub fn new(r: R, level: Compression) -> EncoderReader<R> {
        Builder::new().read(r, level)
    }

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

fn copy(into: &mut [u8], from: &[u8], pos: &mut usize) -> usize {
    let min = cmp::min(into.len(), from.len() - *pos);
    for (slot, val) in into.iter_mut().zip(from[*pos..*pos + min].iter()) {
        *slot = *val;
    }
    *pos += min;
    return min;
}

impl<R: Read> Read for EncoderReader<R> {
    fn read(&mut self, mut into: &mut [u8]) -> io::Result<usize> {
        self.inner.read(into)
    }
}

impl<R: Read + Write> Write for EncoderReader<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}

impl<R: BufRead> EncoderReaderBuf<R> {
    /// Creates a new encoder which will use the given compression level.
    ///
    /// The encoder is not configured specially for the emitted header. For
    /// header configuration, see the `Builder` type.
    ///
    /// The data read from the stream `r` will be compressed and available
    /// through the returned reader.
    pub fn new(r: R, level: Compression) -> EncoderReaderBuf<R> {
        Builder::new().buf_read(r, level)
    }

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

    fn read_footer(&mut self, into: &mut [u8]) -> io::Result<usize> {
        if self.pos == 8 {
            return Ok(0);
        }
        let crc = self.inner.get_ref().crc();
        let ref arr = [(crc.sum() >> 0) as u8,
                       (crc.sum() >> 8) as u8,
                       (crc.sum() >> 16) as u8,
                       (crc.sum() >> 24) as u8,
                       (crc.amount() >> 0) as u8,
                       (crc.amount() >> 8) as u8,
                       (crc.amount() >> 16) as u8,
                       (crc.amount() >> 24) as u8];
        Ok(copy(into, arr, &mut self.pos))
    }
}

impl<R: BufRead> Read for EncoderReaderBuf<R> {
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

impl<R: BufRead + Write> Write for EncoderReaderBuf<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}

impl<R: Read> DecoderReader<R> {
    /// Creates a new decoder from the given reader, immediately parsing the
    /// gzip header.
    ///
    /// If an error is encountered when parsing the gzip header, an error is
    /// returned.
    pub fn new(r: R) -> io::Result<DecoderReader<R>> {
        DecoderReaderBuf::new(BufReader::new(r)).map(|r| {
            DecoderReader { inner: r }
        })
    }

    /// Returns the header associated with this stream.
    pub fn header(&self) -> &Header {
        self.inner.header()
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

impl<R: Read> Read for DecoderReader<R> {
    fn read(&mut self, into: &mut [u8]) -> io::Result<usize> {
        self.inner.read(into)
    }
}

impl<R: Read + Write> Write for DecoderReader<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}

impl<R: Read> MultiDecoderReader<R> {
    /// Creates a new decoder from the given reader, immediately parsing the
    /// (first) gzip header. If the gzip stream contains multiple members all will
    /// be decoded.
    ///
    /// If an error is encountered when parsing the gzip header, an error is
    /// returned.
    pub fn new(r: R) -> io::Result<MultiDecoderReader<R>> {
        MultiDecoderReaderBuf::new(BufReader::new(r)).map(|r| {
            MultiDecoderReader { inner: r }
        })
    }

    /// Returns the current header associated with this stream.
    pub fn header(&self) -> &Header {
        self.inner.header()
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

impl<R: Read> Read for MultiDecoderReader<R> {
    fn read(&mut self, into: &mut [u8]) -> io::Result<usize> {
        self.inner.read(into)
    }
}

impl<R: Read + Write> Write for MultiDecoderReader<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}

impl<R: BufRead> DecoderReaderBuf<R> {
    /// Creates a new decoder from the given reader, immediately parsing the
    /// gzip header.
    ///
    /// If an error is encountered when parsing the gzip header, an error is
    /// returned.
    pub fn new(mut r: R) -> io::Result<DecoderReaderBuf<R>> {
        let header = try!(read_gz_header(&mut r));

        let flate = deflate::DecoderReaderBuf::new(r);
        return Ok(DecoderReaderBuf {
            inner: CrcReader::new(flate),
            header: header,
            finished: false,
        });
    }


    /// Returns the header associated with this stream.
    pub fn header(&self) -> &Header {
        &self.header
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

        let crc = ((buf[0] as u32) << 0) | ((buf[1] as u32) << 8) |
                  ((buf[2] as u32) << 16) |
                  ((buf[3] as u32) << 24);
        let amt = ((buf[4] as u32) << 0) | ((buf[5] as u32) << 8) |
                  ((buf[6] as u32) << 16) |
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

impl<R: BufRead> Read for DecoderReaderBuf<R> {
    fn read(&mut self, into: &mut [u8]) -> io::Result<usize> {
        match try!(self.inner.read(into)) {
            0 => {
                try!(self.finish());
                Ok(0)
            }
            n => Ok(n),
        }
    }
}

impl<R: BufRead + Write> Write for DecoderReaderBuf<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}

impl<R: BufRead> MultiDecoderReaderBuf<R> {
    /// Creates a new decoder from the given reader, immediately parsing the
    /// (first) gzip header. If the gzip stream contains multiple members all will
    /// be decoded.
    ///
    /// If an error is encountered when parsing the gzip header, an error is
    /// returned.
    pub fn new(mut r: R) -> io::Result<MultiDecoderReaderBuf<R>> {
        let header = try!(read_gz_header(&mut r));

        let flate = deflate::DecoderReaderBuf::new(r);
        return Ok(MultiDecoderReaderBuf {
            inner: CrcReader::new(flate),
            header: header,
            finished: false,
        });
    }


    /// Returns the current header associated with this stream.
    pub fn header(&self) -> &Header {
        &self.header
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

        let crc = ((buf[0] as u32) << 0) | ((buf[1] as u32) << 8) |
                  ((buf[2] as u32) << 16) |
                  ((buf[3] as u32) << 24);
        let amt = ((buf[4] as u32) << 0) | ((buf[5] as u32) << 8) |
                  ((buf[6] as u32) << 16) |
                  ((buf[7] as u32) << 24);
        if crc != self.inner.crc().sum() as u32 {
            return Err(corrupt());
        }
        if amt != self.inner.crc().amount() {
            return Err(corrupt());
        }
        let remaining = match self.inner.get_mut().get_mut().fill_buf() {
            Ok(b) => {
                if b.is_empty() {
                    self.finished = true;
                    return Ok(0);
                } else {
                    b.len()
                }
            },
            Err(e) => return Err(e)
        };

        let next_header = try!(read_gz_header(self.inner.get_mut().get_mut()));
        mem::replace(&mut self.header, next_header);
        self.inner.reset();
        self.inner.get_mut().reset_data();

        Ok(remaining)
    }
}

impl<R: BufRead> Read for MultiDecoderReaderBuf<R> {
    fn read(&mut self, into: &mut [u8]) -> io::Result<usize> {
        match try!(self.inner.read(into)) {
            0 => {
                match self.finish_member() {
                    Ok(0) => Ok(0),
                    Ok(_) => self.read(into),
                    Err(e) => Err(e)
                }
            },
            n => Ok(n),
        }
    }
}

impl<R: BufRead + Write> Write for MultiDecoderReaderBuf<R> {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.get_mut().write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.get_mut().flush()
    }
}

impl Header {
    /// Returns the `filename` field of this gzip stream's header, if present.
    pub fn filename(&self) -> Option<&[u8]> {
        self.filename.as_ref().map(|s| &s[..])
    }

    /// Returns the `extra` field of this gzip stream's header, if present.
    pub fn extra(&self) -> Option<&[u8]> {
        self.extra.as_ref().map(|s| &s[..])
    }

    /// Returns the `comment` field of this gzip stream's header, if present.
    pub fn comment(&self) -> Option<&[u8]> {
        self.comment.as_ref().map(|s| &s[..])
    }

    /// Returns the `mtime` field of this gzip stream's header, if present.
    pub fn mtime(&self) -> u32 {
        self.mtime
    }
}

fn corrupt() -> io::Error {
    io::Error::new(io::ErrorKind::InvalidInput,
                   "corrupt gzip stream does not have a matching checksum")
}

fn bad_header() -> io::Error {
    io::Error::new(io::ErrorKind::InvalidInput, "invalid gzip header")
}

fn read_le_u16<R: Read>(r: &mut R) -> io::Result<u16> {
    let mut b = [0; 2];
    try!(r.read_exact(&mut b));
    Ok((b[0] as u16) | ((b[1] as u16) << 8))
}

fn read_gz_header<R: Read>(r: &mut R) -> io::Result<Header> {
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
    let mtime = ((header[4] as u32) << 0) | ((header[5] as u32) << 8) |
        ((header[6] as u32) << 16) |
        ((header[7] as u32) << 24);
    let _xfl = header[8];
    let _os = header[9];

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

    Ok(Header {
        extra: extra,
        filename: filename,
        comment: comment,
        mtime: mtime,
    })
}

#[cfg(test)]
mod tests {
    use std::io::prelude::*;

    use super::{EncoderWriter, EncoderReader, DecoderReader, Builder};
    use Compression::Default;
    use rand::{thread_rng, Rng};

    #[test]
    fn roundtrip() {
        let mut e = EncoderWriter::new(Vec::new(), Default);
        e.write_all(b"foo bar baz").unwrap();
        let inner = e.finish().unwrap();
        let mut d = DecoderReader::new(&inner[..]).unwrap();
        let mut s = String::new();
        d.read_to_string(&mut s).unwrap();
        assert_eq!(s, "foo bar baz");
    }

    #[test]
    fn roundtrip_zero() {
        let e = EncoderWriter::new(Vec::new(), Default);
        let inner = e.finish().unwrap();
        let mut d = DecoderReader::new(&inner[..]).unwrap();
        let mut s = String::new();
        d.read_to_string(&mut s).unwrap();
        assert_eq!(s, "");
    }

    #[test]
    fn roundtrip_big() {
        let mut real = Vec::new();
        let mut w = EncoderWriter::new(Vec::new(), Default);
        let v = thread_rng().gen_iter::<u8>().take(1024).collect::<Vec<_>>();
        for _ in 0..200 {
            let to_write = &v[..thread_rng().gen_range(0, v.len())];
            real.extend(to_write.iter().map(|x| *x));
            w.write_all(to_write).unwrap();
        }
        let result = w.finish().unwrap();
        let mut r = DecoderReader::new(&result[..]).unwrap();
        let mut v = Vec::new();
        r.read_to_end(&mut v).unwrap();
        assert!(v == real);
    }

    #[test]
    fn roundtrip_big2() {
        let v = thread_rng()
                    .gen_iter::<u8>()
                    .take(1024 * 1024)
                    .collect::<Vec<_>>();
        let mut r = DecoderReader::new(EncoderReader::new(&v[..], Default))
                        .unwrap();
        let mut res = Vec::new();
        r.read_to_end(&mut res).unwrap();
        assert!(res == v);
    }

    #[test]
    fn fields() {
        let r = vec![0, 2, 4, 6];
        let e = Builder::new()
                    .filename(b"foo.rs")
                    .comment(b"bar")
                    .extra(vec![0, 1, 2, 3])
                    .read(&r[..], Default);
        let mut d = DecoderReader::new(e).unwrap();
        assert_eq!(d.header().filename(), Some(&b"foo.rs"[..]));
        assert_eq!(d.header().comment(), Some(&b"bar"[..]));
        assert_eq!(d.header().extra(), Some(&b"\x00\x01\x02\x03"[..]));
        let mut res = Vec::new();
        d.read_to_end(&mut res).unwrap();
        assert_eq!(res, vec![0, 2, 4, 6]);

    }

    #[test]
    fn keep_reading_after_end() {
        let mut e = EncoderWriter::new(Vec::new(), Default);
        e.write_all(b"foo bar baz").unwrap();
        let inner = e.finish().unwrap();
        let mut d = DecoderReader::new(&inner[..]).unwrap();
        let mut s = String::new();
        d.read_to_string(&mut s).unwrap();
        assert_eq!(s, "foo bar baz");
        d.read_to_string(&mut s).unwrap();
        assert_eq!(s, "foo bar baz");
    }

    #[test]
    fn qc_reader() {
        ::quickcheck::quickcheck(test as fn(_) -> _);

        fn test(v: Vec<u8>) -> bool {
            let r = EncoderReader::new(&v[..], Default);
            let mut r = DecoderReader::new(r).unwrap();
            let mut v2 = Vec::new();
            r.read_to_end(&mut v2).unwrap();
            v == v2
        }
    }

    #[test]
    fn flush_after_write() {
		let mut f = EncoderWriter::new(Vec::new(), Default);
		write!(f, "Hello world").unwrap();
		f.flush().unwrap();
    }
}
