// Copyright 2013-2016 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::error::Error;
use std::fmt::{self, Formatter, Write};
use std::str;

use host::{Host, HostInternal};
use percent_encoding::{percent_encode, utf8_percent_encode, AsciiSet, CONTROLS};
use query_encoding::EncodingOverride;
use Url;

/// https://url.spec.whatwg.org/#fragment-percent-encode-set
const FRAGMENT: &AsciiSet = &CONTROLS.add(b' ').add(b'"').add(b'<').add(b'>').add(b'`');

/// https://url.spec.whatwg.org/#path-percent-encode-set
const PATH: &AsciiSet = &FRAGMENT.add(b'#').add(b'?').add(b'{').add(b'}');

/// https://url.spec.whatwg.org/#userinfo-percent-encode-set
pub(crate) const USERINFO: &AsciiSet = &PATH
    .add(b'/')
    .add(b':')
    .add(b';')
    .add(b'=')
    .add(b'@')
    .add(b'[')
    .add(b'\\')
    .add(b']')
    .add(b'^')
    .add(b'|');

pub(crate) const PATH_SEGMENT: &AsciiSet = &PATH.add(b'/').add(b'%');

// The backslash (\) character is treated as a path separator in special URLs
// so it needs to be additionally escaped in that case.
pub(crate) const SPECIAL_PATH_SEGMENT: &AsciiSet = &PATH_SEGMENT.add(b'\\');

// https://url.spec.whatwg.org/#query-state
const QUERY: &AsciiSet = &CONTROLS.add(b' ').add(b'"').add(b'#').add(b'<').add(b'>');
const SPECIAL_QUERY: &AsciiSet = &QUERY.add(b'\'');

pub type ParseResult<T> = Result<T, ParseError>;

macro_rules! simple_enum_error {
    ($($name: ident => $description: expr,)+) => {
        /// Errors that can occur during parsing.
        ///
        /// This may be extended in the future so exhaustive matching is
        /// discouraged with an unused variant.
        #[derive(PartialEq, Eq, Clone, Copy, Debug)]
        pub enum ParseError {
            $(
                $name,
            )+
            /// Unused variant enable non-exhaustive matching
            #[doc(hidden)]
            __FutureProof,
        }

        impl Error for ParseError {
            fn description(&self) -> &str {
                match *self {
                    $(
                        ParseError::$name => $description,
                    )+
                    ParseError::__FutureProof => {
                        unreachable!("Don't abuse the FutureProof!");
                    }
                }
            }
        }
    }
}

simple_enum_error! {
    EmptyHost => "empty host",
    IdnaError => "invalid international domain name",
    InvalidPort => "invalid port number",
    InvalidIpv4Address => "invalid IPv4 address",
    InvalidIpv6Address => "invalid IPv6 address",
    InvalidDomainCharacter => "invalid domain character",
    RelativeUrlWithoutBase => "relative URL without a base",
    RelativeUrlWithCannotBeABaseBase => "relative URL with a cannot-be-a-base base",
    SetHostOnCannotBeABaseUrl => "a cannot-be-a-base URL doesn’t have a host to set",
    Overflow => "URLs more than 4 GB are not supported",
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        fmt::Display::fmt(self.description(), f)
    }
}

impl From<::idna::Errors> for ParseError {
    fn from(_: ::idna::Errors) -> ParseError {
        ParseError::IdnaError
    }
}

macro_rules! syntax_violation_enum {
    ($($name: ident => $description: expr,)+) => {
        /// Non-fatal syntax violations that can occur during parsing.
        ///
        /// This may be extended in the future so exhaustive matching is
        /// discouraged with an unused variant.
        #[derive(PartialEq, Eq, Clone, Copy, Debug)]
        pub enum SyntaxViolation {
            $(
                $name,
            )+
            /// Unused variant enable non-exhaustive matching
            #[doc(hidden)]
            __FutureProof,
        }

        impl SyntaxViolation {
            pub fn description(&self) -> &'static str {
                match *self {
                    $(
                        SyntaxViolation::$name => $description,
                    )+
                    SyntaxViolation::__FutureProof => {
                        unreachable!("Don't abuse the FutureProof!");
                    }
                }
            }
        }
    }
}

syntax_violation_enum! {
    Backslash => "backslash",
    C0SpaceIgnored =>
        "leading or trailing control or space character are ignored in URLs",
    EmbeddedCredentials =>
        "embedding authentication information (username or password) \
         in an URL is not recommended",
    ExpectedDoubleSlash => "expected //",
    ExpectedFileDoubleSlash => "expected // after file:",
    FileWithHostAndWindowsDrive => "file: with host and Windows drive letter",
    NonUrlCodePoint => "non-URL code point",
    NullInFragment => "NULL characters are ignored in URL fragment identifiers",
    PercentDecode => "expected 2 hex digits after %",
    TabOrNewlineIgnored => "tabs or newlines are ignored in URLs",
    UnencodedAtSign => "unencoded @ sign in username or password",
}

impl fmt::Display for SyntaxViolation {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        fmt::Display::fmt(self.description(), f)
    }
}

#[derive(Copy, Clone)]
pub enum SchemeType {
    File,
    SpecialNotFile,
    NotSpecial,
}

impl SchemeType {
    pub fn is_special(&self) -> bool {
        !matches!(*self, SchemeType::NotSpecial)
    }

    pub fn is_file(&self) -> bool {
        matches!(*self, SchemeType::File)
    }

    pub fn from(s: &str) -> Self {
        match s {
            "http" | "https" | "ws" | "wss" | "ftp" | "gopher" => SchemeType::SpecialNotFile,
            "file" => SchemeType::File,
            _ => SchemeType::NotSpecial,
        }
    }
}

