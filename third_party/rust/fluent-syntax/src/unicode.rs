use std::borrow::Cow;
use std::char;
use std::fmt;

const UNKNOWN_CHAR: char = '�';

fn encode_unicode(s: Option<&str>) -> char {
    s.and_then(|s| u32::from_str_radix(s, 16).ok().and_then(char::from_u32))
        .unwrap_or(UNKNOWN_CHAR)
}

pub fn unescape_unicode<W>(w: &mut W, input: &str) -> fmt::Result
where
    W: fmt::Write,
{
    let bytes = input.as_bytes();

    let mut start = 0;
    let mut ptr = 0;

    while let Some(b) = bytes.get(ptr) {
        if b != &b'\\' {
            ptr += 1;
            continue;
        }
        if start != ptr {
            w.write_str(&input[start..ptr])?;
        }

        ptr += 1;

        let new_char = match bytes.get(ptr) {
            Some(b'\\') => '\\',
            Some(b'"') => '"',
            Some(u @ b'u') | Some(u @ b'U') => {
                let seq_start = ptr + 1;
                let len = if u == &b'u' { 4 } else { 6 };
                ptr += len;
                encode_unicode(input.get(seq_start..seq_start + len))
            }
            _ => UNKNOWN_CHAR,
        };
        ptr += 1;
        w.write_char(new_char)?;
        start = ptr;
    }
    if start != ptr {
        w.write_str(&input[start..ptr])?;
    }
    Ok(())
}

pub fn unescape_unicode_to_string(input: &str) -> Cow<str> {
    let bytes = input.as_bytes();
    let mut result = Cow::from(input);

    let mut ptr = 0;

    while let Some(b) = bytes.get(ptr) {
        if b != &b'\\' {
            if let Cow::Owned(ref mut s) = result {
                s.push(*b as char);
            }
            ptr += 1;
            continue;
        }

        if let Cow::Borrowed(_) = result {
            result = Cow::from(&input[0..ptr]);
        }

        ptr += 1;

        let new_char = match bytes.get(ptr) {
            Some(b'\\') => '\\',
            Some(b'"') => '"',
            Some(u @ b'u') | Some(u @ b'U') => {
                let start = ptr + 1;
                let len = if u == &b'u' { 4 } else { 6 };
                ptr += len;
                input
                    .get(start..(start + len))
                    .map_or(UNKNOWN_CHAR, |slice| encode_unicode(Some(slice)))
            }
            _ => UNKNOWN_CHAR,
        };
        result.to_mut().push(new_char);
        ptr += 1;
    }
    result
}
