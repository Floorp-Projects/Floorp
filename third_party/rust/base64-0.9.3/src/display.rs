//! Enables base64'd output anywhere you might use a `Display` implementation, like a format string.
//!
//! ```
//! use base64::display::Base64Display;
//!
//! let data = vec![0x0, 0x1, 0x2, 0x3];
//! let wrapper = Base64Display::standard(&data);
//!
//! assert_eq!("base64: AAECAw==", format!("base64: {}", wrapper));
//! ```

use super::chunked_encoder::{ChunkedEncoder, ChunkedEncoderError};
use super::Config;
use std::fmt::{Display, Formatter};
use std::{fmt, str};

// I'm not convinced that we should expose ChunkedEncoder or its error type since it's just an
// implementation detail, so use a different error type.
/// Errors that can occur initializing a Base64Display.
#[derive(Debug, PartialEq)]
pub enum DisplayError {
    /// If wrapping is configured, the line length must be a multiple of 4, and must not be absurdly
    /// large (currently capped at 1024, subject to change).
    InvalidLineLength,
}

/// A convenience wrapper for base64'ing bytes into a format string without heap allocation.
pub struct Base64Display<'a> {
    bytes: &'a [u8],
    chunked_encoder: ChunkedEncoder,
}

impl<'a> Base64Display<'a> {
    /// Create a `Base64Display` with the provided config.
    pub fn with_config(bytes: &[u8], config: Config) -> Result<Base64Display, DisplayError> {
        ChunkedEncoder::new(config)
            .map(|c| Base64Display {
                bytes,
                chunked_encoder: c,
            })
            .map_err(|e| match e {
                ChunkedEncoderError::InvalidLineLength => DisplayError::InvalidLineLength,
            })
    }

    /// Convenience method for creating a `Base64Display` with the `STANDARD` configuration.
    pub fn standard(bytes: &[u8]) -> Base64Display {
        Base64Display::with_config(bytes, super::STANDARD).expect("STANDARD is valid")
    }

    /// Convenience method for creating a `Base64Display` with the `URL_SAFE` configuration.
    pub fn url_safe(bytes: &[u8]) -> Base64Display {
        Base64Display::with_config(bytes, super::URL_SAFE).expect("URL_SAFE is valid")
    }
}

impl<'a> Display for Base64Display<'a> {
    fn fmt(&self, formatter: &mut Formatter) -> Result<(), fmt::Error> {
        let mut sink = FormatterSink { f: formatter };
        self.chunked_encoder.encode(self.bytes, &mut sink)
    }
}

struct FormatterSink<'a, 'b: 'a> {
    f: &'a mut Formatter<'b>,
}

impl<'a, 'b: 'a> super::chunked_encoder::Sink for FormatterSink<'a, 'b> {
    type Error = fmt::Error;

    fn write_encoded_bytes(&mut self, encoded: &[u8]) -> Result<(), Self::Error> {
        // Avoid unsafe. If max performance is needed, write your own display wrapper that uses
        // unsafe here to gain about 10-15%.
        self.f
            .write_str(str::from_utf8(encoded).expect("base64 data was not utf8"))
    }
}

#[cfg(test)]
mod tests {
    use super::super::chunked_encoder::tests::{chunked_encode_matches_normal_encode_random,
                                               SinkTestHelper};
    use super::super::*;
    use super::*;

    #[test]
    fn basic_display() {
        assert_eq!(
            "~$Zm9vYmFy#*",
            format!("~${}#*", Base64Display::standard("foobar".as_bytes()))
        );
        assert_eq!(
            "~$Zm9vYmFyZg==#*",
            format!("~${}#*", Base64Display::standard("foobarf".as_bytes()))
        );
    }

    #[test]
    fn display_encode_matches_normal_encode() {
        let helper = DisplaySinkTestHelper;
        chunked_encode_matches_normal_encode_random(&helper);
    }

    struct DisplaySinkTestHelper;

    impl SinkTestHelper for DisplaySinkTestHelper {
        fn encode_to_string(&self, config: Config, bytes: &[u8]) -> String {
            format!("{}", Base64Display::with_config(bytes, config).unwrap())
        }
    }

}
