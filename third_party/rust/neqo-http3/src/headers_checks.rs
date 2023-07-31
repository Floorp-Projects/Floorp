// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::unused_unit)] // see https://github.com/Lymia/enumset/issues/44

use crate::{Error, MessageType, Res};
use enumset::{enum_set, EnumSet, EnumSetType};
use neqo_common::Header;
use std::convert::TryFrom;

#[derive(EnumSetType, Debug)]
enum PseudoHeaderState {
    Status,
    Method,
    Scheme,
    Authority,
    Path,
    Protocol,
    Regular,
}

impl PseudoHeaderState {
    fn is_pseudo(self) -> bool {
        self != Self::Regular
    }
}

impl TryFrom<(MessageType, &str)> for PseudoHeaderState {
    type Error = Error;

    fn try_from(v: (MessageType, &str)) -> Res<Self> {
        match v {
            (MessageType::Response, ":status") => Ok(Self::Status),
            (MessageType::Request, ":method") => Ok(Self::Method),
            (MessageType::Request, ":scheme") => Ok(Self::Scheme),
            (MessageType::Request, ":authority") => Ok(Self::Authority),
            (MessageType::Request, ":path") => Ok(Self::Path),
            (MessageType::Request, ":protocol") => Ok(Self::Protocol),
            (_, _) => Err(Error::InvalidHeader),
        }
    }
}

/// Check whether the response is informational(1xx).
/// # Errors
/// Returns an error if response headers do not contain
/// a status header or if the value of the header is 101 or cannot be parsed.
pub fn is_interim(headers: &[Header]) -> Res<bool> {
    let status = headers.iter().take(1).find(|h| h.name() == ":status");
    if let Some(h) = status {
        #[allow(clippy::map_err_ignore)]
        let status_code = h.value().parse::<i32>().map_err(|_| Error::InvalidHeader)?;
        if status_code == 101 {
            // https://datatracker.ietf.org/doc/html/draft-ietf-quic-http#section-4.3
            Err(Error::InvalidHeader)
        } else {
            Ok((100..200).contains(&status_code))
        }
    } else {
        Err(Error::InvalidHeader)
    }
}

fn track_pseudo(
    name: &str,
    result_state: &mut EnumSet<PseudoHeaderState>,
    message_type: MessageType,
) -> Res<bool> {
    let new_state = if name.starts_with(':') {
        if result_state.contains(PseudoHeaderState::Regular) {
            return Err(Error::InvalidHeader);
        }
        PseudoHeaderState::try_from((message_type, name))?
    } else {
        PseudoHeaderState::Regular
    };

    let pseudo = new_state.is_pseudo();
    if *result_state & new_state == EnumSet::empty() || !pseudo {
        *result_state |= new_state;
        Ok(pseudo)
    } else {
        Err(Error::InvalidHeader)
    }
}

/// Checks if request/response headers are well formed, i.e. contain
/// allowed pseudo headers and in a right order, etc.
/// # Errors
/// Returns an error if headers are not well formed.
pub fn headers_valid(headers: &[Header], message_type: MessageType) -> Res<()> {
    let mut method_value: Option<&str> = None;
    let mut protocol_value: Option<&str> = None;
    let mut scheme_value: Option<&str> = None;
    let mut pseudo_state = EnumSet::new();
    for header in headers {
        let is_pseudo = track_pseudo(header.name(), &mut pseudo_state, message_type)?;

        let mut bytes = header.name().bytes();
        if is_pseudo {
            if header.name() == ":method" {
                method_value = Some(header.value());
            } else if header.name() == ":protocol" {
                protocol_value = Some(header.value());
            } else if header.name() == ":scheme" {
                scheme_value = Some(header.value());
            }
            _ = bytes.next();
        }

        if bytes.any(|b| matches!(b, 0 | 0x10 | 0x13 | 0x3a | 0x41..=0x5a)) {
            return Err(Error::InvalidHeader); // illegal characters.
        }
    }
    // Clear the regular header bit, since we only check pseudo headers below.
    pseudo_state.remove(PseudoHeaderState::Regular);
    let pseudo_header_mask = match message_type {
        MessageType::Response => enum_set!(PseudoHeaderState::Status),
        MessageType::Request => {
            if method_value == Some("CONNECT") {
                let connect_mask = PseudoHeaderState::Method | PseudoHeaderState::Authority;
                if let Some(protocol) = protocol_value {
                    // For a webtransport CONNECT, the :scheme field must be set to https.
                    if protocol == "webtransport" && scheme_value != Some("https") {
                        return Err(Error::InvalidHeader);
                    }
                    // The CONNECT request for with :protocol included must have the scheme,
                    // authority, and path set.
                    connect_mask | PseudoHeaderState::Scheme | PseudoHeaderState::Path
                } else {
                    connect_mask
                }
            } else {
                PseudoHeaderState::Method | PseudoHeaderState::Scheme | PseudoHeaderState::Path
            }
        }
    };

    if (MessageType::Request == message_type)
        && pseudo_state.contains(PseudoHeaderState::Protocol)
        && method_value != Some("CONNECT")
    {
        return Err(Error::InvalidHeader);
    }

    if pseudo_state & pseudo_header_mask != pseudo_header_mask {
        return Err(Error::InvalidHeader);
    }

    Ok(())
}

/// Checks if trailers are well formed, i.e. pseudo headers are not
/// allowed in trailers.
/// # Errors
/// Returns an error if trailers are not well formed.
pub fn trailers_valid(headers: &[Header]) -> Res<()> {
    for header in headers {
        if header.name().starts_with(':') {
            return Err(Error::InvalidHeader);
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::headers_valid;
    use crate::MessageType;
    use neqo_common::Header;

    fn create_connect_headers() -> Vec<Header> {
        vec![
            Header::new(":method", "CONNECT"),
            Header::new(":protocol", "webtransport"),
            Header::new(":scheme", "https"),
            Header::new(":authority", "something.com"),
            Header::new(":path", "/here"),
        ]
    }

    fn create_connect_headers_without_field(field: &str) -> Vec<Header> {
        create_connect_headers()
            .into_iter()
            .filter(|header| header.name() != field)
            .collect()
    }

    #[test]
    fn connect_with_missing_header() {
        for field in &[":scheme", ":path", ":authority"] {
            assert!(headers_valid(
                &create_connect_headers_without_field(field),
                MessageType::Request
            )
            .is_err());
        }
    }

    #[test]
    fn invalid_scheme_webtransport_connect() {
        assert!(headers_valid(
            &[
                Header::new(":method", "CONNECT"),
                Header::new(":protocol", "webtransport"),
                Header::new(":authority", "something.com"),
                Header::new(":scheme", "http"),
                Header::new(":path", "/here"),
            ],
            MessageType::Request
        )
        .is_err());
    }

    #[test]
    fn valid_webtransport_connect() {
        assert!(headers_valid(&create_connect_headers(), MessageType::Request).is_ok());
    }
}
