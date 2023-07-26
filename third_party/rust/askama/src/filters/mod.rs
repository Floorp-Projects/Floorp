//! Module for built-in filter functions
//!
//! Contains all the built-in filter functions for use in templates.
//! You can define your own filters, as well.
//! For more information, read the [book](https://djc.github.io/askama/filters.html).
#![allow(clippy::trivially_copy_pass_by_ref)]

use std::fmt::{self, Write};

#[cfg(feature = "serde-json")]
mod json;
#[cfg(feature = "serde-json")]
pub use self::json::json;

#[cfg(feature = "serde-yaml")]
mod yaml;
#[cfg(feature = "serde-yaml")]
pub use self::yaml::yaml;

#[allow(unused_imports)]
use crate::error::Error::Fmt;
use askama_escape::{Escaper, MarkupDisplay};
#[cfg(feature = "humansize")]
use dep_humansize::{format_size_i, ToF64, DECIMAL};
#[cfg(feature = "num-traits")]
use dep_num_traits::{cast::NumCast, Signed};
#[cfg(feature = "percent-encoding")]
use percent_encoding::{utf8_percent_encode, AsciiSet, NON_ALPHANUMERIC};

use super::Result;

#[cfg(feature = "percent-encoding")]
// Urlencode char encoding set. Only the characters in the unreserved set don't
// have any special purpose in any part of a URI and can be safely left
// unencoded as specified in https://tools.ietf.org/html/rfc3986.html#section-2.3
const URLENCODE_STRICT_SET: &AsciiSet = &NON_ALPHANUMERIC
    .remove(b'_')
    .remove(b'.')
    .remove(b'-')
    .remove(b'~');

#[cfg(feature = "percent-encoding")]
// Same as URLENCODE_STRICT_SET, but preserves forward slashes for encoding paths
const URLENCODE_SET: &AsciiSet = &URLENCODE_STRICT_SET.remove(b'/');

/// Marks a string (or other `Display` type) as safe
///
/// Use this is you want to allow markup in an expression, or if you know
/// that the expression's contents don't need to be escaped.
///
/// Askama will automatically insert the first (`Escaper`) argument,
/// so this filter only takes a single argument of any type that implements
/// `Display`.
pub fn safe<E, T>(e: E, v: T) -> Result<MarkupDisplay<E, T>>
where
    E: Escaper,
    T: fmt::Display,
{
    Ok(MarkupDisplay::new_safe(v, e))
}

/// Escapes strings according to the escape mode.
///
/// Askama will automatically insert the first (`Escaper`) argument,
/// so this filter only takes a single argument of any type that implements
/// `Display`.
///
/// It is possible to optionally specify an escaper other than the default for
/// the template's extension, like `{{ val|escape("txt") }}`.
pub fn escape<E, T>(e: E, v: T) -> Result<MarkupDisplay<E, T>>
where
    E: Escaper,
    T: fmt::Display,
{
    Ok(MarkupDisplay::new_unsafe(v, e))
}

#[cfg(feature = "humansize")]
/// Returns adequate string representation (in KB, ..) of number of bytes
pub fn filesizeformat(b: &(impl ToF64 + Copy)) -> Result<String> {
    Ok(format_size_i(*b, DECIMAL))
}

#[cfg(feature = "percent-encoding")]
/// Percent-encodes the argument for safe use in URI; does not encode `/`.
///
/// This should be safe for all parts of URI (paths segments, query keys, query
/// values). In the rare case that the server can't deal with forward slashes in
/// the query string, use [`urlencode_strict`], which encodes them as well.
///
/// Encodes all characters except ASCII letters, digits, and `_.-~/`. In other
/// words, encodes all characters which are not in the unreserved set,
/// as specified by [RFC3986](https://tools.ietf.org/html/rfc3986#section-2.3),
/// with the exception of `/`.
///
/// ```none,ignore
/// <a href="/metro{{ "/stations/Château d'Eau"|urlencode }}">Station</a>
/// <a href="/page?text={{ "look, unicode/emojis ✨"|urlencode }}">Page</a>
/// ```
///
/// To encode `/` as well, see [`urlencode_strict`](./fn.urlencode_strict.html).
///
/// [`urlencode_strict`]: ./fn.urlencode_strict.html
pub fn urlencode<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    Ok(utf8_percent_encode(&s, URLENCODE_SET).to_string())
}

