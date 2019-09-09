use std::{
    fs::File,
    io::{BufReader, BufWriter, Read, Seek, Write},
    path::Path,
};

use crate::{
    error::{self, Error, ErrorKind, EventKind},
    stream::{BinaryWriter, Event, IntoEvents, Reader, Writer, XmlReader, XmlWriter},
    u64_to_usize, Date, Dictionary, Integer, Uid,
};

/// Represents any plist value.
#[derive(Clone, Debug, PartialEq)]
pub enum Value {
    Array(Vec<Value>),
    Dictionary(Dictionary),
    Boolean(bool),
    Data(Vec<u8>),
    Date(Date),
    Real(f64),
    Integer(Integer),
    String(String),
    Uid(Uid),
    #[doc(hidden)]
    __Nonexhaustive,
}

impl Value {
    /// Reads a `Value` from a plist file of any encoding.
    pub fn from_file<P: AsRef<Path>>(path: P) -> Result<Value, Error> {
        let file = File::open(path).map_err(error::from_io_without_position)?;
        Value::from_reader(BufReader::new(file))
    }

    /// Reads a `Value` from a seekable byte stream containing a plist of any encoding.
    pub fn from_reader<R: Read + Seek>(reader: R) -> Result<Value, Error> {
        let reader = Reader::new(reader);
        Value::from_events(reader)
    }

    /// Reads a `Value` from a seekable byte stream containing an XML encoded plist.
    pub fn from_reader_xml<R: Read>(reader: R) -> Result<Value, Error> {
        let reader = XmlReader::new(reader);
        Value::from_events(reader)
    }

    /// Serializes a `Value` to a file as a binary encoded plist.
    pub fn to_file_binary<P: AsRef<Path>>(&self, path: P) -> Result<(), Error> {
        let mut file = File::create(path).map_err(error::from_io_without_position)?;
        self.to_writer_binary(BufWriter::new(&mut file))?;
        file.sync_all().map_err(error::from_io_without_position)?;
        Ok(())
    }

    /// Serializes a `Value` to a file as an XML encoded plist.
    pub fn to_file_xml<P: AsRef<Path>>(&self, path: P) -> Result<(), Error> {
        let mut file = File::create(path).map_err(error::from_io_without_position)?;
        self.to_writer_xml(BufWriter::new(&mut file))?;
        file.sync_all().map_err(error::from_io_without_position)?;
        Ok(())
    }

    /// Serializes a `Value` to a byte stream as a binary encoded plist.
    pub fn to_writer_binary<W: Write>(&self, writer: W) -> Result<(), Error> {
        let mut writer = BinaryWriter::new(writer);
        self.to_writer_inner(&mut writer)
    }

    /// Serializes a `Value` to a byte stream as an XML encoded plist.
    pub fn to_writer_xml<W: Write>(&self, writer: W) -> Result<(), Error> {
        let mut writer = XmlWriter::new(writer);
        self.to_writer_inner(&mut writer)
    }

    fn to_writer_inner(&self, writer: &mut dyn Writer) -> Result<(), Error> {
        let events = self.clone().into_events();
        for event in events {
            writer.write(&event)?;
        }
        Ok(())
    }

    /// Builds a single `Value` from an `Event` iterator.
    /// On success any excess `Event`s will remain in the iterator.
    #[cfg(feature = "enable_unstable_features_that_may_break_with_minor_version_bumps")]
    pub fn from_events<T>(events: T) -> Result<Value, Error>
    where
        T: IntoIterator<Item = Result<Event, Error>>,
    {
        Builder::new(events.into_iter()).build()
    }

    /// Builds a single `Value` from an `Event` iterator.
    /// On success any excess `Event`s will remain in the iterator.
    #[cfg(not(feature = "enable_unstable_features_that_may_break_with_minor_version_bumps"))]
    pub(crate) fn from_events<T>(events: T) -> Result<Value, Error>
    where
        T: IntoIterator<Item = Result<Event, Error>>,
    {
        Builder::new(events.into_iter()).build()
    }

    /// Converts a `Value` into an `Event` iterator.
    #[cfg(feature = "enable_unstable_features_that_may_break_with_minor_version_bumps")]
    pub fn into_events(self) -> IntoEvents {
        IntoEvents::new(self)
    }

    /// Converts a `Value` into an `Event` iterator.
    #[cfg(not(feature = "enable_unstable_features_that_may_break_with_minor_version_bumps"))]
    pub(crate) fn into_events(self) -> IntoEvents {
        IntoEvents::new(self)
    }

