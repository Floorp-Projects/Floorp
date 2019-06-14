use std::net::IpAddr;
use std::str::FromStr;

use error::SdpParserInternalError;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum SdpAddressType {
    IP4 = 4,
    IP6 = 6,
}

impl SdpAddressType {
    pub fn same_protocol(self, addr: &IpAddr) -> bool {
        (addr.is_ipv6() && self == SdpAddressType::IP6)
            || (addr.is_ipv4() && self == SdpAddressType::IP4)
    }
}

pub fn address_to_string(addr: IpAddr) -> String {
    match addr {
        IpAddr::V4(ipv4) => format!("IN IP4 {}", ipv4.to_string()),
        IpAddr::V6(ipv6) => format!("IN IP6 {}", ipv6.to_string()),
    }
}

pub fn parse_network_type(value: &str) -> Result<(), SdpParserInternalError> {
    if value.to_uppercase() != "IN" {
        return Err(SdpParserInternalError::Generic(
            "nettype needs to be IN".to_string(),
        ));
    };
    Ok(())
}

pub fn parse_address_type(value: &str) -> Result<SdpAddressType, SdpParserInternalError> {
    Ok(match value.to_uppercase().as_ref() {
        "IP4" => SdpAddressType::IP4,
        "IP6" => SdpAddressType::IP6,
        _ => {
            return Err(SdpParserInternalError::Generic(
                "address type needs to be IP4 or IP6".to_string(),
            ));
        }
    })
}

pub fn parse_unicast_address(value: &str) -> Result<IpAddr, SdpParserInternalError> {
    Ok(IpAddr::from_str(value)?)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_network_type() {
        let internet = parse_network_type("iN");
        assert!(internet.is_ok());

        assert!(parse_network_type("").is_err());
        assert!(parse_network_type("FOO").is_err());
    }

    #[test]
    fn test_parse_address_type() {
        let ip4 = parse_address_type("iP4");
        assert!(ip4.is_ok());
        assert_eq!(ip4.unwrap(), SdpAddressType::IP4);
        let ip6 = parse_address_type("Ip6");
        assert!(ip6.is_ok());
        assert_eq!(ip6.unwrap(), SdpAddressType::IP6);

        assert!(parse_address_type("").is_err());
        assert!(parse_address_type("IP5").is_err());
    }

    #[test]
    fn test_parse_unicast_address() {
        let ip4 = parse_unicast_address("127.0.0.1");
        assert!(ip4.is_ok());
        let ip6 = parse_unicast_address("::1");
        assert!(ip6.is_ok());
    }
}
