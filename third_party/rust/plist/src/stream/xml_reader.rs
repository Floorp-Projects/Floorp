use base64;
use std::io::Read;
use std::str::FromStr;
use xml_rs::reader::{EventReader, ParserConfig, XmlEvent};

use stream::Event;
use {Date, Error};

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

    fn read_content<F>(&mut self, f: F) -> Result<Event, Error>
    where
        F: FnOnce(String) -> Result<Event, Error>,
    {
        match self.xml_reader.next() {
            Ok(XmlEvent::Characters(s)) => f(s),
            Ok(event @ XmlEvent::EndElement { .. }) => {
                self.queued_event = Some(event);
                f("".to_owned())
            }
            _ => Err(Error::InvalidData),
        }
    }

    fn next_event(&mut self) -> ::std::result::Result<XmlEvent, ()> {
        if let Some(event) = self.queued_event.take() {
            Ok(event)
        } else {
            self.xml_reader.next().map_err(|_| ())
        }
    }

    fn read_next(&mut self) -> Option<Result<Event, Error>> {
        loop {
            match self.next_event() {
                Ok(XmlEvent::StartElement { name, .. }) => {
                    // Add the current element to the element stack
                    self.element_stack.push(name.local_name.clone());

                    match &name.local_name[..] {
                        "plist" => (),
                        "array" => return Some(Ok(Event::StartArray(None))),
                        "dict" => return Some(Ok(Event::StartDictionary(None))),
                        "key" => return Some(self.read_content(|s| Ok(Event::StringValue(s)))),
                        "true" => return Some(Ok(Event::BooleanValue(true))),
                        "false" => return Some(Ok(Event::BooleanValue(false))),
                        "data" => {
                            return Some(self.read_content(|s| {
                                let data = base64::decode_config(&s, base64::MIME)
                                    .map_err(|_| Error::InvalidData)?;
                                Ok(Event::DataValue(data))
                            }))
                        }
                        "date" => {
                            return Some(self.read_content(|s| {
                                Ok(Event::DateValue(
                                    Date::from_rfc3339(&s).map_err(|()| Error::InvalidData)?,
                                ))
                            }))
                        }
                        "integer" => {
                            return Some(self.read_content(|s| match FromStr::from_str(&s) {
                                Ok(i) => Ok(Event::IntegerValue(i)),
                                Err(_) => Err(Error::InvalidData),
                            }))
                        }
                        "real" => {
                            return Some(self.read_content(|s| match FromStr::from_str(&s) {
                                Ok(f) => Ok(Event::RealValue(f)),
                                Err(_) => Err(Error::InvalidData),
                            }))
                        }
                        "string" => return Some(self.read_content(|s| Ok(Event::StringValue(s)))),
                        _ => return Some(Err(Error::InvalidData)),
                    }
                }
                Ok(XmlEvent::EndElement { name, .. }) => {
                    // Check the corrent element is being closed
                    match self.element_stack.pop() {
                        Some(ref open_name) if &name.local_name == open_name => (),
                        Some(ref _open_name) => return Some(Err(Error::InvalidData)),
                        None => return Some(Err(Error::InvalidData)),
                    }

                    match &name.local_name[..] {
                        "array" => return Some(Ok(Event::EndArray)),
                        "dict" => return Some(Ok(Event::EndDictionary)),
                        "plist" => (),
                        _ => (),
                    }
                }
                Ok(XmlEvent::EndDocument) => {
                    if self.element_stack.is_empty() {
                        return None;
                    } else {
                        return Some(Err(Error::UnexpectedEof));
                    }
                }
                Err(_) => return Some(Err(Error::InvalidData)),
                _ => (),
            }
        }
    }
}

impl<R: Read> Iterator for XmlReader<R> {
    type Item = Result<Event, Error>;

    fn next(&mut self) -> Option<Result<Event, Error>> {
        if self.finished {
            None
        } else {
            match self.read_next() {
                Some(Ok(event)) => Some(Ok(event)),
                Some(Err(err)) => {
                    self.finished = true;
                    Some(Err(err))
                }
                None => {
                    self.finished = true;
                    None
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use humantime::parse_rfc3339_weak;
    use std::fs::File;
    use std::path::Path;

    use super::*;
    use stream::Event;
    use stream::Event::*;

    #[test]
    fn streaming_parser() {
        let reader = File::open(&Path::new("./tests/data/xml.plist")).unwrap();
        let streaming_parser = XmlReader::new(reader);
        let events: Vec<Event> = streaming_parser.map(|e| e.unwrap()).collect();

        let comparison = &[
            StartDictionary(None),
            StringValue("Author".to_owned()),
            StringValue("William Shakespeare".to_owned()),
            StringValue("Lines".to_owned()),
            StartArray(None),
            StringValue("It is a tale told by an idiot,".to_owned()),
            StringValue("Full of sound and fury, signifying nothing.".to_owned()),
            EndArray,
            StringValue("Death".to_owned()),
            IntegerValue(1564),
            StringValue("Height".to_owned()),
            RealValue(1.60),
            StringValue("Data".to_owned()),
            DataValue(vec![0, 0, 0, 190, 0, 0, 0, 3, 0, 0, 0, 30, 0, 0, 0]),
            StringValue("Birthdate".to_owned()),
            DateValue(parse_rfc3339_weak("1981-05-16 11:32:06").unwrap().into()),
            StringValue("Blank".to_owned()),
            StringValue("".to_owned()),
            EndDictionary,
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
