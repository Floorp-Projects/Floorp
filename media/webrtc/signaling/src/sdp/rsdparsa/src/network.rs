use address::{Address, AddressType};
use error::SdpParserInternalError;
use std::net::IpAddr;
use std::str::FromStr;

pub fn ip_address_to_string(addr: IpAddr) -> String {
    match addr {
        IpAddr::V4(ipv4) => format!("IN IP4 {}", ipv4.to_string()),
        IpAddr::V6(ipv6) => format!("IN IP6 {}", ipv6.to_string()),
    }
}

pub fn parse_network_type(value: &str) -> Result<(), SdpParserInternalError> {
    if value.to_uppercase() != "IN" {
        return Err(SdpParserInternalError::Generic(
            "nettype must be IN".to_string(),
        ));
    };
    Ok(())
}

pub fn parse_address_type(value: &str) -> Result<AddressType, SdpParserInternalError> {
    AddressType::from_str(value.to_uppercase().as_str())
        .map_err(|_| SdpParserInternalError::Generic("address type must be IP4 or IP6".to_string()))
}

pub fn parse_unicast_address(value: &str) -> Result<Address, SdpParserInternalError> {
    Ok(Address::from_str(value)?)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_network_type() -> Result<(), SdpParserInternalError> {
        parse_network_type("iN")?;

        assert!(parse_network_type("").is_err());
        assert!(parse_network_type("FOO").is_err());
        Ok(())
    }

    #[test]
    fn test_parse_address_type() -> Result<(), SdpParserInternalError> {
        let ip4 = parse_address_type("iP4")?;
        assert_eq!(ip4, AddressType::IpV4);
        let ip6 = parse_address_type("Ip6")?;
        assert_eq!(ip6, AddressType::IpV6);

        assert!(parse_address_type("").is_err());
        assert!(parse_address_type("IP5").is_err());
        Ok(())
    }

    #[test]
    fn test_parse_unicast_address() -> Result<(), SdpParserInternalError> {
        parse_unicast_address("127.0.0.1")?;
        parse_unicast_address("::1")?;
        Ok(())
    }
}
