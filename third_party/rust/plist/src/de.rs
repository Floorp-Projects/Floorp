use serde::de;
use std::fmt::Display;
use std::fs::File;
use std::io::{BufReader, Read, Seek};
use std::iter::Peekable;
use std::path::Path;

use stream::{self, Event};
use {u64_to_usize, Error};

macro_rules! expect {
    ($next:expr, $pat:pat) => {
        match $next {
            Some(Ok(v @ $pat)) => v,
            None => return Err(Error::UnexpectedEof),
            _ => return Err(event_mismatch_error()),
        }
    };
    ($next:expr, $pat:pat => $save:expr) => {
        match $next {
            Some(Ok($pat)) => $save,
            None => return Err(Error::UnexpectedEof),
            _ => return Err(event_mismatch_error()),
        }
    };
}

macro_rules! try_next {
    ($next:expr) => {
        match $next {
            Some(Ok(v)) => v,
            Some(Err(_)) => return Err(event_mismatch_error()),
            None => return Err(Error::UnexpectedEof),
        }
    };
}

fn event_mismatch_error() -> Error {
    Error::InvalidData
}

impl de::Error for Error {
    fn custom<T: Display>(msg: T) -> Self {
        Error::Serde(msg.to_string())
    }
}

/// A structure that deserializes plist event streams into Rust values.
pub struct Deserializer<I>
where
    I: IntoIterator<Item = Result<Event, Error>>,
{
    events: Peekable<<I as IntoIterator>::IntoIter>,
}

impl<I> Deserializer<I>
where
    I: IntoIterator<Item = Result<Event, Error>>,
{
    pub fn new(iter: I) -> Deserializer<I> {
        Deserializer {
            events: iter.into_iter().peekable(),
        }
    }
}

impl<'de, 'a, I> de::Deserializer<'de> for &'a mut Deserializer<I>
where
    I: IntoIterator<Item = Result<Event, Error>>,
{
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        match try_next!(self.events.next()) {
            Event::StartArray(len) => {
                let len = len.and_then(u64_to_usize);
                let ret = visitor.visit_seq(MapAndSeqAccess::new(self, false, len))?;
                expect!(self.events.next(), Event::EndArray);
                Ok(ret)
            }
            Event::EndArray => Err(event_mismatch_error()),

            Event::StartDictionary(len) => {
                let len = len.and_then(u64_to_usize);
                let ret = visitor.visit_map(MapAndSeqAccess::new(self, false, len))?;
                expect!(self.events.next(), Event::EndDictionary);
                Ok(ret)
            }
            Event::EndDictionary => Err(event_mismatch_error()),

            Event::BooleanValue(v) => visitor.visit_bool(v),
            Event::DataValue(v) => visitor.visit_byte_buf(v),
            Event::DateValue(v) => visitor.visit_string(v.to_rfc3339()),
            Event::IntegerValue(v) if v.is_positive() => visitor.visit_u64(v as u64),
            Event::IntegerValue(v) => visitor.visit_i64(v as i64),
            Event::RealValue(v) => visitor.visit_f64(v),
            Event::StringValue(v) => visitor.visit_string(v),
        }
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string
        seq bytes byte_buf map unit_struct
        tuple_struct tuple ignored_any identifier
    }

    fn deserialize_unit<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        expect!(self.events.next(), Event::StringValue(_));
        visitor.visit_unit()
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        expect!(self.events.next(), Event::StartDictionary(_));

        let ret = match try_next!(self.events.next()) {
            Event::StringValue(ref s) if &s[..] == "None" => {
                expect!(self.events.next(), Event::StringValue(_));
                visitor.visit_none::<Self::Error>()?
            }
            Event::StringValue(ref s) if &s[..] == "Some" => visitor.visit_some(&mut *self)?,
            _ => return Err(event_mismatch_error()),
        };

        expect!(self.events.next(), Event::EndDictionary);

        Ok(ret)
    }

    fn deserialize_newtype_struct<V>(
        self,
        _name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_struct<V>(
        self,
        _name: &'static str,
        _fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        expect!(self.events.next(), Event::StartDictionary(_));
        let ret = visitor.visit_map(MapAndSeqAccess::new(self, true, None))?;
        expect!(self.events.next(), Event::EndDictionary);
        Ok(ret)
    }

    fn deserialize_enum<V>(
        self,
        _enum: &'static str,
        _variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        expect!(self.events.next(), Event::StartDictionary(_));
        let ret = visitor.visit_enum(&mut *self)?;
        expect!(self.events.next(), Event::EndDictionary);
        Ok(ret)
    }
}

impl<'de, 'a, I> de::EnumAccess<'de> for &'a mut Deserializer<I>
where
    I: IntoIterator<Item = Result<Event, Error>>,
{
    type Error = Error;
    type Variant = Self;

    fn variant_seed<V>(self, seed: V) -> Result<(V::Value, Self), Self::Error>
    where
        V: de::DeserializeSeed<'de>,
    {
        Ok((seed.deserialize(&mut *self)?, self))
    }
}