#[cfg(feature = "percent-encoding")]
/// Percent-encodes the argument for safe use in URI; encodes `/`.
///
/// Use this filter for encoding query keys and values in the rare case that
/// the server can't process them unencoded.
///
/// Encodes all characters except ASCII letters, digits, and `_.-~`. In other
/// words, encodes all characters which are not in the unreserved set,
/// as specified by [RFC3986](https://tools.ietf.org/html/rfc3986#section-2.3).
///
/// ```none,ignore
/// <a href="/page?text={{ "look, unicode/emojis ✨"|urlencode_strict }}">Page</a>
/// ```
///
/// If you want to preserve `/`, see [`urlencode`](./fn.urlencode.html).
pub fn urlencode_strict<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    Ok(utf8_percent_encode(&s, URLENCODE_STRICT_SET).to_string())
}

/// Formats arguments according to the specified format
///
/// The *second* argument to this filter must be a string literal (as in normal
/// Rust). The two arguments are passed through to the `format!()`
/// [macro](https://doc.rust-lang.org/stable/std/macro.format.html) by
/// the Askama code generator, but the order is swapped to support filter
/// composition.
///
/// ```ignore
/// {{ value | fmt("{:?}") }}
/// ```
///
/// Compare with [format](./fn.format.html).
pub fn fmt() {}

/// Formats arguments according to the specified format
///
/// The first argument to this filter must be a string literal (as in normal
/// Rust). All arguments are passed through to the `format!()`
/// [macro](https://doc.rust-lang.org/stable/std/macro.format.html) by
/// the Askama code generator.
///
/// ```ignore
/// {{ "{:?}{:?}" | format(value, other_value) }}
/// ```
///
/// Compare with [fmt](./fn.fmt.html).
pub fn format() {}

/// Replaces line breaks in plain text with appropriate HTML
///
/// A single newline becomes an HTML line break `<br>` and a new line
/// followed by a blank line becomes a paragraph break `<p>`.
pub fn linebreaks<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    let linebroken = s.replace("\n\n", "</p><p>").replace('\n', "<br/>");

    Ok(format!("<p>{linebroken}</p>"))
}

/// Converts all newlines in a piece of plain text to HTML line breaks
pub fn linebreaksbr<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    Ok(s.replace('\n', "<br/>"))
}

/// Replaces only paragraph breaks in plain text with appropriate HTML
///
/// A new line followed by a blank line becomes a paragraph break `<p>`.
/// Paragraph tags only wrap content; empty paragraphs are removed.
/// No `<br/>` tags are added.
pub fn paragraphbreaks<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    let linebroken = s.replace("\n\n", "</p><p>").replace("<p></p>", "");

    Ok(format!("<p>{linebroken}</p>"))
}

/// Converts to lowercase
pub fn lower<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    Ok(s.to_lowercase())
}

/// Alias for the `lower()` filter
pub fn lowercase<T: fmt::Display>(s: T) -> Result<String> {
    lower(s)
}

/// Converts to uppercase
pub fn upper<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    Ok(s.to_uppercase())
}

/// Alias for the `upper()` filter
pub fn uppercase<T: fmt::Display>(s: T) -> Result<String> {
    upper(s)
}

/// Strip leading and trailing whitespace
pub fn trim<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    Ok(s.trim().to_owned())
}

