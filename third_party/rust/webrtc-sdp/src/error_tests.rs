/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::*;
use address::Address;
use std::str::FromStr;
#[test]
fn test_sdp_parser_internal_error_unknown_address_type() {
    let error = SdpParserInternalError::UnknownAddressType("foo".to_string());
    assert_eq!(
        format!("{error}"),
        format!("{}: {}", INTERNAL_ERROR_MESSAGE_UNKNOWN_ADDRESS_TYPE, "foo")
    );
    assert!(error.source().is_none());
}
#[test]
fn test_sdp_parser_internal_error_address_type_mismatch() {
    let error = SdpParserInternalError::AddressTypeMismatch {
        found: AddressType::IpV4,
        expected: AddressType::IpV6,
    };
    assert_eq!(
        format!("{error}"),
        format!(
            "{}: {}, {}",
            INTERNAL_ERROR_MESSAGE_ADDRESS_TYPE_MISMATCH,
            AddressType::IpV4,
            AddressType::IpV6
        )
    );
    assert!(error.source().is_none());
}

#[test]
fn test_sdp_parser_internal_error_generic() {
    let generic = SdpParserInternalError::Generic("generic message".to_string());
    assert_eq!(format!("{generic}"), "Parsing error: generic message");
    assert!(generic.source().is_none());
}

#[test]
fn test_sdp_parser_internal_error_unsupported() {
    let unsupported =
        SdpParserInternalError::Unsupported("unsupported internal message".to_string());
    assert_eq!(
        format!("{unsupported}"),
        "Unsupported parsing error: unsupported internal message"
    );
    assert!(unsupported.source().is_none());
}

#[test]
fn test_sdp_parser_internal_error_integer() {
    let v = "12a";
    let integer = v.parse::<u64>();
    assert!(integer.is_err());
    let int_err = SdpParserInternalError::Integer(integer.err().unwrap());
    assert_eq!(
        format!("{int_err}"),
        "Integer parsing error: invalid digit found in string"
    );
    assert!(int_err.source().is_some());
}

#[test]
fn test_sdp_parser_internal_error_float() {
    let v = "12.2a";
    let float = v.parse::<f32>();
    assert!(float.is_err());
    let int_err = SdpParserInternalError::Float(float.err().unwrap());
    assert_eq!(
        format!("{int_err}"),
        "Float parsing error: invalid float literal"
    );
    assert!(int_err.source().is_some());
}

#[test]
fn test_sdp_parser_internal_error_address() {
    let v = "127.0.0.500";
    let addr_err = Address::from_str(v).err().unwrap();
    assert_eq!(
        format!("{addr_err}"),
        "Domain name parsing error: invalid IPv4 address"
    );
    assert!(addr_err.source().is_some());
}

#[test]
fn test_sdp_parser_error_line() {
    let line1 = SdpParserError::Line {
        error: SdpParserInternalError::Generic("test message".to_string()),
        line: "test line".to_string(),
        line_number: 13,
    };
    assert_eq!(
        format!("{line1}"),
        "Line error: Parsing error: test message in line(13): test line"
    );
    assert!(line1.source().is_some());
}

#[test]
fn test_sdp_parser_error_unsupported() {
    let unsupported1 = SdpParserError::Unsupported {
        error: SdpParserInternalError::Generic("unsupported value".to_string()),
        line: "unsupported line".to_string(),
        line_number: 21,
    };
    assert_eq!(
        format!("{unsupported1}"),
        "Unsupported: Parsing error: unsupported value in line(21): unsupported line"
    );
    assert!(unsupported1.source().is_some());
}

#[test]
fn test_sdp_parser_error_sequence() {
    let sequence1 = SdpParserError::Sequence {
        message: "sequence message".to_string(),
        line_number: 42,
    };
    assert_eq!(
        format!("{sequence1}"),
        "Sequence error in line(42): sequence message"
    );
    assert!(sequence1.source().is_none());
}
