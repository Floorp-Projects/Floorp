use std::convert::{From, Into};
use std::fmt;
use std::result::Result as StdResult;
use std::str::from_utf8;

use protocol::OpCode;
use result::Result;

use self::Message::*;

/// An enum representing the various forms of a WebSocket message.
#[derive(Debug, Eq, PartialEq, Clone)]
pub enum Message {
    /// A text WebSocket message
    Text(String),
    /// A binary WebSocket message
    Binary(Vec<u8>),
}

impl Message {
    /// Create a new text WebSocket message from a stringable.
    pub fn text<S>(string: S) -> Message
    where
        S: Into<String>,
    {
        Message::Text(string.into())
    }

    /// Create a new binary WebSocket message by converting to Vec<u8>.
    pub fn binary<B>(bin: B) -> Message
    where
        B: Into<Vec<u8>>,
    {
        Message::Binary(bin.into())
    }

    /// Indicates whether a message is a text message.
    pub fn is_text(&self) -> bool {
        match *self {
            Text(_) => true,
            Binary(_) => false,
        }
    }

    /// Indicates whether a message is a binary message.
    pub fn is_binary(&self) -> bool {
        match *self {
            Text(_) => false,
            Binary(_) => true,
        }
    }

    /// Get the length of the WebSocket message.
    pub fn len(&self) -> usize {
        match *self {
            Text(ref string) => string.len(),
            Binary(ref data) => data.len(),
        }
    }

    /// Returns true if the WebSocket message has no content.
    /// For example, if the other side of the connection sent an empty string.
    pub fn is_empty(&self) -> bool {
        match *self {
            Text(ref string) => string.is_empty(),
            Binary(ref data) => data.is_empty(),
        }
    }

    #[doc(hidden)]
    pub fn opcode(&self) -> OpCode {
        match *self {
            Text(_) => OpCode::Text,
            Binary(_) => OpCode::Binary,
        }
    }

    /// Consume the WebSocket and return it as binary data.
    pub fn into_data(self) -> Vec<u8> {
        match self {
            Text(string) => string.into_bytes(),
            Binary(data) => data,
        }
    }

    /// Attempt to consume the WebSocket message and convert it to a String.
    pub fn into_text(self) -> Result<String> {
        match self {
            Text(string) => Ok(string),
            Binary(data) => Ok(String::from_utf8(data).map_err(|err| err.utf8_error())?),
        }
    }

    /// Attempt to get a &str from the WebSocket message,
    /// this will try to convert binary data to utf8.
    pub fn as_text(&self) -> Result<&str> {
        match *self {
            Text(ref string) => Ok(string),
            Binary(ref data) => Ok(from_utf8(data)?),
        }
    }
}

impl From<String> for Message {
    fn from(string: String) -> Message {
        Message::text(string)
    }
}

impl<'s> From<&'s str> for Message {
    fn from(string: &'s str) -> Message {
        Message::text(string)
    }
}

impl<'b> From<&'b [u8]> for Message {
    fn from(data: &'b [u8]) -> Message {
        Message::binary(data)
    }
}

impl From<Vec<u8>> for Message {
    fn from(data: Vec<u8>) -> Message {
        Message::binary(data)
    }
}

impl fmt::Display for Message {
    fn fmt(&self, f: &mut fmt::Formatter) -> StdResult<(), fmt::Error> {
        if let Ok(string) = self.as_text() {
            write!(f, "{}", string)
        } else {
            write!(f, "Binary Data<length={}>", self.len())
        }
    }
}

mod test {
    #![allow(unused_imports, unused_variables, dead_code)]
    use super::*;

    #[test]
    fn display() {
        let t = Message::text(format!("test"));
        assert_eq!(t.to_string(), "test".to_owned());

        let bin = Message::binary(vec![0, 1, 3, 4, 241]);
        assert_eq!(bin.to_string(), "Binary Data<length=5>".to_owned());
    }

    #[test]
    fn binary_convert() {
        let bin = [6u8, 7, 8, 9, 10, 241];
        let msg = Message::from(&bin[..]);
        assert!(msg.is_binary());
        assert!(msg.into_text().is_err());
    }

    #[test]
    fn binary_convert_vec() {
        let bin = vec![6u8, 7, 8, 9, 10, 241];
        let msg = Message::from(bin);
        assert!(msg.is_binary());
        assert!(msg.into_text().is_err());
    }

    #[test]
    fn text_convert() {
        let s = "kiwotsukete";
        let msg = Message::from(s);
        assert!(msg.is_text());
    }
}
