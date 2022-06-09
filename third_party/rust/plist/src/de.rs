use serde::de::{
    self,
    value::{MapAccessDeserializer, MapDeserializer},
    IntoDeserializer,
};
use std::{
    array::IntoIter,
    fmt::Display,
    fs::File,
    io::{BufReader, Cursor, Read, Seek},
    iter::Peekable,
    mem,
    path::Path,
};

use crate::{
    date::serde_impls::DATE_NEWTYPE_STRUCT_NAME,
    error::{self, Error, ErrorKind, EventKind},
    stream::{self, Event, OwnedEvent},
    u64_to_usize,
    uid::serde_impls::UID_NEWTYPE_STRUCT_NAME,
    value::serde_impls::VALUE_NEWTYPE_STRUCT_NAME,
};

macro_rules! expect {
    ($next:expr, $kind:expr) => {
        match $next {
            Some(Ok(ref event)) if EventKind::of_event(event) != $kind => {
                return Err(error::unexpected_event_type($kind, event))?;
            }
            Some(Ok(event)) => event,
            Some(Err(err)) => return Err(err),
            None => return Err(ErrorKind::UnexpectedEndOfEventStream.without_position()),
        }
    };
}

macro_rules! try_next {
    ($next:expr) => {
        match $next {
            Some(Ok(event)) => event,
            Some(Err(err)) => return Err(err)?,
            None => return Err(ErrorKind::UnexpectedEndOfEventStream.without_position())?,
        }
    };
}

#[doc(hidden)]
impl de::Error for Error {
    fn custom<T: Display>(msg: T) -> Self {
        ErrorKind::Serde(msg.to_string()).without_position()
    }
}

enum OptionMode {
    Root,
    StructField,
    Explicit,
}

/// A structure that deserializes plist event streams into Rust values.
pub struct Deserializer<I>
where
    I: IntoIterator<Item = Result<OwnedEvent, Error>>,
{
    events: Peekable<<I as IntoIterator>::IntoIter>,
    option_mode: OptionMode,
    in_plist_value: bool,
}

impl<I> Deserializer<I>
where
    I: IntoIterator<Item = Result<OwnedEvent, Error>>,
{
    pub fn new(iter: I) -> Deserializer<I> {
        Deserializer {
            events: iter.into_iter().peekable(),
            option_mode: OptionMode::Root,
            in_plist_value: false,
        }
    }

    fn with_option_mode<T, F: FnOnce(&mut Deserializer<I>) -> Result<T, Error>>(
        &mut self,
        option_mode: OptionMode,
        f: F,
    ) -> Result<T, Error> {
        let prev_option_mode = mem::replace(&mut self.option_mode, option_mode);
        let ret = f(&mut *self);
        self.option_mode = prev_option_mode;
        ret
    }

    fn enter_plist_value<T, F: FnOnce(&mut Deserializer<I>) -> Result<T, Error>>(
        &mut self,
        f: F,
    ) -> Result<T, Error> {
        let prev = mem::replace(&mut self.in_plist_value, true);
        let ret = f(&mut *self);
        self.in_plist_value = prev;
        ret
    }
}

