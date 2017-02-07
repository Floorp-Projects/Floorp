use std::io::Read;
use std::io::Error as IoError;
use std::error::Error;
use std::fmt;
use std::convert::From;

use byteorder::{BigEndian, ReadBytesExt};
use num_traits;
use serde_crate as serde;
use serde_crate::de::value::ValueDeserializer;

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
pub enum DeserializeError {
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
    SizeLimit,
    Serde(serde::de::value::Error)
}

impl Error for DeserializeError {
    fn description(&self) -> &str {
        match *self {
            DeserializeError::IoError(ref err) => Error::description(err),
            DeserializeError::InvalidEncoding(ref ib) => ib.desc,
            DeserializeError::SizeLimit => "the size limit for decoding has been reached",
            DeserializeError::Serde(ref s) => s.description(),

        }
    }

    fn cause(&self) -> Option<&Error> {
        match *self {
            DeserializeError::IoError(ref err) => err.cause(),
            DeserializeError::InvalidEncoding(_) => None,
            DeserializeError::SizeLimit => None,
            DeserializeError::Serde(ref s) => s.cause(),
        }
    }
}

impl From<IoError> for DeserializeError {
    fn from(err: IoError) -> DeserializeError {
        DeserializeError::IoError(err)
    }
}

impl From<serde::de::value::Error> for DeserializeError {
    fn from(err: serde::de::value::Error) -> DeserializeError {
        DeserializeError::Serde(err)
    }
}

impl fmt::Display for DeserializeError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            DeserializeError::IoError(ref ioerr) =>
                write!(fmt, "IoError: {}", ioerr),
            DeserializeError::InvalidEncoding(ref ib) =>
                write!(fmt, "InvalidEncoding: {}", ib),
            DeserializeError::SizeLimit =>
                write!(fmt, "SizeLimit"),
            DeserializeError::Serde(ref s) =>
                s.fmt(fmt),
        }
    }
}

impl serde::de::Error for DeserializeError {
    fn custom<T: Into<String>>(desc: T) -> DeserializeError {
        DeserializeError::Serde(serde::de::value::Error::Custom(desc.into()))
    }

    fn end_of_stream() -> DeserializeError {
        DeserializeError::Serde(serde::de::value::Error::EndOfStream)
    }
}

pub type DeserializeResult<T> = Result<T, DeserializeError>;


/// A Deserializer that reads bytes from a buffer.
///
/// This struct should rarely be used.
/// In most cases, prefer the `decode_from` function.
///
/// ```rust,ignore
/// let d = Deserializer::new(&mut some_reader, SizeLimit::new());
/// serde::Deserialize::deserialize(&mut deserializer);
/// let bytes_read = d.bytes_read();
/// ```
pub struct Deserializer<'a, R: 'a> {
    reader: &'a mut R,
    size_limit: SizeLimit,
    read: u64
}

impl<'a, R: Read> Deserializer<'a, R> {
    pub fn new(r: &'a mut R, size_limit: SizeLimit) -> Deserializer<'a, R> {
        Deserializer {
            reader: r,
            size_limit: size_limit,
            read: 0
        }
    }

    /// Returns the number of bytes read from the contained Reader.
    pub fn bytes_read(&self) -> u64 {
        self.read
    }

    fn read_bytes(&mut self, count: u64) -> Result<(), DeserializeError> {
        self.read += count;
        match self.size_limit {
            SizeLimit::Infinite => Ok(()),
            SizeLimit::Bounded(x) if self.read <= x => Ok(()),
            SizeLimit::Bounded(_) => Err(DeserializeError::SizeLimit)
        }
    }

    fn read_type<T>(&mut self) -> Result<(), DeserializeError> {
        use std::mem::size_of;
        self.read_bytes(size_of::<T>() as u64)
    }

    fn read_string(&mut self) -> DeserializeResult<String> {
        let len = try!(serde::Deserialize::deserialize(self));
        try!(self.read_bytes(len));

        let mut buffer = Vec::new();
        try!(self.reader.by_ref().take(len as u64).read_to_end(&mut buffer));

        String::from_utf8(buffer).map_err(|err|
            DeserializeError::InvalidEncoding(InvalidEncoding {
                desc: "error while decoding utf8 string",
                detail: Some(format!("Deserialize error: {}", err))
            }))
    }
}

macro_rules! impl_nums {
    ($ty:ty, $dser_method:ident, $visitor_method:ident, $reader_method:ident) => {
        #[inline]
        fn $dser_method<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
            where V: serde::de::Visitor,
        {
            try!(self.read_type::<$ty>());
            let value = try!(self.reader.$reader_method::<BigEndian>());
            visitor.$visitor_method(value)
        }
    }
}


impl<'a, R: Read> serde::Deserializer for Deserializer<'a, R> {
    type Error = DeserializeError;

