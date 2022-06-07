#![cfg_attr(not(any(feature = "json", test)), no_std)]
#![deny(elided_lifetimes_in_paths)]
#![deny(unreachable_pub)]

use core::fmt::{self, Display, Formatter, Write};
use core::str;

#[derive(Debug)]
pub struct MarkupDisplay<E, T>
where
    E: Escaper,
    T: Display,
{
    value: DisplayValue<T>,
    escaper: E,
}

impl<E, T> MarkupDisplay<E, T>
where
    E: Escaper,
    T: Display,
{
    pub fn new_unsafe(value: T, escaper: E) -> Self {
        Self {
            value: DisplayValue::Unsafe(value),
            escaper,
        }
    }

    pub fn new_safe(value: T, escaper: E) -> Self {
        Self {
            value: DisplayValue::Safe(value),
            escaper,
        }
    }

    #[must_use]
    pub fn mark_safe(mut self) -> MarkupDisplay<E, T> {
        self.value = match self.value {
            DisplayValue::Unsafe(t) => DisplayValue::Safe(t),
            _ => self.value,
        };
        self
    }
}

impl<E, T> Display for MarkupDisplay<E, T>
where
    E: Escaper,
    T: Display,
{
    fn fmt(&self, fmt: &mut Formatter<'_>) -> fmt::Result {
        match self.value {
            DisplayValue::Unsafe(ref t) => write!(
                EscapeWriter {
                    fmt,
                    escaper: &self.escaper
                },
                "{}",
                t
            ),
            DisplayValue::Safe(ref t) => t.fmt(fmt),
        }
    }
}

#[derive(Debug)]
pub struct EscapeWriter<'a, E, W> {
    fmt: W,
    escaper: &'a E,
}

impl<E, W> Write for EscapeWriter<'_, E, W>
where
    W: Write,
    E: Escaper,
{
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.escaper.write_escaped(&mut self.fmt, s)
    }
}

pub fn escape<E>(string: &str, escaper: E) -> Escaped<'_, E>
where
    E: Escaper,
{
    Escaped { string, escaper }
}

#[derive(Debug)]
pub struct Escaped<'a, E>
where
    E: Escaper,
{
    string: &'a str,
    escaper: E,
}

impl<E> Display for Escaped<'_, E>
where
    E: Escaper,
{
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.escaper.write_escaped(fmt, self.string)
    }
}

pub struct Html;

macro_rules! escaping_body {
    ($start:ident, $i:ident, $fmt:ident, $bytes:ident, $quote:expr) => {{
        if $start < $i {
            $fmt.write_str(unsafe { str::from_utf8_unchecked(&$bytes[$start..$i]) })?;
        }
        $fmt.write_str($quote)?;
        $start = $i + 1;
    }};
}

impl Escaper for Html {
    fn write_escaped<W>(&self, mut fmt: W, string: &str) -> fmt::Result
    where
        W: Write,
    {
        let bytes = string.as_bytes();
        let mut start = 0;
        for (i, b) in bytes.iter().enumerate() {
            if b.wrapping_sub(b'"') <= FLAG {
                match *b {
                    b'<' => escaping_body!(start, i, fmt, bytes, "&lt;"),
                    b'>' => escaping_body!(start, i, fmt, bytes, "&gt;"),
                    b'&' => escaping_body!(start, i, fmt, bytes, "&amp;"),
                    b'"' => escaping_body!(start, i, fmt, bytes, "&quot;"),
                    b'\'' => escaping_body!(start, i, fmt, bytes, "&#x27;"),
                    _ => (),
                }
            }
        }
        if start < bytes.len() {
            fmt.write_str(unsafe { str::from_utf8_unchecked(&bytes[start..]) })
        } else {
            Ok(())
        }
    }
}

pub struct Text;

impl Escaper for Text {
    fn write_escaped<W>(&self, mut fmt: W, string: &str) -> fmt::Result
    where
        W: Write,
    {
        fmt.write_str(string)
    }
}

#[derive(Debug, PartialEq)]
enum DisplayValue<T>
where
    T: Display,
{
    Safe(T),
    Unsafe(T),
}

pub trait Escaper {
    fn write_escaped<W>(&self, fmt: W, string: &str) -> fmt::Result
    where
        W: Write;
}

const FLAG: u8 = b'>' - b'"';

/// Escape chevrons, ampersand and apostrophes for use in JSON
#[cfg(feature = "json")]
#[derive(Debug, Clone, Default)]
pub struct JsonEscapeBuffer(Vec<u8>);

#[cfg(feature = "json")]
impl JsonEscapeBuffer {
    pub fn new() -> Self {
        Self(Vec::new())
    }

    pub fn finish(self) -> String {
        unsafe { String::from_utf8_unchecked(self.0) }
    }
}

#[cfg(feature = "json")]
impl std::io::Write for JsonEscapeBuffer {
    fn write(&mut self, bytes: &[u8]) -> std::io::Result<usize> {
        macro_rules! push_esc_sequence {
            ($start:ident, $i:ident, $self:ident, $bytes:ident, $quote:expr) => {{
                if $start < $i {
                    $self.0.extend_from_slice(&$bytes[$start..$i]);
                }
                $self.0.extend_from_slice($quote);
                $start = $i + 1;
            }};
        }

        self.0.reserve(bytes.len());
        let mut start = 0;
        for (i, b) in bytes.iter().enumerate() {
            match *b {
                b'&' => push_esc_sequence!(start, i, self, bytes, br#"\u0026"#),
                b'\'' => push_esc_sequence!(start, i, self, bytes, br#"\u0027"#),
                b'<' => push_esc_sequence!(start, i, self, bytes, br#"\u003c"#),
                b'>' => push_esc_sequence!(start, i, self, bytes, br#"\u003e"#),
                _ => (),
            }
        }
        if start < bytes.len() {
            self.0.extend_from_slice(&bytes[start..]);
        }
        Ok(bytes.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::string::ToString;

    #[test]
    fn test_escape() {
        assert_eq!(escape("", Html).to_string(), "");
        assert_eq!(escape("<&>", Html).to_string(), "&lt;&amp;&gt;");
        assert_eq!(escape("bla&", Html).to_string(), "bla&amp;");
        assert_eq!(escape("<foo", Html).to_string(), "&lt;foo");
        assert_eq!(escape("bla&h", Html).to_string(), "bla&amp;h");
    }
}
