use base64;
use line_wrap;
use std::{borrow::Cow, io::Write};
use xml_rs::{
    name::Name,
    namespace::Namespace,
    writer::{EmitterConfig, Error as XmlWriterError, EventWriter, XmlEvent},
};

use crate::{
    error::{self, Error, ErrorKind, EventKind},
    stream::{Writer, XmlWriteOptions},
    Date, Integer, Uid,
};

static XML_PROLOGUE: &str = r#"<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
"#;

#[derive(PartialEq)]
enum Element {
    Dictionary,
    Array,
}

pub struct XmlWriter<W: Write> {
    xml_writer: EventWriter<W>,
    stack: Vec<Element>,
    expecting_key: bool,
    written_prologue: bool,
    // Not very nice
    empty_namespace: Namespace,
}

impl<W: Write> XmlWriter<W> {
    pub fn new(writer: W) -> XmlWriter<W> {
        let opts = XmlWriteOptions::default();
        XmlWriter::new_with_options(writer, &opts)
    }

    pub fn new_with_options(writer: W, opts: &XmlWriteOptions) -> XmlWriter<W> {
        let config = EmitterConfig::new()
            .line_separator("\n")
            .indent_string(opts.indent_str.clone())
            .perform_indent(true)
            .write_document_declaration(false)
            .normalize_empty_elements(true)
            .cdata_to_characters(true)
            .keep_element_names_stack(false)
            .autopad_comments(true)
            .pad_self_closing(false);

        XmlWriter {
            xml_writer: EventWriter::new_with_config(writer, config),
            stack: Vec::new(),
            expecting_key: false,
            written_prologue: false,
            empty_namespace: Namespace::empty(),
        }
    }

    fn write_element_and_value(&mut self, name: &str, value: &str) -> Result<(), Error> {
        self.start_element(name)?;
        self.write_value(value)?;
        self.end_element(name)?;
        Ok(())
    }

    fn start_element(&mut self, name: &str) -> Result<(), Error> {
        self.xml_writer
            .write(XmlEvent::StartElement {
                name: Name::local(name),
                attributes: Cow::Borrowed(&[]),
                namespace: Cow::Borrowed(&self.empty_namespace),
            })
            .map_err(from_xml_error)?;
        Ok(())
    }

    fn end_element(&mut self, name: &str) -> Result<(), Error> {
        self.xml_writer
            .write(XmlEvent::EndElement {
                name: Some(Name::local(name)),
            })
            .map_err(from_xml_error)?;
        Ok(())
    }

    fn write_value(&mut self, value: &str) -> Result<(), Error> {
        self.xml_writer
            .write(XmlEvent::Characters(value))
            .map_err(from_xml_error)?;
        Ok(())
    }

    pub fn into_inner(self) -> W {
        self.xml_writer.into_inner()
    }

    fn write_event<F: FnOnce(&mut Self) -> Result<(), Error>>(
        &mut self,
        f: F,
    ) -> Result<(), Error> {
        if !self.written_prologue {
            self.xml_writer
                .inner_mut()
                .write_all(XML_PROLOGUE.as_bytes())
                .map_err(error::from_io_without_position)?;

            self.written_prologue = true;
        }

        f(self)?;

        // If there are no more open tags then write the </plist> element
        if self.stack.is_empty() {
            // We didn't tell the xml_writer about the <plist> tag so we'll skip telling it
            // about the </plist> tag as well.
            self.xml_writer
                .inner_mut()
                .write_all(b"\n</plist>")
                .map_err(error::from_io_without_position)?;
            self.xml_writer
                .inner_mut()
                .flush()
                .map_err(error::from_io_without_position)?;
        }

        Ok(())
    }

    fn write_value_event<F: FnOnce(&mut Self) -> Result<(), Error>>(
        &mut self,
        event_kind: EventKind,
        f: F,
    ) -> Result<(), Error> {
        self.write_event(|this| {
            if this.expecting_key {
                return Err(ErrorKind::UnexpectedEventType {
                    expected: EventKind::DictionaryKeyOrEndCollection,
                    found: event_kind,
                }
                .without_position());
            }
            f(this)?;
            this.expecting_key = this.stack.last() == Some(&Element::Dictionary);
            Ok(())
        })
    }
}