pub fn default_port(scheme: &str) -> Option<u16> {
    match scheme {
        "http" | "ws" => Some(80),
        "https" | "wss" => Some(443),
        "ftp" => Some(21),
        "gopher" => Some(70),
        _ => None,
    }
}

#[derive(Clone)]
pub struct Input<'i> {
    chars: str::Chars<'i>,
}

impl<'i> Input<'i> {
    pub fn new(input: &'i str) -> Self {
        Input::with_log(input, None)
    }

    pub fn with_log(original_input: &'i str, vfn: Option<&dyn Fn(SyntaxViolation)>) -> Self {
        let input = original_input.trim_matches(c0_control_or_space);
        if let Some(vfn) = vfn {
            if input.len() < original_input.len() {
                vfn(SyntaxViolation::C0SpaceIgnored)
            }
            if input.chars().any(|c| matches!(c, '\t' | '\n' | '\r')) {
                vfn(SyntaxViolation::TabOrNewlineIgnored)
            }
        }
        Input {
            chars: input.chars(),
        }
    }

    #[inline]
    pub fn is_empty(&self) -> bool {
        self.clone().next().is_none()
    }

    #[inline]
    fn starts_with<P: Pattern>(&self, p: P) -> bool {
        p.split_prefix(&mut self.clone())
    }

    #[inline]
    pub fn split_prefix<P: Pattern>(&self, p: P) -> Option<Self> {
        let mut remaining = self.clone();
        if p.split_prefix(&mut remaining) {
            Some(remaining)
        } else {
            None
        }
    }

    #[inline]
    fn split_first(&self) -> (Option<char>, Self) {
        let mut remaining = self.clone();
        (remaining.next(), remaining)
    }

    #[inline]
    fn count_matching<F: Fn(char) -> bool>(&self, f: F) -> (u32, Self) {
        let mut count = 0;
        let mut remaining = self.clone();
        loop {
            let mut input = remaining.clone();
            if matches!(input.next(), Some(c) if f(c)) {
                remaining = input;
                count += 1;
            } else {
                return (count, remaining);
            }
        }
    }

    #[inline]
    fn next_utf8(&mut self) -> Option<(char, &'i str)> {
        loop {
            let utf8 = self.chars.as_str();
            match self.chars.next() {
                Some(c) => {
                    if !matches!(c, '\t' | '\n' | '\r') {
                        return Some((c, &utf8[..c.len_utf8()]));
                    }
                }
                None => return None,
            }
        }
    }
}

pub trait Pattern {
    fn split_prefix<'i>(self, input: &mut Input<'i>) -> bool;
}

impl Pattern for char {
    fn split_prefix<'i>(self, input: &mut Input<'i>) -> bool {
        input.next() == Some(self)
    }
}

impl<'a> Pattern for &'a str {
    fn split_prefix<'i>(self, input: &mut Input<'i>) -> bool {
        for c in self.chars() {
            if input.next() != Some(c) {
                return false;
            }
        }
        true
    }
}

impl<F: FnMut(char) -> bool> Pattern for F {
    fn split_prefix<'i>(self, input: &mut Input<'i>) -> bool {
        input.next().map_or(false, self)
    }
}

impl<'i> Iterator for Input<'i> {
    type Item = char;
    fn next(&mut self) -> Option<char> {
        self.chars
            .by_ref()
            .find(|&c| !matches!(c, '\t' | '\n' | '\r'))
    }
}

pub struct Parser<'a> {
    pub serialization: String,
    pub base_url: Option<&'a Url>,
    pub query_encoding_override: EncodingOverride<'a>,
    pub violation_fn: Option<&'a dyn Fn(SyntaxViolation)>,
    pub context: Context,
}

#[derive(PartialEq, Eq, Copy, Clone)]
pub enum Context {
    UrlParser,
    Setter,
    PathSegmentSetter,
}

impl<'a> Parser<'a> {
    fn log_violation(&self, v: SyntaxViolation) {
        if let Some(f) = self.violation_fn {
            f(v)
        }
    }

    fn log_violation_if(&self, v: SyntaxViolation, test: impl FnOnce() -> bool) {
        if let Some(f) = self.violation_fn {
            if test() {
                f(v)
            }
        }
    }

