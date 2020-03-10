use std::borrow::Cow;
use std::char;

const UNKNOWN_CHAR: char = '�';

fn encode_unicode(s: &str) -> char {
    u32::from_str_radix(s, 16)
        .ok()
        .and_then(char::from_u32)
        .unwrap_or(UNKNOWN_CHAR)
}

pub fn unescape_unicode(input: &str) -> Cow<str> {
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
                    .map(|slice| encode_unicode(slice))
                    .unwrap_or(UNKNOWN_CHAR)
            }
            _ => UNKNOWN_CHAR,
        };
        result.to_mut().push(new_char);
        ptr += 1;
    }
    result
}