    /// If the `Value` is an Array, returns the associated `Vec`.
    ///
    /// Returns `None` otherwise.
    pub fn as_array(&self) -> Option<&Vec<Value>> {
        match *self {
            Value::Array(ref array) => Some(array),
            _ => None,
        }
    }

    /// If the `Value` is an Array, returns the associated mutable `Vec`.
    ///
    /// Returns `None` otherwise.
    pub fn as_array_mut(&mut self) -> Option<&mut Vec<Value>> {
        match *self {
            Value::Array(ref mut array) => Some(array),
            _ => None,
        }
    }

    /// If the `Value` is a Dictionary, returns the associated `BTreeMap`.
    ///
    /// Returns `None` otherwise.
    pub fn as_dictionary(&self) -> Option<&Dictionary> {
        match *self {
            Value::Dictionary(ref dict) => Some(dict),
            _ => None,
        }
    }

    /// If the `Value` is a Dictionary, returns the associated mutable `BTreeMap`.
    ///
    /// Returns `None` otherwise.
    pub fn as_dictionary_mut(&mut self) -> Option<&mut Dictionary> {
        match *self {
            Value::Dictionary(ref mut dict) => Some(dict),
            _ => None,
        }
    }

    /// If the `Value` is a Boolean, returns the associated `bool`.
    ///
    /// Returns `None` otherwise.
    pub fn as_boolean(&self) -> Option<bool> {
        match *self {
            Value::Boolean(v) => Some(v),
            _ => None,
        }
    }

    /// If the `Value` is a Data, returns the underlying `Vec`.
    ///
    /// Returns `None` otherwise.
    ///
    /// This method consumes the `Value`. If this is not desired, please use
    /// `as_data` method.
    pub fn into_data(self) -> Option<Vec<u8>> {
        match self {
            Value::Data(data) => Some(data),
            _ => None,
        }
    }

    /// If the `Value` is a Data, returns the associated `Vec`.
    ///
    /// Returns `None` otherwise.
    pub fn as_data(&self) -> Option<&[u8]> {
        match *self {
            Value::Data(ref data) => Some(data),
            _ => None,
        }
    }

    /// If the `Value` is a Date, returns the associated `Date`.
    ///
    /// Returns `None` otherwise.
    pub fn as_date(&self) -> Option<Date> {
        match *self {
            Value::Date(date) => Some(date),
            _ => None,
        }
    }

    /// If the `Value` is a Real, returns the associated `f64`.
    ///
    /// Returns `None` otherwise.
    pub fn as_real(&self) -> Option<f64> {
        match *self {
            Value::Real(v) => Some(v),
            _ => None,
        }
    }

    /// If the `Value` is a signed Integer, returns the associated `i64`.
    ///
    /// Returns `None` otherwise.
    pub fn as_signed_integer(&self) -> Option<i64> {
        match *self {
            Value::Integer(v) => v.as_signed(),
            _ => None,
        }
    }

    /// If the `Value` is an unsigned Integer, returns the associated `u64`.
    ///
    /// Returns `None` otherwise.
    pub fn as_unsigned_integer(&self) -> Option<u64> {
        match *self {
            Value::Integer(v) => v.as_unsigned(),
            _ => None,
        }
    }

    /// If the `Value` is a String, returns the underlying `String`.
    ///
    /// Returns `None` otherwise.
    ///
    /// This method consumes the `Value`. If this is not desired, please use
    /// `as_string` method.
    pub fn into_string(self) -> Option<String> {
        match self {
            Value::String(v) => Some(v),
            _ => None,
        }
    }

    /// If the `Value` is a String, returns the associated `str`.
    ///
    /// Returns `None` otherwise.
    pub fn as_string(&self) -> Option<&str> {
        match *self {
            Value::String(ref v) => Some(v),
            _ => None,
        }
    }
}

impl From<Vec<Value>> for Value {
    fn from(from: Vec<Value>) -> Value {
        Value::Array(from)
    }
}

impl From<Dictionary> for Value {
    fn from(from: Dictionary) -> Value {
        Value::Dictionary(from)
    }
}

impl From<bool> for Value {
    fn from(from: bool) -> Value {
        Value::Boolean(from)
    }
}

impl<'a> From<&'a bool> for Value {
    fn from(from: &'a bool) -> Value {
        Value::Boolean(*from)
    }
}

impl From<Date> for Value {
    fn from(from: Date) -> Value {
        Value::Date(from)
    }
}