    #[inline]
    fn deserialize<V>(&mut self, _visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        let message = "bincode does not support Deserializer::deserialize";
        Err(DeserializeError::Serde(serde::de::value::Error::Custom(message.into())))
    }

    fn deserialize_bool<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        let value: u8 = try!(serde::Deserialize::deserialize(self));
        match value {
            1 => visitor.visit_bool(true),
            0 => visitor.visit_bool(false),
            value => {
                Err(DeserializeError::InvalidEncoding(InvalidEncoding {
                    desc: "invalid u8 when decoding bool",
                    detail: Some(format!("Expected 0 or 1, got {}", value))
                }))
            }
        }
    }

    impl_nums!(u16, deserialize_u16, visit_u16, read_u16);
    impl_nums!(u32, deserialize_u32, visit_u32, read_u32);
    impl_nums!(u64, deserialize_u64, visit_u64, read_u64);
    impl_nums!(i16, deserialize_i16, visit_i16, read_i16);
    impl_nums!(i32, deserialize_i32, visit_i32, read_i32);
    impl_nums!(i64, deserialize_i64, visit_i64, read_i64);
    impl_nums!(f32, deserialize_f32, visit_f32, read_f32);
    impl_nums!(f64, deserialize_f64, visit_f64, read_f64);


    #[inline]
    fn deserialize_u8<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        try!(self.read_type::<u8>());
        visitor.visit_u8(try!(self.reader.read_u8()))
    }

    #[inline]
    fn deserialize_usize<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        try!(self.read_type::<u64>());
        let value = try!(self.reader.read_u64::<BigEndian>());
        match num_traits::cast(value) {
            Some(value) => visitor.visit_usize(value),
            None => Err(DeserializeError::Serde(serde::de::value::Error::Custom("expected usize".into())))
        }
    }

    #[inline]
    fn deserialize_i8<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        try!(self.read_type::<i8>());
        visitor.visit_i8(try!(self.reader.read_i8()))
    }

    #[inline]
    fn deserialize_isize<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        try!(self.read_type::<i64>());
        let value = try!(self.reader.read_i64::<BigEndian>());
        match num_traits::cast(value) {
            Some(value) => visitor.visit_isize(value),
            None => Err(DeserializeError::Serde(serde::de::value::Error::Custom("expected isize".into()))),
        }
    }

    fn deserialize_unit<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_unit()
    }

    fn deserialize_char<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        use std::str;

        let error = DeserializeError::InvalidEncoding(InvalidEncoding {
            desc: "Invalid char encoding",
            detail: None
        });

        let mut buf = [0];

        let _ = try!(self.reader.read(&mut buf[..]));
        let first_byte = buf[0];
        let width = utf8_char_width(first_byte);
        if width == 1 { return visitor.visit_char(first_byte as char) }
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

        visitor.visit_char(res)
    }

    fn deserialize_str<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_str(&try!(self.read_string()))
    }

    fn deserialize_string<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_string(try!(self.read_string()))
    }

    fn deserialize_bytes<V>(&mut self, visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        self.deserialize_seq(visitor)
    }

    fn deserialize_enum<V>(&mut self,
                     _enum: &'static str,
                     _variants: &'static [&'static str],
                     mut visitor: V) -> Result<V::Value, Self::Error>
        where V: serde::de::EnumVisitor,
    {
        visitor.visit(self)
    }

    fn deserialize_tuple<V>(&mut self,
                      _len: usize,
                      mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        struct TupleVisitor<'a, 'b: 'a, R: Read + 'b>(&'a mut Deserializer<'b, R>);

        impl<'a, 'b: 'a, R: Read + 'b> serde::de::SeqVisitor for TupleVisitor<'a, 'b, R> {
            type Error = DeserializeError;

            fn visit<T>(&mut self) -> Result<Option<T>, Self::Error>
                where T: serde::de::Deserialize,
            {
                let value = try!(serde::Deserialize::deserialize(self.0));
                Ok(Some(value))
            }

            fn end(&mut self) -> Result<(), Self::Error> {
                Ok(())
            }
        }

        visitor.visit_seq(TupleVisitor(self))
    }

    fn deserialize_seq_fixed_size<V>(&mut self,
                            _: usize,
                            visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        self.deserialize_seq(visitor)
    }

    fn deserialize_option<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        let value: u8 = try!(serde::de::Deserialize::deserialize(self));
        match value {
            0 => visitor.visit_none(),
            1 => visitor.visit_some(self),
            _ => Err(DeserializeError::InvalidEncoding(InvalidEncoding {
                desc: "invalid tag when decoding Option",
                detail: Some(format!("Expected 0 or 1, got {}", value))
            })),
        }
    }

    fn deserialize_seq<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        struct SeqVisitor<'a, 'b: 'a, R: Read + 'b> {
            deserializer: &'a mut Deserializer<'b, R>,
            len: usize,
        }

        impl<'a, 'b: 'a, R: Read + 'b> serde::de::SeqVisitor for SeqVisitor<'a, 'b, R> {
            type Error = DeserializeError;

            fn visit<T>(&mut self) -> Result<Option<T>, Self::Error>
                where T: serde::de::Deserialize,
            {
                if self.len > 0 {
                    self.len -= 1;
                    let value = try!(serde::Deserialize::deserialize(self.deserializer));
                    Ok(Some(value))
                } else {
                    Ok(None)
                }
            }

            fn end(&mut self) -> Result<(), Self::Error> {
                if self.len == 0 {
                    Ok(())
                } else {
                    Err(DeserializeError::Serde(serde::de::value::Error::Custom("expected end".into())))
                }
            }
        }

        let len = try!(serde::Deserialize::deserialize(self));

        visitor.visit_seq(SeqVisitor { deserializer: self, len: len })
    }

    fn deserialize_map<V>(&mut self, mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        struct MapVisitor<'a, 'b: 'a, R: Read + 'b> {
            deserializer: &'a mut Deserializer<'b, R>,
            len: usize,
        }

        impl<'a, 'b: 'a, R: Read + 'b> serde::de::MapVisitor for MapVisitor<'a, 'b, R> {
            type Error = DeserializeError;

            fn visit_key<K>(&mut self) -> Result<Option<K>, Self::Error>
                where K: serde::de::Deserialize,
            {
                if self.len > 0 {
                    self.len -= 1;
                    let key = try!(serde::Deserialize::deserialize(self.deserializer));
                    Ok(Some(key))
                } else {
                    Ok(None)
                }
            }

            fn visit_value<V>(&mut self) -> Result<V, Self::Error>
                where V: serde::de::Deserialize,
            {
                let value = try!(serde::Deserialize::deserialize(self.deserializer));
                Ok(value)
            }

            fn end(&mut self) -> Result<(), Self::Error> {
                if self.len == 0 {
                    Ok(())
                } else {
                    Err(DeserializeError::Serde(serde::de::value::Error::Custom("expected end".into())))
                }
            }
        }

        let len = try!(serde::Deserialize::deserialize(self));

        visitor.visit_map(MapVisitor { deserializer: self, len: len })
    }

    fn deserialize_struct<V>(&mut self,
                       _name: &str,
                       fields: &'static [&'static str],
                       visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        self.deserialize_tuple(fields.len(), visitor)
    }

    fn deserialize_struct_field<V>(&mut self,
                                   _visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        let message = "bincode does not support Deserializer::deserialize_struct_field";
        Err(DeserializeError::Serde(serde::de::value::Error::Custom(message.into())))
    }

    fn deserialize_newtype_struct<V>(&mut self,
                               _name: &str,
                               mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_unit_struct<V>(&mut self,
                                  _name: &'static str,
                                  mut visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_unit()
    }

    fn deserialize_tuple_struct<V>(&mut self,
                                   _name: &'static str,
                                   len: usize,
                                   visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        self.deserialize_tuple(len, visitor)
    }

    fn deserialize_ignored_any<V>(&mut self,
                                  _visitor: V) -> DeserializeResult<V::Value>
        where V: serde::de::Visitor,
    {
        let message = "bincode does not support Deserializer::deserialize_ignored_any";
        Err(DeserializeError::Serde(serde::de::value::Error::Custom(message.into())))
    }
}

