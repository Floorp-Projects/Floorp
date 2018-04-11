// Copyright 2013-2016 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[cfg(feature = "heapsize")] use heapsize::HeapSizeOf;
use std::cmp;
use std::fmt::{self, Formatter};
use std::io;
use std::net::{Ipv4Addr, Ipv6Addr, SocketAddr, SocketAddrV4, SocketAddrV6, ToSocketAddrs};
use std::vec;
use parser::{ParseResult, ParseError};
use percent_encoding::{percent_decode, utf8_percent_encode, SIMPLE_ENCODE_SET};
use idna;

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum HostInternal {
    None,
    Domain,
    Ipv4(Ipv4Addr),
    Ipv6(Ipv6Addr),
}

#[cfg(feature = "heapsize")]
known_heap_size!(0, HostInternal);

#[cfg(feature="serde")]
impl ::serde::Serialize for HostInternal {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error> where S: ::serde::Serializer {
        // This doesn’t use `derive` because that involves
        // large dependencies (that take a long time to build), and
        // either Macros 1.1 which are not stable yet or a cumbersome build script.
        //
        // Implementing `Serializer` correctly for an enum is tricky,
        // so let’s use existing enums that already do.
        use std::net::IpAddr;
        match *self {
            HostInternal::None => None,
            HostInternal::Domain => Some(None),
            HostInternal::Ipv4(addr) => Some(Some(IpAddr::V4(addr))),
            HostInternal::Ipv6(addr) => Some(Some(IpAddr::V6(addr))),
        }.serialize(serializer)
    }
}

#[cfg(feature="serde")]
impl ::serde::Deserialize for HostInternal {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: ::serde::Deserializer {
        use std::net::IpAddr;
        Ok(match ::serde::Deserialize::deserialize(deserializer)? {
            None => HostInternal::None,
            Some(None) => HostInternal::Domain,
            Some(Some(IpAddr::V4(addr))) => HostInternal::Ipv4(addr),
            Some(Some(IpAddr::V6(addr))) => HostInternal::Ipv6(addr),
        })
    }
}

impl<S> From<Host<S>> for HostInternal {
    fn from(host: Host<S>) -> HostInternal {
        match host {
            Host::Domain(_) => HostInternal::Domain,
            Host::Ipv4(address) => HostInternal::Ipv4(address),
            Host::Ipv6(address) => HostInternal::Ipv6(address),
        }
    }
}

/// The host name of an URL.
#[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub enum Host<S=String> {
    /// A DNS domain name, as '.' dot-separated labels.
    /// Non-ASCII labels are encoded in punycode per IDNA if this is the host of
    /// a special URL, or percent encoded for non-special URLs. Hosts for
    /// non-special URLs are also called opaque hosts.
    Domain(S),

    /// An IPv4 address.
    /// `Url::host_str` returns the serialization of this address,
    /// as four decimal integers separated by `.` dots.
    Ipv4(Ipv4Addr),

    /// An IPv6 address.
    /// `Url::host_str` returns the serialization of that address between `[` and `]` brackets,
    /// in the format per [RFC 5952 *A Recommendation
    /// for IPv6 Address Text Representation*](https://tools.ietf.org/html/rfc5952):
    /// lowercase hexadecimal with maximal `::` compression.
    Ipv6(Ipv6Addr),
}

#[cfg(feature="serde")]
impl<S: ::serde::Serialize>  ::serde::Serialize for Host<S> {
    fn serialize<R>(&self, serializer: &mut R) -> Result<(), R::Error> where R: ::serde::Serializer {
        use std::net::IpAddr;
        match *self {
            Host::Domain(ref s) => Ok(s),
            Host::Ipv4(addr) => Err(IpAddr::V4(addr)),
            Host::Ipv6(addr) => Err(IpAddr::V6(addr)),
        }.serialize(serializer)
    }
}

#[cfg(feature="serde")]
impl<S: ::serde::Deserialize> ::serde::Deserialize for Host<S> {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error> where D: ::serde::Deserializer {
        use std::net::IpAddr;
        Ok(match ::serde::Deserialize::deserialize(deserializer)? {
            Ok(s) => Host::Domain(s),
            Err(IpAddr::V4(addr)) => Host::Ipv4(addr),
            Err(IpAddr::V6(addr)) => Host::Ipv6(addr),
        })
    }
}

#[cfg(feature = "heapsize")]
impl<S: HeapSizeOf> HeapSizeOf for Host<S> {
    fn heap_size_of_children(&self) -> usize {
        match *self {
            Host::Domain(ref s) => s.heap_size_of_children(),
            _ => 0,
        }
    }
}

