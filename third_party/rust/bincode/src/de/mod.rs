use std::io::Read;
use std::marker::PhantomData;

use byteorder::{ReadBytesExt, ByteOrder};
use serde_crate as serde;
use serde_crate::de::IntoDeserializer;
use serde_crate::de::Error as DeError;
use ::SizeLimit;
use super::{Result, Error, ErrorKind};
use self::read::BincodeRead;

pub mod read;

/// A Deserializer that reads bytes from a buffer.
///
/// This struct should rarely be used.
/// In most cases, prefer the `decode_from` function.
///
/// The ByteOrder that is chosen will impact the endianness that
/// is used to read integers out of the reader.
///
/// ```rust,ignore
/// let d = Deserializer::new(&mut some_reader, SizeLimit::new());
/// serde::Deserialize::deserialize(&mut deserializer);
/// let bytes_read = d.bytes_read();
/// ```
pub struct Deserializer<R, S: SizeLimit, E: ByteOrder> {
    reader: R,
    size_limit: S,
    _phantom: PhantomData<E>,
}

impl<'de, R: BincodeRead<'de>, E: ByteOrder, S: SizeLimit> Deserializer<R, S, E> {
    /// Creates a new Deserializer with a given `Read`er and a size_limit.
    pub fn new(r: R, size_limit: S) -> Deserializer<R, S, E> {
        Deserializer {
            reader: r,
            size_limit: size_limit,
            _phantom: PhantomData
        }
    }

    fn read_bytes(&mut self, count: u64) -> Result<()> {
        self.size_limit.add(count)
    }

    fn read_type<T>(&mut self) -> Result<()> {
        use std::mem::size_of;
        self.read_bytes(size_of::<T>() as u64)
    }

    fn read_vec(&mut self) -> Result<Vec<u8>> {
        let len: usize = try!(serde::Deserialize::deserialize(&mut *self));
        self.read_bytes(len as u64)?;
        self.reader.get_byte_buffer(len)
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
            where V: serde::de::Visitor<'de>,
        {
            try!(self.read_type::<$ty>());
            let value = try!(self.reader.$reader_method::<E>());
            visitor.$visitor_method(value)
        }
    }
}