impl<'de, 'a, I> de::VariantAccess<'de> for &'a mut Deserializer<I>
where
    I: IntoIterator<Item = Result<Event, Error>>,
{
    type Error = Error;

    fn unit_variant(self) -> Result<(), Self::Error> {
        <() as de::Deserialize>::deserialize(self)
    }

    fn newtype_variant_seed<T>(self, seed: T) -> Result<T::Value, Self::Error>
    where
        T: de::DeserializeSeed<'de>,
    {
        seed.deserialize(self)
    }

    fn tuple_variant<V>(self, len: usize, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        <Self as de::Deserializer>::deserialize_tuple(self, len, visitor)
    }

    fn struct_variant<V>(
        self,
        fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        let name = "";
        <Self as de::Deserializer>::deserialize_struct(self, name, fields, visitor)
    }
}

pub struct StructValueDeserializer<'a, I: 'a>
where
    I: IntoIterator<Item = Result<Event, Error>>,
{
    de: &'a mut Deserializer<I>,
}

impl<'de, 'a, I> de::Deserializer<'de> for StructValueDeserializer<'a, I>
where
    I: IntoIterator<Item = Result<Event, Error>>,
{
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.de.deserialize_any(visitor)
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string
        seq bytes byte_buf map unit_struct
        tuple_struct tuple ignored_any identifier
    }

    fn deserialize_unit<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.de.deserialize_unit(visitor)
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        // None struct values are ignored so if we're here the value must be Some.
        visitor.visit_some(self.de)
    }

    fn deserialize_newtype_struct<V>(
        self,
        name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.de.deserialize_newtype_struct(name, visitor)
    }

    fn deserialize_struct<V>(
        self,
        name: &'static str,
        fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.de.deserialize_struct(name, fields, visitor)
    }

    fn deserialize_enum<V>(
        self,
        enum_: &'static str,
        variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Self::Error>
    where
        V: de::Visitor<'de>,
    {
        self.de.deserialize_enum(enum_, variants, visitor)
    }
}

struct MapAndSeqAccess<'a, I>
where
    I: 'a + IntoIterator<Item = Result<Event, Error>>,
{
    de: &'a mut Deserializer<I>,
    is_struct: bool,
    remaining: Option<usize>,
}

impl<'a, I> MapAndSeqAccess<'a, I>
where
    I: 'a + IntoIterator<Item = Result<Event, Error>>,
{
    fn new(
        de: &'a mut Deserializer<I>,
        is_struct: bool,
        len: Option<usize>,
    ) -> MapAndSeqAccess<'a, I> {
        MapAndSeqAccess {
            de,
            is_struct,
            remaining: len,
        }
    }
}

impl<'de, 'a, I> de::SeqAccess<'de> for MapAndSeqAccess<'a, I>
where
    I: 'a + IntoIterator<Item = Result<Event, Error>>,
{
    type Error = Error;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, Self::Error>
    where
        T: de::DeserializeSeed<'de>,
    {
        if let Some(&Ok(Event::EndArray)) = self.de.events.peek() {
            return Ok(None);
        }

        self.remaining = self.remaining.map(|r| r.saturating_sub(1));
        seed.deserialize(&mut *self.de).map(Some)
    }

    fn size_hint(&self) -> Option<usize> {
        self.remaining
    }
}

impl<'de, 'a, I> de::MapAccess<'de> for MapAndSeqAccess<'a, I>
where
    I: 'a + IntoIterator<Item = Result<Event, Error>>,
{
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Self::Error>
    where
        K: de::DeserializeSeed<'de>,
    {
        if let Some(&Ok(Event::EndDictionary)) = self.de.events.peek() {
            return Ok(None);
        }

        self.remaining = self.remaining.map(|r| r.saturating_sub(1));
        seed.deserialize(&mut *self.de).map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Self::Error>
    where
        V: de::DeserializeSeed<'de>,
    {
        if self.is_struct {
            seed.deserialize(StructValueDeserializer { de: &mut *self.de })
        } else {
            seed.deserialize(&mut *self.de)
        }
    }

    fn size_hint(&self) -> Option<usize> {
        self.remaining
    }
}

/// Deserializes an instance of type `T` from a plist file of any encoding.
pub fn from_file<P: AsRef<Path>, T: de::DeserializeOwned>(path: P) -> Result<T, Error> {
    let file = File::open(path)?;
    from_reader(BufReader::new(file))
}

/// Deserializes an instance of type `T` from a seekable byte stream containing a plist file of any encoding.
pub fn from_reader<R: Read + Seek, T: de::DeserializeOwned>(reader: R) -> Result<T, Error> {
    let reader = stream::Reader::new(reader);
    let mut de = Deserializer::new(reader);
    de::Deserialize::deserialize(&mut de)
}

/// Deserializes an instance of type `T` from a byte stream containing an XML encoded plist file.
pub fn from_reader_xml<R: Read, T: de::DeserializeOwned>(reader: R) -> Result<T, Error> {
    let reader = stream::XmlReader::new(reader);
    let mut de = Deserializer::new(reader);
    de::Deserialize::deserialize(&mut de)
}