impl<'a> Host<&'a str> {
    /// Return a copy of `self` that owns an allocated `String` but does not borrow an `&Url`.
    pub fn to_owned(&self) -> Host<String> {
        match *self {
            Host::Domain(domain) => Host::Domain(domain.to_owned()),
            Host::Ipv4(address) => Host::Ipv4(address),
            Host::Ipv6(address) => Host::Ipv6(address),
        }
    }
}

impl Host<String> {
    /// Parse a host: either an IPv6 address in [] square brackets, or a domain.
    ///
    /// <https://url.spec.whatwg.org/#host-parsing>
    pub fn parse(input: &str) -> Result<Self, ParseError> {
        if input.starts_with('[') {
            if !input.ends_with(']') {
                return Err(ParseError::InvalidIpv6Address)
            }
            return parse_ipv6addr(&input[1..input.len() - 1]).map(Host::Ipv6)
        }
        let domain = percent_decode(input.as_bytes()).decode_utf8_lossy();
        let domain = idna::domain_to_ascii(&domain)?;
        if domain.find(|c| matches!(c,
            '\0' | '\t' | '\n' | '\r' | ' ' | '#' | '%' | '/' | ':' | '?' | '@' | '[' | '\\' | ']'
        )).is_some() {
            return Err(ParseError::InvalidDomainCharacter)
        }
        if let Some(address) = parse_ipv4addr(&domain)? {
            Ok(Host::Ipv4(address))
        } else {
            Ok(Host::Domain(domain.into()))
        }
    }

    // <https://url.spec.whatwg.org/#concept-opaque-host-parser>
    pub fn parse_opaque(input: &str) -> Result<Self, ParseError> {
        if input.starts_with('[') {
            if !input.ends_with(']') {
                return Err(ParseError::InvalidIpv6Address)
            }
            return parse_ipv6addr(&input[1..input.len() - 1]).map(Host::Ipv6)
        }
        if input.find(|c| matches!(c,
            '\0' | '\t' | '\n' | '\r' | ' ' | '#' | '/' | ':' | '?' | '@' | '[' | '\\' | ']'
        )).is_some() {
            return Err(ParseError::InvalidDomainCharacter)
        }
        let s = utf8_percent_encode(input, SIMPLE_ENCODE_SET).to_string();
        Ok(Host::Domain(s))
    }
}

impl<S: AsRef<str>> fmt::Display for Host<S> {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        match *self {
            Host::Domain(ref domain) => domain.as_ref().fmt(f),
            Host::Ipv4(ref addr) => addr.fmt(f),
            Host::Ipv6(ref addr) => {
                f.write_str("[")?;
                write_ipv6(addr, f)?;
                f.write_str("]")
            }
        }
    }
}

/// This mostly exists because coherence rules don’t allow us to implement
/// `ToSocketAddrs for (Host<S>, u16)`.
#[derive(Clone, Debug)]
pub struct HostAndPort<S=String> {
    pub host: Host<S>,
    pub port: u16,
}

impl<'a> HostAndPort<&'a str> {
    /// Return a copy of `self` that owns an allocated `String` but does not borrow an `&Url`.
    pub fn to_owned(&self) -> HostAndPort<String> {
        HostAndPort {
            host: self.host.to_owned(),
            port: self.port
        }
    }
}

impl<S: AsRef<str>> fmt::Display for HostAndPort<S> {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        self.host.fmt(f)?;
        f.write_str(":")?;
        self.port.fmt(f)
    }
}


impl<S: AsRef<str>> ToSocketAddrs for HostAndPort<S> {
    type Iter = SocketAddrs;

    fn to_socket_addrs(&self) -> io::Result<Self::Iter> {
        let port = self.port;
        match self.host {
            Host::Domain(ref domain) => Ok(SocketAddrs {
                // FIXME: use std::net::lookup_host when it’s stable.
                state: SocketAddrsState::Domain((domain.as_ref(), port).to_socket_addrs()?)
            }),
            Host::Ipv4(address) => Ok(SocketAddrs {
                state: SocketAddrsState::One(SocketAddr::V4(SocketAddrV4::new(address, port)))
            }),
            Host::Ipv6(address) => Ok(SocketAddrs {
                state: SocketAddrsState::One(SocketAddr::V6(SocketAddrV6::new(address, port, 0, 0)))
            }),
        }
    }
}

/// Socket addresses for an URL.
#[derive(Debug)]
pub struct SocketAddrs {
    state: SocketAddrsState
}