impl<'de, 'a, R, S, E> serde::Deserializer<'de> for &'a mut Deserializer<R, S, E>
where R: BincodeRead<'de>, S: SizeLimit, E: ByteOrder {
    type Error = Error;

    #[inline]
    fn deserialize_any<V>(self, _visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        let message = "bincode does not support Deserializer::deserialize";
        Err(Error::custom(message))
    }

    fn deserialize_bool<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
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
        where V: serde::de::Visitor<'de>,
    {
        try!(self.read_type::<u8>());
        visitor.visit_u8(try!(self.reader.read_u8()))
    }

    #[inline]
    fn deserialize_i8<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        try!(self.read_type::<i8>());
        visitor.visit_i8(try!(self.reader.read_i8()))
    }

    fn deserialize_unit<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        visitor.visit_unit()
    }

    fn deserialize_char<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
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
        where V: serde::de::Visitor<'de>,
    {
        let len: usize = try!(serde::Deserialize::deserialize(&mut *self));
        try!(self.read_bytes(len as u64));
        self.reader.forward_read_str(len, visitor)
    }

    fn deserialize_string<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        visitor.visit_string(try!(self.read_string()))
    }

    fn deserialize_bytes<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        let len: usize = try!(serde::Deserialize::deserialize(&mut *self));
        try!(self.read_bytes(len as u64));
        self.reader.forward_read_bytes(len, visitor)
    }

    fn deserialize_byte_buf<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        visitor.visit_byte_buf(try!(self.read_vec()))
    }

    fn deserialize_enum<V>(self,
                     _enum: &'static str,
                     _variants: &'static [&'static str],
                     visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        impl<'de, 'a, R: 'a, S, E> serde::de::EnumAccess<'de> for &'a mut Deserializer<R, S, E>
        where R: BincodeRead<'de>, S: SizeLimit, E: ByteOrder {
            type Error = Error;
            type Variant = Self;

            fn variant_seed<V>(self, seed: V) -> Result<(V::Value, Self::Variant)>
                where V: serde::de::DeserializeSeed<'de>,
            {
                let idx: u32 = try!(serde::de::Deserialize::deserialize(&mut *self));
                let val: Result<_> = seed.deserialize(idx.into_deserializer());
                Ok((try!(val), self))
            }
        }

        visitor.visit_enum(self)
    }

    fn deserialize_tuple<V>(self,
                            len: usize,
                            visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        struct Access<'a, R: Read + 'a, S: SizeLimit + 'a, E: ByteOrder + 'a> {
            deserializer: &'a mut Deserializer<R, S, E>,
            len: usize,
        }

        impl<'de, 'a, 'b: 'a, R: BincodeRead<'de>+ 'b, S: SizeLimit, E: ByteOrder> serde::de::SeqAccess<'de> for Access<'a, R, S, E> {
            type Error = Error;

            fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>>
                where T: serde::de::DeserializeSeed<'de>,
            {
                if self.len > 0 {
                    self.len -= 1;
                    let value = try!(serde::de::DeserializeSeed::deserialize(seed, &mut *self.deserializer));
                    Ok(Some(value))
                } else {
                    Ok(None)
                }
            }

            fn size_hint(&self) -> Option<usize> {
                Some(self.len)
            }
        }

        visitor.visit_seq(Access { deserializer: self, len: len })
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
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
        where V: serde::de::Visitor<'de>,
    {
        let len = try!(serde::Deserialize::deserialize(&mut *self));

        self.deserialize_tuple(len, visitor)
    }

    fn deserialize_map<V>(self, visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        struct Access<'a, R: Read + 'a, S: SizeLimit + 'a, E: ByteOrder + 'a> {
            deserializer: &'a mut Deserializer<R, S, E>,
            len: usize,
        }

        impl<'de, 'a, 'b: 'a, R: BincodeRead<'de> + 'b, S: SizeLimit, E: ByteOrder> serde::de::MapAccess<'de> for Access<'a, R, S, E> {
            type Error = Error;

            fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>>
                where K: serde::de::DeserializeSeed<'de>,
            {
                if self.len > 0 {
                    self.len -= 1;
                    let key = try!(serde::de::DeserializeSeed::deserialize(seed, &mut *self.deserializer));
                    Ok(Some(key))
                } else {
                    Ok(None)
                }
            }

            fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value>
                where V: serde::de::DeserializeSeed<'de>,
            {
                let value = try!(serde::de::DeserializeSeed::deserialize(seed, &mut *self.deserializer));
                Ok(value)
            }

            fn size_hint(&self) -> Option<usize> {
                Some(self.len)
            }
        }

        let len = try!(serde::Deserialize::deserialize(&mut *self));

        visitor.visit_map(Access { deserializer: self, len: len })
    }

    fn deserialize_struct<V>(self,
                       _name: &str,
                       fields: &'static [&'static str],
                       visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        self.deserialize_tuple(fields.len(), visitor)
    }

    fn deserialize_identifier<V>(self,
                                   _visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        let message = "bincode does not support Deserializer::deserialize_identifier";
        Err(Error::custom(message))
    }

    fn deserialize_newtype_struct<V>(self,
                               _name: &str,
                               visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_unit_struct<V>(self,
                                  _name: &'static str,
                                  visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        visitor.visit_unit()
    }

    fn deserialize_tuple_struct<V>(self,
                                   _name: &'static str,
                                   len: usize,
                                   visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        self.deserialize_tuple(len, visitor)
    }

    fn deserialize_ignored_any<V>(self,
                                  _visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        let message = "bincode does not support Deserializer::deserialize_ignored_any";
        Err(Error::custom(message))
    }
}

impl<'de, 'a, R, S, E> serde::de::VariantAccess<'de> for &'a mut Deserializer<R, S, E>
where R: BincodeRead<'de>, S: SizeLimit, E: ByteOrder {
    type Error = Error;

    fn unit_variant(self) -> Result<()> {
        Ok(())
    }

    fn newtype_variant_seed<T>(self, seed: T) -> Result<T::Value>
        where T: serde::de::DeserializeSeed<'de>,
    {
        serde::de::DeserializeSeed::deserialize(seed, self)
    }

    fn tuple_variant<V>(self,
                      len: usize,
                      visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
    {
        serde::de::Deserializer::deserialize_tuple(self, len, visitor)
    }

    fn struct_variant<V>(self,
                       fields: &'static [&'static str],
                       visitor: V) -> Result<V::Value>
        where V: serde::de::Visitor<'de>,
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
