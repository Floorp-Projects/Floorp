use error::SdpParserInternalError;
use SdpType;

pub fn parse_repeat(value: &str) -> Result<SdpType, SdpParserInternalError> {
    // TODO implement this if it's ever needed
    Err(SdpParserInternalError::Unsupported(format!("unsupported type repeat: {} ", value)))
}

#[test]
fn test_repeat_works() {
    // FIXME use a proper r value here
    assert!(parse_repeat("0 0").is_err());
}

pub fn parse_zone(value: &str) -> Result<SdpType, SdpParserInternalError> {
    // TODO implement this if it's ever needed
    Err(SdpParserInternalError::Unsupported(format!("unsupported type zone: {}", value)))
}

#[test]
fn test_zone_works() {
    // FIXME use a proper z value here
    assert!(parse_zone("0 0").is_err());
}

pub fn parse_key(value: &str) -> Result<SdpType, SdpParserInternalError> {
    // TODO implement this if it's ever needed
    Err(SdpParserInternalError::Unsupported(format!("unsupported type key: {}", value)))
}

#[test]
fn test_keys_works() {
    // FIXME use a proper k value here
    assert!(parse_key("12345").is_err());
}

pub fn parse_information(value: &str) -> Result<SdpType, SdpParserInternalError> {
    Err(SdpParserInternalError::Unsupported(format!("unsupported type information: {}", value)))
}

#[test]
fn test_information_works() {
    assert!(parse_information("foobar").is_err());
}

pub fn parse_uri(value: &str) -> Result<SdpType, SdpParserInternalError> {
    // TODO check if this is really a URI
    Err(SdpParserInternalError::Unsupported(format!("unsupported type uri: {}", value)))
}

#[test]
fn test_uri_works() {
    assert!(parse_uri("http://www.mozilla.org").is_err());
}

pub fn parse_email(value: &str) -> Result<SdpType, SdpParserInternalError> {
    // TODO check if this is really an email address
    Err(SdpParserInternalError::Unsupported(format!("unsupported type email: {}", value)))
}

#[test]
fn test_email_works() {
    assert!(parse_email("nils@mozilla.com").is_err());
}

pub fn parse_phone(value: &str) -> Result<SdpType, SdpParserInternalError> {
    // TODO check if this is really a phone number
    Err(SdpParserInternalError::Unsupported(format!("unsupported type phone: {}", value)))
}

#[test]
fn test_phone_works() {
    assert!(parse_phone("+123456789").is_err());
}
