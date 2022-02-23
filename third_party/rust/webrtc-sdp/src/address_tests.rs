/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use self::url::ParseError;
use super::*;
use std::error::Error;
use std::net::{AddrParseError, Ipv4Addr, Ipv6Addr};

#[derive(Debug)]
enum ParseTestError {
    Host(ParseError),
    Ip(AddrParseError),
}
impl fmt::Display for ParseTestError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            ParseTestError::Host(inner) => inner.fmt(f),
            ParseTestError::Ip(inner) => inner.fmt(f),
        }
    }
}
impl From<ParseError> for ParseTestError {
    fn from(err: ParseError) -> Self {
        ParseTestError::Host(err)
    }
}
impl From<AddrParseError> for ParseTestError {
    fn from(err: AddrParseError) -> Self {
        ParseTestError::Ip(err)
    }
}
impl Error for ParseTestError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        // Generic error, underlying cause isn't tracked.
        match self {
            ParseTestError::Host(a) => Some(a),
            ParseTestError::Ip(a) => Some(a),
        }
    }
}
#[test]
fn test_domain_name_parsing() -> Result<(), ParseTestError> {
    let address = Host::parse("this.is.a.fqdn")?;
    if let Host::Domain(domain) = address {
        assert_eq!(domain, "this.is.a.fqdn");
    } else {
        panic!();
    }
    Ok(())
}

#[test]
fn test_ipv4_address_parsing() -> Result<(), ParseTestError> {
    let address = Host::parse("1.0.0.1")?;
    if let Host::Ipv4(ip) = address {
        assert_eq!(ip, "1.0.0.1".parse::<Ipv4Addr>()?);
    } else {
        panic!();
    }
    Ok(())
}

#[test]
fn test_ipv6_address_parsing() -> Result<(), ParseTestError> {
    let address = Host::parse("[::1]")?;
    if let Host::Ipv6(ip) = address {
        assert_eq!(ip, "::1".parse::<Ipv6Addr>()?);
    } else {
        panic!();
    }
    Ok(())
}