impl<'de, 'a, I> de::Deserializer<'de> for &'a mut Deserializer<I>
where
    I: IntoIterator<Item = Result<OwnedEvent, Error>>,
{
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        match try_next!(self.events.next()) {
            Event::StartArray(len) => {
                let len = len.and_then(u64_to_usize);
                let ret = visitor.visit_seq(MapAndSeqAccess::new(self, false, len))?;
                expect!(self.events.next(), EventKind::EndCollection);
                Ok(ret)
            }
            Event::StartDictionary(len) => {
                let len = len.and_then(u64_to_usize);
                let ret = visitor.visit_map(MapAndSeqAccess::new(self, false, len))?;
                expect!(self.events.next(), EventKind::EndCollection);
                Ok(ret)
            }
            event @ Event::EndCollection => Err(error::unexpected_event_type(
                EventKind::ValueOrStartCollection,
                &event,
            )),

            Event::Boolean(v) => visitor.visit_bool(v),
            Event::Data(v) => visitor.visit_byte_buf(v.into_owned()),
            Event::Date(v) if self.in_plist_value => {
                visitor.visit_enum(MapAccessDeserializer::new(MapDeserializer::new(
                    IntoIter::new([(DATE_NEWTYPE_STRUCT_NAME, v.to_rfc3339())]),
                )))
            }
            Event::Date(v) => visitor.visit_string(v.to_rfc3339()),
            Event::Integer(v) => {
                if let Some(v) = v.as_unsigned() {
                    visitor.visit_u64(v)
                } else if let Some(v) = v.as_signed() {
                    visitor.visit_i64(v)
                } else {
                    unreachable!()
                }
            }
            Event::Real(v) => visitor.visit_f64(v),
            Event::String(v) => visitor.visit_string(v.into_owned()),
            Event::Uid(v) if self.in_plist_value => visitor.visit_enum(MapAccessDeserializer::new(
                MapDeserializer::new(IntoIter::new([(UID_NEWTYPE_STRUCT_NAME, v.get())])),
            )),
            Event::Uid(v) => visitor.visit_u64(v.get()),
        }
    }

    forward_to_deserialize_any! {
        bool u8 u16 u32 u64 i8 i16 i32 i64 f32 f64 char str string
        seq bytes byte_buf map unit_struct
        tuple_struct tuple ignored_any identifier
    }

    fn deserialize_unit<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        expect!(self.events.next(), EventKind::String);
        visitor.visit_unit()
    }

    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        match self.option_mode {
            OptionMode::Root => {
                if self.events.peek().is_none() {
                    visitor.visit_none::<Error>()
                } else {
                    self.with_option_mode(OptionMode::Explicit, |this| visitor.visit_some(this))
                }
            }
            OptionMode::StructField => {
                // None struct values are ignored so if we're here the value must be Some.
                self.with_option_mode(OptionMode::Explicit, |this| Ok(visitor.visit_some(this)?))
            }
            OptionMode::Explicit => {
                expect!(self.events.next(), EventKind::StartDictionary);

                let ret = match try_next!(self.events.next()) {
                    Event::String(ref s) if &s[..] == "None" => {
                        expect!(self.events.next(), EventKind::String);
                        visitor.visit_none::<Error>()?
                    }
                    Event::String(ref s) if &s[..] == "Some" => visitor.visit_some(&mut *self)?,
                    event => return Err(error::unexpected_event_type(EventKind::String, &event))?,
                };

                expect!(self.events.next(), EventKind::EndCollection);

                Ok(ret)
            }
        }
    }

    fn deserialize_newtype_struct<V>(
        self,
        name: &'static str,
        visitor: V,
    ) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        if name == VALUE_NEWTYPE_STRUCT_NAME {
            self.enter_plist_value(|this| visitor.visit_newtype_struct(this))
        } else {
            visitor.visit_newtype_struct(self)
        }
    }

    fn deserialize_struct<V>(
        self,
        _name: &'static str,
        _fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        expect!(self.events.next(), EventKind::StartDictionary);
        let ret = visitor.visit_map(MapAndSeqAccess::new(self, true, None))?;
        expect!(self.events.next(), EventKind::EndCollection);
        Ok(ret)
    }

    fn deserialize_enum<V>(
        self,
        name: &'static str,
        variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        // `plist` since v1.1 serialises unit enum variants as plain strings.
        if let Some(Ok(Event::String(s))) = self.events.peek() {
            return s
                .as_ref()
                .into_deserializer()
                .deserialize_enum(name, variants, visitor);
        }

        expect!(self.events.next(), EventKind::StartDictionary);
        let ret = visitor.visit_enum(&mut *self)?;
        expect!(self.events.next(), EventKind::EndCollection);
        Ok(ret)
    }
}

impl<'de, 'a, I> de::EnumAccess<'de> for &'a mut Deserializer<I>
where
    I: IntoIterator<Item = Result<OwnedEvent, Error>>,
{
    type Error = Error;
    type Variant = Self;

    fn variant_seed<V>(self, seed: V) -> Result<(V::Value, Self), Error>
    where
        V: de::DeserializeSeed<'de>,
    {
        Ok((seed.deserialize(&mut *self)?, self))
    }
}