/// Limit string length, appends '...' if truncated
pub fn truncate<T: fmt::Display>(s: T, len: usize) -> Result<String> {
    let mut s = s.to_string();
    if s.len() > len {
        let mut real_len = len;
        while !s.is_char_boundary(real_len) {
            real_len += 1;
        }
        s.truncate(real_len);
        s.push_str("...");
    }
    Ok(s)
}

/// Indent lines with `width` spaces
pub fn indent<T: fmt::Display>(s: T, width: usize) -> Result<String> {
    let s = s.to_string();

    let mut indented = String::new();

    for (i, c) in s.char_indices() {
        indented.push(c);

        if c == '\n' && i < s.len() - 1 {
            for _ in 0..width {
                indented.push(' ');
            }
        }
    }

    Ok(indented)
}

#[cfg(feature = "num-traits")]
/// Casts number to f64
pub fn into_f64<T>(number: T) -> Result<f64>
where
    T: NumCast,
{
    number.to_f64().ok_or(Fmt(fmt::Error))
}

#[cfg(feature = "num-traits")]
/// Casts number to isize
pub fn into_isize<T>(number: T) -> Result<isize>
where
    T: NumCast,
{
    number.to_isize().ok_or(Fmt(fmt::Error))
}

/// Joins iterable into a string separated by provided argument
pub fn join<T, I, S>(input: I, separator: S) -> Result<String>
where
    T: fmt::Display,
    I: Iterator<Item = T>,
    S: AsRef<str>,
{
    let separator: &str = separator.as_ref();

    let mut rv = String::new();

    for (num, item) in input.enumerate() {
        if num > 0 {
            rv.push_str(separator);
        }

        write!(rv, "{item}")?;
    }

    Ok(rv)
}

#[cfg(feature = "num-traits")]
/// Absolute value
pub fn abs<T>(number: T) -> Result<T>
where
    T: Signed,
{
    Ok(number.abs())
}

/// Capitalize a value. The first character will be uppercase, all others lowercase.
pub fn capitalize<T: fmt::Display>(s: T) -> Result<String> {
    let s = s.to_string();
    match s.chars().next() {
        Some(c) => {
            let mut replacement: String = c.to_uppercase().collect();
            replacement.push_str(&s[c.len_utf8()..].to_lowercase());
            Ok(replacement)
        }
        _ => Ok(s),
    }
}

/// Centers the value in a field of a given width
pub fn center(src: &dyn fmt::Display, dst_len: usize) -> Result<String> {
    let src = src.to_string();
    let len = src.len();

    if dst_len <= len {
        Ok(src)
    } else {
        let diff = dst_len - len;
        let mid = diff / 2;
        let r = diff % 2;
        let mut buf = String::with_capacity(dst_len);

        for _ in 0..mid {
            buf.push(' ');
        }

        buf.push_str(&src);

        for _ in 0..mid + r {
            buf.push(' ');
        }

        Ok(buf)
    }
}

/// Count the words in that string
pub fn wordcount<T: fmt::Display>(s: T) -> Result<usize> {
    let s = s.to_string();

    Ok(s.split_whitespace().count())
}

