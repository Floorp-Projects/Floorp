use std::fmt::{self, Display, Formatter, Write};

/// Simple but still over-engineered XML generator for a [`Formatter`], for generating
/// Windows Manifest XML. This can easily generate invalid XML.
///
/// When used correctly, this should generate the same output as MT’s `-canonicalize`
/// option.
pub struct XmlFormatter<'a, 'f> {
    f: &'f mut Formatter<'a>,
    state: State,
    depth: usize,
}

#[derive(Eq, PartialEq)]
enum State {
    Init,
    StartTag,
    Text,
}

impl<'a, 'f> XmlFormatter<'a, 'f> {
    pub fn new(f: &'f mut Formatter<'a>) -> Self {
        Self {
            f,
            state: State::Init,
            depth: 0,
        }
    }

    fn pretty(&mut self) -> fmt::Result {
        if self.f.alternate() {
            self.f.write_str("\r\n")?;
            for _ in 0..self.depth {
                self.f.write_str("    ")?;
            }
        }
        Ok(())
    }

    pub fn start_document(&mut self) -> fmt::Result {
        if !self.f.alternate() {
            self.f.write_char('\u{FEFF}')?;
        }
        self.f
            .write_str("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n")
    }

    pub fn element<F: FnOnce(&mut Self) -> fmt::Result>(&mut self, name: &str, attrs: &[(&str, &str)], f: F) -> fmt::Result {
        self.start_element(name, attrs)?;
        f(self)?;
        self.end_element(name)
    }

    pub fn empty_element(&mut self, name: &str, attrs: &[(&str, &str)]) -> fmt::Result {
        self.start_element(name, attrs)?;
        self.end_element(name)
    }

    pub fn start_element(&mut self, name: &str, attrs: &[(&str, &str)]) -> fmt::Result {
        if self.state == State::StartTag {
            self.f.write_char('>')?;
        }
        if self.depth != 0 {
            self.pretty()?;
        }
        write!(self.f, "<{}", name)?;
        for (name, value) in attrs {
            write!(self.f, " {}=\"{}\"", name, Xml(value))?;
        }
        self.depth += 1;
        self.state = State::StartTag;
        Ok(())
    }

    pub fn end_element(&mut self, name: &str) -> fmt::Result {
        self.depth -= 1;
        match self.state {
            State::Init => {
                self.pretty()?;
                write!(self.f, "</{}>", name)
            }
            State::Text => {
                self.state = State::Init;
                write!(self.f, "</{}>", name)
            }
            State::StartTag => {
                self.state = State::Init;
                if self.f.alternate() {
                    self.f.write_str("/>")
                } else {
                    write!(self.f, "></{}>", name)
                }
            }
        }
    }

    pub fn text(&mut self, s: &str) -> fmt::Result {
        if self.state == State::StartTag {
            self.state = State::Text;
            self.f.write_char('>')?;
        }
        Xml(s).fmt(self.f)
    }
}

/// Temporary wrapper for outputting a string with XML attribute encoding.
/// This does not do anything with the control characters which are not
/// valid in XML, encoded or not.
struct Xml<'a>(&'a str);

impl<'a> Display for Xml<'a> {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        // Process the string in blocks separated by special characters, so that the parts that
        // don't need encoding can be written all at once, not character by character, and with
        // no checks for whether string slices are aligned on character boundaries.
        for s in self.0.split_inclusive(&['<', '&', '>', '"', '\r'][..]) {
            // Check whether the last character in the substring needs encoding. This will be
            // `None` at the end of the input string.
            let mut iter = s.chars();
            let ch = match iter.next_back() {
                Some('<') => Some("&lt;"),
                Some('&') => Some("&amp;"),
                Some('>') => Some("&gt;"),
                Some('"') => Some("&quot;"),
                Some('\r') => Some("&#13;"),
                _ => None,
            };
            // Write the substring except the last character, then the encoded character;
            // or the entire substring if it is not terminated by a special character.
            match ch {
                Some(enc) => {
                    f.write_str(iter.as_str())?;
                    f.write_str(enc)?;
                }
                None => f.write_str(s)?,
            }
        }
        Ok(())
    }
}
