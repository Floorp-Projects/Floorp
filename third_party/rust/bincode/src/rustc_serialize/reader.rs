use std::io::Read;
use std::io::Error as IoError;
use std::error::Error;
use std::fmt;
use std::convert::From;

use rustc_serialize_crate::Decoder;

use byteorder::{BigEndian, ReadBytesExt};
use ::SizeLimit;

#[derive(Eq, PartialEq, Clone, Debug)]
pub struct InvalidEncoding {
    pub desc: &'static str,
    pub detail: Option<String>,
}

impl fmt::Display for InvalidEncoding {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            InvalidEncoding { detail: None, desc } =>
                write!(fmt, "{}", desc),
            InvalidEncoding { detail: Some(ref detail), desc } =>
                write!(fmt, "{} ({})", desc, detail)
        }
    }
}

/// An error that can be produced during decoding.
///
/// If decoding from a Buffer, assume that the buffer has been left
/// in an invalid state.
#[derive(Debug)]
pub enum DecodingError {
    /// If the error stems from the reader that is being used
    /// during decoding, that error will be stored and returned here.
    IoError(IoError),
    /// If the bytes in the reader are not decodable because of an invalid
    /// encoding, this error will be returned.  This error is only possible
    /// if a stream is corrupted.  A stream produced from `encode` or `encode_into`
    /// should **never** produce an InvalidEncoding error.
    InvalidEncoding(InvalidEncoding),
    /// If decoding a message takes more than the provided size limit, this
    /// error is returned.
    SizeLimit
}

impl fmt::Display for DecodingError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            DecodingError::IoError(ref ioerr) =>
                write!(fmt, "IoError: {}", ioerr),
            DecodingError::InvalidEncoding(ref ib) =>
                write!(fmt, "InvalidEncoding: {}", ib),
            DecodingError::SizeLimit =>
                write!(fmt, "SizeLimit")
        }
    }
}

pub type DecodingResult<T> = Result<T, DecodingError>;

fn wrap_io(err: IoError) -> DecodingError {
    DecodingError::IoError(err)
}

impl Error for DecodingError {
    fn description(&self) -> &str {
        match *self {
            DecodingError::IoError(ref err) => Error::description(err),
            DecodingError::InvalidEncoding(ref ib) => ib.desc,
            DecodingError::SizeLimit => "the size limit for decoding has been reached"
        }
    }

    fn cause(&self) -> Option<&Error> {
        match *self {
            DecodingError::IoError(ref err)     => err.cause(),
            DecodingError::InvalidEncoding(_) => None,
            DecodingError::SizeLimit => None
        }
    }
}

impl From<IoError> for DecodingError {
    fn from(err: IoError) -> DecodingError {
        DecodingError::IoError(err)
    }
}

/// A Decoder that reads bytes from a buffer.
///
/// This struct should rarely be used.
/// In most cases, prefer the `decode_from` function.
///
/// ```rust,ignore
/// let dr = bincode::rustc_serialize::DecoderReader::new(&mut some_reader, SizeLimit::Infinite);
/// let result: T = Decodable::decode(&mut dr);
/// let bytes_read = dr.bytes_read();
/// ```
pub struct DecoderReader<'a, R: 'a> {
    reader: &'a mut R,
    size_limit: SizeLimit,
    read: u64
}

impl<'a, R: Read> DecoderReader<'a, R> {
    pub fn new(r: &'a mut R, size_limit: SizeLimit) -> DecoderReader<'a, R> {
        DecoderReader {
            reader: r,
            size_limit: size_limit,
            read: 0
        }
    }

    /// Returns the number of bytes read from the contained Reader.
    pub fn bytes_read(&self) -> u64 {
        self.read
    }
}

impl <'a, A> DecoderReader<'a, A> {
    fn read_bytes(&mut self, count: u64) -> Result<(), DecodingError> {
        self.read = match self.read.checked_add(count) {
            Some(read) => read,
            None => return Err(DecodingError::SizeLimit),
        };
        match self.size_limit {
            SizeLimit::Infinite => Ok(()),
            SizeLimit::Bounded(x) if self.read <= x => Ok(()),
            SizeLimit::Bounded(_) => Err(DecodingError::SizeLimit)
        }
    }