    pub fn for_setter(serialization: String) -> Parser<'a> {
        Parser {
            serialization,
            base_url: None,
            query_encoding_override: None,
            violation_fn: None,
            context: Context::Setter,
        }
    }

    /// https://url.spec.whatwg.org/#concept-basic-url-parser
    pub fn parse_url(mut self, input: &str) -> ParseResult<Url> {
        let input = Input::with_log(input, self.violation_fn);
        if let Ok(remaining) = self.parse_scheme(input.clone()) {
            return self.parse_with_scheme(remaining);
        }

        // No-scheme state
        if let Some(base_url) = self.base_url {
            if input.starts_with('#') {
                self.fragment_only(base_url, input)
            } else if base_url.cannot_be_a_base() {
                Err(ParseError::RelativeUrlWithCannotBeABaseBase)
            } else {
                let scheme_type = SchemeType::from(base_url.scheme());
                if scheme_type.is_file() {
                    self.parse_file(input, scheme_type, Some(base_url))
                } else {
                    self.parse_relative(input, scheme_type, base_url)
                }
            }
        } else {
            Err(ParseError::RelativeUrlWithoutBase)
        }
    }

    pub fn parse_scheme<'i>(&mut self, mut input: Input<'i>) -> Result<Input<'i>, ()> {
        if input.is_empty() || !input.starts_with(ascii_alpha) {
            return Err(());
        }
        debug_assert!(self.serialization.is_empty());
        while let Some(c) = input.next() {
            match c {
                'a'..='z' | 'A'..='Z' | '0'..='9' | '+' | '-' | '.' => {
                    self.serialization.push(c.to_ascii_lowercase())
                }
                ':' => return Ok(input),
                _ => {
                    self.serialization.clear();
                    return Err(());
                }
            }
        }
        // EOF before ':'
        if self.context == Context::Setter {
            Ok(input)
        } else {
            self.serialization.clear();
            Err(())
        }
    }

    fn parse_with_scheme(mut self, input: Input) -> ParseResult<Url> {
        use SyntaxViolation::{ExpectedDoubleSlash, ExpectedFileDoubleSlash};
        let scheme_end = to_u32(self.serialization.len())?;
        let scheme_type = SchemeType::from(&self.serialization);
        self.serialization.push(':');
        match scheme_type {
            SchemeType::File => {
                self.log_violation_if(ExpectedFileDoubleSlash, || !input.starts_with("//"));
                let base_file_url = self.base_url.and_then(|base| {
                    if base.scheme() == "file" {
                        Some(base)
                    } else {
                        None
                    }
                });
                self.serialization.clear();
                self.parse_file(input, scheme_type, base_file_url)
            }
            SchemeType::SpecialNotFile => {
                // special relative or authority state
                let (slashes_count, remaining) = input.count_matching(|c| matches!(c, '/' | '\\'));
                if let Some(base_url) = self.base_url {
                    if slashes_count < 2
                        && base_url.scheme() == &self.serialization[..scheme_end as usize]
                    {
                        // "Cannot-be-a-base" URLs only happen with "not special" schemes.
                        debug_assert!(!base_url.cannot_be_a_base());
                        self.serialization.clear();
                        return self.parse_relative(input, scheme_type, base_url);
                    }
                }
                // special authority slashes state
                self.log_violation_if(ExpectedDoubleSlash, || {
                    input
                        .clone()
                        .take_while(|&c| matches!(c, '/' | '\\'))
                        .collect::<String>()
                        != "//"
                });
                self.after_double_slash(remaining, scheme_type, scheme_end)
            }
            SchemeType::NotSpecial => self.parse_non_special(input, scheme_type, scheme_end),
        }
    }

    /// Scheme other than file, http, https, ws, ws, ftp, gopher.
    fn parse_non_special(
        mut self,
        input: Input,
        scheme_type: SchemeType,
        scheme_end: u32,
    ) -> ParseResult<Url> {
        // path or authority state (
        if let Some(input) = input.split_prefix("//") {
            return self.after_double_slash(input, scheme_type, scheme_end);
        }
        // Anarchist URL (no authority)
        let path_start = to_u32(self.serialization.len())?;
        let username_end = path_start;
        let host_start = path_start;
        let host_end = path_start;
        let host = HostInternal::None;
        let port = None;
        let remaining = if let Some(input) = input.split_prefix('/') {
            let path_start = self.serialization.len();
            self.serialization.push('/');
            self.parse_path(scheme_type, &mut false, path_start, input)
        } else {
            self.parse_cannot_be_a_base_path(input)
        };
        self.with_query_and_fragment(
            scheme_type,
            scheme_end,
            username_end,
            host_start,
            host_end,
            host,
            port,
            path_start,
            remaining,
        )
    }

    fn parse_file(
        mut self,
        input: Input,
        scheme_type: SchemeType,
        mut base_file_url: Option<&Url>,
    ) -> ParseResult<Url> {
        use SyntaxViolation::Backslash;
        // file state
        debug_assert!(self.serialization.is_empty());
        let (first_char, input_after_first_char) = input.split_first();
        match first_char {
            None => {
                if let Some(base_url) = base_file_url {
                    // Copy everything except the fragment
                    let before_fragment = match base_url.fragment_start {
                        Some(i) => &base_url.serialization[..i as usize],
                        None => &*base_url.serialization,
                    };
                    self.serialization.push_str(before_fragment);
                    Ok(Url {
                        serialization: self.serialization,
                        fragment_start: None,
                        ..*base_url
                    })
                } else {
                    self.serialization.push_str("file:///");
                    let scheme_end = "file".len() as u32;
                    let path_start = "file://".len() as u32;
                    Ok(Url {
                        serialization: self.serialization,
                        scheme_end,
                        username_end: path_start,
                        host_start: path_start,
                        host_end: path_start,
                        host: HostInternal::None,
                        port: None,
                        path_start,
                        query_start: None,
                        fragment_start: None,
                    })
                }
            }
            Some('?') => {
                if let Some(base_url) = base_file_url {
                    // Copy everything up to the query string
                    let before_query = match (base_url.query_start, base_url.fragment_start) {
                        (None, None) => &*base_url.serialization,
                        (Some(i), _) | (None, Some(i)) => base_url.slice(..i),
                    };
                    self.serialization.push_str(before_query);
                    let (query_start, fragment_start) =
                        self.parse_query_and_fragment(scheme_type, base_url.scheme_end, input)?;
                    Ok(Url {
                        serialization: self.serialization,
                        query_start,
                        fragment_start,
                        ..*base_url
                    })
                } else {
                    self.serialization.push_str("file:///");
                    let scheme_end = "file".len() as u32;
                    let path_start = "file://".len() as u32;
                    let (query_start, fragment_start) =
                        self.parse_query_and_fragment(scheme_type, scheme_end, input)?;
                    Ok(Url {
                        serialization: self.serialization,
                        scheme_end,
                        username_end: path_start,
                        host_start: path_start,
                        host_end: path_start,
                        host: HostInternal::None,
                        port: None,
                        path_start,
                        query_start,
                        fragment_start,
                    })
                }
            }
            Some('#') => {
                if let Some(base_url) = base_file_url {
                    self.fragment_only(base_url, input)
                } else {
                    self.serialization.push_str("file:///");
                    let scheme_end = "file".len() as u32;
                    let path_start = "file://".len() as u32;
                    let fragment_start = "file:///".len() as u32;
                    self.serialization.push('#');
                    self.parse_fragment(input_after_first_char);
                    Ok(Url {
                        serialization: self.serialization,
                        scheme_end,
                        username_end: path_start,
                        host_start: path_start,
                        host_end: path_start,
                        host: HostInternal::None,
                        port: None,
                        path_start,
                        query_start: None,
                        fragment_start: Some(fragment_start),
                    })
                }
            }
            Some('/') | Some('\\') => {
                self.log_violation_if(Backslash, || first_char == Some('\\'));
                // file slash state
                let (next_char, input_after_next_char) = input_after_first_char.split_first();
                self.log_violation_if(Backslash, || next_char == Some('\\'));
                if matches!(next_char, Some('/') | Some('\\')) {
                    // file host state
                    self.serialization.push_str("file://");
                    let scheme_end = "file".len() as u32;
                    let host_start = "file://".len() as u32;
                    let (path_start, mut host, remaining) =
                        self.parse_file_host(input_after_next_char)?;
                    let mut host_end = to_u32(self.serialization.len())?;
                    let mut has_host = !matches!(host, HostInternal::None);
                    let remaining = if path_start {
                        self.parse_path_start(SchemeType::File, &mut has_host, remaining)
                    } else {
                        let path_start = self.serialization.len();
                        self.serialization.push('/');
                        self.parse_path(SchemeType::File, &mut has_host, path_start, remaining)
                    };
                    // For file URLs that have a host and whose path starts
                    // with the windows drive letter we just remove the host.
                    if !has_host {
                        self.serialization
                            .drain(host_start as usize..host_end as usize);
                        host_end = host_start;
                        host = HostInternal::None;
                    }
                    let (query_start, fragment_start) =
                        self.parse_query_and_fragment(scheme_type, scheme_end, remaining)?;
                    Ok(Url {
                        serialization: self.serialization,
                        scheme_end,
                        username_end: host_start,
                        host_start,
                        host_end,
                        host,
                        port: None,
                        path_start: host_end,
                        query_start,
                        fragment_start,
                    })
                } else {
                    self.serialization.push_str("file:///");
                    let scheme_end = "file".len() as u32;
                    let path_start = "file://".len();
                    if let Some(base_url) = base_file_url {
                        let first_segment = base_url.path_segments().unwrap().next().unwrap();
                        // FIXME: *normalized* drive letter
                        if is_windows_drive_letter(first_segment) {
                            self.serialization.push_str(first_segment);
                            self.serialization.push('/');
                        }
                    }
                    let remaining = self.parse_path(
                        SchemeType::File,
                        &mut false,
                        path_start,
                        input_after_first_char,
                    );
                    let (query_start, fragment_start) =
                        self.parse_query_and_fragment(scheme_type, scheme_end, remaining)?;
                    let path_start = path_start as u32;
                    Ok(Url {
                        serialization: self.serialization,
                        scheme_end,
                        username_end: path_start,
                        host_start: path_start,
                        host_end: path_start,
                        host: HostInternal::None,
                        port: None,
                        path_start,
                        query_start,
                        fragment_start,
                    })
                }
            }
            _ => {
                if starts_with_windows_drive_letter_segment(&input) {
                    base_file_url = None;
                }
                if let Some(base_url) = base_file_url {
                    let before_query = match (base_url.query_start, base_url.fragment_start) {
                        (None, None) => &*base_url.serialization,
                        (Some(i), _) | (None, Some(i)) => base_url.slice(..i),
                    };
                    self.serialization.push_str(before_query);
                    self.pop_path(SchemeType::File, base_url.path_start as usize);
                    let remaining = self.parse_path(
                        SchemeType::File,
                        &mut true,
                        base_url.path_start as usize,
                        input,
                    );
                    self.with_query_and_fragment(
                        SchemeType::File,
                        base_url.scheme_end,
                        base_url.username_end,
                        base_url.host_start,
                        base_url.host_end,
                        base_url.host,
                        base_url.port,
                        base_url.path_start,
                        remaining,
                    )
                } else {
                    self.serialization.push_str("file:///");
                    let scheme_end = "file".len() as u32;
                    let path_start = "file://".len();
                    let remaining =
                        self.parse_path(SchemeType::File, &mut false, path_start, input);
                    let (query_start, fragment_start) =
                        self.parse_query_and_fragment(SchemeType::File, scheme_end, remaining)?;
                    let path_start = path_start as u32;
                    Ok(Url {
                        serialization: self.serialization,
                        scheme_end,
                        username_end: path_start,
                        host_start: path_start,
                        host_end: path_start,
                        host: HostInternal::None,
                        port: None,
                        path_start,
                        query_start,
                        fragment_start,
                    })
                }
            }
        }
    }

    fn parse_relative(
        mut self,
        input: Input,
        scheme_type: SchemeType,
        base_url: &Url,
    ) -> ParseResult<Url> {
        // relative state
        debug_assert!(self.serialization.is_empty());
        let (first_char, input_after_first_char) = input.split_first();
        match first_char {
            None => {
                // Copy everything except the fragment
                let before_fragment = match base_url.fragment_start {
                    Some(i) => &base_url.serialization[..i as usize],
                    None => &*base_url.serialization,
                };
                self.serialization.push_str(before_fragment);
                Ok(Url {
                    serialization: self.serialization,
                    fragment_start: None,
                    ..*base_url
                })
            }
            Some('?') => {
                // Copy everything up to the query string
                let before_query = match (base_url.query_start, base_url.fragment_start) {
                    (None, None) => &*base_url.serialization,
                    (Some(i), _) | (None, Some(i)) => base_url.slice(..i),
                };
                self.serialization.push_str(before_query);
                let (query_start, fragment_start) =
                    self.parse_query_and_fragment(scheme_type, base_url.scheme_end, input)?;
                Ok(Url {
                    serialization: self.serialization,
                    query_start,
                    fragment_start,
                    ..*base_url
                })
            }
            Some('#') => self.fragment_only(base_url, input),
            Some('/') | Some('\\') => {
                let (slashes_count, remaining) = input.count_matching(|c| matches!(c, '/' | '\\'));
                if slashes_count >= 2 {
                    self.log_violation_if(SyntaxViolation::ExpectedDoubleSlash, || {
                        input
                            .clone()
                            .take_while(|&c| matches!(c, '/' | '\\'))
                            .collect::<String>()
                            != "//"
                    });
                    let scheme_end = base_url.scheme_end;
                    debug_assert!(base_url.byte_at(scheme_end) == b':');
                    self.serialization
                        .push_str(base_url.slice(..scheme_end + 1));
                    return self.after_double_slash(remaining, scheme_type, scheme_end);
                }
                let path_start = base_url.path_start;
                debug_assert!(base_url.byte_at(path_start) == b'/');
                self.serialization
                    .push_str(base_url.slice(..path_start + 1));
                let remaining = self.parse_path(
                    scheme_type,
                    &mut true,
                    path_start as usize,
                    input_after_first_char,
                );
                self.with_query_and_fragment(
                    scheme_type,
                    base_url.scheme_end,
                    base_url.username_end,
                    base_url.host_start,
                    base_url.host_end,
                    base_url.host,
                    base_url.port,
                    base_url.path_start,
                    remaining,
                )
            }
            _ => {
                let before_query = match (base_url.query_start, base_url.fragment_start) {
                    (None, None) => &*base_url.serialization,
                    (Some(i), _) | (None, Some(i)) => base_url.slice(..i),
                };
                self.serialization.push_str(before_query);
                // FIXME spec says just "remove last entry", not the "pop" algorithm
                self.pop_path(scheme_type, base_url.path_start as usize);
                let remaining =
                    self.parse_path(scheme_type, &mut true, base_url.path_start as usize, input);
                self.with_query_and_fragment(
                    scheme_type,
                    base_url.scheme_end,
                    base_url.username_end,
                    base_url.host_start,
                    base_url.host_end,
                    base_url.host,
                    base_url.port,
                    base_url.path_start,
                    remaining,
                )
            }
        }
    }

    fn after_double_slash(
        mut self,
        input: Input,
        scheme_type: SchemeType,
        scheme_end: u32,
    ) -> ParseResult<Url> {
        self.serialization.push('/');
        self.serialization.push('/');
        // authority state
        let (username_end, remaining) = self.parse_userinfo(input, scheme_type)?;
        // host state
        let host_start = to_u32(self.serialization.len())?;
        let (host_end, host, port, remaining) =
            self.parse_host_and_port(remaining, scheme_end, scheme_type)?;
        // path state
        let path_start = to_u32(self.serialization.len())?;
        let remaining = self.parse_path_start(scheme_type, &mut true, remaining);
        self.with_query_and_fragment(
            scheme_type,
            scheme_end,
            username_end,
            host_start,
            host_end,
            host,
            port,
            path_start,
            remaining,
        )
    }

    /// Return (username_end, remaining)
    fn parse_userinfo<'i>(
        &mut self,
        mut input: Input<'i>,
        scheme_type: SchemeType,
    ) -> ParseResult<(u32, Input<'i>)> {
        let mut last_at = None;
        let mut remaining = input.clone();
        let mut char_count = 0;
        while let Some(c) = remaining.next() {
            match c {
                '@' => {
                    if last_at.is_some() {
                        self.log_violation(SyntaxViolation::UnencodedAtSign)
                    } else {
                        self.log_violation(SyntaxViolation::EmbeddedCredentials)
                    }
                    last_at = Some((char_count, remaining.clone()))
                }
                '/' | '?' | '#' => break,
                '\\' if scheme_type.is_special() => break,
                _ => (),
            }
            char_count += 1;
        }
        let (mut userinfo_char_count, remaining) = match last_at {
            None => return Ok((to_u32(self.serialization.len())?, input)),
            Some((0, remaining)) => return Ok((to_u32(self.serialization.len())?, remaining)),
            Some(x) => x,
        };

        let mut username_end = None;
        let mut has_password = false;
        let mut has_username = false;
        while userinfo_char_count > 0 {
            let (c, utf8_c) = input.next_utf8().unwrap();
            userinfo_char_count -= 1;
            if c == ':' && username_end.is_none() {
                // Start parsing password
                username_end = Some(to_u32(self.serialization.len())?);
                // We don't add a colon if the password is empty
                if userinfo_char_count > 0 {
                    self.serialization.push(':');
                    has_password = true;
                }
            } else {
                if !has_password {
                    has_username = true;
                }
                self.check_url_code_point(c, &input);
                self.serialization
                    .extend(utf8_percent_encode(utf8_c, USERINFO));
            }
        }
        let username_end = match username_end {
            Some(i) => i,
            None => to_u32(self.serialization.len())?,
        };
        if has_username || has_password {
            self.serialization.push('@');
        }
        Ok((username_end, remaining))
    }

    fn parse_host_and_port<'i>(
        &mut self,
        input: Input<'i>,
        scheme_end: u32,
        scheme_type: SchemeType,
    ) -> ParseResult<(u32, HostInternal, Option<u16>, Input<'i>)> {
        let (host, remaining) = Parser::parse_host(input, scheme_type)?;
        write!(&mut self.serialization, "{}", host).unwrap();
        let host_end = to_u32(self.serialization.len())?;
        let (port, remaining) = if let Some(remaining) = remaining.split_prefix(':') {
            let scheme = || default_port(&self.serialization[..scheme_end as usize]);
            Parser::parse_port(remaining, scheme, self.context)?
        } else {
            (None, remaining)
        };
        if let Some(port) = port {
            write!(&mut self.serialization, ":{}", port).unwrap()
        }
        Ok((host_end, host.into(), port, remaining))
    }

    pub fn parse_host(
        mut input: Input,
        scheme_type: SchemeType,
    ) -> ParseResult<(Host<String>, Input)> {
        // Undo the Input abstraction here to avoid allocating in the common case
        // where the host part of the input does not contain any tab or newline
        let input_str = input.chars.as_str();
        let mut inside_square_brackets = false;
        let mut has_ignored_chars = false;
        let mut non_ignored_chars = 0;
        let mut bytes = 0;
        for c in input_str.chars() {
            match c {
                ':' if !inside_square_brackets => break,
                '\\' if scheme_type.is_special() => break,
                '/' | '?' | '#' => break,
                '\t' | '\n' | '\r' => {
                    has_ignored_chars = true;
                }
                '[' => {
                    inside_square_brackets = true;
                    non_ignored_chars += 1
                }
                ']' => {
                    inside_square_brackets = false;
                    non_ignored_chars += 1
                }
                _ => non_ignored_chars += 1,
            }
            bytes += c.len_utf8();
        }
        let replaced: String;
        let host_str;
        {
            let host_input = input.by_ref().take(non_ignored_chars);
            if has_ignored_chars {
                replaced = host_input.collect();
                host_str = &*replaced
            } else {
                for _ in host_input {}
                host_str = &input_str[..bytes]
            }
        }
        if scheme_type.is_special() && host_str.is_empty() {
            return Err(ParseError::EmptyHost);
        }
        if !scheme_type.is_special() {
            let host = Host::parse_opaque(host_str)?;
            return Ok((host, input));
        }
        let host = Host::parse(host_str)?;
        Ok((host, input))
    }

    pub(crate) fn parse_file_host<'i>(
        &mut self,
        input: Input<'i>,
    ) -> ParseResult<(bool, HostInternal, Input<'i>)> {
        // Undo the Input abstraction here to avoid allocating in the common case
        // where the host part of the input does not contain any tab or newline
        let input_str = input.chars.as_str();
        let mut has_ignored_chars = false;
        let mut non_ignored_chars = 0;
        let mut bytes = 0;
        for c in input_str.chars() {
            match c {
                '/' | '\\' | '?' | '#' => break,
                '\t' | '\n' | '\r' => has_ignored_chars = true,
                _ => non_ignored_chars += 1,
            }
            bytes += c.len_utf8();
        }
        let replaced: String;
        let host_str;
        let mut remaining = input.clone();
        {
            let host_input = remaining.by_ref().take(non_ignored_chars);
            if has_ignored_chars {
                replaced = host_input.collect();
                host_str = &*replaced
            } else {
                for _ in host_input {}
                host_str = &input_str[..bytes]
            }
        }
        if is_windows_drive_letter(host_str) {
            return Ok((false, HostInternal::None, input));
        }
        let host = if host_str.is_empty() {
            HostInternal::None
        } else {
            match Host::parse(host_str)? {
                Host::Domain(ref d) if d == "localhost" => HostInternal::None,
                host => {
                    write!(&mut self.serialization, "{}", host).unwrap();
                    host.into()
                }
            }
        };
        Ok((true, host, remaining))
    }

    pub fn parse_port<P>(
        mut input: Input,
        default_port: P,
        context: Context,
    ) -> ParseResult<(Option<u16>, Input)>
    where
        P: Fn() -> Option<u16>,
    {
        let mut port: u32 = 0;
        let mut has_any_digit = false;
        while let (Some(c), remaining) = input.split_first() {
            if let Some(digit) = c.to_digit(10) {
                port = port * 10 + digit;
                if port > ::std::u16::MAX as u32 {
                    return Err(ParseError::InvalidPort);
                }
                has_any_digit = true;
            } else if context == Context::UrlParser && !matches!(c, '/' | '\\' | '?' | '#') {
                return Err(ParseError::InvalidPort);
            } else {
                break;
            }
            input = remaining;
        }
        let mut opt_port = Some(port as u16);
        if !has_any_digit || opt_port == default_port() {
            opt_port = None;
        }
        Ok((opt_port, input))
    }

    pub fn parse_path_start<'i>(
        &mut self,
        scheme_type: SchemeType,
        has_host: &mut bool,
        mut input: Input<'i>,
    ) -> Input<'i> {
        // Path start state
        match input.split_first() {
            (Some('/'), remaining) => input = remaining,
            (Some('\\'), remaining) => {
                if scheme_type.is_special() {
                    self.log_violation(SyntaxViolation::Backslash);
                    input = remaining
                }
            }
            _ => {}
        }
        let path_start = self.serialization.len();
        self.serialization.push('/');
        self.parse_path(scheme_type, has_host, path_start, input)
    }

    pub fn parse_path<'i>(
        &mut self,
        scheme_type: SchemeType,
        has_host: &mut bool,
        path_start: usize,
        mut input: Input<'i>,
    ) -> Input<'i> {
        // Relative path state
        debug_assert!(self.serialization.ends_with('/'));
        loop {
            let segment_start = self.serialization.len();
            let mut ends_with_slash = false;
            loop {
                let input_before_c = input.clone();
                let (c, utf8_c) = if let Some(x) = input.next_utf8() {
                    x
                } else {
                    break;
                };
                match c {
                    '/' if self.context != Context::PathSegmentSetter => {
                        ends_with_slash = true;
                        break;
                    }
                    '\\' if self.context != Context::PathSegmentSetter
                        && scheme_type.is_special() =>
                    {
                        self.log_violation(SyntaxViolation::Backslash);
                        ends_with_slash = true;
                        break;
                    }
                    '?' | '#' if self.context == Context::UrlParser => {
                        input = input_before_c;
                        break;
                    }
                    _ => {
                        self.check_url_code_point(c, &input);
                        if self.context == Context::PathSegmentSetter {
                            if scheme_type.is_special() {
                                self.serialization
                                    .extend(utf8_percent_encode(utf8_c, SPECIAL_PATH_SEGMENT));
                            } else {
                                self.serialization
                                    .extend(utf8_percent_encode(utf8_c, PATH_SEGMENT));
                            }
                        } else {
                            self.serialization.extend(utf8_percent_encode(utf8_c, PATH));
                        }
                    }
                }
            }
            match &self.serialization[segment_start..] {
                ".." | "%2e%2e" | "%2e%2E" | "%2E%2e" | "%2E%2E" | "%2e." | "%2E." | ".%2e"
                | ".%2E" => {
                    debug_assert!(self.serialization.as_bytes()[segment_start - 1] == b'/');
                    self.serialization.truncate(segment_start - 1); // Truncate "/.."
                    self.pop_path(scheme_type, path_start);
                    if !self.serialization[path_start..].ends_with('/') {
                        self.serialization.push('/')
                    }
                }
                "." | "%2e" | "%2E" => {
                    self.serialization.truncate(segment_start);
                }
                _ => {
                    if scheme_type.is_file()
                        && is_windows_drive_letter(&self.serialization[path_start + 1..])
                    {
                        if self.serialization.ends_with('|') {
                            self.serialization.pop();
                            self.serialization.push(':');
                        }
                        if *has_host {
                            self.log_violation(SyntaxViolation::FileWithHostAndWindowsDrive);
                            *has_host = false; // FIXME account for this in callers
                        }
                    }
                    if ends_with_slash {
                        self.serialization.push('/')
                    }
                }
            }
            if !ends_with_slash {
                break;
            }
        }
        input
    }

    /// https://url.spec.whatwg.org/#pop-a-urls-path
    fn pop_path(&mut self, scheme_type: SchemeType, path_start: usize) {
        if self.serialization.len() > path_start {
            let slash_position = self.serialization[path_start..].rfind('/').unwrap();
            // + 1 since rfind returns the position before the slash.
            let segment_start = path_start + slash_position + 1;
            // Don’t pop a Windows drive letter
            // FIXME: *normalized* Windows drive letter
            if !(scheme_type.is_file()
                && is_windows_drive_letter(&self.serialization[segment_start..]))
            {
                self.serialization.truncate(segment_start);
            }
        }
    }

    pub fn parse_cannot_be_a_base_path<'i>(&mut self, mut input: Input<'i>) -> Input<'i> {
        loop {
            let input_before_c = input.clone();
            match input.next_utf8() {
                Some(('?', _)) | Some(('#', _)) if self.context == Context::UrlParser => {
                    return input_before_c
                }
                Some((c, utf8_c)) => {
                    self.check_url_code_point(c, &input);
                    self.serialization
                        .extend(utf8_percent_encode(utf8_c, CONTROLS));
                }
                None => return input,
            }
        }
    }

    fn with_query_and_fragment(
        mut self,
        scheme_type: SchemeType,
        scheme_end: u32,
        username_end: u32,
        host_start: u32,
        host_end: u32,
        host: HostInternal,
        port: Option<u16>,
        path_start: u32,
        remaining: Input,
    ) -> ParseResult<Url> {
        let (query_start, fragment_start) =
            self.parse_query_and_fragment(scheme_type, scheme_end, remaining)?;
        Ok(Url {
            serialization: self.serialization,
            scheme_end,
            username_end,
            host_start,
            host_end,
            host,
            port,
            path_start,
            query_start,
            fragment_start,
        })
    }

    /// Return (query_start, fragment_start)
    fn parse_query_and_fragment(
        &mut self,
        scheme_type: SchemeType,
        scheme_end: u32,
        mut input: Input,
    ) -> ParseResult<(Option<u32>, Option<u32>)> {
        let mut query_start = None;
        match input.next() {
            Some('#') => {}
            Some('?') => {
                query_start = Some(to_u32(self.serialization.len())?);
                self.serialization.push('?');
                let remaining = self.parse_query(scheme_type, scheme_end, input);
                if let Some(remaining) = remaining {
                    input = remaining
                } else {
                    return Ok((query_start, None));
                }
            }
            None => return Ok((None, None)),
            _ => panic!("Programming error. parse_query_and_fragment() called without ? or #"),
        }

        let fragment_start = to_u32(self.serialization.len())?;
        self.serialization.push('#');
        self.parse_fragment(input);
        Ok((query_start, Some(fragment_start)))
    }

    pub fn parse_query<'i>(
        &mut self,
        scheme_type: SchemeType,
        scheme_end: u32,
        mut input: Input<'i>,
    ) -> Option<Input<'i>> {
        let mut query = String::new(); // FIXME: use a streaming decoder instead
        let mut remaining = None;
        while let Some(c) = input.next() {
            if c == '#' && self.context == Context::UrlParser {
                remaining = Some(input);
                break;
            } else {
                self.check_url_code_point(c, &input);
                query.push(c);
            }
        }

        let encoding = match &self.serialization[..scheme_end as usize] {
            "http" | "https" | "file" | "ftp" | "gopher" => self.query_encoding_override,
            _ => None,
        };
        let query_bytes = ::query_encoding::encode(encoding, &query);
        let set = if scheme_type.is_special() {
            SPECIAL_QUERY
        } else {
            QUERY
        };
        self.serialization.extend(percent_encode(&query_bytes, set));
        remaining
    }

    fn fragment_only(mut self, base_url: &Url, mut input: Input) -> ParseResult<Url> {
        let before_fragment = match base_url.fragment_start {
            Some(i) => base_url.slice(..i),
            None => &*base_url.serialization,
        };
        debug_assert!(self.serialization.is_empty());
        self.serialization
            .reserve(before_fragment.len() + input.chars.as_str().len());
        self.serialization.push_str(before_fragment);
        self.serialization.push('#');
        let next = input.next();
        debug_assert!(next == Some('#'));
        self.parse_fragment(input);
        Ok(Url {
            serialization: self.serialization,
            fragment_start: Some(to_u32(before_fragment.len())?),
            ..*base_url
        })
    }

    pub fn parse_fragment(&mut self, mut input: Input) {
        while let Some((c, utf8_c)) = input.next_utf8() {
            if c == '\0' {
                self.log_violation(SyntaxViolation::NullInFragment)
            } else {
                self.check_url_code_point(c, &input);
                self.serialization.extend(utf8_percent_encode(
                    utf8_c,
                    // FIXME: tests fail when we use the FRAGMENT set here
                    // as defined in the spec as of 2019-07-17,
                    // likely because tests are out of date.
                    // See https://github.com/servo/rust-url/issues/290
                    CONTROLS,
                ));
            }
        }
    }

    fn check_url_code_point(&self, c: char, input: &Input) {
        if let Some(vfn) = self.violation_fn {
            if c == '%' {
                let mut input = input.clone();
                if !matches!((input.next(), input.next()), (Some(a), Some(b))
                             if is_ascii_hex_digit(a) && is_ascii_hex_digit(b))
                {
                    vfn(SyntaxViolation::PercentDecode)
                }
            } else if !is_url_code_point(c) {
                vfn(SyntaxViolation::NonUrlCodePoint)
            }
        }
    }
}

