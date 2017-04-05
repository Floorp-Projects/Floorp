use std::cmp;
use std::io::Read;
use std::marker::PhantomData;

use byteorder::{ReadBytesExt, ByteOrder};
use serde_crate as serde;
use serde_crate::de::value::ValueDeserializer;
use serde_crate::de::Error as DeError;
use ::SizeLimit;
use super::{Result, Error, ErrorKind};

const BLOCK_SIZE: usize = 65536;

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
pub struct Deserializer<R, S: SizeLimit, E: ByteOrder> {
    reader: R,
    size_limit: S,
    read: u64,
    _phantom: PhantomData<E>,
}

impl<R: Read, E: ByteOrder, S: SizeLimit> Deserializer<R, S, E> {
    pub fn new(r: R, size_limit: S) -> Deserializer<R, S, E> {
        Deserializer {
            reader: r,
            size_limit: size_limit,
            read: 0,
            _phantom: PhantomData
        }
    }

    /// Returns the number of bytes read from the contained Reader.
    pub fn bytes_read(&self) -> u64 {
        self.read
    }

    fn read_bytes(&mut self, count: u64) -> Result<()> {
        self.size_limit.add(count)
    }

    fn read_type<T>(&mut self) -> Result<()> {
        use std::mem::size_of;
        self.read_bytes(size_of::<T>() as u64)
    }

    fn read_vec(&mut self) -> Result<Vec<u8>> {
        let mut len: usize = try!(serde::Deserialize::deserialize(&mut *self));

        let mut result = Vec::new();
        let mut off = 0;
        while len > 0 {
            let reserve = cmp::min(len, BLOCK_SIZE);
            try!(self.read_bytes(reserve as u64));
            unsafe {
                result.reserve(reserve);
                result.set_len(off + reserve);
            }
            try!(self.reader.read_exact(&mut result[off..]));
            len -= reserve;
            off += reserve;
        }
        Ok(result)
    }

    fn read_string(&mut self) -> Result<String> {
        String::from_utf8(try!(self.read_vec())).map_err(|err|
            ErrorKind::InvalidEncoding{
                desc: "error while decoding utf8 string",
                detail: Some(format!("Deserialize error: {}", err))
            }.into())
    }
}

macro_rules! impl_nums {
    ($ty:ty, $dser_method:ident, $visitor_method:ident, $reader_method:ident) => {
        #[inline]
        fn $dser_method<V>(self, visitor: V) -> Result<V::Value>
            where V: serde::de::Visitor,
        {
            try!(self.read_type::<$ty>());
            let value = try!(self.reader.$reader_method::<E>());
            visitor.$visitor_method(value)
        }
    }
}

