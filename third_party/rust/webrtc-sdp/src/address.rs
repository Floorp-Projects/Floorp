extern crate url;
use self::url::Host;
use error::SdpParserInternalError;
use std::convert::TryFrom;
use std::fmt;
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};
use std::str::FromStr;

#[derive(Clone, Debug)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub enum Address {
    Fqdn(String),
    Ip(IpAddr),
}

impl fmt::Display for Address {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Address::Fqdn(fqdn) => fqdn.fmt(f),
            Address::Ip(ip) => ip.fmt(f),
        }
    }
}

impl FromStr for Address {
    type Err = SdpParserInternalError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut e: Option<SdpParserInternalError> = None;
        if s.find(':').is_some() {
            match IpAddr::from_str(s) {
                Ok(ip) => return Ok(Address::Ip(ip)),
                Err(err) => e = Some(err.into()),
            }
        }
        Host::parse(s)
            .and_then(|host| match host {
                Host::Domain(s) => Ok(Address::Fqdn(s)),
                Host::Ipv4(ip) => Ok(Address::Ip(IpAddr::V4(ip))),
                Host::Ipv6(ip) => Ok(Address::Ip(IpAddr::V6(ip))),
            })
            .map_err(|err| e.unwrap_or_else(|| err.into()))
    }
}

impl From<ExplicitlyTypedAddress> for Address {
    fn from(item: ExplicitlyTypedAddress) -> Self {
        match item {
            ExplicitlyTypedAddress::Fqdn { domain, .. } => Address::Fqdn(domain),
            ExplicitlyTypedAddress::Ip(ip) => Address::Ip(ip),
        }
    }
}

impl PartialEq for Address {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Address::Fqdn(a), Address::Fqdn(b)) => a.to_lowercase() == b.to_lowercase(),
            (Address::Ip(a), Address::Ip(b)) => a == b,
            (_, _) => false,
        }
    }
}

#[derive(Clone, Copy, PartialEq, Debug)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub enum AddressType {
    IpV4 = 4,
    IpV6 = 6,
}

impl fmt::Display for AddressType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            AddressType::IpV4 => "IP4",
            AddressType::IpV6 => "IP6",
        }
        .fmt(f)
    }
}

impl FromStr for AddressType {
    type Err = SdpParserInternalError;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_uppercase().as_str() {
            "IP4" => Ok(AddressType::IpV4),
            "IP6" => Ok(AddressType::IpV6),
            _ => Err(SdpParserInternalError::UnknownAddressType(s.to_owned())),
        }
    }
}

pub trait AddressTyped {
    fn address_type(&self) -> AddressType;
}

impl AddressTyped for IpAddr {
    fn address_type(&self) -> AddressType {
        match self {
            IpAddr::V4(_) => AddressType::IpV4,
            IpAddr::V6(_) => AddressType::IpV6,
        }
    }
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub enum ExplicitlyTypedAddress {
    Fqdn {
        address_type: AddressType,
        domain: String,
    },
    Ip(IpAddr),
}

impl fmt::Display for ExplicitlyTypedAddress {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "IN {} ", self.address_type())?;
        match self {
            ExplicitlyTypedAddress::Fqdn { domain, .. } => domain.fmt(f),
            ExplicitlyTypedAddress::Ip(ip) => ip.fmt(f),
        }
    }
}

impl AddressTyped for ExplicitlyTypedAddress {
    fn address_type(&self) -> AddressType {
        match self {
            ExplicitlyTypedAddress::Fqdn { address_type, .. } => *address_type,
            ExplicitlyTypedAddress::Ip(ip) => ip.address_type(),
        }
    }
}

impl From<IpAddr> for ExplicitlyTypedAddress {
    fn from(item: IpAddr) -> Self {
        ExplicitlyTypedAddress::Ip(item)
    }
}

impl From<Ipv4Addr> for ExplicitlyTypedAddress {
    fn from(item: Ipv4Addr) -> Self {
        ExplicitlyTypedAddress::Ip(IpAddr::V4(item))
    }
}

impl From<Ipv6Addr> for ExplicitlyTypedAddress {
    fn from(item: Ipv6Addr) -> Self {
        ExplicitlyTypedAddress::Ip(IpAddr::V6(item))
    }
}

impl TryFrom<(AddressType, &str)> for ExplicitlyTypedAddress {
    type Error = SdpParserInternalError;
    fn try_from(item: (AddressType, &str)) -> Result<Self, Self::Error> {
        match Address::from_str(item.1)? {
            Address::Ip(ip) => {
                if ip.address_type() != item.0 {
                    Err(SdpParserInternalError::AddressTypeMismatch {
                        found: ip.address_type(),
                        expected: item.0,
                    })
                } else {
                    Ok(ExplicitlyTypedAddress::Ip(ip))
                }
            }
            Address::Fqdn(domain) => Ok(ExplicitlyTypedAddress::Fqdn {
                address_type: item.0,
                domain,
            }),
        }
    }
}

impl PartialEq for ExplicitlyTypedAddress {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (
                ExplicitlyTypedAddress::Fqdn {
                    address_type: a1,
                    domain: d1,
                },
                ExplicitlyTypedAddress::Fqdn {
                    address_type: a2,
                    domain: d2,
                },
            ) => a1 == a2 && d1.to_lowercase() == d2.to_lowercase(),
            (ExplicitlyTypedAddress::Ip(a), ExplicitlyTypedAddress::Ip(b)) => a == b,
            (_, _) => false,
        }
    }
}

#[cfg(test)]
mod tests {
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
}
