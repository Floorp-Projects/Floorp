use data_encoding::{Encoding, Specification};
use std::iter::Peekable;
use std::str::Chars;

pub(crate) fn base64() -> Result<Encoding, &'static str> {
    let mut spec = Specification::new();
    spec.check_trailing_bits = false;
    spec.symbols
        .push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    spec.padding = None;
    spec.ignore = "=".to_owned();
    spec.encoding()
        .map_err(|_err| "invalid base64 specification")
}

pub(crate) fn is_tchar(c: char) -> bool {
    // See tchar values list in https://tools.ietf.org/html/rfc7230#section-3.2.6
    let tchars = "!#$%&'*+-.^_`|~";
    tchars.contains(c) || c.is_ascii_alphanumeric()
}

pub(crate) fn is_allowed_b64_content(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '+' || c == '=' || c == '/'
}

pub(crate) fn consume_ows_chars(input_chars: &mut Peekable<Chars>) {
    while let Some(c) = input_chars.peek() {
        if c == &' ' || c == &'\t' {
            input_chars.next();
        } else {
            break;
        }
    }
}

pub(crate) fn consume_sp_chars(input_chars: &mut Peekable<Chars>) {
    while let Some(c) = input_chars.peek() {
        if c == &' ' {
            input_chars.next();
        } else {
            break;
        }
    }
}