#[inline]
fn is_ascii_hex_digit(c: char) -> bool {
    matches!(c, 'a'..='f' | 'A'..='F' | '0'..='9')
}

// Non URL code points:
// U+0000 to U+0020 (space)
// " # % < > [ \ ] ^ ` { | }
// U+007F to U+009F
// surrogates
// U+FDD0 to U+FDEF
// Last two of each plane: U+__FFFE to U+__FFFF for __ in 00 to 10 hex
#[inline]
fn is_url_code_point(c: char) -> bool {
    matches!(c,
        'a'..='z' |
        'A'..='Z' |
        '0'..='9' |
        '!' | '$' | '&' | '\'' | '(' | ')' | '*' | '+' | ',' | '-' |
        '.' | '/' | ':' | ';' | '=' | '?' | '@' | '_' | '~' |
        '\u{A0}'..='\u{D7FF}' | '\u{E000}'..='\u{FDCF}' | '\u{FDF0}'..='\u{FFFD}' |
        '\u{10000}'..='\u{1FFFD}' | '\u{20000}'..='\u{2FFFD}' |
        '\u{30000}'..='\u{3FFFD}' | '\u{40000}'..='\u{4FFFD}' |
        '\u{50000}'..='\u{5FFFD}' | '\u{60000}'..='\u{6FFFD}' |
        '\u{70000}'..='\u{7FFFD}' | '\u{80000}'..='\u{8FFFD}' |
        '\u{90000}'..='\u{9FFFD}' | '\u{A0000}'..='\u{AFFFD}' |
        '\u{B0000}'..='\u{BFFFD}' | '\u{C0000}'..='\u{CFFFD}' |
        '\u{D0000}'..='\u{DFFFD}' | '\u{E1000}'..='\u{EFFFD}' |
        '\u{F0000}'..='\u{FFFFD}' | '\u{100000}'..='\u{10FFFD}')
}