impl<'a, R, S, E> serde::Deserializer for &'a mut Deserializer<R, S, E>
where R: Read, S: SizeLimit, E: ByteOrder {
    type Error = Error;

    #[inline]
    fn deserialize<V>(self, _visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        let message = "bincode does not support Deserializer::deserialize";
        Err(Error::custom(message))
    }

    fn deserialize_bool<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        let value: u8 = try!(serde::Deserialize::deserialize(self));
        match value {
            1 => visitor.visit_bool(true),
            0 => visitor.visit_bool(false),
            value => {
                Err(ErrorKind::InvalidEncoding{
                    desc: "invalid u8 when decoding bool",
                    detail: Some(format!("Expected 0 or 1, got {}", value))
                }.into())
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
    fn deserialize_u8<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        try!(self.read_type::<u8>());
        visitor.visit_u8(try!(self.reader.read_u8()))
    }

    #[inline]
    fn deserialize_i8<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        try!(self.read_type::<i8>());
        visitor.visit_i8(try!(self.reader.read_i8()))
    }

    fn deserialize_unit<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_unit()
    }

    fn deserialize_char<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        use std::str;

        let error = || {
            ErrorKind::InvalidEncoding{
                desc: "Invalid char encoding",
                detail: None,
            }.into()
        };

        let mut buf = [0u8; 4];

        // Look at the first byte to see how many bytes must be read
        let _ = try!(self.reader.read_exact(&mut buf[..1]));
        let width = utf8_char_width(buf[0]);
        if width == 1 { return visitor.visit_char(buf[0] as char) }
        if width == 0 { return Err(error())}

        if self.reader.read_exact(&mut buf[1..width]).is_err() {
            return Err(error());
        }

        let res = try!(str::from_utf8(&buf[..width]).ok().and_then(|s| s.chars().next()).ok_or(error()));
        visitor.visit_char(res)
    }

    fn deserialize_str<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_str(&try!(self.read_string()))
    }

    fn deserialize_string<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_string(try!(self.read_string()))
    }

    fn deserialize_bytes<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_bytes(&try!(self.read_vec()))
    }

    fn deserialize_byte_buf<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_byte_buf(try!(self.read_vec()))
    }

    fn deserialize_enum<V>(self,
                     _enum: &'static str,
                     _variants: &'static [&'static str],
                     visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        impl<'a, R: 'a, S, E> serde::de::EnumVisitor for &'a mut Deserializer<R, S, E>
        where R: Read, S: SizeLimit, E: ByteOrder {
            type Error = Error;
            type Variant = Self;

            fn visit_variant_seed<V>(self, seed: V) -> Result<(V::Value, Self::Variant)>
                where V: serde::de::DeserializeSeed,
            {
                let idx: u32 = try!(serde::de::Deserialize::deserialize(&mut *self));
                let val: Result<_> = seed.deserialize(idx.into_deserializer());
                Ok((try!(val), self))
            }
        }

        visitor.visit_enum(self)
    }
    
    fn deserialize_tuple<V>(self, _len: usize, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        struct TupleVisitor<'a, R: Read + 'a, S: SizeLimit + 'a, E: ByteOrder + 'a>(&'a mut Deserializer<R, S, E>);

        impl<'a, 'b: 'a, R: Read + 'b, S: SizeLimit, E: ByteOrder> serde::de::SeqVisitor for TupleVisitor<'a, R, S, E> {
            type Error = Error;

            fn visit_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>>
                where T: serde::de::DeserializeSeed,
            {
                let value = try!(serde::de::DeserializeSeed::deserialize(seed, &mut *self.0));
                Ok(Some(value))
            }
        }

        visitor.visit_seq(TupleVisitor(self))
    }

    fn deserialize_seq_fixed_size<V>(self,
                            len: usize,
                            visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        struct SeqVisitor<'a, R: Read + 'a, S: SizeLimit + 'a, E: ByteOrder + 'a> {
            deserializer: &'a mut Deserializer<R, S, E>,
            len: usize,
        }

        impl<'a, 'b: 'a, R: Read + 'b, S: SizeLimit, E: ByteOrder> serde::de::SeqVisitor for SeqVisitor<'a, R, S, E> {
            type Error = Error;

            fn visit_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>>
                where T: serde::de::DeserializeSeed,
            {
                if self.len > 0 {
                    self.len -= 1;
                    let value = try!(serde::de::DeserializeSeed::deserialize(seed, &mut *self.deserializer));
                    Ok(Some(value))
                } else {
                    Ok(None)
                }
            }
        }

        visitor.visit_seq(SeqVisitor { deserializer: self, len: len })
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        let value: u8 = try!(serde::de::Deserialize::deserialize(&mut *self));
        match value {
            0 => visitor.visit_none(),
            1 => visitor.visit_some(&mut *self),
            _ => Err(ErrorKind::InvalidEncoding{
                desc: "invalid tag when decoding Option",
                detail: Some(format!("Expected 0 or 1, got {}", value))
            }.into()),
        }
    }

    fn deserialize_seq<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        let len = try!(serde::Deserialize::deserialize(&mut *self));

        self.deserialize_seq_fixed_size(len, visitor)
    }

    fn deserialize_map<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        struct MapVisitor<'a, R: Read + 'a, S: SizeLimit + 'a, E: ByteOrder + 'a> {
            deserializer: &'a mut Deserializer<R, S, E>,
            len: usize,
        }

        impl<'a, 'b: 'a, R: Read + 'b, S: SizeLimit, E: ByteOrder> serde::de::MapVisitor for MapVisitor<'a, R, S, E> {
            type Error = Error;

            fn visit_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>>
                where K: serde::de::DeserializeSeed,
            {
                if self.len > 0 {
                    self.len -= 1;
                    let key = try!(serde::de::DeserializeSeed::deserialize(seed, &mut *self.deserializer));
                    Ok(Some(key))
                } else {
                    Ok(None)
                }
            }

            fn visit_value_seed<V>(&mut self, seed: V) -> Result<V::Value>
                where V: serde::de::DeserializeSeed,
            {
                let value = try!(serde::de::DeserializeSeed::deserialize(seed, &mut *self.deserializer));
                Ok(value)
            }
        }

        let len = try!(serde::Deserialize::deserialize(&mut *self));

        visitor.visit_map(MapVisitor { deserializer: self, len: len })
    }

    fn deserialize_struct<V>(self,
                       _name: &str,
                       fields: &'static [&'static str],
                       visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        self.deserialize_tuple(fields.len(), visitor)
    }

    fn deserialize_struct_field<V>(self,
                                   _visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        let message = "bincode does not support Deserializer::deserialize_struct_field";
        Err(Error::custom(message))
    }

    fn deserialize_newtype_struct<V>(self,
                               _name: &str,
                               visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_unit_struct<V>(self,
                                  _name: &'static str,
                                  visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        visitor.visit_unit()
    }

    fn deserialize_tuple_struct<V>(self,
                                   _name: &'static str,
                                   len: usize,
                                   visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        self.deserialize_tuple(len, visitor)
    }

    fn deserialize_ignored_any<V>(self,
                                  _visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        let message = "bincode does not support Deserializer::deserialize_ignored_any";
        Err(Error::custom(message))
    }
}

impl<'a, R, S, E> serde::de::VariantVisitor for &'a mut Deserializer<R, S, E>
where R: Read, S: SizeLimit, E: ByteOrder {
    type Error = Error;

    fn visit_unit(self) -> Result<()> {
        Ok(())
    }

    fn visit_newtype_seed<T>(self, seed: T) -> Result<T::Value>
        where T: serde::de::DeserializeSeed,
    {
        serde::de::DeserializeSeed::deserialize(seed, self)
    }

    fn visit_tuple<V>(self,
                      len: usize,
                      visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor,
    {
        serde::de::Deserializer::deserialize_tuple(self, len, visitor)
    }

    fn visit_struct<V>(self,
                       fields: &'static [&'static str],
                       visitor: V) -> Result<V::Value>
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