#[cfg(feature = "markdown")]
pub fn markdown<E, S>(
    e: E,
    s: S,
    options: Option<&comrak::ComrakOptions>,
) -> Result<MarkupDisplay<E, String>>
where
    E: Escaper,
    S: AsRef<str>,
{
    use comrak::{
        markdown_to_html, ComrakExtensionOptions, ComrakOptions, ComrakParseOptions,
        ComrakRenderOptions, ListStyleType,
    };

    const DEFAULT_OPTIONS: ComrakOptions = ComrakOptions {
        extension: ComrakExtensionOptions {
            strikethrough: true,
            tagfilter: true,
            table: true,
            autolink: true,
            // default:
            tasklist: false,
            superscript: false,
            header_ids: None,
            footnotes: false,
            description_lists: false,
            front_matter_delimiter: None,
        },
        parse: ComrakParseOptions {
            // default:
            smart: false,
            default_info_string: None,
            relaxed_tasklist_matching: false,
        },
        render: ComrakRenderOptions {
            unsafe_: false,
            escape: true,
            // default:
            hardbreaks: false,
            github_pre_lang: false,
            width: 0,
            list_style: ListStyleType::Dash,
        },
    };

    let s = markdown_to_html(s.as_ref(), options.unwrap_or(&DEFAULT_OPTIONS));
    Ok(MarkupDisplay::new_safe(s, e))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[cfg(feature = "num-traits")]
    use std::f64::INFINITY;

    #[cfg(feature = "humansize")]
    #[test]
    fn test_filesizeformat() {
        assert_eq!(filesizeformat(&0).unwrap(), "0 B");
        assert_eq!(filesizeformat(&999u64).unwrap(), "999 B");
        assert_eq!(filesizeformat(&1000i32).unwrap(), "1 kB");
        assert_eq!(filesizeformat(&1023).unwrap(), "1.02 kB");
        assert_eq!(filesizeformat(&1024usize).unwrap(), "1.02 kB");
    }

    #[cfg(feature = "percent-encoding")]
    #[test]
    fn test_urlencoding() {
        // Unreserved (https://tools.ietf.org/html/rfc3986.html#section-2.3)
        // alpha / digit
        assert_eq!(urlencode("AZaz09").unwrap(), "AZaz09");
        assert_eq!(urlencode_strict("AZaz09").unwrap(), "AZaz09");
        // other
        assert_eq!(urlencode("_.-~").unwrap(), "_.-~");
        assert_eq!(urlencode_strict("_.-~").unwrap(), "_.-~");

        // Reserved (https://tools.ietf.org/html/rfc3986.html#section-2.2)
        // gen-delims
        assert_eq!(urlencode(":/?#[]@").unwrap(), "%3A/%3F%23%5B%5D%40");
        assert_eq!(
            urlencode_strict(":/?#[]@").unwrap(),
            "%3A%2F%3F%23%5B%5D%40"
        );
        // sub-delims
        assert_eq!(
            urlencode("!$&'()*+,;=").unwrap(),
            "%21%24%26%27%28%29%2A%2B%2C%3B%3D"
        );
        assert_eq!(
            urlencode_strict("!$&'()*+,;=").unwrap(),
            "%21%24%26%27%28%29%2A%2B%2C%3B%3D"
        );

        // Other
        assert_eq!(
            urlencode("žŠďŤňĚáÉóŮ").unwrap(),
            "%C5%BE%C5%A0%C4%8F%C5%A4%C5%88%C4%9A%C3%A1%C3%89%C3%B3%C5%AE"
        );
        assert_eq!(
            urlencode_strict("žŠďŤňĚáÉóŮ").unwrap(),
            "%C5%BE%C5%A0%C4%8F%C5%A4%C5%88%C4%9A%C3%A1%C3%89%C3%B3%C5%AE"
        );

        // Ferris
        assert_eq!(urlencode("🦀").unwrap(), "%F0%9F%A6%80");
        assert_eq!(urlencode_strict("🦀").unwrap(), "%F0%9F%A6%80");
    }

    #[test]
    fn test_linebreaks() {
        assert_eq!(
            linebreaks("Foo\nBar Baz").unwrap(),
            "<p>Foo<br/>Bar Baz</p>"
        );
        assert_eq!(
            linebreaks("Foo\nBar\n\nBaz").unwrap(),
            "<p>Foo<br/>Bar</p><p>Baz</p>"
        );
    }

    #[test]
    fn test_linebreaksbr() {
        assert_eq!(linebreaksbr("Foo\nBar").unwrap(), "Foo<br/>Bar");
        assert_eq!(
            linebreaksbr("Foo\nBar\n\nBaz").unwrap(),
            "Foo<br/>Bar<br/><br/>Baz"
        );
    }

    #[test]
    fn test_paragraphbreaks() {
        assert_eq!(
            paragraphbreaks("Foo\nBar Baz").unwrap(),
            "<p>Foo\nBar Baz</p>"
        );
        assert_eq!(
            paragraphbreaks("Foo\nBar\n\nBaz").unwrap(),
            "<p>Foo\nBar</p><p>Baz</p>"
        );
        assert_eq!(
            paragraphbreaks("Foo\n\n\n\n\nBar\n\nBaz").unwrap(),
            "<p>Foo</p><p>\nBar</p><p>Baz</p>"
        );
    }

    #[test]
    fn test_lower() {
        assert_eq!(lower("Foo").unwrap(), "foo");
        assert_eq!(lower("FOO").unwrap(), "foo");
        assert_eq!(lower("FooBar").unwrap(), "foobar");
        assert_eq!(lower("foo").unwrap(), "foo");
    }

    #[test]
    fn test_upper() {
        assert_eq!(upper("Foo").unwrap(), "FOO");
        assert_eq!(upper("FOO").unwrap(), "FOO");
        assert_eq!(upper("FooBar").unwrap(), "FOOBAR");
        assert_eq!(upper("foo").unwrap(), "FOO");
    }

    #[test]
    fn test_trim() {
        assert_eq!(trim(" Hello\tworld\t").unwrap(), "Hello\tworld");
    }

    #[test]
    fn test_truncate() {
        assert_eq!(truncate("hello", 2).unwrap(), "he...");
        let a = String::from("您好");
        assert_eq!(a.len(), 6);
        assert_eq!(String::from("您").len(), 3);
        assert_eq!(truncate("您好", 1).unwrap(), "您...");
        assert_eq!(truncate("您好", 2).unwrap(), "您...");
        assert_eq!(truncate("您好", 3).unwrap(), "您...");
        assert_eq!(truncate("您好", 4).unwrap(), "您好...");
        assert_eq!(truncate("您好", 6).unwrap(), "您好");
        assert_eq!(truncate("您好", 7).unwrap(), "您好");
        let s = String::from("🤚a🤚");
        assert_eq!(s.len(), 9);
        assert_eq!(String::from("🤚").len(), 4);
        assert_eq!(truncate("🤚a🤚", 1).unwrap(), "🤚...");
        assert_eq!(truncate("🤚a🤚", 2).unwrap(), "🤚...");
        assert_eq!(truncate("🤚a🤚", 3).unwrap(), "🤚...");
        assert_eq!(truncate("🤚a🤚", 4).unwrap(), "🤚...");
        assert_eq!(truncate("🤚a🤚", 5).unwrap(), "🤚a...");
        assert_eq!(truncate("🤚a🤚", 6).unwrap(), "🤚a🤚...");
        assert_eq!(truncate("🤚a🤚", 9).unwrap(), "🤚a🤚");
        assert_eq!(truncate("🤚a🤚", 10).unwrap(), "🤚a🤚");
    }

    #[test]
    fn test_indent() {
        assert_eq!(indent("hello", 2).unwrap(), "hello");
        assert_eq!(indent("hello\n", 2).unwrap(), "hello\n");
        assert_eq!(indent("hello\nfoo", 2).unwrap(), "hello\n  foo");
        assert_eq!(
            indent("hello\nfoo\n bar", 4).unwrap(),
            "hello\n    foo\n     bar"
        );
    }

    #[cfg(feature = "num-traits")]
    #[test]
    #[allow(clippy::float_cmp)]
    fn test_into_f64() {
        assert_eq!(into_f64(1).unwrap(), 1.0_f64);
        assert_eq!(into_f64(1.9).unwrap(), 1.9_f64);
        assert_eq!(into_f64(-1.9).unwrap(), -1.9_f64);
        assert_eq!(into_f64(INFINITY as f32).unwrap(), INFINITY);
        assert_eq!(into_f64(-INFINITY as f32).unwrap(), -INFINITY);
    }

    #[cfg(feature = "num-traits")]
    #[test]
    fn test_into_isize() {
        assert_eq!(into_isize(1).unwrap(), 1_isize);
        assert_eq!(into_isize(1.9).unwrap(), 1_isize);
        assert_eq!(into_isize(-1.9).unwrap(), -1_isize);
        assert_eq!(into_isize(1.5_f64).unwrap(), 1_isize);
        assert_eq!(into_isize(-1.5_f64).unwrap(), -1_isize);
        match into_isize(INFINITY) {
            Err(Fmt(fmt::Error)) => {}
            _ => panic!("Should return error of type Err(Fmt(fmt::Error))"),
        };
    }

    #[allow(clippy::needless_borrow)]
    #[test]
    fn test_join() {
        assert_eq!(
            join((&["hello", "world"]).iter(), ", ").unwrap(),
            "hello, world"
        );
        assert_eq!(join((&["hello"]).iter(), ", ").unwrap(), "hello");

        let empty: &[&str] = &[];
        assert_eq!(join(empty.iter(), ", ").unwrap(), "");

        let input: Vec<String> = vec!["foo".into(), "bar".into(), "bazz".into()];
        assert_eq!(join(input.iter(), ":").unwrap(), "foo:bar:bazz");

        let input: &[String] = &["foo".into(), "bar".into()];
        assert_eq!(join(input.iter(), ":").unwrap(), "foo:bar");

        let real: String = "blah".into();
        let input: Vec<&str> = vec![&real];
        assert_eq!(join(input.iter(), ";").unwrap(), "blah");

        assert_eq!(
            join((&&&&&["foo", "bar"]).iter(), ", ").unwrap(),
            "foo, bar"
        );
    }

    #[cfg(feature = "num-traits")]
    #[test]
    #[allow(clippy::float_cmp)]
    fn test_abs() {
        assert_eq!(abs(1).unwrap(), 1);
        assert_eq!(abs(-1).unwrap(), 1);
        assert_eq!(abs(1.0).unwrap(), 1.0);
        assert_eq!(abs(-1.0).unwrap(), 1.0);
        assert_eq!(abs(1.0_f64).unwrap(), 1.0_f64);
        assert_eq!(abs(-1.0_f64).unwrap(), 1.0_f64);
    }

    #[test]
    fn test_capitalize() {
        assert_eq!(capitalize("foo").unwrap(), "Foo".to_string());
        assert_eq!(capitalize("f").unwrap(), "F".to_string());
        assert_eq!(capitalize("fO").unwrap(), "Fo".to_string());
        assert_eq!(capitalize("").unwrap(), "".to_string());
        assert_eq!(capitalize("FoO").unwrap(), "Foo".to_string());
        assert_eq!(capitalize("foO BAR").unwrap(), "Foo bar".to_string());
        assert_eq!(capitalize("äØÄÅÖ").unwrap(), "Äøäåö".to_string());
        assert_eq!(capitalize("ß").unwrap(), "SS".to_string());
        assert_eq!(capitalize("ßß").unwrap(), "SSß".to_string());
    }

    #[test]
    fn test_center() {
        assert_eq!(center(&"f", 3).unwrap(), " f ".to_string());
        assert_eq!(center(&"f", 4).unwrap(), " f  ".to_string());
        assert_eq!(center(&"foo", 1).unwrap(), "foo".to_string());
        assert_eq!(center(&"foo bar", 8).unwrap(), "foo bar ".to_string());
    }

    #[test]
    fn test_wordcount() {
        assert_eq!(wordcount("").unwrap(), 0);
        assert_eq!(wordcount(" \n\t").unwrap(), 0);
        assert_eq!(wordcount("foo").unwrap(), 1);
        assert_eq!(wordcount("foo bar").unwrap(), 2);
    }
}