impl<'a> From<&'a Date> for Value {
    fn from(from: &'a Date) -> Value {
        Value::Date(*from)
    }
}

impl From<f64> for Value {
    fn from(from: f64) -> Value {
        Value::Real(from)
    }
}

impl From<f32> for Value {
    fn from(from: f32) -> Value {
        Value::Real(from.into())
    }
}

impl From<i64> for Value {
    fn from(from: i64) -> Value {
        Value::Integer(Integer::from(from))
    }
}

impl From<i32> for Value {
    fn from(from: i32) -> Value {
        Value::Integer(Integer::from(from))
    }
}

impl From<i16> for Value {
    fn from(from: i16) -> Value {
        Value::Integer(Integer::from(from))
    }
}

impl From<i8> for Value {
    fn from(from: i8) -> Value {
        Value::Integer(Integer::from(from))
    }
}

impl From<u64> for Value {
    fn from(from: u64) -> Value {
        Value::Integer(Integer::from(from))
    }
}

impl From<u32> for Value {
    fn from(from: u32) -> Value {
        Value::Integer(Integer::from(from))
    }
}

impl From<u16> for Value {
    fn from(from: u16) -> Value {
        Value::Integer(Integer::from(from))
    }
}

impl From<u8> for Value {
    fn from(from: u8) -> Value {
        Value::Integer(Integer::from(from))
    }
}

impl<'a> From<&'a f64> for Value {
    fn from(from: &'a f64) -> Value {
        Value::Real(*from)
    }
}

impl<'a> From<&'a f32> for Value {
    fn from(from: &'a f32) -> Value {
        Value::Real((*from).into())
    }
}

impl<'a> From<&'a i64> for Value {
    fn from(from: &'a i64) -> Value {
        Value::Integer(Integer::from(*from))
    }
}

impl<'a> From<&'a i32> for Value {
    fn from(from: &'a i32) -> Value {
        Value::Integer(Integer::from(*from))
    }
}

impl<'a> From<&'a i16> for Value {
    fn from(from: &'a i16) -> Value {
        Value::Integer(Integer::from(*from))
    }
}

impl<'a> From<&'a i8> for Value {
    fn from(from: &'a i8) -> Value {
        Value::Integer(Integer::from(*from))
    }
}

impl<'a> From<&'a u64> for Value {
    fn from(from: &'a u64) -> Value {
        Value::Integer(Integer::from(*from))
    }
}

impl<'a> From<&'a u32> for Value {
    fn from(from: &'a u32) -> Value {
        Value::Integer(Integer::from(*from))
    }
}

impl<'a> From<&'a u16> for Value {
    fn from(from: &'a u16) -> Value {
        Value::Integer((*from).into())
    }
}

impl<'a> From<&'a u8> for Value {
    fn from(from: &'a u8) -> Value {
        Value::Integer((*from).into())
    }
}

impl From<String> for Value {
    fn from(from: String) -> Value {
        Value::String(from)
    }
}

impl<'a> From<&'a str> for Value {
    fn from(from: &'a str) -> Value {
        Value::String(from.into())
    }
}

struct Builder<T> {
    stream: T,
    token: Option<Event>,
}

impl<T: Iterator<Item = Result<Event, Error>>> Builder<T> {
    fn new(stream: T) -> Builder<T> {
        Builder {
            stream,
            token: None,
        }
    }

    fn build(mut self) -> Result<Value, Error> {
        self.bump()?;
        self.build_value()
    }

    fn bump(&mut self) -> Result<(), Error> {
        self.token = match self.stream.next() {
            Some(Ok(token)) => Some(token),
            Some(Err(err)) => return Err(err),
            None => None,
        };
        Ok(())
    }

    fn build_value(&mut self) -> Result<Value, Error> {
        match self.token.take() {
            Some(Event::StartArray(len)) => Ok(Value::Array(self.build_array(len)?)),
            Some(Event::StartDictionary(len)) => Ok(Value::Dictionary(self.build_dict(len)?)),

            Some(Event::Boolean(b)) => Ok(Value::Boolean(b)),
            Some(Event::Data(d)) => Ok(Value::Data(d)),
            Some(Event::Date(d)) => Ok(Value::Date(d)),
            Some(Event::Integer(i)) => Ok(Value::Integer(i)),
            Some(Event::Real(f)) => Ok(Value::Real(f)),
            Some(Event::String(s)) => Ok(Value::String(s)),
            Some(Event::Uid(u)) => Ok(Value::Uid(u)),

            Some(event @ Event::EndCollection) => Err(error::unexpected_event_type(
                EventKind::ValueOrStartCollection,
                &event,
            )),

            Some(Event::__Nonexhaustive) => unreachable!(),

            None => Err(ErrorKind::UnexpectedEndOfEventStream.without_position()),
        }
    }