impl<'de, 'a, I> de::VariantAccess<'de> for &'a mut Deserializer<I>
where
    I: IntoIterator<Item = Result<OwnedEvent, Error>>,
{
    type Error = Error;

    fn unit_variant(self) -> Result<(), Error> {
        <() as de::Deserialize>::deserialize(self)
    }

    fn newtype_variant_seed<T>(self, seed: T) -> Result<T::Value, Error>
    where
        T: de::DeserializeSeed<'de>,
    {
        seed.deserialize(self)
    }

    fn tuple_variant<V>(self, len: usize, visitor: V) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        de::Deserializer::deserialize_tuple(self, len, visitor)
    }

    fn struct_variant<V>(
        self,
        fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value, Error>
    where
        V: de::Visitor<'de>,
    {
        let name = "";
        de::Deserializer::deserialize_struct(self, name, fields, visitor)
    }
}

struct MapAndSeqAccess<'a, I>
where
    I: 'a + IntoIterator<Item = Result<OwnedEvent, Error>>,
{
    de: &'a mut Deserializer<I>,
    is_struct: bool,
    remaining: Option<usize>,
}

impl<'a, I> MapAndSeqAccess<'a, I>
where
    I: 'a + IntoIterator<Item = Result<OwnedEvent, Error>>,
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
    I: 'a + IntoIterator<Item = Result<OwnedEvent, Error>>,
{
    type Error = Error;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>, Error>
    where
        T: de::DeserializeSeed<'de>,
    {
        if let Some(&Ok(Event::EndCollection)) = self.de.events.peek() {
            return Ok(None);
        }

        self.remaining = self.remaining.map(|r| r.saturating_sub(1));
        self.de
            .with_option_mode(OptionMode::Explicit, |this| seed.deserialize(this))
            .map(Some)
    }

    fn size_hint(&self) -> Option<usize> {
        self.remaining
    }
}

impl<'de, 'a, I> de::MapAccess<'de> for MapAndSeqAccess<'a, I>
where
    I: 'a + IntoIterator<Item = Result<OwnedEvent, Error>>,
{
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>, Error>
    where
        K: de::DeserializeSeed<'de>,
    {
        if let Some(&Ok(Event::EndCollection)) = self.de.events.peek() {
            return Ok(None);
        }

        self.remaining = self.remaining.map(|r| r.saturating_sub(1));
        self.de
            .with_option_mode(OptionMode::Explicit, |this| seed.deserialize(this))
            .map(Some)
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value, Error>
    where
        V: de::DeserializeSeed<'de>,
    {
        let option_mode = if self.is_struct {
            OptionMode::StructField
        } else {
            OptionMode::Explicit
        };
        self.de
            .with_option_mode(option_mode, |this| Ok(seed.deserialize(this)?))
    }

    fn size_hint(&self) -> Option<usize> {
        self.remaining
    }
}

/// Deserializes an instance of type `T` from a byte slice.
pub fn from_bytes<T: de::DeserializeOwned>(bytes: &[u8]) -> Result<T, Error> {
    let cursor = Cursor::new(bytes);
    from_reader(cursor)
}

/// Deserializes an instance of type `T` from a plist file of any encoding.
pub fn from_file<P: AsRef<Path>, T: de::DeserializeOwned>(path: P) -> Result<T, Error> {
    let file = File::open(path).map_err(error::from_io_without_position)?;
    from_reader(BufReader::new(file))
}

/// Deserializes an instance of type `T` from a seekable byte stream containing a plist of any encoding.
pub fn from_reader<R: Read + Seek, T: de::DeserializeOwned>(reader: R) -> Result<T, Error> {
    let reader = stream::Reader::new(reader);
    let mut de = Deserializer::new(reader);
    de::Deserialize::deserialize(&mut de)
}

/// Deserializes an instance of type `T` from a byte stream containing an XML encoded plist.
pub fn from_reader_xml<R: Read, T: de::DeserializeOwned>(reader: R) -> Result<T, Error> {
    let reader = stream::XmlReader::new(reader);
    let mut de = Deserializer::new(reader);
    de::Deserialize::deserialize(&mut de)
}
