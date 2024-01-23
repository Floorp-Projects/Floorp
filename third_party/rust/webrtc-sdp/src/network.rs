/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use address::{Address, AddressType};
use error::SdpParserInternalError;
use std::net::IpAddr;
use std::str::FromStr;

pub fn ip_address_to_string(addr: IpAddr) -> String {
    match addr {
        IpAddr::V4(ipv4) => format!("IN IP4 {ipv4}"),
        IpAddr::V6(ipv6) => format!("IN IP6 {ipv6}"),
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
    Address::from_str(value)
}

#[cfg(test)]
#[path = "./network_tests.rs"]
mod tests;