    fn build_array(&mut self, len: Option<u64>) -> Result<Vec<Value>, Error> {
        let mut values = match len.and_then(u64_to_usize) {
            Some(len) => Vec::with_capacity(len),
            None => Vec::new(),
        };

        loop {
            self.bump()?;
            if let Some(Event::EndCollection) = self.token {
                self.token.take();
                return Ok(values);
            }
            values.push(self.build_value()?);
        }
    }

    fn build_dict(&mut self, _len: Option<u64>) -> Result<Dictionary, Error> {
        let mut dict = Dictionary::new();

        loop {
            self.bump()?;
            match self.token.take() {
                Some(Event::EndCollection) => return Ok(dict),
                Some(Event::String(s)) => {
                    self.bump()?;
                    dict.insert(s, self.build_value()?);
                }
                Some(event) => {
                    return Err(error::unexpected_event_type(
                        EventKind::DictionaryKeyOrEndCollection,
                        &event,
                    ))
                }
                None => return Err(ErrorKind::UnexpectedEndOfEventStream.without_position()),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use std::time::SystemTime;

    use super::*;
    use crate::{stream::Event::*, Date, Dictionary, Value};

    #[test]
    fn value_accessors() {
        let vec = vec![Value::Real(0.0)];
        let mut array = Value::Array(vec.clone());
        assert_eq!(array.as_array(), Some(&vec.clone()));
        assert_eq!(array.as_array_mut(), Some(&mut vec.clone()));

        let mut map = Dictionary::new();
        map.insert("key1".to_owned(), Value::String("value1".to_owned()));
        let mut dict = Value::Dictionary(map.clone());
        assert_eq!(dict.as_dictionary(), Some(&map.clone()));
        assert_eq!(dict.as_dictionary_mut(), Some(&mut map.clone()));

        assert_eq!(Value::Boolean(true).as_boolean(), Some(true));

        let slice: &[u8] = &[1, 2, 3];
        assert_eq!(Value::Data(slice.to_vec()).as_data(), Some(slice));
        assert_eq!(
            Value::Data(slice.to_vec()).into_data(),
            Some(slice.to_vec())
        );

        let date: Date = SystemTime::now().into();
        assert_eq!(Value::Date(date.clone()).as_date(), Some(date));

        assert_eq!(Value::Real(0.0).as_real(), Some(0.0));
        assert_eq!(Value::Integer(1.into()).as_signed_integer(), Some(1));
        assert_eq!(Value::Integer(1.into()).as_unsigned_integer(), Some(1));
        assert_eq!(Value::Integer((-1).into()).as_unsigned_integer(), None);
        assert_eq!(
            Value::Integer((i64::max_value() as u64 + 1).into()).as_signed_integer(),
            None
        );
        assert_eq!(Value::String("2".to_owned()).as_string(), Some("2"));
        assert_eq!(
            Value::String("t".to_owned()).into_string(),
            Some("t".to_owned())
        );
    }

    #[test]
    fn builder() {
        // Input
        let events = vec![
            StartDictionary(None),
            String("Author".to_owned()),
            String("William Shakespeare".to_owned()),
            String("Lines".to_owned()),
            StartArray(None),
            String("It is a tale told by an idiot,".to_owned()),
            String("Full of sound and fury, signifying nothing.".to_owned()),
            EndCollection,
            String("Birthdate".to_owned()),
            Integer(1564.into()),
            String("Height".to_owned()),
            Real(1.60),
            EndCollection,
        ];

        let builder = Builder::new(events.into_iter().map(|e| Ok(e)));
        let plist = builder.build();

        // Expected output
        let mut lines = Vec::new();
        lines.push(Value::String("It is a tale told by an idiot,".to_owned()));
        lines.push(Value::String(
            "Full of sound and fury, signifying nothing.".to_owned(),
        ));

        let mut dict = Dictionary::new();
        dict.insert(
            "Author".to_owned(),
            Value::String("William Shakespeare".to_owned()),
        );
        dict.insert("Lines".to_owned(), Value::Array(lines));
        dict.insert("Birthdate".to_owned(), Value::Integer(1564.into()));
        dict.insert("Height".to_owned(), Value::Real(1.60));

        assert_eq!(plist.unwrap(), Value::Dictionary(dict));
    }
}