impl<'a, R: Read> serde::de::VariantVisitor for Deserializer<'a, R> {
    type Error = DeserializeError;

    fn visit_variant<V>(&mut self) -> Result<V, Self::Error>
        where V: serde::Deserialize,
    {
        let index: u32 = try!(serde::Deserialize::deserialize(self));
        let mut deserializer = (index as usize).into_deserializer();
        let attempt: Result<V, serde::de::value::Error> = serde::Deserialize::deserialize(&mut deserializer);
        Ok(try!(attempt))
    }

    fn visit_unit(&mut self) -> Result<(), Self::Error> {
        Ok(())
    }

    fn visit_newtype<T>(&mut self) -> Result<T, Self::Error>
        where T: serde::de::Deserialize,
    {
        serde::de::Deserialize::deserialize(self)
    }

    fn visit_tuple<V>(&mut self,
                      len: usize,
                      visitor: V) -> Result<V::Value, Self::Error>
        where V: serde::de::Visitor,
    {
        serde::de::Deserializer::deserialize_tuple(self, len, visitor)
    }

    fn visit_struct<V>(&mut self,
                       fields: &'static [&'static str],
                       visitor: V) -> Result<V::Value, Self::Error>
        where V: serde::de::Visitor,
    {
        serde::de::Deserializer::deserialize_tuple(self, fields.len(), visitor)
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