/// https://url.spec.whatwg.org/#c0-controls-and-space
#[inline]
fn c0_control_or_space(ch: char) -> bool {
    ch <= ' ' // U+0000 to U+0020
}

/// https://url.spec.whatwg.org/#ascii-alpha
#[inline]
pub fn ascii_alpha(ch: char) -> bool {
    matches!(ch, 'a'..='z' | 'A'..='Z')
}

#[inline]
pub fn to_u32(i: usize) -> ParseResult<u32> {
    if i <= ::std::u32::MAX as usize {
        Ok(i as u32)
    } else {
        Err(ParseError::Overflow)
    }
}

/// Wether the scheme is file:, the path has a single segment, and that segment
/// is a Windows drive letter
fn is_windows_drive_letter(segment: &str) -> bool {
    segment.len() == 2 && starts_with_windows_drive_letter(segment)
}

fn starts_with_windows_drive_letter(s: &str) -> bool {
    ascii_alpha(s.as_bytes()[0] as char) && matches!(s.as_bytes()[1], b':' | b'|')
}

fn starts_with_windows_drive_letter_segment(input: &Input) -> bool {
    let mut input = input.clone();
    matches!((input.next(), input.next(), input.next()), (Some(a), Some(b), Some(c))
             if ascii_alpha(a) && matches!(b, ':' | '|') && matches!(c, '/' | '\\' | '?' | '#'))
}
