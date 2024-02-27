use crate::{ParseError, err::{perr, ParseErrorKind::*}, parse::{hex_digit_value, check_suffix}};


/// Must start with `\`
pub(crate) fn unescape<E: Escapee>(input: &str, offset: usize) -> Result<(E, usize), ParseError> {
    let first = input.as_bytes().get(1)
        .ok_or(perr(offset, UnterminatedEscape))?;
    let out = match first {
        // Quote escapes
        b'\'' => (E::from_byte(b'\''), 2),
        b'"' => (E::from_byte(b'"'), 2),

        // Ascii escapes
        b'n' => (E::from_byte(b'\n'), 2),
        b'r' => (E::from_byte(b'\r'), 2),
        b't' => (E::from_byte(b'\t'), 2),
        b'\\' => (E::from_byte(b'\\'), 2),
        b'0' => (E::from_byte(b'\0'), 2),
        b'x' => {
            let hex_string = input.get(2..4)
                .ok_or(perr(offset..offset + input.len(), UnterminatedEscape))?
                .as_bytes();
            let first = hex_digit_value(hex_string[0])
                .ok_or(perr(offset..offset + 4, InvalidXEscape))?;
            let second = hex_digit_value(hex_string[1])
                .ok_or(perr(offset..offset + 4, InvalidXEscape))?;
            let value = second + 16 * first;

            if E::SUPPORTS_UNICODE && value > 0x7F {
                return Err(perr(offset..offset + 4, NonAsciiXEscape));
            }

            (E::from_byte(value), 4)
        },

        // Unicode escape
        b'u' => {
            if !E::SUPPORTS_UNICODE {
                return Err(perr(offset..offset + 2, UnicodeEscapeInByteLiteral));
            }

            if input.as_bytes().get(2) != Some(&b'{') {
                return Err(perr(offset..offset + 2, UnicodeEscapeWithoutBrace));
            }

            let closing_pos = input.bytes().position(|b| b == b'}')
                .ok_or(perr(offset..offset + input.len(), UnterminatedUnicodeEscape))?;

            let inner = &input[3..closing_pos];
            if inner.as_bytes().first() == Some(&b'_') {
                return Err(perr(4, InvalidStartOfUnicodeEscape));
            }

            let mut v: u32 = 0;
            let mut digit_count = 0;
            for (i, b) in inner.bytes().enumerate() {
                if b == b'_'{
                    continue;
                }

                let digit = hex_digit_value(b)
                    .ok_or(perr(offset + 3 + i, NonHexDigitInUnicodeEscape))?;

                if digit_count == 6 {
                    return Err(perr(offset + 3 + i, TooManyDigitInUnicodeEscape));
                }
                digit_count += 1;
                v = 16 * v + digit as u32;
            }

            let c = std::char::from_u32(v)
                .ok_or(perr(offset..closing_pos + 1, InvalidUnicodeEscapeChar))?;

            (E::from_char(c), closing_pos + 1)
        }

        _ => return Err(perr(offset..offset + 2, UnknownEscape)),
    };

    Ok(out)
}

pub(crate) trait Escapee: Into<char> {
    const SUPPORTS_UNICODE: bool;
    fn from_byte(b: u8) -> Self;
    fn from_char(c: char) -> Self;
}

impl Escapee for u8 {
    const SUPPORTS_UNICODE: bool = false;
    fn from_byte(b: u8) -> Self {
        b
    }
    fn from_char(_: char) -> Self {
        panic!("bug: `<u8 as Escapee>::from_char` was called");
    }
}

impl Escapee for char {
    const SUPPORTS_UNICODE: bool = true;
    fn from_byte(b: u8) -> Self {
        b.into()
    }
    fn from_char(c: char) -> Self {
        c
    }
}

/// Checks whether the character is skipped after a string continue start
/// (unescaped backlash followed by `\n`).
fn is_string_continue_skipable_whitespace(b: u8) -> bool {
    b == b' ' || b == b'\t' || b == b'\n' || b == b'\r'
}

