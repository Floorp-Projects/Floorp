/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use super::*;

fn mock_verify(_signature: &[u8; 64], _data: &[u8]) -> bool {
    true
}

/// We'd like to just assert_eq!(original_payload, our_payload), but our JSON
/// serialization format is different (we don't have spaces after commas or
/// colons), so we need to do this instead.
fn assert_payloads_equivalent(our_payload: &[u8], original_payload: &[u8]) {
    // Per the above we expect our payload to always be smaller than the
    // original.
    assert!(our_payload.len() <= original_payload.len());

    let our_value: serde_json::Value = serde_json::from_slice(our_payload).unwrap();
    let original_value: serde_json::Value = serde_json::from_slice(original_payload).unwrap();
    if our_value == original_value {
        return;
    }

    assert_eq!(
        std::str::from_utf8(our_payload).unwrap(),
        std::str::from_utf8(original_payload).unwrap(),
        "Mismatched payloads"
    );
}

fn test_roundtrip(payload: &[u8], token: &Token, base64: &[u8]) {
    let binary = base64::decode(base64).unwrap();
    let raw_token = RawToken::from_buffer(&binary).unwrap();
    let from_binary_token = Token::from_raw_token(&raw_token, mock_verify).unwrap();
    assert_eq!(&from_binary_token, token);

    // LMAO, payload in the documentation and the examples have members out of
    // order so this doesn't hold.
    // assert_eq!(std::str::from_utf8(raw_token.payload()).unwrap(), std::str::from_utf8(payload).unwrap());

    let our_payload = from_binary_token.to_payload();
    assert_payloads_equivalent(&our_payload, payload);
    assert_payloads_equivalent(&our_payload, raw_token.payload());

    let signed = from_binary_token
        .to_signed_token_with_payload(|_data| raw_token.signature.clone(), raw_token.payload());
    assert_eq!(binary, signed);

    let new_base64 = base64::encode(signed);
    assert_eq!(new_base64, std::str::from_utf8(base64).unwrap());
}

#[test]
fn basic() {
    // The one from the example.
    let payload =
        r#"{"origin": "https://example.com:443", "feature": "Frobulate", "expiry": 1609459199}"#;
    let token = Token::from_payload(LATEST_VERSION, payload.as_bytes()).unwrap();
    assert_eq!(token.origin, "https://example.com:443");
    assert_eq!(token.feature, "Frobulate");
    assert_eq!(token.expiry, 1609459199);
    assert_eq!(token.is_subdomain, false);
    assert_eq!(token.is_third_party, false);
    assert!(token.usage.is_none());

    test_roundtrip(payload.as_bytes(), &token, b"A9YTk5WLM0uhXPj2OE/dEj8mEdWbcWOvCyWMNdRFiCZpBRuynxJMx1i/SO5pRT7UhoCSDTieoh9qOCMHsc2y5w4AAABTeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5IjogMTYwOTQ1OTE5OX0=");
}

#[test]
fn subdomain() {
    // The one from the example.
    let payload = r#"{"origin": "https://example.com:443", "isSubdomain": true, "feature": "Frobulate", "expiry": 1609459199}"#;
    let token = Token::from_payload(LATEST_VERSION, payload.as_bytes()).unwrap();
    assert_eq!(token.origin, "https://example.com:443");
    assert_eq!(token.feature, "Frobulate");
    assert_eq!(token.expiry, 1609459199);
    assert_eq!(token.is_subdomain, true);
    assert_eq!(token.is_third_party, false);
    assert!(token.usage.is_none());

    test_roundtrip(payload.as_bytes(), &token, b"AzHieSb3NXHXhJ1zvxNcmUeR351wzlXwJK7pYM8MCFfNenvonZi30kS0GOKWUleIyats/2aTB1HoiCmLWIvG5AgAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiaXNTdWJkb21haW4iOiB0cnVlLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5IjogMTYwOTQ1OTE5OX0=");
}

#[test]
fn third_party() {
    let payload = r#"{"origin": "https://thirdparty.com:443", "feature": "Frobulate", "expiry": 1609459199, "isThirdParty": true}"#;
    let token = Token::from_payload(LATEST_VERSION, payload.as_bytes()).unwrap();
    assert_eq!(token.origin, "https://thirdparty.com:443");
    assert_eq!(token.feature, "Frobulate");
    assert_eq!(token.expiry, 1609459199);
    assert_eq!(token.is_subdomain, false);
    assert_eq!(token.is_third_party, true);
    assert!(token.usage.is_none());

    test_roundtrip(payload.as_bytes(), &token, b"Ax8UsCU9EUBRj8PZG147cOO7VqR86BF13TSu6w2wRqixzJ+fEUULvOQimXwWl1ETYCfAZMlvvAqoFYB8HxrsZA4AAABseyJvcmlnaW4iOiAiaHR0cHM6Ly90aGlyZHBhcnR5LmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgImZlYXR1cmUiOiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDE2MDk0NTkxOTl9");
}

#[test]
fn third_party_usage_restriction() {
    let payload = r#"{"origin": "https://thirdparty.com:443", "feature": "Frobulate", "expiry": 1609459199, "isThirdParty": true, "usage": "subset"}"#;
    let token = Token::from_payload(LATEST_VERSION, payload.as_bytes()).unwrap();
    assert_eq!(token.origin, "https://thirdparty.com:443");
    assert_eq!(token.feature, "Frobulate");
    assert_eq!(token.expiry, 1609459199);
    assert_eq!(token.is_subdomain, false);
    assert_eq!(token.is_third_party, true);
    assert_eq!(token.usage, Usage::Subset);

    test_roundtrip(payload.as_bytes(), &token, b"AzEs7XzQG5ktWF/puroSU5RzxPEdEUUhqwXtL2hItZoJU0bghKwbsTKVghkR95GHSfINTBnxwRBnFVfYGJLm8AUAAAB/eyJvcmlnaW4iOiAiaHR0cHM6Ly90aGlyZHBhcnR5LmNvbTo0NDMiLCAiaXNUaGlyZFBhcnR5IjogdHJ1ZSwgInVzYWdlIjogInN1YnNldCIsICJmZWF0dXJlIjogIkZyb2J1bGF0ZSIsICJleHBpcnkiOiAxNjA5NDU5MTk5fQ==");
}
