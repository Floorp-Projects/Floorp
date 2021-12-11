//! An abstraction of a plist file as a stream of events. Used to support multiple encodings.

mod binary_reader;
pub use self::binary_reader::BinaryReader;

mod binary_writer;
pub use self::binary_writer::BinaryWriter;

mod xml_reader;
pub use self::xml_reader::XmlReader;

mod xml_writer;
pub use self::xml_writer::XmlWriter;

use std::{
    io::{self, Read, Seek, SeekFrom},
    vec,
};

use crate::{
    dictionary,
    error::{Error, ErrorKind},
    Date, Integer, Uid, Value,
};

/// An encoding of a plist as a flat structure.
///
/// Output by the event readers.
///
/// Dictionary keys and values are represented as pairs of values e.g.:
///
/// ```ignore rust
/// StartDictionary
/// String("Height") // Key
/// Real(181.2)      // Value
/// String("Age")    // Key
/// Integer(28)      // Value
/// EndDictionary
/// ```
#[derive(Clone, Debug, PartialEq)]
pub enum Event {
    // While the length of an array or dict cannot be feasably greater than max(usize) this better
    // conveys the concept of an effectively unbounded event stream.
    StartArray(Option<u64>),
    StartDictionary(Option<u64>),
    EndCollection,

    Boolean(bool),
    Data(Vec<u8>),
    Date(Date),
    Integer(Integer),
    Real(f64),
    String(String),
    Uid(Uid),

    #[doc(hidden)]
    __Nonexhaustive,
}

/// An `Event` stream returned by `Value::into_events`.
pub struct IntoEvents {
    stack: Vec<StackItem>,
}

enum StackItem {
    Root(Value),
    Array(vec::IntoIter<Value>),
    Dict(dictionary::IntoIter),
    DictValue(Value),
}

impl IntoEvents {
    pub(crate) fn new(value: Value) -> IntoEvents {
        IntoEvents {
            stack: vec![StackItem::Root(value)],
        }
    }
}

impl Iterator for IntoEvents {
    type Item = Event;

    fn next(&mut self) -> Option<Event> {
        fn handle_value(value: Value, stack: &mut Vec<StackItem>) -> Event {
            match value {
                Value::Array(array) => {
                    let len = array.len();
                    let iter = array.into_iter();
                    stack.push(StackItem::Array(iter));
                    Event::StartArray(Some(len as u64))
                }
                Value::Dictionary(dict) => {
                    let len = dict.len();
                    let iter = dict.into_iter();
                    stack.push(StackItem::Dict(iter));
                    Event::StartDictionary(Some(len as u64))
                }
                Value::Boolean(value) => Event::Boolean(value),
                Value::Data(value) => Event::Data(value),
                Value::Date(value) => Event::Date(value),
                Value::Real(value) => Event::Real(value),
                Value::Integer(value) => Event::Integer(value),
                Value::String(value) => Event::String(value),
                Value::Uid(value) => Event::Uid(value),
                Value::__Nonexhaustive => unreachable!(),
            }
        }

        Some(match self.stack.pop()? {
            StackItem::Root(value) => handle_value(value, &mut self.stack),
            StackItem::Array(mut array) => {
                if let Some(value) = array.next() {
                    // There might still be more items in the array so return it to the stack.
                    self.stack.push(StackItem::Array(array));
                    handle_value(value, &mut self.stack)
                } else {
                    Event::EndCollection
                }
            }
            StackItem::Dict(mut dict) => {
                if let Some((key, value)) = dict.next() {
                    // There might still be more items in the dictionary so return it to the stack.
                    self.stack.push(StackItem::Dict(dict));
                    // The next event to be returned must be the dictionary value.
                    self.stack.push(StackItem::DictValue(value));
                    // Return the key event now.
                    Event::String(key)
                } else {
                    Event::EndCollection
                }
            }
            StackItem::DictValue(value) => handle_value(value, &mut self.stack),
        })
    }
}

pub struct Reader<R: Read + Seek>(ReaderInner<R>);

enum ReaderInner<R: Read + Seek> {
    Uninitialized(Option<R>),
    Xml(XmlReader<R>),
    Binary(BinaryReader<R>),
}

impl<R: Read + Seek> Reader<R> {
    pub fn new(reader: R) -> Reader<R> {
        Reader(ReaderInner::Uninitialized(Some(reader)))
    }

    fn is_binary(reader: &mut R) -> Result<bool, Error> {
        fn from_io_offset_0(err: io::Error) -> Error {
            ErrorKind::Io(err).with_byte_offset(0)
        }

        reader.seek(SeekFrom::Start(0)).map_err(from_io_offset_0)?;
        let mut magic = [0; 8];
        reader.read_exact(&mut magic).map_err(from_io_offset_0)?;
        reader.seek(SeekFrom::Start(0)).map_err(from_io_offset_0)?;

        Ok(&magic == b"bplist00")
    }
}

impl<R: Read + Seek> Iterator for Reader<R> {
    type Item = Result<Event, Error>;

    fn next(&mut self) -> Option<Result<Event, Error>> {
        let mut reader = match self.0 {
            ReaderInner::Xml(ref mut parser) => return parser.next(),
            ReaderInner::Binary(ref mut parser) => return parser.next(),
            ReaderInner::Uninitialized(ref mut reader) => reader.take().unwrap(),
        };

        let event_reader = match Reader::is_binary(&mut reader) {
            Ok(true) => ReaderInner::Binary(BinaryReader::new(reader)),
            Ok(false) => ReaderInner::Xml(XmlReader::new(reader)),
            Err(err) => {
                ::std::mem::replace(&mut self.0, ReaderInner::Uninitialized(Some(reader)));
                return Some(Err(err));
            }
        };

        ::std::mem::replace(&mut self.0, event_reader);

        self.next()
    }
}

/// Supports writing event streams in different plist encodings.
pub trait Writer: private::Sealed {
    fn write(&mut self, event: &Event) -> Result<(), Error> {
        match event {
            Event::StartArray(len) => self.write_start_array(*len),
            Event::StartDictionary(len) => self.write_start_dictionary(*len),
            Event::EndCollection => self.write_end_collection(),
            Event::Boolean(value) => self.write_boolean(*value),
            Event::Data(value) => self.write_data(value),
            Event::Date(value) => self.write_date(*value),
            Event::Integer(value) => self.write_integer(*value),
            Event::Real(value) => self.write_real(*value),
            Event::String(value) => self.write_string(value),
            Event::Uid(value) => self.write_uid(*value),
            Event::__Nonexhaustive => unreachable!(),
        }
    }

    fn write_start_array(&mut self, len: Option<u64>) -> Result<(), Error>;
    fn write_start_dictionary(&mut self, len: Option<u64>) -> Result<(), Error>;
    fn write_end_collection(&mut self) -> Result<(), Error>;

    fn write_boolean(&mut self, value: bool) -> Result<(), Error>;
    fn write_data(&mut self, value: &[u8]) -> Result<(), Error>;
    fn write_date(&mut self, value: Date) -> Result<(), Error>;
    fn write_integer(&mut self, value: Integer) -> Result<(), Error>;
    fn write_real(&mut self, value: f64) -> Result<(), Error>;
    fn write_string(&mut self, value: &str) -> Result<(), Error>;
    fn write_uid(&mut self, value: Uid) -> Result<(), Error>;
}

pub(crate) mod private {
    use std::io::Write;

    pub trait Sealed {}

    impl<W: Write> Sealed for super::BinaryWriter<W> {}
    impl<W: Write> Sealed for super::XmlWriter<W> {}
}
