use base64;
use std::{
    io::{self, Read},
    str::FromStr,
};
use xml_rs::{
    common::{is_whitespace_str, Position},
    reader::{
        Error as XmlReaderError, ErrorKind as XmlReaderErrorKind, EventReader, ParserConfig,
        XmlEvent,
    },
};

use crate::{
    error::{Error, ErrorKind, FilePosition},
    stream::{Event, OwnedEvent},
    Date, Integer,
};

pub struct XmlReader<R: Read> {
    xml_reader: EventReader<R>,
    queued_event: Option<XmlEvent>,
    element_stack: Vec<String>,
    finished: bool,
}

impl<R: Read> XmlReader<R> {
    pub fn new(reader: R) -> XmlReader<R> {
        let config = ParserConfig::new()
            .trim_whitespace(false)
            .whitespace_to_characters(true)
            .cdata_to_characters(true)
            .ignore_comments(true)
            .coalesce_characters(true);

        XmlReader {
            xml_reader: EventReader::new_with_config(reader, config),
            queued_event: None,
            element_stack: Vec::new(),
            finished: false,
        }
    }

    fn read_content(&mut self) -> Result<String, Error> {
        loop {
            match self.xml_reader.next() {
                Ok(XmlEvent::Characters(s)) => return Ok(s),
                Ok(event @ XmlEvent::EndElement { .. }) => {
                    self.queued_event = Some(event);
                    return Ok("".to_owned());
                }
                Ok(XmlEvent::EndDocument) => {
                    return Err(self.with_pos(ErrorKind::UnclosedXmlElement))
                }
                Ok(XmlEvent::StartElement { .. }) => {
                    return Err(self.with_pos(ErrorKind::UnexpectedXmlOpeningTag));
                }
                Ok(XmlEvent::ProcessingInstruction { .. }) => (),
                Ok(XmlEvent::StartDocument { .. })
                | Ok(XmlEvent::CData(_))
                | Ok(XmlEvent::Comment(_))
                | Ok(XmlEvent::Whitespace(_)) => {
                    unreachable!("parser does not output CData, Comment or Whitespace events");
                }
                Err(err) => return Err(from_xml_error(err)),
            }
        }
    }

    fn next_event(&mut self) -> Result<XmlEvent, XmlReaderError> {
        if let Some(event) = self.queued_event.take() {
            Ok(event)
        } else {
            self.xml_reader.next()
        }
    }

    fn read_next(&mut self) -> Result<Option<OwnedEvent>, Error> {
        loop {
            match self.next_event() {
                Ok(XmlEvent::StartDocument { .. }) => {}
                Ok(XmlEvent::StartElement { name, .. }) => {
                    // Add the current element to the element stack
                    self.element_stack.push(name.local_name.clone());

                    match &name.local_name[..] {
                        "plist" => (),
                        "array" => return Ok(Some(Event::StartArray(None))),
                        "dict" => return Ok(Some(Event::StartDictionary(None))),
                        "key" => return Ok(Some(Event::String(self.read_content()?.into()))),
                        "true" => return Ok(Some(Event::Boolean(true))),
                        "false" => return Ok(Some(Event::Boolean(false))),
                        "data" => {
                            let mut s = self.read_content()?;
                            // Strip whitespace and line endings from input string
                            s.retain(|c| !c.is_ascii_whitespace());
                            let data = base64::decode(&s)
                                .map_err(|_| self.with_pos(ErrorKind::InvalidDataString))?;
                            return Ok(Some(Event::Data(data.into())));
                        }
                        "date" => {
                            let s = self.read_content()?;
                            let date = Date::from_rfc3339(&s)
                                .map_err(|()| self.with_pos(ErrorKind::InvalidDateString))?;
                            return Ok(Some(Event::Date(date)));
                        }
                        "integer" => {
                            let s = self.read_content()?;
                            match Integer::from_str(&s) {
                                Ok(i) => return Ok(Some(Event::Integer(i))),
                                Err(_) => {
                                    return Err(self.with_pos(ErrorKind::InvalidIntegerString))
                                }
                            }
                        }
                        "real" => {
                            let s = self.read_content()?;
                            match f64::from_str(&s) {
                                Ok(f) => return Ok(Some(Event::Real(f))),
                                Err(_) => return Err(self.with_pos(ErrorKind::InvalidRealString)),
                            }
                        }
                        "string" => return Ok(Some(Event::String(self.read_content()?.into()))),
                        _ => return Err(self.with_pos(ErrorKind::UnknownXmlElement)),
                    }
                }
                Ok(XmlEvent::EndElement { name, .. }) => {
                    // Check the corrent element is being closed
                    match self.element_stack.pop() {
                        Some(ref open_name) if &name.local_name == open_name => (),
                        Some(ref _open_name) => {
                            return Err(self.with_pos(ErrorKind::UnclosedXmlElement))
                        }
                        None => return Err(self.with_pos(ErrorKind::UnpairedXmlClosingTag)),
                    }

                    match &name.local_name[..] {
                        "array" | "dict" => return Ok(Some(Event::EndCollection)),
                        "plist" | _ => (),
                    }
                }
                Ok(XmlEvent::EndDocument) => {
                    if self.element_stack.is_empty() {
                        return Ok(None);
                    } else {
                        return Err(self.with_pos(ErrorKind::UnclosedXmlElement));
                    }
                }

                Ok(XmlEvent::Characters(c)) => {
                    if !is_whitespace_str(&c) {
                        return Err(
                            self.with_pos(ErrorKind::UnexpectedXmlCharactersExpectedElement)
                        );
                    }
                }
                Ok(XmlEvent::CData(_)) | Ok(XmlEvent::Comment(_)) | Ok(XmlEvent::Whitespace(_)) => {
                    unreachable!("parser does not output CData, Comment or Whitespace events")
                }
                Ok(XmlEvent::ProcessingInstruction { .. }) => (),
                Err(err) => return Err(from_xml_error(err)),
            }
        }
    }

