/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