#[derive(Debug)]
enum SocketAddrsState {
    Domain(vec::IntoIter<SocketAddr>),
    One(SocketAddr),
    Done,
}

impl Iterator for SocketAddrs {
    type Item = SocketAddr;
    fn next(&mut self) -> Option<SocketAddr> {
        match self.state {
            SocketAddrsState::Domain(ref mut iter) => iter.next(),
            SocketAddrsState::One(s) => {
                self.state = SocketAddrsState::Done;
                Some(s)
            }
            SocketAddrsState::Done => None
        }
    }
}

fn write_ipv6(addr: &Ipv6Addr, f: &mut Formatter) -> fmt::Result {
    let segments = addr.segments();
    let (compress_start, compress_end) = longest_zero_sequence(&segments);
    let mut i = 0;
    while i < 8 {
        if i == compress_start {
            f.write_str(":")?;
            if i == 0 {
                f.write_str(":")?;
            }
            if compress_end < 8 {
                i = compress_end;
            } else {
                break;
            }
        }
        write!(f, "{:x}", segments[i as usize])?;
        if i < 7 {
            f.write_str(":")?;
        }
        i += 1;
    }
    Ok(())
}

// https://url.spec.whatwg.org/#concept-ipv6-serializer step 2 and 3
fn longest_zero_sequence(pieces: &[u16; 8]) -> (isize, isize) {
    let mut longest = -1;
    let mut longest_length = -1;
    let mut start = -1;
    macro_rules! finish_sequence(
        ($end: expr) => {
            if start >= 0 {
                let length = $end - start;
                if length > longest_length {
                    longest = start;
                    longest_length = length;
                }
            }
        };
    );
    for i in 0..8 {
        if pieces[i as usize] == 0 {
            if start < 0 {
                start = i;
            }
        } else {
            finish_sequence!(i);
            start = -1;
        }
    }
    finish_sequence!(8);
    // https://url.spec.whatwg.org/#concept-ipv6-serializer
    // step 3: ignore lone zeroes
    if longest_length < 2 {
        (-1, -2)
    } else {
        (longest, longest + longest_length)
    }
}

/// <https://url.spec.whatwg.org/#ipv4-number-parser>
fn parse_ipv4number(mut input: &str) -> Result<Option<u32>, ()> {
    let mut r = 10;
    if input.starts_with("0x") || input.starts_with("0X") {
        input = &input[2..];
        r = 16;
    } else if input.len() >= 2 && input.starts_with('0') {
        input = &input[1..];
        r = 8;
    }

    // At the moment we can't know the reason why from_str_radix fails
    // https://github.com/rust-lang/rust/issues/22639
    // So instead we check if the input looks like a real number and only return
    // an error when it's an overflow.
    let valid_number = match r {
        8 => input.chars().all(|c| c >= '0' && c <='7'),
        10 => input.chars().all(|c| c >= '0' && c <='9'),
        16 => input.chars().all(|c| (c >= '0' && c <='9') || (c >='a' && c <= 'f') || (c >= 'A' && c <= 'F')),
        _ => false
    };

    if !valid_number {
        return Ok(None);
    }

    if input.is_empty() {
        return Ok(Some(0));
    }
    if input.starts_with('+') {
        return Ok(None);
    }
    match u32::from_str_radix(input, r) {
        Ok(number) => Ok(Some(number)),
        Err(_) => Err(()),
    }
}

/// <https://url.spec.whatwg.org/#concept-ipv4-parser>
fn parse_ipv4addr(input: &str) -> ParseResult<Option<Ipv4Addr>> {
    if input.is_empty() {
        return Ok(None)
    }
    let mut parts: Vec<&str> = input.split('.').collect();
    if parts.last() == Some(&"") {
        parts.pop();
    }
    if parts.len() > 4 {
        return Ok(None);
    }
    let mut numbers: Vec<u32> = Vec::new();
    let mut overflow = false;
    for part in parts {
        if part == "" {
            return Ok(None);
        }
        match parse_ipv4number(part) {
            Ok(Some(n)) => numbers.push(n),
            Ok(None) => return Ok(None),
            Err(()) => overflow = true
        };
    }
    if overflow {
        return Err(ParseError::InvalidIpv4Address);
    }
    let mut ipv4 = numbers.pop().expect("a non-empty list of numbers");
    // Equivalent to: ipv4 >= 256 ** (4 − numbers.len())
    if ipv4 > u32::max_value() >> (8 * numbers.len() as u32)  {
        return Err(ParseError::InvalidIpv4Address);
    }
    if numbers.iter().any(|x| *x > 255) {
        return Err(ParseError::InvalidIpv4Address);
    }
    for (counter, n) in numbers.iter().enumerate() {
        ipv4 += n << (8 * (3 - counter as u32))
    }
    Ok(Some(Ipv4Addr::from(ipv4)))
}