    fn with_pos(&self, kind: ErrorKind) -> Error {
        kind.with_position(convert_xml_pos(self.xml_reader.position()))
    }
}

impl<R: Read> Iterator for XmlReader<R> {
    type Item = Result<OwnedEvent, Error>;

    fn next(&mut self) -> Option<Result<OwnedEvent, Error>> {
        if self.finished {
            None
        } else {
            match self.read_next() {
                Ok(Some(event)) => Some(Ok(event)),
                Ok(None) => {
                    self.finished = true;
                    None
                }
                Err(err) => {
                    self.finished = true;
                    Some(Err(err))
                }
            }
        }
    }
}

fn convert_xml_pos(pos: xml_rs::common::TextPosition) -> FilePosition {
    // TODO: pos.row and pos.column counts from 0. what do we want to do?
    FilePosition::LineColumn(pos.row, pos.column)
}

fn from_xml_error(err: XmlReaderError) -> Error {
    let kind = match err.kind() {
        XmlReaderErrorKind::Io(err) if err.kind() == io::ErrorKind::UnexpectedEof => {
            ErrorKind::UnexpectedEof
        }
        XmlReaderErrorKind::Io(err) => {
            let err = if let Some(code) = err.raw_os_error() {
                io::Error::from_raw_os_error(code)
            } else {
                io::Error::new(err.kind(), err.to_string())
            };
            ErrorKind::Io(err)
        }
        XmlReaderErrorKind::Syntax(_) => ErrorKind::InvalidXmlSyntax,
        XmlReaderErrorKind::UnexpectedEof => ErrorKind::UnexpectedEof,
        XmlReaderErrorKind::Utf8(_) => ErrorKind::InvalidXmlUtf8,
    };

    kind.with_position(convert_xml_pos(err.position()))
}

#[cfg(test)]
mod tests {
    use std::{fs::File, path::Path};

    use super::*;
    use crate::stream::Event::{self, *};

    #[test]
    fn streaming_parser() {
        let reader = File::open(&Path::new("./tests/data/xml.plist")).unwrap();
        let streaming_parser = XmlReader::new(reader);
        let events: Vec<Event> = streaming_parser.map(|e| e.unwrap()).collect();

        let comparison = &[
            StartDictionary(None),
            String("Author".into()),
            String("William Shakespeare".into()),
            String("Lines".into()),
            StartArray(None),
            String("It is a tale told by an idiot,".into()),
            String("Full of sound and fury, signifying nothing.".into()),
            EndCollection,
            String("Death".into()),
            Integer(1564.into()),
            String("Height".into()),
            Real(1.60),
            String("Data".into()),
            Data(vec![0, 0, 0, 190, 0, 0, 0, 3, 0, 0, 0, 30, 0, 0, 0].into()),
            String("Birthdate".into()),
            Date(super::Date::from_rfc3339("1981-05-16T11:32:06Z").unwrap()),
            String("Blank".into()),
            String("".into()),
            String("BiggestNumber".into()),
            Integer(18446744073709551615u64.into()),
            String("SmallestNumber".into()),
            Integer((-9223372036854775808i64).into()),
            String("HexademicalNumber".into()),
            Integer(0xdead_beef_u64.into()),
            String("IsTrue".into()),
            Boolean(true),
            String("IsNotFalse".into()),
            Boolean(false),
            EndCollection,
        ];

        assert_eq!(events, comparison);
    }

    #[test]
    fn bad_data() {
        let reader = File::open(&Path::new("./tests/data/xml_error.plist")).unwrap();
        let streaming_parser = XmlReader::new(reader);
        let events: Vec<_> = streaming_parser.collect();

        assert!(events.last().unwrap().is_err());
    }
}