impl<W: Write> Writer for XmlWriter<W> {
    fn write_start_array(&mut self, _len: Option<u64>) -> Result<(), Error> {
        self.write_value_event(EventKind::StartArray, |this| {
            this.start_element("array")?;
            this.stack.push(Element::Array);
            Ok(())
        })
    }

    fn write_start_dictionary(&mut self, _len: Option<u64>) -> Result<(), Error> {
        self.write_value_event(EventKind::StartDictionary, |this| {
            this.start_element("dict")?;
            this.stack.push(Element::Dictionary);
            Ok(())
        })
    }

    fn write_end_collection(&mut self) -> Result<(), Error> {
        self.write_event(|this| {
            match (this.stack.pop(), this.expecting_key) {
                (Some(Element::Dictionary), true) => {
                    this.end_element("dict")?;
                }
                (Some(Element::Array), _) => {
                    this.end_element("array")?;
                }
                (Some(Element::Dictionary), false) | (None, _) => {
                    return Err(ErrorKind::UnexpectedEventType {
                        expected: EventKind::ValueOrStartCollection,
                        found: EventKind::EndCollection,
                    }
                    .without_position());
                }
            }
            this.expecting_key = this.stack.last() == Some(&Element::Dictionary);
            Ok(())
        })
    }

    fn write_boolean(&mut self, value: bool) -> Result<(), Error> {
        self.write_value_event(EventKind::Boolean, |this| {
            let value_str = if value { "true" } else { "false" };
            this.start_element(value_str)?;
            this.end_element(value_str)
        })
    }

    fn write_data(&mut self, value: &[u8]) -> Result<(), Error> {
        self.write_value_event(EventKind::Data, |this| {
            let base64_data = base64_encode_plist(&value, this.stack.len());
            this.write_element_and_value("data", &base64_data)
        })
    }

    fn write_date(&mut self, value: Date) -> Result<(), Error> {
        self.write_value_event(EventKind::Date, |this| {
            this.write_element_and_value("date", &value.to_rfc3339())
        })
    }

    fn write_integer(&mut self, value: Integer) -> Result<(), Error> {
        self.write_value_event(EventKind::Integer, |this| {
            this.write_element_and_value("integer", &value.to_string())
        })
    }

    fn write_real(&mut self, value: f64) -> Result<(), Error> {
        self.write_value_event(EventKind::Real, |this| {
            this.write_element_and_value("real", &value.to_string())
        })
    }

    fn write_string(&mut self, value: &str) -> Result<(), Error> {
        self.write_event(|this| {
            if this.expecting_key {
                this.write_element_and_value("key", &*value)?;
                this.expecting_key = false;
            } else {
                this.write_element_and_value("string", &*value)?;
                this.expecting_key = this.stack.last() == Some(&Element::Dictionary);
            }
            Ok(())
        })
    }

    fn write_uid(&mut self, _value: Uid) -> Result<(), Error> {
        Err(ErrorKind::UidNotSupportedInXmlPlist.without_position())
    }
}

pub(crate) fn from_xml_error(err: XmlWriterError) -> Error {
    match err {
        XmlWriterError::Io(err) => ErrorKind::Io(err).without_position(),
        XmlWriterError::DocumentStartAlreadyEmitted
        | XmlWriterError::LastElementNameNotAvailable
        | XmlWriterError::EndElementNameIsNotEqualToLastStartElementName
        | XmlWriterError::EndElementNameIsNotSpecified => unreachable!(),
    }
}