    fn read_type<T>(&mut self) -> Result<(), DecodingError> {
        use std::mem::size_of;
        self.read_bytes(size_of::<T>() as u64)
    }
}

impl<'a, R: Read> Decoder for DecoderReader<'a, R> {
    type Error = DecodingError;

    fn read_nil(&mut self) -> DecodingResult<()> {
        Ok(())
    }
    fn read_usize(&mut self) -> DecodingResult<usize> {
        Ok(try!(self.read_u64().map(|x| x as usize)))
    }
    fn read_u64(&mut self) -> DecodingResult<u64> {
        try!(self.read_type::<u64>());
        self.reader.read_u64::<BigEndian>().map_err(wrap_io)
    }
    fn read_u32(&mut self) -> DecodingResult<u32> {
        try!(self.read_type::<u32>());
        self.reader.read_u32::<BigEndian>().map_err(wrap_io)
    }
    fn read_u16(&mut self) -> DecodingResult<u16> {
        try!(self.read_type::<u16>());
        self.reader.read_u16::<BigEndian>().map_err(wrap_io)
    }
    fn read_u8(&mut self) -> DecodingResult<u8> {
        try!(self.read_type::<u8>());
        self.reader.read_u8().map_err(wrap_io)
    }
    fn read_isize(&mut self) -> DecodingResult<isize> {
        self.read_i64().map(|x| x as isize)
    }
    fn read_i64(&mut self) -> DecodingResult<i64> {
        try!(self.read_type::<i64>());
        self.reader.read_i64::<BigEndian>().map_err(wrap_io)
    }
    fn read_i32(&mut self) -> DecodingResult<i32> {
        try!(self.read_type::<i32>());
        self.reader.read_i32::<BigEndian>().map_err(wrap_io)
    }
    fn read_i16(&mut self) -> DecodingResult<i16> {
        try!(self.read_type::<i16>());
        self.reader.read_i16::<BigEndian>().map_err(wrap_io)
    }
    fn read_i8(&mut self) -> DecodingResult<i8> {
        try!(self.read_type::<i8>());
        self.reader.read_i8().map_err(wrap_io)
    }
    fn read_bool(&mut self) -> DecodingResult<bool> {
        let x = try!(self.read_i8());
        match x {
            1 => Ok(true),
            0 => Ok(false),
            _ => Err(DecodingError::InvalidEncoding(InvalidEncoding{
                desc: "invalid u8 when decoding bool",
                detail: Some(format!("Expected 0 or 1, got {}", x))
            })),
        }
    }
    fn read_f64(&mut self) -> DecodingResult<f64> {
        try!(self.read_type::<f64>());
        self.reader.read_f64::<BigEndian>().map_err(wrap_io)
    }
    fn read_f32(&mut self) -> DecodingResult<f32> {
        try!(self.read_type::<f32>());
        self.reader.read_f32::<BigEndian>().map_err(wrap_io)
    }
    fn read_char(&mut self) -> DecodingResult<char> {
        use std::str;

        let error = DecodingError::InvalidEncoding(InvalidEncoding {
            desc: "Invalid char encoding",
            detail: None
        });

        let mut buf = [0];

        let _ = try!(self.reader.read(&mut buf[..]));
        let first_byte = buf[0];
        let width = utf8_char_width(first_byte);
        if width == 1 { return Ok(first_byte as char) }
        if width == 0 { return Err(error)}

        let mut buf = [first_byte, 0, 0, 0];
        {
            let mut start = 1;
            while start < width {
                match try!(self.reader.read(&mut buf[start .. width])) {
                    n if n == width - start => break,
                    n if n < width - start => { start += n; }
                    _ => return Err(error)
                }
            }
        }

        let res = try!(match str::from_utf8(&buf[..width]).ok() {
            Some(s) => Ok(s.chars().next().unwrap()),
            None => Err(error)
        });

        try!(self.read_bytes(res.len_utf8() as u64));
        Ok(res)
    }

