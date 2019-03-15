//! An abstraction of a plist file as a stream of events. Used to support multiple encodings.

mod binary_reader;
pub use self::binary_reader::BinaryReader;

mod xml_reader;
pub use self::xml_reader::XmlReader;

mod xml_writer;
pub use self::xml_writer::XmlWriter;

use std::io::{Read, Seek, SeekFrom};
use {Date, Error};

/// An encoding of a plist as a flat structure.
///
/// Output by the event readers.
///
/// Dictionary keys and values are represented as pairs of values e.g.:
///
/// ```ignore rust
/// StartDictionary
/// StringValue("Height") // Key
/// RealValue(181.2)      // Value
/// StringValue("Age")    // Key
/// IntegerValue(28)      // Value
/// EndDictionary
/// ```
#[derive(Clone, Debug, PartialEq)]
pub enum Event {
    // While the length of an array or dict cannot be feasably greater than max(usize) this better
    // conveys the concept of an effectively unbounded event stream.
    StartArray(Option<u64>),
    EndArray,

    StartDictionary(Option<u64>),
    EndDictionary,

    BooleanValue(bool),
    DataValue(Vec<u8>),
    DateValue(Date),
    IntegerValue(i64),
    RealValue(f64),
    StringValue(String),
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
        reader.seek(SeekFrom::Start(0))?;
        let mut magic = [0; 8];
        reader.read_exact(&mut magic)?;
        reader.seek(SeekFrom::Start(0))?;

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
pub trait Writer {
    fn write(&mut self, event: &Event) -> Result<(), Error>;
}
