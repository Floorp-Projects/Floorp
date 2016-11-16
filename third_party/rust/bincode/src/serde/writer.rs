use std::error::Error;
use std::fmt;
use std::io::Error as IoError;
use std::io::Write;
use std::u32;

use serde_crate as serde;

use byteorder::{BigEndian, WriteBytesExt};

pub type SerializeResult<T> = Result<T, SerializeError>;


/// An error that can be produced during encoding.
#[derive(Debug)]
pub enum SerializeError {
    /// An error originating from the underlying `Writer`.
    IoError(IoError),
    /// An object could not be encoded with the given size limit.
    ///
    /// This error is returned before any bytes are written to the
    /// output `Writer`.
    SizeLimit,
    /// A custom error message
    Custom(String)
}

/// An Serializer that encodes values directly into a Writer.
///
/// This struct should not be used often.
/// For most cases, prefer the `encode_into` function.
pub struct Serializer<'a, W: 'a> {
    writer: &'a mut W,
}

fn wrap_io(err: IoError) -> SerializeError {
    SerializeError::IoError(err)
}

impl serde::ser::Error for SerializeError {
    fn custom<T: Into<String>>(msg: T) -> Self {
        SerializeError::Custom(msg.into())
    }
}

impl fmt::Display for SerializeError {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match *self {
            SerializeError::IoError(ref err) => write!(f, "IoError: {}", err),
            SerializeError::Custom(ref s) => write!(f, "Custom Error {}", s),
            SerializeError::SizeLimit => write!(f, "SizeLimit"),
        }
    }
}

impl Error for SerializeError {
    fn description(&self) -> &str {
        match *self {
            SerializeError::IoError(ref err) => Error::description(err),
            SerializeError::SizeLimit => "the size limit for decoding has been reached",
            SerializeError::Custom(_) => "a custom serialization error was reported",
        }
    }

    fn cause(&self) -> Option<&Error> {
        match *self {
            SerializeError::IoError(ref err) => err.cause(),
            SerializeError::SizeLimit => None,
            SerializeError::Custom(_) => None,
        }
    }
}

impl<'a, W: Write> Serializer<'a, W> {
    pub fn new(w: &'a mut W) -> Serializer<'a, W> {
        Serializer {
            writer: w,
        }
    }

    fn add_enum_tag(&mut self, tag: usize) -> SerializeResult<()> {
        if tag > u32::MAX as usize {
            panic!("Variant tag doesn't fit in a u32")
        }

        serde::Serializer::serialize_u32(self, tag as u32)
    }
}

impl<'a, W: Write> serde::Serializer for Serializer<'a, W> {
    type Error = SerializeError;
    type SeqState = ();
    type TupleState = ();
    type TupleStructState = ();
    type TupleVariantState = ();
    type MapState = ();
    type StructState = ();
    type StructVariantState = ();

    fn serialize_unit(&mut self) -> SerializeResult<()> { Ok(()) }