    fn read_str(&mut self) -> DecodingResult<String> {
        let len = try!(self.read_usize());
        try!(self.read_bytes(len as u64));

        let mut buff = Vec::new();
        try!(self.reader.by_ref().take(len as u64).read_to_end(&mut buff));
        match String::from_utf8(buff) {
            Ok(s) => Ok(s),
            Err(err) => Err(DecodingError::InvalidEncoding(InvalidEncoding {
                desc: "error while decoding utf8 string",
                detail: Some(format!("Decoding error: {}", err))
            })),
        }
    }
    fn read_enum<T, F>(&mut self, _: &str, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn read_enum_variant<T, F>(&mut self, names: &[&str], mut f: F) -> DecodingResult<T>
        where F: FnMut(&mut DecoderReader<'a, R>, usize) -> DecodingResult<T>
    {
        let id = try!(self.read_u32());
        let id = id as usize;
        if id >= names.len() {
                Err(DecodingError::InvalidEncoding(InvalidEncoding {
                    desc: "out of bounds tag when reading enum variant",
                    detail: Some(format!("Expected tag < {}, got {}", names.len(), id))
                }))
            } else {
                f(self, id)
            }
    }
    fn read_enum_variant_arg<T, F>(&mut self, _: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn read_enum_struct_variant<T, F>(&mut self, names: &[&str], f: F) -> DecodingResult<T>
        where F: FnMut(&mut DecoderReader<'a, R>, usize) -> DecodingResult<T>
    {
        self.read_enum_variant(names, f)
    }
    fn read_enum_struct_variant_field<T, F>(&mut self,
                                            _: &str,
                                            f_idx: usize,
                                            f: F)
                                            -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        self.read_enum_variant_arg(f_idx, f)
    }
    fn read_struct<T, F>(&mut self, _: &str, _: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn read_struct_field<T, F>(&mut self, _: &str, _: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn read_tuple<T, F>(&mut self, _: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn read_tuple_arg<T, F>(&mut self, _: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn read_tuple_struct<T, F>(&mut self, _: &str, len: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        self.read_tuple(len, f)
    }
    fn read_tuple_struct_arg<T, F>(&mut self, a_idx: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        self.read_tuple_arg(a_idx, f)
    }
    fn read_option<T, F>(&mut self, mut f: F) -> DecodingResult<T>
        where F: FnMut(&mut DecoderReader<'a, R>, bool) -> DecodingResult<T>
    {
        let x = try!(self.read_u8());
        match x {
                1 => f(self, true),
                0 => f(self, false),
                _ => Err(DecodingError::InvalidEncoding(InvalidEncoding {
                    desc: "invalid tag when decoding Option",
                    detail: Some(format!("Expected 0 or 1, got {}", x))
                })),
            }
    }
    fn read_seq<T, F>(&mut self, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>, usize) -> DecodingResult<T>
    {
        let len = try!(self.read_usize());
        f(self, len)
    }
    fn read_seq_elt<T, F>(&mut self, _: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn read_map<T, F>(&mut self, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>, usize) -> DecodingResult<T>
    {
        let len = try!(self.read_usize());
        f(self, len)
    }
    fn read_map_elt_key<T, F>(&mut self, _: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn read_map_elt_val<T, F>(&mut self, _: usize, f: F) -> DecodingResult<T>
        where F: FnOnce(&mut DecoderReader<'a, R>) -> DecodingResult<T>
    {
        f(self)
    }
    fn error(&mut self, err: &str) -> DecodingError {
        DecodingError::InvalidEncoding(InvalidEncoding {
            desc: "user-induced error",
            detail: Some(err.to_string()),
        })
    }
}

static UTF8_CHAR_WIDTH: [u8; 256] = [
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x1F
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x3F
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x5F
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // 0x7F
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0x9F
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0xBF
0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // 0xDF
3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, // 0xEF
4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0, // 0xFF
];

fn utf8_char_width(b: u8) -> usize {
    UTF8_CHAR_WIDTH[b as usize] as usize
}