fn base64_encode_plist(data: &[u8], indent: usize) -> String {
    // XML plist data elements are always formatted by apple tools as
    // <data>
    // AAAA..AA (68 characters per line)
    // </data>
    // Allocate space for base 64 string and line endings up front
    const LINE_LEN: usize = 68;
    let mut line_ending = Vec::with_capacity(1 + indent);
    line_ending.push(b'\n');
    (0..indent).for_each(|_| line_ending.push(b'\t'));

    // Find the max length of `data` encoded as a base 64 string with padding
    let base64_max_string_len = data.len() * 4 / 3 + 4;

    // Find the max length of the formatted base 64 string as: max length of the base 64 string
    // + line endings and indents at the start of the string and after every line
    let base64_max_string_len_with_formatting =
        base64_max_string_len + (2 + base64_max_string_len / LINE_LEN) * line_ending.len();

    let mut output = vec![0; base64_max_string_len_with_formatting];

    // Start output with a line ending and indent
    output[..line_ending.len()].copy_from_slice(&line_ending);

    // Encode `data` as a base 64 string
    let base64_string_len =
        base64::encode_config_slice(data, base64::STANDARD, &mut output[line_ending.len()..]);

    // Line wrap the base 64 encoded string
    let line_wrap_len = line_wrap::line_wrap(
        &mut output[line_ending.len()..],
        base64_string_len,
        LINE_LEN,
        &line_wrap::SliceLineEnding::new(&line_ending),
    );

    // Add the final line ending and indent
    output[line_ending.len() + base64_string_len + line_wrap_len..][..line_ending.len()]
        .copy_from_slice(&line_ending);

    // Ensure output is the correct length
    output.truncate(base64_string_len + line_wrap_len + 2 * line_ending.len());
    String::from_utf8(output).expect("base 64 string must be valid utf8")
}

#[cfg(test)]
mod tests {
    use std::io::Cursor;

    use super::*;
    use crate::stream::Event;

    #[test]
    fn streaming_parser() {
        let plist = &[
            Event::StartDictionary(None),
            Event::String("Author".into()),
            Event::String("William Shakespeare".into()),
            Event::String("Lines".into()),
            Event::StartArray(None),
            Event::String("It is a tale told by an idiot,".into()),
            Event::String("Full of sound and fury, signifying nothing.".into()),
            Event::Data((0..128).collect::<Vec<_>>().into()),
            Event::EndCollection,
            Event::String("Death".into()),
            Event::Integer(1564.into()),
            Event::String("Height".into()),
            Event::Real(1.60),
            Event::String("Data".into()),
            Event::Data(vec![0, 0, 0, 190, 0, 0, 0, 3, 0, 0, 0, 30, 0, 0, 0].into()),
            Event::String("Birthdate".into()),
            Event::Date(super::Date::from_rfc3339("1981-05-16T11:32:06Z").unwrap()),
            Event::String("Comment".into()),
            Event::String("2 < 3".into()), // make sure characters are escaped
            Event::String("BiggestNumber".into()),
            Event::Integer(18446744073709551615u64.into()),
            Event::String("SmallestNumber".into()),
            Event::Integer((-9223372036854775808i64).into()),
            Event::String("IsTrue".into()),
            Event::Boolean(true),
            Event::String("IsNotFalse".into()),
            Event::Boolean(false),
            Event::EndCollection,
        ];

        let mut cursor = Cursor::new(Vec::new());

        {
            let mut plist_w = XmlWriter::new(&mut cursor);

            for item in plist {
                plist_w.write(item).unwrap();
            }
        }

        let comparison = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
\t<key>Author</key>
\t<string>William Shakespeare</string>
\t<key>Lines</key>
\t<array>
\t\t<string>It is a tale told by an idiot,</string>
\t\t<string>Full of sound and fury, signifying nothing.</string>
\t\t<data>
\t\tAAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEy
\t\tMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2Rl
\t\tZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn8=
\t\t</data>
\t</array>
\t<key>Death</key>
\t<integer>1564</integer>
\t<key>Height</key>
\t<real>1.6</real>
\t<key>Data</key>
\t<data>
\tAAAAvgAAAAMAAAAeAAAA
\t</data>
\t<key>Birthdate</key>
\t<date>1981-05-16T11:32:06Z</date>
\t<key>Comment</key>
\t<string>2 &lt; 3</string>
\t<key>BiggestNumber</key>
\t<integer>18446744073709551615</integer>
\t<key>SmallestNumber</key>
\t<integer>-9223372036854775808</integer>
\t<key>IsTrue</key>
\t<true/>
\t<key>IsNotFalse</key>
\t<false/>
</dict>
</plist>";

        let s = String::from_utf8(cursor.into_inner()).unwrap();

        assert_eq!(s, comparison);
    }
}
