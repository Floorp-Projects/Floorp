// Copyright 2016 The rust-url developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Getters and setters for URL components implemented per https://url.spec.whatwg.org/#api
//!
//! Unless you need to be interoperable with web browsers,
//! you probably want to use `Url` method instead.

use {Url, Position, Host, ParseError, idna};
use parser::{Parser, SchemeType, default_port, Context, Input};

/// https://url.spec.whatwg.org/#dom-url-domaintoascii
pub fn domain_to_ascii(domain: &str) -> String {
    match Host::parse(domain) {
        Ok(Host::Domain(domain)) => domain,
        _ => String::new(),
    }
}

/// https://url.spec.whatwg.org/#dom-url-domaintounicode
pub fn domain_to_unicode(domain: &str) -> String {
    match Host::parse(domain) {
        Ok(Host::Domain(ref domain)) => {
            let (unicode, _errors) = idna::domain_to_unicode(domain);
            unicode
        }
        _ => String::new(),
    }
}

/// Getter for https://url.spec.whatwg.org/#dom-url-href
pub fn href(url: &Url) -> &str {
    url.as_str()
}

/// Setter for https://url.spec.whatwg.org/#dom-url-href
pub fn set_href(url: &mut Url, value: &str) -> Result<(), ParseError> {
    *url = Url::parse(value)?;
    Ok(())
}

/// Getter for https://url.spec.whatwg.org/#dom-url-origin
pub fn origin(url: &Url) -> String {
    url.origin().unicode_serialization()
}

/// Getter for https://url.spec.whatwg.org/#dom-url-protocol
#[inline]
pub fn protocol(url: &Url) -> &str {
    &url.as_str()[..url.scheme().len() + ":".len()]
}

/// Setter for https://url.spec.whatwg.org/#dom-url-protocol
pub fn set_protocol(url: &mut Url, mut new_protocol: &str) -> Result<(), ()> {
    // The scheme state in the spec ignores everything after the first `:`,
    // but `set_scheme` errors if there is more.
    if let Some(position) = new_protocol.find(':') {
        new_protocol = &new_protocol[..position];
    }
    url.set_scheme(new_protocol)
}

/// Getter for https://url.spec.whatwg.org/#dom-url-username
#[inline]
pub fn username(url: &Url) -> &str {
    url.username()
}

/// Setter for https://url.spec.whatwg.org/#dom-url-username
pub fn set_username(url: &mut Url, new_username: &str) -> Result<(), ()> {
    url.set_username(new_username)
}

/// Getter for https://url.spec.whatwg.org/#dom-url-password
#[inline]
pub fn password(url: &Url) -> &str {
    url.password().unwrap_or("")
}

/// Setter for https://url.spec.whatwg.org/#dom-url-password
pub fn set_password(url: &mut Url, new_password: &str) -> Result<(), ()> {
    url.set_password(if new_password.is_empty() { None } else { Some(new_password) })
}

/// Getter for https://url.spec.whatwg.org/#dom-url-host
#[inline]
pub fn host(url: &Url) -> &str {
    &url[Position::BeforeHost..Position::AfterPort]
}

/// Setter for https://url.spec.whatwg.org/#dom-url-host
pub fn set_host(url: &mut Url, new_host: &str) -> Result<(), ()> {
    if url.cannot_be_a_base() {
        return Err(())
    }
    let host;
    let opt_port;
    {
        let scheme = url.scheme();
        let result = Parser::parse_host(Input::new(new_host), SchemeType::from(scheme));
        match result {
            Ok((h, remaining)) => {
                host = h;
                opt_port = if let Some(remaining) = remaining.split_prefix(':') {
                    Parser::parse_port(remaining, || default_port(scheme), Context::Setter)
                    .ok().map(|(port, _remaining)| port)
                } else {
                    None
                };
            }
            Err(_) => return Err(())
        }
    }
    url.set_host_internal(host, opt_port);
    Ok(())
}

/// Getter for https://url.spec.whatwg.org/#dom-url-hostname
#[inline]
pub fn hostname(url: &Url) -> &str {
    url.host_str().unwrap_or("")
}

/// Setter for https://url.spec.whatwg.org/#dom-url-hostname
pub fn set_hostname(url: &mut Url, new_hostname: &str) -> Result<(), ()> {
    if url.cannot_be_a_base() {
        return Err(())
    }
    let result = Parser::parse_host(Input::new(new_hostname), SchemeType::from(url.scheme()));
    if let Ok((host, _remaining)) = result {
        url.set_host_internal(host, None);
        Ok(())
    } else {
        Err(())
    }
}

/// Getter for https://url.spec.whatwg.org/#dom-url-port
#[inline]
pub fn port(url: &Url) -> &str {
    &url[Position::BeforePort..Position::AfterPort]
}

/// Setter for https://url.spec.whatwg.org/#dom-url-port
pub fn set_port(url: &mut Url, new_port: &str) -> Result<(), ()> {
    let result;
    {
        // has_host implies !cannot_be_a_base
        let scheme = url.scheme();
        if !url.has_host() || scheme == "file" {
            return Err(())
        }
        result = Parser::parse_port(Input::new(new_port), || default_port(scheme), Context::Setter)
    }
    if let Ok((new_port, _remaining)) = result {
        url.set_port_internal(new_port);
        Ok(())
    } else {
        Err(())
    }
}

/// Getter for https://url.spec.whatwg.org/#dom-url-pathname
#[inline]
pub fn pathname(url: &Url) -> &str {
     url.path()
}

/// Setter for https://url.spec.whatwg.org/#dom-url-pathname
pub fn set_pathname(url: &mut Url, new_pathname: &str) {
    if !url.cannot_be_a_base() {
        url.set_path(new_pathname)
    }
}

/// Getter for https://url.spec.whatwg.org/#dom-url-search
pub fn search(url: &Url) -> &str {
    trim(&url[Position::AfterPath..Position::AfterQuery])
}

/// Setter for https://url.spec.whatwg.org/#dom-url-search
pub fn set_search(url: &mut Url, new_search: &str) {
    url.set_query(match new_search {
        "" => None,
        _ if new_search.starts_with('?') => Some(&new_search[1..]),
        _ => Some(new_search),
    })
}

/// Getter for https://url.spec.whatwg.org/#dom-url-hash
pub fn hash(url: &Url) -> &str {
    trim(&url[Position::AfterQuery..])
}

/// Setter for https://url.spec.whatwg.org/#dom-url-hash
pub fn set_hash(url: &mut Url, new_hash: &str) {
    if url.scheme() != "javascript" {
        url.set_fragment(match new_hash {
            "" => None,
            _ if new_hash.starts_with('#') => Some(&new_hash[1..]),
            _ => Some(new_hash),
        })
    }
}

fn trim(s: &str) -> &str {
    if s.len() == 1 {
        ""
    } else {
        s
    }
}