/// <https://url.spec.whatwg.org/#concept-ipv6-parser>
fn parse_ipv6addr(input: &str) -> ParseResult<Ipv6Addr> {
    let input = input.as_bytes();
    let len = input.len();
    let mut is_ip_v4 = false;
    let mut pieces = [0, 0, 0, 0, 0, 0, 0, 0];
    let mut piece_pointer = 0;
    let mut compress_pointer = None;
    let mut i = 0;

    if len < 2 {
        return Err(ParseError::InvalidIpv6Address)
    }

    if input[0] == b':' {
        if input[1] != b':' {
            return Err(ParseError::InvalidIpv6Address)
        }
        i = 2;
        piece_pointer = 1;
        compress_pointer = Some(1);
    }

    while i < len {
        if piece_pointer == 8 {
            return Err(ParseError::InvalidIpv6Address)
        }
        if input[i] == b':' {
            if compress_pointer.is_some() {
                return Err(ParseError::InvalidIpv6Address)
            }
            i += 1;
            piece_pointer += 1;
            compress_pointer = Some(piece_pointer);
            continue
        }
        let start = i;
        let end = cmp::min(len, start + 4);
        let mut value = 0u16;
        while i < end {
            match (input[i] as char).to_digit(16) {
                Some(digit) => {
                    value = value * 0x10 + digit as u16;
                    i += 1;
                },
                None => break
            }
        }
        if i < len {
            match input[i] {
                b'.' => {
                    if i == start {
                        return Err(ParseError::InvalidIpv6Address)
                    }
                    i = start;
                    if piece_pointer > 6 {
                        return Err(ParseError::InvalidIpv6Address)
                    }
                    is_ip_v4 = true;
                },
                b':' => {
                    i += 1;
                    if i == len {
                        return Err(ParseError::InvalidIpv6Address)
                    }
                },
                _ => return Err(ParseError::InvalidIpv6Address)
            }
        }
        if is_ip_v4 {
            break
        }
        pieces[piece_pointer] = value;
        piece_pointer += 1;
    }

    if is_ip_v4 {
        if piece_pointer > 6 {
            return Err(ParseError::InvalidIpv6Address)
        }
        let mut numbers_seen = 0;
        while i < len {
            if numbers_seen > 0 {
                if numbers_seen < 4 && (i < len && input[i] == b'.') {
                    i += 1
                } else {
                    return Err(ParseError::InvalidIpv6Address)
                }
            }

            let mut ipv4_piece = None;
            while i < len {
                let digit = match input[i] {
                    c @ b'0' ... b'9' => c - b'0',
                    _ => break
                };
                match ipv4_piece {
                    None => ipv4_piece = Some(digit as u16),
                    Some(0) => return Err(ParseError::InvalidIpv6Address),  // No leading zero
                    Some(ref mut v) => {
                        *v = *v * 10 + digit as u16;
                        if *v > 255 {
                            return Err(ParseError::InvalidIpv6Address)
                        }
                    }
                }
                i += 1;
            }

            pieces[piece_pointer] = if let Some(v) = ipv4_piece {
                pieces[piece_pointer] * 0x100 + v
            } else {
                return Err(ParseError::InvalidIpv6Address)
            };
            numbers_seen += 1;

            if numbers_seen == 2 || numbers_seen == 4 {
                piece_pointer += 1;
            }
        }

        if numbers_seen != 4 {
            return Err(ParseError::InvalidIpv6Address)
        }
    }

    if i < len {
        return Err(ParseError::InvalidIpv6Address)
    }

    match compress_pointer {
        Some(compress_pointer) => {
            let mut swaps = piece_pointer - compress_pointer;
            piece_pointer = 7;
            while swaps > 0 {
                pieces.swap(piece_pointer, compress_pointer + swaps - 1);
                swaps -= 1;
                piece_pointer -= 1;
            }
        }
        _ => if piece_pointer != 8 {
            return Err(ParseError::InvalidIpv6Address)
        }
    }
    Ok(Ipv6Addr::new(pieces[0], pieces[1], pieces[2], pieces[3],
                     pieces[4], pieces[5], pieces[6], pieces[7]))
}
