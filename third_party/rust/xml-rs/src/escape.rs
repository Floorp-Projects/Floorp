//! Contains functions for performing XML special characters escaping.

use std::borrow::Cow;

use self::Value::{C, S};
use self::Process::{B, O};

enum Value {
    C(char),
    S(&'static str)
}

impl Value {
    fn dispatch_for_attribute(c: char) -> Value {
        match c {
            '<'  => S("&lt;"),
            '>'  => S("&gt;"),
            '"'  => S("&quot;"),
            '\'' => S("&apos;"),
            '&'  => S("&amp;"),
            _    => C(c)
        }
    }

    fn dispatch_for_pcdata(c: char) -> Value {
        match c {
            '<'  => S("&lt;"),
            '&'  => S("&amp;"),
            _    => C(c)
        }
    }
}

enum Process<'a> {
    B(&'a str),
    O(String)
}

impl<'a> Process<'a> {
    fn process(&mut self, (i, next): (usize, Value)) {
        match next {
            S(s) => if let O(ref mut o) = *self {
                o.push_str(s);
            } else if let B(b) = *self {
                let mut r = String::with_capacity(b.len() + s.len());
                r.push_str(&b[..i]);
                r.push_str(s);
                *self = O(r);
            },
            C(c) => match *self {
                B(_) => {}
                O(ref mut o) => o.push(c)
            }
        }
    }

    fn into_result(self) -> Cow<'a, str> {
        match self {
            B(b) => Cow::Borrowed(b),
            O(o) => Cow::Owned(o)
        }
    }
}

impl<'a> Extend<Value> for Process<'a> {
    fn extend<I: IntoIterator<Item=Value>>(&mut self, it: I) {
        for v in it.into_iter().enumerate() {
            self.process(v);
        }
    }
}

fn escape_str(s: &str, dispatch: fn(char) -> Value) -> Cow<str> {
    let mut p = B(s);
    p.extend(s.chars().map(dispatch));
    p.into_result()
}

/// Performs escaping of common XML characters inside an attribute value.
///
/// This function replaces several important markup characters with their
/// entity equivalents:
///
/// * `<` → `&lt;`
/// * `>` → `&gt;`
/// * `"` → `&quot;`
/// * `'` → `&apos;`
/// * `&` → `&amp;`
///
/// The resulting string is safe to use inside XML attribute values or in PCDATA sections.
///
/// Does not perform allocations if the given string does not contain escapable characters.
#[inline]
pub fn escape_str_attribute(s: &str) -> Cow<str> {
    escape_str(s, Value::dispatch_for_attribute)
}

/// Performs escaping of common XML characters inside PCDATA.
///
/// This function replaces several important markup characters with their
/// entity equivalents:
///
/// * `<` → `&lt;`
/// * `&` → `&amp;`
///
/// The resulting string is safe to use inside PCDATA sections but NOT inside attribute values.
///
/// Does not perform allocations if the given string does not contain escapable characters.
#[inline]
pub fn escape_str_pcdata(s: &str) -> Cow<str> {
    escape_str(s, Value::dispatch_for_pcdata)
}