/// Unescapes a whole string or byte string.
#[inline(never)]
pub(crate) fn unescape_string<E: Escapee>(
    input: &str,
    offset: usize,
) -> Result<(Option<String>, usize), ParseError> {
    let mut closing_quote_pos = None;
    let mut i = offset;
    let mut end_last_escape = offset;
    let mut value = String::new();
    while i < input.len() {
        match input.as_bytes()[i] {
            // Handle "string continue".
            b'\\' if input.as_bytes().get(i + 1) == Some(&b'\n') => {
                value.push_str(&input[end_last_escape..i]);

                // Find the first non-whitespace character.
                let end_escape = input[i + 2..].bytes()
                    .position(|b| !is_string_continue_skipable_whitespace(b))
                    .ok_or(perr(None, UnterminatedString))?;

                i += 2 + end_escape;
                end_last_escape = i;
            }
            b'\\' => {
                let (c, len) = unescape::<E>(&input[i..input.len() - 1], i)?;
                value.push_str(&input[end_last_escape..i]);
                value.push(c.into());
                i += len;
                end_last_escape = i;
            }
            b'\r' => {
                if input.as_bytes().get(i + 1) == Some(&b'\n') {
                    value.push_str(&input[end_last_escape..i]);
                    value.push('\n');
                    i += 2;
                    end_last_escape = i;
                } else {
                    return Err(perr(i, IsolatedCr))
                }
            }
            b'"' => {
                closing_quote_pos = Some(i);
                break;
            },
            b if !E::SUPPORTS_UNICODE && !b.is_ascii()
                => return Err(perr(i, NonAsciiInByteLiteral)),
            _ => i += 1,
        }
    }

    let closing_quote_pos = closing_quote_pos.ok_or(perr(None, UnterminatedString))?;

    let start_suffix = closing_quote_pos + 1;
    let suffix = &input[start_suffix..];
    check_suffix(suffix).map_err(|kind| perr(start_suffix, kind))?;

    // `value` is only empty if there was no escape in the input string
    // (with the special case of the input being empty). This means the
    // string value basically equals the input, so we store `None`.
    let value = if value.is_empty() {
        None
    } else {
        // There was an escape in the string, so we need to push the
        // remaining unescaped part of the string still.
        value.push_str(&input[end_last_escape..closing_quote_pos]);
        Some(value)
    };

    Ok((value, start_suffix))
}

/// Reads and checks a raw (byte) string literal, converting `\r\n` sequences to
/// just `\n` sequences. Returns an optional new string (if the input contained
/// any `\r\n`) and the number of hashes used by the literal.
#[inline(never)]
pub(crate) fn scan_raw_string<E: Escapee>(
    input: &str,
    offset: usize,
) -> Result<(Option<String>, u32, usize), ParseError> {
    // Raw string literal
    let num_hashes = input[offset..].bytes().position(|b| b != b'#')
        .ok_or(perr(None, InvalidLiteral))?;

    if input.as_bytes().get(offset + num_hashes) != Some(&b'"') {
        return Err(perr(None, InvalidLiteral));
    }
    let start_inner = offset + num_hashes + 1;
    let hashes = &input[offset..num_hashes + offset];

    let mut closing_quote_pos = None;
    let mut i = start_inner;
    let mut end_last_escape = start_inner;
    let mut value = String::new();
    while i < input.len() {
        let b = input.as_bytes()[i];
        if b == b'"' && input[i + 1..].starts_with(hashes) {
            closing_quote_pos = Some(i);
            break;
        }

        if b == b'\r' {
            // Convert `\r\n` into `\n`. This is currently not well documented
            // in the Rust reference, but is done even for raw strings. That's
            // because rustc simply converts all line endings when reading
            // source files.
            if input.as_bytes().get(i + 1) == Some(&b'\n') {
                value.push_str(&input[end_last_escape..i]);
                value.push('\n');
                i += 2;
                end_last_escape = i;
                continue;
            } else if E::SUPPORTS_UNICODE {
                // If no \n follows the \r and we are scanning a raw string
                // (not raw byte string), we error.
                return Err(perr(i, IsolatedCr))
            }
        }

        if !E::SUPPORTS_UNICODE {
            if !b.is_ascii() {
                return Err(perr(i, NonAsciiInByteLiteral));
            }
        }

        i += 1;
    }

    let closing_quote_pos = closing_quote_pos.ok_or(perr(None, UnterminatedRawString))?;

    let start_suffix = closing_quote_pos + num_hashes + 1;
    let suffix = &input[start_suffix..];
    check_suffix(suffix).map_err(|kind| perr(start_suffix, kind))?;

    // `value` is only empty if there was no \r\n in the input string (with the
    // special case of the input being empty). This means the string value
    // equals the input, so we store `None`.
    let value = if value.is_empty() {
        None
    } else {
        // There was an \r\n in the string, so we need to push the remaining
        // unescaped part of the string still.
        value.push_str(&input[end_last_escape..closing_quote_pos]);
        Some(value)
    };

    Ok((value, num_hashes as u32, start_suffix))
}