    fn serialize_unit_struct(&mut self, _: &'static str) -> SerializeResult<()> { Ok(()) }

    fn serialize_bool(&mut self, v: bool) -> SerializeResult<()> {
        self.writer.write_u8(if v {1} else {0}).map_err(wrap_io)
    }

    fn serialize_u8(&mut self, v: u8) -> SerializeResult<()> {
        self.writer.write_u8(v).map_err(wrap_io)
    }

    fn serialize_u16(&mut self, v: u16) -> SerializeResult<()> {
        self.writer.write_u16::<BigEndian>(v).map_err(wrap_io)
    }

    fn serialize_u32(&mut self, v: u32) -> SerializeResult<()> {
        self.writer.write_u32::<BigEndian>(v).map_err(wrap_io)
    }

    fn serialize_u64(&mut self, v: u64) -> SerializeResult<()> {
        self.writer.write_u64::<BigEndian>(v).map_err(wrap_io)
    }

    fn serialize_usize(&mut self, v: usize) -> SerializeResult<()> {
        self.serialize_u64(v as u64)
    }

    fn serialize_i8(&mut self, v: i8) -> SerializeResult<()> {
        self.writer.write_i8(v).map_err(wrap_io)
    }

    fn serialize_i16(&mut self, v: i16) -> SerializeResult<()> {
        self.writer.write_i16::<BigEndian>(v).map_err(wrap_io)
    }

    fn serialize_i32(&mut self, v: i32) -> SerializeResult<()> {
        self.writer.write_i32::<BigEndian>(v).map_err(wrap_io)
    }

    fn serialize_i64(&mut self, v: i64) -> SerializeResult<()> {
        self.writer.write_i64::<BigEndian>(v).map_err(wrap_io)
    }

    fn serialize_isize(&mut self, v: isize) -> SerializeResult<()> {
        self.serialize_i64(v as i64)
    }

    fn serialize_f32(&mut self, v: f32) -> SerializeResult<()> {
        self.writer.write_f32::<BigEndian>(v).map_err(wrap_io)
    }

    fn serialize_f64(&mut self, v: f64) -> SerializeResult<()> {
        self.writer.write_f64::<BigEndian>(v).map_err(wrap_io)
    }

    fn serialize_str(&mut self, v: &str) -> SerializeResult<()> {
        try!(self.serialize_usize(v.len()));
        self.writer.write_all(v.as_bytes()).map_err(SerializeError::IoError)
    }

    fn serialize_char(&mut self, c: char) -> SerializeResult<()> {
        self.writer.write_all(encode_utf8(c).as_slice()).map_err(SerializeError::IoError)
    }

    fn serialize_bytes(&mut self, v: &[u8]) -> SerializeResult<()> {
        let mut state = try!(self.serialize_seq(Some(v.len())));
        for c in v {
            try!(self.serialize_seq_elt(&mut state, c));
        }
        self.serialize_seq_end(state)
    }

    fn serialize_none(&mut self) -> SerializeResult<()> {
        self.writer.write_u8(0).map_err(wrap_io)
    }

    fn serialize_some<T>(&mut self, v: T) -> SerializeResult<()>
        where T: serde::Serialize,
    {
        try!(self.writer.write_u8(1).map_err(wrap_io));
        v.serialize(self)
    }

    fn serialize_seq(&mut self, len: Option<usize>) -> SerializeResult<()> {
        let len = len.expect("do not know how to serialize a sequence with no length");
        self.serialize_usize(len)
    }

    fn serialize_seq_elt<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_seq_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_seq_fixed_size(&mut self, len: usize) -> SerializeResult<()> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple(&mut self, _len: usize) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_tuple_elt<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_tuple_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_tuple_struct(&mut self, _name: &'static str, _len: usize) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_tuple_struct_elt<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_tuple_struct_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_tuple_variant(&mut self,
                              _name: &'static str,
                              variant_index: usize,
                              _variant: &'static str,
                              _len: usize) -> SerializeResult<()>
    {
        self.add_enum_tag(variant_index)
    }

    fn serialize_tuple_variant_elt<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_tuple_variant_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_map(&mut self, len: Option<usize>) -> SerializeResult<()> {
        let len = len.expect("do not know how to serialize a map with no length");
        self.serialize_usize(len)
    }

    fn serialize_map_key<K>(&mut self, _: &mut (), key: K) -> SerializeResult<()>
        where K: serde::Serialize,
    {
        key.serialize(self)
    }

    fn serialize_map_value<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_map_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_struct(&mut self, _name: &'static str, _len: usize) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_struct_elt<V>(&mut self, _: &mut (), _key: &'static str, value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_struct_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_struct_variant(&mut self,
                               _name: &'static str,
                               variant_index: usize,
                               _variant: &'static str,
                               _len: usize) -> SerializeResult<()>
    {
        self.add_enum_tag(variant_index)
    }

    fn serialize_struct_variant_elt<V>(&mut self, _: &mut (), _key: &'static str, value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_struct_variant_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_newtype_struct<T>(&mut self,
                               _name: &'static str,
                               value: T) -> SerializeResult<()>
        where T: serde::ser::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_newtype_variant<T>(&mut self,
                               _name: &'static str,
                               variant_index: usize,
                               _variant: &'static str,
                               value: T) -> SerializeResult<()>
        where T: serde::ser::Serialize,
    {
        try!(self.add_enum_tag(variant_index));
        value.serialize(self)
    }

    fn serialize_unit_variant(&mut self,
                          _name: &'static str,
                          variant_index: usize,
                          _variant: &'static str) -> SerializeResult<()> {
        self.add_enum_tag(variant_index)
    }
}

pub struct SizeChecker {
    pub size_limit: u64,
    pub written: u64
}

impl SizeChecker {
    pub fn new(limit: u64) -> SizeChecker {
        SizeChecker {
            size_limit: limit,
            written: 0
        }
    }

    fn add_raw(&mut self, size: usize) -> SerializeResult<()> {
        self.written += size as u64;
        if self.written <= self.size_limit {
            Ok(())
        } else {
            Err(SerializeError::SizeLimit)
        }
    }

    fn add_value<T>(&mut self, t: T) -> SerializeResult<()> {
        use std::mem::size_of_val;
        self.add_raw(size_of_val(&t))
    }

    fn add_enum_tag(&mut self, tag: usize) -> SerializeResult<()> {
        if tag > u32::MAX as usize {
            panic!("Variant tag doesn't fit in a u32")
        }

        self.add_value(tag as u32)
    }
}

impl serde::Serializer for SizeChecker {
    type Error = SerializeError;
    type SeqState = ();
    type TupleState = ();
    type TupleStructState = ();
    type TupleVariantState = ();
    type MapState = ();
    type StructState = ();
    type StructVariantState = ();

    fn serialize_unit(&mut self) -> SerializeResult<()> { Ok(()) }

    fn serialize_unit_struct(&mut self, _: &'static str) -> SerializeResult<()> { Ok(()) }

    fn serialize_bool(&mut self, _: bool) -> SerializeResult<()> {
        self.add_value(0 as u8)
    }

    fn serialize_u8(&mut self, v: u8) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_u16(&mut self, v: u16) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_u32(&mut self, v: u32) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_u64(&mut self, v: u64) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_usize(&mut self, v: usize) -> SerializeResult<()> {
        self.serialize_u64(v as u64)
    }

    fn serialize_i8(&mut self, v: i8) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_i16(&mut self, v: i16) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_i32(&mut self, v: i32) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_i64(&mut self, v: i64) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_isize(&mut self, v: isize) -> SerializeResult<()> {
        self.serialize_i64(v as i64)
    }

    fn serialize_f32(&mut self, v: f32) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_f64(&mut self, v: f64) -> SerializeResult<()> {
        self.add_value(v)
    }

    fn serialize_str(&mut self, v: &str) -> SerializeResult<()> {
        try!(self.add_value(0 as u64));
        self.add_raw(v.len())
    }

    fn serialize_char(&mut self, c: char) -> SerializeResult<()> {
        self.add_raw(encode_utf8(c).as_slice().len())
    }

    fn serialize_bytes(&mut self, v: &[u8]) -> SerializeResult<()> {
        let mut state = try!(self.serialize_seq(Some(v.len())));
        for c in v {
            try!(self.serialize_seq_elt(&mut state, c));
        }
        self.serialize_seq_end(state)
    }

    fn serialize_none(&mut self) -> SerializeResult<()> {
        self.add_value(0 as u8)
    }

    fn serialize_some<T>(&mut self, v: T) -> SerializeResult<()>
        where T: serde::Serialize,
    {
        try!(self.add_value(1 as u8));
        v.serialize(self)
    }

    fn serialize_seq(&mut self, len: Option<usize>) -> SerializeResult<()> {
        let len = len.expect("do not know how to serialize a sequence with no length");

        self.serialize_usize(len)
    }

    fn serialize_seq_elt<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_seq_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_seq_fixed_size(&mut self, len: usize) -> SerializeResult<()> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple(&mut self, _len: usize) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_tuple_elt<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_tuple_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_tuple_struct(&mut self, _name: &'static str, _len: usize) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_tuple_struct_elt<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_tuple_struct_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_tuple_variant(&mut self,
                         _name: &'static str,
                         variant_index: usize,
                         _variant: &'static str,
                         _len: usize) -> SerializeResult<()>
    {
        self.add_enum_tag(variant_index)
    }

    fn serialize_tuple_variant_elt<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_tuple_variant_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_map(&mut self, len: Option<usize>) -> SerializeResult<()>
    {
        let len = len.expect("do not know how to serialize a map with no length");

        self.serialize_usize(len)
    }

    fn serialize_map_key<K>(&mut self, _: &mut (), key: K) -> SerializeResult<()>
        where K: serde::Serialize,
    {
        key.serialize(self)
    }

    fn serialize_map_value<V>(&mut self, _: &mut (), value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_map_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_struct(&mut self, _name: &'static str, _len: usize) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_struct_elt<V>(&mut self, _: &mut (), _key: &'static str, value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_struct_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_struct_variant(&mut self,
                               _name: &'static str,
                               variant_index: usize,
                               _variant: &'static str,
                               _len: usize) -> SerializeResult<()>
    {
        self.add_enum_tag(variant_index)
    }

    fn serialize_struct_variant_elt<V>(&mut self, _: &mut (), _field: &'static str, value: V) -> SerializeResult<()>
        where V: serde::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_struct_variant_end(&mut self, _: ()) -> SerializeResult<()> {
        Ok(())
    }

    fn serialize_newtype_struct<V: serde::Serialize>(&mut self, _name: &'static str, v: V) -> SerializeResult<()> {
        v.serialize(self)
    }

    fn serialize_unit_variant(&mut self,
                          _name: &'static str,
                          variant_index: usize,
                          _variant: &'static str) -> SerializeResult<()> {
        self.add_enum_tag(variant_index)
    }

    fn serialize_newtype_variant<V: serde::Serialize>(&mut self,
                               _name: &'static str,
                               variant_index: usize,
                               _variant: &'static str,
                               value: V) -> SerializeResult<()>
    {
        try!(self.add_enum_tag(variant_index));
        value.serialize(self)
    }
}

const TAG_CONT: u8    = 0b1000_0000;
const TAG_TWO_B: u8   = 0b1100_0000;
const TAG_THREE_B: u8 = 0b1110_0000;
const TAG_FOUR_B: u8  = 0b1111_0000;
const MAX_ONE_B: u32   =     0x80;
const MAX_TWO_B: u32   =    0x800;
const MAX_THREE_B: u32 =  0x10000;

fn encode_utf8(c: char) -> EncodeUtf8 {
    let code = c as u32;
    let mut buf = [0; 4];
    let pos = if code < MAX_ONE_B {
        buf[3] = code as u8;
        3
    } else if code < MAX_TWO_B {
        buf[2] = (code >> 6 & 0x1F) as u8 | TAG_TWO_B;
        buf[3] = (code & 0x3F) as u8 | TAG_CONT;
        2
    } else if code < MAX_THREE_B {
        buf[1] = (code >> 12 & 0x0F) as u8 | TAG_THREE_B;
        buf[2] = (code >>  6 & 0x3F) as u8 | TAG_CONT;
        buf[3] = (code & 0x3F) as u8 | TAG_CONT;
        1
    } else {
        buf[0] = (code >> 18 & 0x07) as u8 | TAG_FOUR_B;
        buf[1] = (code >> 12 & 0x3F) as u8 | TAG_CONT;
        buf[2] = (code >>  6 & 0x3F) as u8 | TAG_CONT;
        buf[3] = (code & 0x3F) as u8 | TAG_CONT;
        0
    };
    EncodeUtf8 { buf: buf, pos: pos }
}

struct EncodeUtf8 {
    buf: [u8; 4],
    pos: usize,
}

impl EncodeUtf8 {
    fn as_slice(&self) -> &[u8] {
        &self.buf[self.pos..]
    }
}
