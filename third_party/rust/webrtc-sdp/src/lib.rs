/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![warn(clippy::all)]
#![forbid(unsafe_code)]

#[macro_use]
extern crate log;
#[cfg(feature = "serialize")]
#[macro_use]
extern crate serde_derive;
#[cfg(feature = "serialize")]
extern crate serde;
use std::convert::TryFrom;
use std::fmt;

#[macro_use]
pub mod attribute_type;
pub mod address;
pub mod anonymizer;
pub mod error;
pub mod media_type;
pub mod network;

use address::{AddressTyped, ExplicitlyTypedAddress};
use anonymizer::{AnonymizingClone, StatefulSdpAnonymizer};
use attribute_type::{
    parse_attribute, SdpAttribute, SdpAttributeRid, SdpAttributeSimulcastVersion, SdpAttributeType,
    SdpSingleDirection,
};
use error::{SdpParserError, SdpParserInternalError};
use media_type::{
    parse_media, parse_media_vector, SdpFormatList, SdpMedia, SdpMediaLine, SdpMediaValue,
    SdpProtocolValue,
};
use network::{parse_address_type, parse_network_type};

/*
 * RFC4566
 * bandwidth-fields =    *(%x62 "=" bwtype ":" bandwidth CRLF)
 */
#[derive(Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub enum SdpBandwidth {
    As(u32),
    Ct(u32),
    Tias(u32),
    Unknown(String, u32),
}

impl fmt::Display for SdpBandwidth {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let (tp_string, value) = match *self {
            SdpBandwidth::As(ref x) => ("AS", x),
            SdpBandwidth::Ct(ref x) => ("CT", x),
            SdpBandwidth::Tias(ref x) => ("TIAS", x),
            SdpBandwidth::Unknown(ref tp, ref x) => (&tp[..], x),
        };
        write!(f, "{tp}:{val}", tp = tp_string, val = value)
    }
}

/*
 * RFC4566
 * connection-field =    [%x63 "=" nettype SP addrtype SP
 *                       connection-address CRLF]
 */
#[derive(Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub struct SdpConnection {
    pub address: ExplicitlyTypedAddress,
    pub ttl: Option<u8>,
    pub amount: Option<u32>,
}

impl fmt::Display for SdpConnection {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.address.fmt(f)?;
        write_option_string!(f, "/{}", self.ttl)?;
        write_option_string!(f, "/{}", self.amount)
    }
}

impl AnonymizingClone for SdpConnection {
    fn masked_clone(&self, anon: &mut StatefulSdpAnonymizer) -> Self {
        let mut masked = self.clone();
        masked.address = anon.mask_typed_address(&self.address);
        masked
    }
}

/*
 * RFC4566
 * origin-field =        %x6f "=" username SP sess-id SP sess-version SP
 *                       nettype SP addrtype SP unicast-address CRLF
 */
#[derive(Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub struct SdpOrigin {
    pub username: String,
    pub session_id: u64,
    pub session_version: u64,
    pub unicast_addr: ExplicitlyTypedAddress,
}

impl fmt::Display for SdpOrigin {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{username} {sess_id} {sess_vers} {unicast_addr}",
            username = self.username,
            sess_id = self.session_id,
            sess_vers = self.session_version,
            unicast_addr = self.unicast_addr
        )
    }
}

impl AnonymizingClone for SdpOrigin {
    fn masked_clone(&self, anon: &mut StatefulSdpAnonymizer) -> Self {
        let mut masked = self.clone();
        masked.username = anon.mask_origin_user(&self.username);
        masked.unicast_addr = anon.mask_typed_address(&masked.unicast_addr);
        masked
    }
}

/*
 * RFC4566
 * time-fields =         1*( %x74 "=" start-time SP stop-time
 *                       *(CRLF repeat-fields) CRLF)
 *                       [zone-adjustments CRLF]
 */
#[derive(Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub struct SdpTiming {
    pub start: u64,
    pub stop: u64,
}

impl fmt::Display for SdpTiming {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{start} {stop}", start = self.start, stop = self.stop)
    }
}

#[cfg_attr(feature = "serialize", derive(Serialize))]
pub enum SdpType {
    // Note: Email, Information, Key, Phone, Repeat, Uri and Zone are left out
    //       on purposes as we don't want to support them.
    Attribute(SdpAttribute),
    Bandwidth(SdpBandwidth),
    Connection(SdpConnection),
    Media(SdpMediaLine),
    Origin(SdpOrigin),
    Session(String),
    Timing(SdpTiming),
    Version(u64),
}

#[cfg_attr(feature = "serialize", derive(Serialize))]
pub struct SdpLine {
    pub line_number: usize,
    pub sdp_type: SdpType,
    pub text: String,
}

/*
 * RFC4566
 * ; SDP Syntax
 * session-description = proto-version
 *                       origin-field
 *                       session-name-field
 *                       information-field
 *                       uri-field
 *                       email-fields
 *                       phone-fields
 *                       connection-field
 *                       bandwidth-fields
 *                       time-fields
 *                       key-field
 *                       attribute-fields
 *                       media-descriptions
 */
#[derive(Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub struct SdpSession {
    pub version: u64,
    pub origin: SdpOrigin,
    pub session: Option<String>,
    pub connection: Option<SdpConnection>,
    pub bandwidth: Vec<SdpBandwidth>,
    pub timing: Option<SdpTiming>,
    pub attribute: Vec<SdpAttribute>,
    pub media: Vec<SdpMedia>,
    pub warnings: Vec<SdpParserError>, // unsupported values:
                                       // information: Option<String>,
                                       // uri: Option<String>,
                                       // email: Option<String>,
                                       // phone: Option<String>,
                                       // repeat: Option<String>,
                                       // zone: Option<String>,
                                       // key: Option<String>
}

impl fmt::Display for SdpSession {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "v={version}\r\n\
             o={origin}\r\n\
             s={session}\r\n\
             {timing}\
             {bandwidth}\
             {connection}\
             {session_attributes}\
             {media_sections}",
            version = self.version,
            origin = self.origin,
            session = self.get_session_text(),
            timing = option_to_string!("t={}\r\n", self.timing),
            bandwidth = maybe_vector_to_string!("b={}\r\n", self.bandwidth, "\r\nb="),
            connection = option_to_string!("c={}\r\n", self.connection),
            session_attributes = maybe_vector_to_string!("a={}\r\n", self.attribute, "\r\na="),
            media_sections = maybe_vector_to_string!("{}", self.media, "\r\n")
        )
    }
}

impl SdpSession {
    pub fn new(version: u64, origin: SdpOrigin, session: String) -> SdpSession {
        let session = match session.trim() {
            s if !s.is_empty() => Some(s.to_owned()),
            _ => None,
        };
        SdpSession {
            version,
            origin,
            session,
            connection: None,
            bandwidth: Vec::new(),
            timing: None,
            attribute: Vec::new(),
            media: Vec::new(),
            warnings: Vec::new(),
        }
    }

    pub fn get_version(&self) -> u64 {
        self.version
    }

    pub fn get_origin(&self) -> &SdpOrigin {
        &self.origin
    }

    pub fn get_session(&self) -> &Option<String> {
        &self.session
    }

    pub fn get_session_text(&self) -> &str {
        if let Some(text) = &self.session {
            text.as_str()
        } else {
            " "
        }
    }
    pub fn get_connection(&self) -> &Option<SdpConnection> {
        &self.connection
    }

    pub fn set_connection(&mut self, c: SdpConnection) {
        self.connection = Some(c)
    }

    pub fn add_bandwidth(&mut self, b: SdpBandwidth) {
        self.bandwidth.push(b)
    }

    pub fn set_timing(&mut self, t: SdpTiming) {
        self.timing = Some(t)
    }

    pub fn add_attribute(&mut self, a: SdpAttribute) -> Result<(), SdpParserInternalError> {
        if !a.allowed_at_session_level() {
            return Err(SdpParserInternalError::Generic(format!(
                "{} not allowed at session level",
                a
            )));
        };
        self.attribute.push(a);
        Ok(())
    }

    pub fn extend_media(&mut self, v: Vec<SdpMedia>) {
        self.media.extend(v)
    }

    pub fn parse_session_vector(&mut self, lines: &mut Vec<SdpLine>) -> Result<(), SdpParserError> {
        while !lines.is_empty() {
            let line = lines.remove(0);
            match line.sdp_type {
                SdpType::Attribute(a) => {
                    let _line_number = line.line_number;
                    self.add_attribute(a).map_err(|e: SdpParserInternalError| {
                        SdpParserError::Sequence {
                            message: format!("{}", e),
                            line_number: _line_number,
                        }
                    })?
                }
                SdpType::Bandwidth(b) => self.add_bandwidth(b),
                SdpType::Timing(t) => self.set_timing(t),
                SdpType::Connection(c) => self.set_connection(c),

                SdpType::Origin(_) | SdpType::Session(_) | SdpType::Version(_) => {
                    return Err(SdpParserError::Sequence {
                        message: "version, origin or session at wrong level".to_string(),
                        line_number: line.line_number,
                    });
                }
                SdpType::Media(_) => {
                    return Err(SdpParserError::Sequence {
                        message: "media line not allowed in session parser".to_string(),
                        line_number: line.line_number,
                    });
                }
            }
        }
        Ok(())
    }

    pub fn get_attribute(&self, t: SdpAttributeType) -> Option<&SdpAttribute> {
        self.attribute
            .iter()
            .find(|a| SdpAttributeType::from(*a) == t)
    }

    pub fn add_media(
        &mut self,
        media_type: SdpMediaValue,
        direction: SdpAttribute,
        port: u32,
        protocol: SdpProtocolValue,
        addr: ExplicitlyTypedAddress,
    ) -> Result<(), SdpParserInternalError> {
        let mut media = SdpMedia::new(SdpMediaLine {
            media: media_type,
            port,
            port_count: 1,
            proto: protocol,
            formats: SdpFormatList::Integers(Vec::new()),
        });

        media.add_attribute(direction)?;

        media.set_connection(SdpConnection {
            address: addr,
            ttl: None,
            amount: None,
        })?;

        self.media.push(media);

        Ok(())
    }
}

impl AnonymizingClone for SdpSession {
    fn masked_clone(&self, anon: &mut StatefulSdpAnonymizer) -> Self {
        let mut masked: SdpSession = SdpSession {
            version: self.version,
            session: self.session.clone(),
            origin: self.origin.masked_clone(anon),
            connection: self.connection.clone(),
            timing: self.timing.clone(),
            bandwidth: self.bandwidth.clone(),
            attribute: Vec::new(),
            media: Vec::new(),
            warnings: Vec::new(),
        };
        masked.origin = self.origin.masked_clone(anon);
        masked.connection = masked.connection.map(|con| con.masked_clone(anon));
        for i in &self.attribute {
            masked.attribute.push(i.masked_clone(anon));
        }
        masked
    }
}

fn parse_session(value: &str) -> Result<SdpType, SdpParserInternalError> {
    trace!("session: {}", value);
    Ok(SdpType::Session(String::from(value)))
}

fn parse_version(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let ver = value.parse::<u64>()?;
    if ver != 0 {
        return Err(SdpParserInternalError::Generic(format!(
            "version type contains unsupported value {}",
            ver
        )));
    };
    trace!("version: {}", ver);
    Ok(SdpType::Version(ver))
}

fn parse_origin(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let mut tokens = value.split_whitespace();
    let username = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                "Origin type is missing username token".to_string(),
            ));
        }
        Some(x) => x,
    };
    let session_id = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                "Origin type is missing session ID token".to_string(),
            ));
        }
        Some(x) => x.parse::<u64>()?,
    };
    let session_version = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                "Origin type is missing session version token".to_string(),
            ));
        }
        Some(x) => x.parse::<u64>()?,
    };
    match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                "Origin type is missing network type token".to_string(),
            ));
        }
        Some(x) => parse_network_type(x)?,
    };
    let addrtype = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                "Origin type is missing address type token".to_string(),
            ));
        }
        Some(x) => parse_address_type(x)?,
    };
    let unicast_addr = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                "Origin type is missing IP address token".to_string(),
            ));
        }
        Some(x) => ExplicitlyTypedAddress::try_from((addrtype, x))?,
    };
    if addrtype != unicast_addr.address_type() {
        return Err(SdpParserInternalError::Generic(
            "Origin addrtype does not match address.".to_string(),
        ));
    }
    let o = SdpOrigin {
        username: String::from(username),
        session_id,
        session_version,
        unicast_addr,
    };
    trace!("origin: {}", o);
    Ok(SdpType::Origin(o))
}

fn parse_connection(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let cv: Vec<&str> = value.split_whitespace().collect();
    if cv.len() != 3 {
        return Err(SdpParserInternalError::Generic(
            "connection attribute must have three tokens".to_string(),
        ));
    }
    parse_network_type(cv[0])?;
    let addrtype = parse_address_type(cv[1])?;
    let mut ttl = None;
    let mut amount = None;
    let mut addr_token = cv[2];
    if addr_token.find('/') != None {
        let addr_tokens: Vec<&str> = addr_token.split('/').collect();
        if addr_tokens.len() >= 3 {
            amount = Some(addr_tokens[2].parse::<u32>()?);
        }
        ttl = Some(addr_tokens[1].parse::<u8>()?);
        addr_token = addr_tokens[0];
    }
    let address = ExplicitlyTypedAddress::try_from((addrtype, addr_token))?;
    let c = SdpConnection {
        address,
        ttl,
        amount,
    };
    trace!("connection: {}", c);
    Ok(SdpType::Connection(c))
}

fn parse_bandwidth(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let bv: Vec<&str> = value.split(':').collect();
    if bv.len() != 2 {
        return Err(SdpParserInternalError::Generic(
            "bandwidth attribute must have two tokens".to_string(),
        ));
    }
    let bandwidth = bv[1].parse::<u32>()?;
    let bw = match bv[0].to_uppercase().as_ref() {
        "AS" => SdpBandwidth::As(bandwidth),
        "CT" => SdpBandwidth::Ct(bandwidth),
        "TIAS" => SdpBandwidth::Tias(bandwidth),
        _ => SdpBandwidth::Unknown(String::from(bv[0]), bandwidth),
    };
    trace!("bandwidth: {}", bw);
    Ok(SdpType::Bandwidth(bw))
}

fn parse_timing(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let tv: Vec<&str> = value.split_whitespace().collect();
    if tv.len() != 2 {
        return Err(SdpParserInternalError::Generic(
            "timing attribute must have two tokens".to_string(),
        ));
    }
    let start = tv[0].parse::<u64>()?;
    let stop = tv[1].parse::<u64>()?;
    let t = SdpTiming { start, stop };
    trace!("timing: {}", t);
    Ok(SdpType::Timing(t))
}

fn parse_sdp_line(line: &str, line_number: usize) -> Result<SdpLine, SdpParserError> {
    if line.find('=') == None {
        return Err(SdpParserError::Line {
            error: SdpParserInternalError::Generic("missing = character in line".to_string()),
            line: line.to_string(),
            line_number,
        });
    }
    let mut splitted_line = line.splitn(2, '=');
    let line_type = match splitted_line.next() {
        None => {
            return Err(SdpParserError::Line {
                error: SdpParserInternalError::Generic("missing type".to_string()),
                line: line.to_string(),
                line_number,
            });
        }
        Some(t) => {
            let trimmed = t.trim();
            if trimmed.len() > 1 {
                return Err(SdpParserError::Line {
                    error: SdpParserInternalError::Generic("type too long".to_string()),
                    line: line.to_string(),
                    line_number,
                });
            }
            if trimmed.is_empty() {
                return Err(SdpParserError::Line {
                    error: SdpParserInternalError::Generic("type is empty".to_string()),
                    line: line.to_string(),
                    line_number,
                });
            }
            trimmed.to_lowercase()
        }
    };
    let (line_value, untrimmed_line_value) = match splitted_line.next() {
        None => {
            return Err(SdpParserError::Line {
                error: SdpParserInternalError::Generic("missing value".to_string()),
                line: line.to_string(),
                line_number,
            });
        }
        Some(v) => {
            let trimmed = v.trim();
            // For compatibility with sites that don't adhere to "s=-" for no session ID
            if trimmed.is_empty() && line_type.as_str() != "s" {
                return Err(SdpParserError::Line {
                    error: SdpParserInternalError::Generic("value is empty".to_string()),
                    line: line.to_string(),
                    line_number,
                });
            }
            (trimmed, v)
        }
    };
    match line_type.as_ref() {
        "a" => parse_attribute(line_value),
        "b" => parse_bandwidth(line_value),
        "c" => parse_connection(line_value),
        "e" => Err(SdpParserInternalError::Generic(format!(
            "unsupported type email: {}",
            line_value
        ))),
        "i" => Err(SdpParserInternalError::Generic(format!(
            "unsupported type information: {}",
            line_value
        ))),
        "k" => Err(SdpParserInternalError::Generic(format!(
            "unsupported insecure key exchange: {}",
            line_value
        ))),
        "m" => parse_media(line_value),
        "o" => parse_origin(line_value),
        "p" => Err(SdpParserInternalError::Generic(format!(
            "unsupported type phone: {}",
            line_value
        ))),
        "r" => Err(SdpParserInternalError::Generic(format!(
            "unsupported type repeat: {}",
            line_value
        ))),
        "s" => parse_session(untrimmed_line_value),
        "t" => parse_timing(line_value),
        "u" => Err(SdpParserInternalError::Generic(format!(
            "unsupported type uri: {}",
            line_value
        ))),
        "v" => parse_version(line_value),
        "z" => Err(SdpParserInternalError::Generic(format!(
            "unsupported type zone: {}",
            line_value
        ))),
        _ => Err(SdpParserInternalError::Generic(
            "unknown sdp type".to_string(),
        )),
    }
    .map(|sdp_type| SdpLine {
        line_number,
        sdp_type,
        text: line.to_owned(),
    })
    .map_err(|e| match e {
        SdpParserInternalError::UnknownAddressType(..)
        | SdpParserInternalError::AddressTypeMismatch { .. }
        | SdpParserInternalError::Generic(..)
        | SdpParserInternalError::Integer(..)
        | SdpParserInternalError::Float(..)
        | SdpParserInternalError::Domain(..)
        | SdpParserInternalError::IpAddress(..) => SdpParserError::Line {
            error: e,
            line: line.to_string(),
            line_number,
        },
        SdpParserInternalError::Unsupported(..) => SdpParserError::Unsupported {
            error: e,
            line: line.to_string(),
            line_number,
        },
    })
}

fn sanity_check_sdp_session(session: &SdpSession) -> Result<(), SdpParserError> {
    let make_seq_error = |x: &str| SdpParserError::Sequence {
        message: x.to_string(),
        line_number: 0,
    };

    if session.timing.is_none() {
        return Err(make_seq_error("Missing timing type at session level"));
    }
    // Checks that all media have connections if there is no top level
    // This explicitly allows for zero connection lines if there are no media
    // sections for interoperability reasons.
    let media_cons = &session.media.iter().all(|m| m.get_connection().is_some());
    if !media_cons && session.get_connection().is_none() {
        return Err(make_seq_error(
            "Without connection type at session level all media sections must have connection types",
        ));
    }

    // Check that extmaps are not defined on session and media level
    if session.get_attribute(SdpAttributeType::Extmap).is_some() {
        for msection in &session.media {
            if msection.get_attribute(SdpAttributeType::Extmap).is_some() {
                return Err(make_seq_error(
                    "Extmap can't be define at session and media level",
                ));
            }
        }
    }

    for msection in &session.media {
        if msection.get_attribute(SdpAttributeType::Sendonly).is_some() {
            if let Some(&SdpAttribute::Simulcast(ref x)) =
                msection.get_attribute(SdpAttributeType::Simulcast)
            {
                if !x.receive.is_empty() {
                    return Err(make_seq_error(
                        "Simulcast can't define receive parameters for sendonly",
                    ));
                }
            }
        }
        if msection.get_attribute(SdpAttributeType::Recvonly).is_some() {
            if let Some(&SdpAttribute::Simulcast(ref x)) =
                msection.get_attribute(SdpAttributeType::Simulcast)
            {
                if !x.send.is_empty() {
                    return Err(make_seq_error(
                        "Simulcast can't define send parameters for recvonly",
                    ));
                }
            }
        }

        let rids: Vec<&SdpAttributeRid> = msection
            .get_attributes()
            .iter()
            .filter_map(|attr| match *attr {
                SdpAttribute::Rid(ref rid) => Some(rid),
                _ => None,
            })
            .collect();
        let recv_rids: Vec<&str> = rids
            .iter()
            .filter_map(|rid| match rid.direction {
                SdpSingleDirection::Recv => Some(rid.id.as_str()),
                _ => None,
            })
            .collect();
        let send_rids: Vec<&str> = rids
            .iter()
            .filter_map(|rid| match rid.direction {
                SdpSingleDirection::Send => Some(rid.id.as_str()),
                _ => None,
            })
            .collect();

        for rid_format in rids.iter().flat_map(|rid| &rid.formats) {
            match *msection.get_formats() {
                SdpFormatList::Integers(ref int_fmt) => {
                    if !int_fmt.contains(&(u32::from(*rid_format))) {
                        return Err(make_seq_error(
                            "Rid pts must be declared in the media section",
                        ));
                    }
                }
                SdpFormatList::Strings(ref str_fmt) => {
                    if !str_fmt.contains(&rid_format.to_string()) {
                        return Err(make_seq_error(
                            "Rid pts must be declared in the media section",
                        ));
                    }
                }
            }
        }

        if let Some(&SdpAttribute::Simulcast(ref simulcast)) =
            msection.get_attribute(SdpAttributeType::Simulcast)
        {
            let check_defined_rids =
                |simulcast_version_list: &Vec<SdpAttributeSimulcastVersion>,
                 rid_ids: &[&str]|
                 -> Result<(), SdpParserError> {
                    for simulcast_rid in simulcast_version_list.iter().flat_map(|x| &x.ids) {
                        if !rid_ids.contains(&simulcast_rid.id.as_str()) {
                            return Err(make_seq_error(
                                "Simulcast RIDs must be defined in any rid attribute",
                            ));
                        }
                    }
                    Ok(())
                };

            check_defined_rids(&simulcast.receive, &recv_rids)?;
            check_defined_rids(&simulcast.send, &send_rids)?;
        }
    }

    Ok(())
}

fn parse_sdp_vector(lines: &mut Vec<SdpLine>) -> Result<SdpSession, SdpParserError> {
    if lines.len() < 4 {
        return Err(SdpParserError::Sequence {
            message: "SDP neeeds at least 4 lines".to_string(),
            line_number: 0,
        });
    }

    let version = match lines.remove(0).sdp_type {
        SdpType::Version(v) => v,
        _ => {
            return Err(SdpParserError::Sequence {
                message: "first line needs to be version number".to_string(),
                line_number: 0,
            });
        }
    };
    let origin = match lines.remove(0).sdp_type {
        SdpType::Origin(v) => v,
        _ => {
            return Err(SdpParserError::Sequence {
                message: "second line needs to be origin".to_string(),
                line_number: 1,
            });
        }
    };
    let session = match lines.remove(0).sdp_type {
        SdpType::Session(v) => v,
        _ => {
            return Err(SdpParserError::Sequence {
                message: "third line needs to be session".to_string(),
                line_number: 2,
            });
        }
    };
    let mut sdp_session = SdpSession::new(version, origin, session);

    let _media_pos = lines.iter().position(|ref l| match l.sdp_type {
        SdpType::Media(_) => true,
        _ => false,
    });

    match _media_pos {
        Some(p) => {
            let mut media: Vec<_> = lines.drain(p..).collect();
            sdp_session.parse_session_vector(lines)?;
            sdp_session.extend_media(parse_media_vector(&mut media)?);
        }
        None => sdp_session.parse_session_vector(lines)?,
    };

    sanity_check_sdp_session(&sdp_session)?;
    Ok(sdp_session)
}

pub fn parse_sdp(sdp: &str, fail_on_warning: bool) -> Result<SdpSession, SdpParserError> {
    if sdp.is_empty() {
        return Err(SdpParserError::Line {
            error: SdpParserInternalError::Generic("empty SDP".to_string()),
            line: sdp.to_string(),
            line_number: 0,
        });
    }
    // see test_parse_sdp_minimal_sdp_successfully
    if sdp.len() < 51 {
        return Err(SdpParserError::Line {
            error: SdpParserInternalError::Generic("string too short to be valid SDP".to_string()),
            line: sdp.to_string(),
            line_number: 0,
        });
    }
    let lines = sdp.lines();
    let mut errors: Vec<SdpParserError> = Vec::new();
    let mut warnings: Vec<SdpParserError> = Vec::new();
    let mut sdp_lines: Vec<SdpLine> = Vec::new();
    for (line_number, line) in lines.enumerate() {
        let stripped_line = line.trim();
        if stripped_line.is_empty() {
            continue;
        }
        match parse_sdp_line(line, line_number) {
            Ok(n) => {
                sdp_lines.push(n);
            }
            Err(e) => {
                match e {
                    // TODO is this really a good way to accomplish this?
                    SdpParserError::Line {
                        error,
                        line,
                        line_number,
                    } => errors.push(SdpParserError::Line {
                        error,
                        line,
                        line_number,
                    }),
                    SdpParserError::Unsupported {
                        error,
                        line,
                        line_number,
                    } => {
                        warnings.push(SdpParserError::Unsupported {
                            error,
                            line,
                            line_number,
                        });
                    }
                    SdpParserError::Sequence {
                        message,
                        line_number,
                    } => errors.push(SdpParserError::Sequence {
                        message,
                        line_number,
                    }),
                }
            }
        };
    }

    if fail_on_warning && (!warnings.is_empty()) {
        return Err(warnings.remove(0));
    }

    // We just return the last of the errors here
    if let Some(e) = errors.pop() {
        return Err(e);
    };

    let mut session = parse_sdp_vector(&mut sdp_lines)?;
    session.warnings = warnings;

    for warning in &session.warnings {
        warn!("Warning: {}", &warning);
    }

    Ok(session)
}

#[cfg(test)]
mod tests {
    extern crate url;
    use super::*;
    use address::{Address, AddressType};
    use anonymizer::ToBytesVec;
    use media_type::create_dummy_media_section;
    use std::net::IpAddr;
    use std::net::Ipv4Addr;

    fn create_dummy_sdp_session() -> SdpSession {
        let origin = parse_origin("mozilla 506705521068071134 0 IN IP4 0.0.0.0");
        assert!(origin.is_ok());
        let connection = parse_connection("IN IP4 198.51.100.7");
        assert!(connection.is_ok());
        let mut sdp_session;
        if let SdpType::Origin(o) = origin.unwrap() {
            sdp_session = SdpSession::new(0, o, "-".to_string());

            if let Ok(SdpType::Connection(c)) = connection {
                sdp_session.connection = Some(c);
            } else {
                unreachable!();
            }
        } else {
            unreachable!();
        }
        sdp_session
    }

    #[test]
    fn test_session_works() -> Result<(), SdpParserInternalError> {
        parse_session("topic")?;
        Ok(())
    }

    #[test]
    fn test_version_works() -> Result<(), SdpParserInternalError> {
        parse_version("0")?;
        Ok(())
    }

    #[test]
    fn test_version_unsupported_input() {
        assert!(parse_version("1").is_err());
        assert!(parse_version("11").is_err());
        assert!(parse_version("a").is_err());
    }

    #[test]
    fn test_origin_works() -> Result<(), SdpParserInternalError> {
        parse_origin("mozilla 506705521068071134 0 IN IP4 0.0.0.0")?;
        parse_origin("mozilla 506705521068071134 0 IN IP6 2001:db8::1")?;
        Ok(())
    }

    #[test]
    fn test_origin_missing_username() {
        assert!(parse_origin("").is_err());
    }

    #[test]
    fn test_origin_missing_session_id() {
        assert!(parse_origin("mozilla ").is_err());
    }

    #[test]
    fn test_origin_missing_session_version() {
        assert!(parse_origin("mozilla 506705521068071134 ").is_err());
    }

    #[test]
    fn test_origin_missing_nettype() {
        assert!(parse_origin("mozilla 506705521068071134 0 ").is_err());
    }

    #[test]
    fn test_origin_unsupported_nettype() {
        assert!(parse_origin("mozilla 506705521068071134 0 UNSUPPORTED IP4 0.0.0.0").is_err());
    }

    #[test]
    fn test_origin_missing_addtype() {
        assert!(parse_origin("mozilla 506705521068071134 0 IN ").is_err());
    }

    #[test]
    fn test_origin_missing_ip_addr() {
        assert!(parse_origin("mozilla 506705521068071134 0 IN IP4 ").is_err());
    }

    #[test]
    fn test_origin_unsupported_addrtpe() {
        assert!(parse_origin("mozilla 506705521068071134 0 IN IP1 0.0.0.0").is_err());
    }

    #[test]
    fn test_origin_invalid_ip_addr() {
        assert!(parse_origin("mozilla 506705521068071134 0 IN IP4 1.1.1.256").is_err());
        assert!(parse_origin("mozilla 506705521068071134 0 IN IP6 ::g").is_err());
    }

    #[test]
    fn test_origin_addr_type_mismatch() {
        assert!(parse_origin("mozilla 506705521068071134 0 IN IP4 ::1").is_err());
    }

    #[test]
    fn connection_works() -> Result<(), SdpParserInternalError> {
        parse_connection("IN IP4 127.0.0.1")?;
        parse_connection("IN IP4 127.0.0.1/10/10")?;
        parse_connection("IN IP6 ::1")?;
        parse_connection("IN IP6 ::1/1/1")?;
        Ok(())
    }

    #[test]
    fn connection_lots_of_whitespace() -> Result<(), SdpParserInternalError> {
        parse_connection("IN   IP4   127.0.0.1")?;
        Ok(())
    }

    #[test]
    fn connection_wrong_amount_of_tokens() {
        assert!(parse_connection("IN IP4").is_err());
        assert!(parse_connection("IN IP4 0.0.0.0 foobar").is_err());
    }

    #[test]
    fn connection_unsupported_nettype() {
        assert!(parse_connection("UNSUPPORTED IP4 0.0.0.0").is_err());
    }

    #[test]
    fn connection_unsupported_addrtpe() {
        assert!(parse_connection("IN IP1 0.0.0.0").is_err());
    }

    #[test]
    fn connection_broken_ip_addr() {
        assert!(parse_connection("IN IP4 1.1.1.256").is_err());
        assert!(parse_connection("IN IP6 ::g").is_err());
    }

    #[test]
    fn connection_addr_type_mismatch() {
        assert!(parse_connection("IN IP4 ::1").is_err());
    }

    #[test]
    fn bandwidth_works() -> Result<(), SdpParserInternalError> {
        parse_bandwidth("AS:1")?;
        parse_bandwidth("CT:123")?;
        parse_bandwidth("TIAS:12345")?;
        Ok(())
    }

    #[test]
    fn bandwidth_wrong_amount_of_tokens() {
        assert!(parse_bandwidth("TIAS").is_err());
        assert!(parse_bandwidth("TIAS:12345:xyz").is_err());
    }

    #[test]
    fn bandwidth_unsupported_type() -> Result<(), SdpParserInternalError> {
        parse_bandwidth("UNSUPPORTED:12345")?;
        Ok(())
    }

    #[test]
    fn test_timing_works() -> Result<(), SdpParserInternalError> {
        parse_timing("0 0")?;
        Ok(())
    }

    #[test]
    fn test_timing_non_numeric_tokens() {
        assert!(parse_timing("a 0").is_err());
        assert!(parse_timing("0 a").is_err());
    }

    #[test]
    fn test_timing_wrong_amount_of_tokens() {
        assert!(parse_timing("0").is_err());
        assert!(parse_timing("0 0 0").is_err());
    }

    #[test]
    fn test_parse_sdp_line_works() -> Result<(), SdpParserError> {
        parse_sdp_line("v=0", 0)?;
        parse_sdp_line("s=somesession", 0)?;
        Ok(())
    }

    #[test]
    fn test_parse_sdp_line_empty_line() {
        assert!(parse_sdp_line("", 0).is_err());
    }

    #[test]
    fn test_parse_sdp_line_unsupported_types() {
        assert!(parse_sdp_line("e=foobar", 0).is_err());
        assert!(parse_sdp_line("i=foobar", 0).is_err());
        assert!(parse_sdp_line("k=foobar", 0).is_err());
        assert!(parse_sdp_line("p=foobar", 0).is_err());
        assert!(parse_sdp_line("r=foobar", 0).is_err());
        assert!(parse_sdp_line("u=foobar", 0).is_err());
        assert!(parse_sdp_line("z=foobar", 0).is_err());
    }

    #[test]
    fn test_parse_sdp_line_unknown_key() {
        assert!(parse_sdp_line("y=foobar", 0).is_err());
    }

    #[test]
    fn test_parse_sdp_line_too_long_type() {
        assert!(parse_sdp_line("ab=foobar", 0).is_err());
    }

    #[test]
    fn test_parse_sdp_line_without_equal() {
        assert!(parse_sdp_line("abcd", 0).is_err());
        assert!(parse_sdp_line("ab cd", 0).is_err());
    }

    #[test]
    fn test_parse_sdp_line_empty_value() {
        assert!(parse_sdp_line("v=", 0).is_err());
        assert!(parse_sdp_line("o=", 0).is_err());
    }

    #[test]
    fn test_parse_sdp_line_empty_name() {
        assert!(parse_sdp_line("=abc", 0).is_err());
    }

    #[test]
    fn test_parse_sdp_line_valid_a_line() -> Result<(), SdpParserError> {
        parse_sdp_line("a=rtpmap:8 PCMA/8000", 0)?;
        Ok(())
    }

    #[test]
    fn test_parse_sdp_line_invalid_a_line() {
        assert!(parse_sdp_line("a=rtpmap:200 PCMA/8000", 0).is_err());
    }

    #[test]
    fn test_add_attribute() -> Result<(), SdpParserInternalError> {
        let mut sdp_session = create_dummy_sdp_session();

        sdp_session.add_attribute(SdpAttribute::Sendrecv)?;
        assert!(sdp_session.add_attribute(SdpAttribute::BundleOnly).is_err());
        assert_eq!(sdp_session.attribute.len(), 1);
        Ok(())
    }

    #[test]
    fn test_sanity_check_sdp_session_timing() -> Result<(), SdpParserError> {
        let mut sdp_session = create_dummy_sdp_session();
        sdp_session.extend_media(vec![create_dummy_media_section()]);

        assert!(sanity_check_sdp_session(&sdp_session).is_err());

        let t = SdpTiming { start: 0, stop: 0 };
        sdp_session.set_timing(t);

        sanity_check_sdp_session(&sdp_session)?;
        Ok(())
    }

    #[test]
    fn test_sanity_check_sdp_session_media() -> Result<(), SdpParserError> {
        let mut sdp_session = create_dummy_sdp_session();
        let t = SdpTiming { start: 0, stop: 0 };
        sdp_session.set_timing(t);

        sanity_check_sdp_session(&sdp_session)?;

        sdp_session.extend_media(vec![create_dummy_media_section()]);

        sanity_check_sdp_session(&sdp_session)?;
        Ok(())
    }

    #[test]
    fn test_sanity_check_sdp_connection() -> Result<(), SdpParserInternalError> {
        let origin = parse_origin("mozilla 506705521068071134 0 IN IP4 0.0.0.0")?;
        let mut sdp_session;
        if let SdpType::Origin(o) = origin {
            sdp_session = SdpSession::new(0, o, "-".to_string());
        } else {
            unreachable!();
        }
        let t = SdpTiming { start: 0, stop: 0 };
        sdp_session.set_timing(t);

        assert!(sanity_check_sdp_session(&sdp_session).is_ok());

        // the dummy media section doesn't contain a connection
        sdp_session.extend_media(vec![create_dummy_media_section()]);

        assert!(sanity_check_sdp_session(&sdp_session).is_err());

        let connection = parse_connection("IN IP6 ::1")?;
        if let SdpType::Connection(c) = connection {
            sdp_session.connection = Some(c);
        } else {
            unreachable!();
        }

        assert!(sanity_check_sdp_session(&sdp_session).is_ok());

        let mut second_media = create_dummy_media_section();
        let mconnection = parse_connection("IN IP4 0.0.0.0")?;
        if let SdpType::Connection(c) = mconnection {
            second_media.set_connection(c)?;
        } else {
            unreachable!();
        }
        sdp_session.extend_media(vec![second_media]);
        assert!(sdp_session.media.len() == 2);

        assert!(sanity_check_sdp_session(&sdp_session).is_ok());
        Ok(())
    }

    #[test]
    fn test_sanity_check_sdp_session_extmap() -> Result<(), SdpParserInternalError> {
        let mut sdp_session = create_dummy_sdp_session();
        let t = SdpTiming { start: 0, stop: 0 };
        sdp_session.set_timing(t);
        sdp_session.extend_media(vec![create_dummy_media_section()]);

        let attribute =
            parse_attribute("extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")?;
        if let SdpType::Attribute(a) = attribute {
            sdp_session.add_attribute(a)?;
        } else {
            unreachable!();
        }
        assert!(sdp_session
            .get_attribute(SdpAttributeType::Extmap)
            .is_some());

        assert!(sanity_check_sdp_session(&sdp_session).is_ok());

        let mut second_media = create_dummy_media_section();
        let mattribute =
            parse_attribute("extmap:1/sendonly urn:ietf:params:rtp-hdrext:ssrc-audio-level")?;
        if let SdpType::Attribute(ma) = mattribute {
            second_media.add_attribute(ma)?;
        } else {
            unreachable!();
        }
        assert!(second_media
            .get_attribute(SdpAttributeType::Extmap)
            .is_some());

        sdp_session.extend_media(vec![second_media]);
        assert!(sdp_session.media.len() == 2);

        assert!(sanity_check_sdp_session(&sdp_session).is_err());

        sdp_session.attribute = Vec::new();

        assert!(sanity_check_sdp_session(&sdp_session).is_ok());
        Ok(())
    }

    #[test]
    fn test_sanity_check_sdp_session_simulcast() -> Result<(), SdpParserError> {
        let mut sdp_session = create_dummy_sdp_session();
        let t = SdpTiming { start: 0, stop: 0 };
        sdp_session.set_timing(t);
        sdp_session.extend_media(vec![create_dummy_media_section()]);

        sanity_check_sdp_session(&sdp_session)?;
        Ok(())
    }

    #[test]
    fn test_parse_sdp_zero_length_string_fails() {
        assert!(parse_sdp("", true).is_err());
    }

    #[test]
    fn test_parse_sdp_to_short_string() {
        assert!(parse_sdp("fooooobarrrr", true).is_err());
    }

    #[test]
    fn test_parse_sdp_minimal_sdp_successfully() -> Result<(), SdpParserError> {
        parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP6 ::1\r\n
s=-\r\n
c=IN IP6 ::1\r\n
t=0 0\r\n",
            true,
        )?;
        Ok(())
    }

    #[test]
    fn test_parse_sdp_too_short() {
        assert!(parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n",
            true
        )
        .is_err());
    }

    #[test]
    fn test_parse_sdp_line_error() {
        assert!(parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
t=0 foobar\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n",
            true
        )
        .is_err());
    }

    #[test]
    fn test_parse_sdp_unsupported_error() {
        assert!(parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
t=0 0\r\n
m=foobar 0 UDP/TLS/RTP/SAVPF 0\r\n",
            true
        )
        .is_err());
    }

    #[test]
    fn test_parse_sdp_unsupported_warning() -> Result<(), SdpParserError> {
        parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
c=IN IP4 198.51.100.7\r\n
t=0 0\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n
a=unsupported\r\n",
            false,
        )?;
        Ok(())
    }

    #[test]
    fn test_parse_sdp_sequence_error() {
        assert!(parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
t=0 0\r\n
a=bundle-only\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n",
            true
        )
        .is_err());
    }

    #[test]
    fn test_parse_sdp_integer_error() {
        assert!(parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
t=0 0\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n
a=rtcp:34er21\r\n",
            true
        )
        .is_err());
    }

    #[test]
    fn test_parse_sdp_ipaddr_error() {
        assert!(parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.a.b.0\r\n
s=-\r\n
t=0 0\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n",
            true
        )
        .is_err());
    }

    #[test]
    fn test_parse_sdp_invalid_session_attribute() {
        assert!(parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.a.b.0\r\n
s=-\r\n
t=0 0\r\n
a=bundle-only\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n",
            true
        )
        .is_err());
    }

    #[test]
    fn test_parse_sdp_invalid_media_attribute() {
        assert!(parse_sdp(
            "v=0\r\n
o=- 0 0 IN IP4 0.a.b.0\r\n
s=-\r\n
t=0 0\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n
a=ice-lite\r\n",
            true
        )
        .is_err());
    }

    #[test]
    fn test_mask_origin() {
        let mut anon = StatefulSdpAnonymizer::new();
        if let SdpType::Origin(origin_1) =
            parse_origin("mozilla 506705521068071134 0 IN IP4 0.0.0.0").unwrap()
        {
            for _ in 0..2 {
                let masked = origin_1.masked_clone(&mut anon);
                assert_eq!(masked.username, "origin-user-00000001");
                assert_eq!(
                    masked.unicast_addr,
                    ExplicitlyTypedAddress::Ip(IpAddr::V4(Ipv4Addr::from(1)))
                );
            }
        } else {
            unreachable!();
        }
    }

    #[test]
    fn test_mask_sdp() {
        let mut anon = StatefulSdpAnonymizer::new();
        let sdp = parse_sdp(
            "v=0\r\n
        o=ausername 4294967296 2 IN IP4 127.0.0.1\r\n
        s=SIP Call\r\n
        c=IN IP4 198.51.100.7/51\r\n
        a=ice-pwd:12340\r\n
        a=ice-ufrag:4a799b2e\r\n
        a=fingerprint:sha-1 CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC\r\n
        t=0 0\r\n
        m=video 56436 RTP/SAVPF 120\r\n
        a=candidate:77142221 1 udp 2113937151 192.168.137.1 54081 typ host\r\n
        a=remote-candidates:0 10.0.0.1 5555\r\n
        a=rtpmap:120 VP8/90000\r\n",
            true,
        )
        .unwrap();
        let mut masked = sdp.masked_clone(&mut anon);
        assert_eq!(masked.origin.username, "origin-user-00000001");
        assert_eq!(
            masked.origin.unicast_addr,
            ExplicitlyTypedAddress::Ip(IpAddr::V4(Ipv4Addr::from(1)))
        );
        assert_eq!(
            masked.connection.unwrap().address,
            ExplicitlyTypedAddress::Ip(IpAddr::V4(Ipv4Addr::from(2)))
        );
        let mut attributes = masked.attribute;
        for m in &mut masked.media {
            for attribute in m.get_attributes() {
                attributes.push(attribute.clone());
            }
        }
        for attribute in attributes {
            match attribute {
                SdpAttribute::Candidate(c) => {
                    assert_eq!(c.address, Address::Ip(IpAddr::V4(Ipv4Addr::from(3))));
                    assert_eq!(c.port, 1);
                }
                SdpAttribute::Fingerprint(f) => {
                    assert_eq!(f.fingerprint, 1u64.to_byte_vec());
                }
                SdpAttribute::IcePwd(p) => {
                    assert_eq!(p, "ice-password-00000001");
                }
                SdpAttribute::IceUfrag(u) => {
                    assert_eq!(u, "ice-user-00000001");
                }
                SdpAttribute::RemoteCandidate(r) => {
                    assert_eq!(r.address, Address::Ip(IpAddr::V4(Ipv4Addr::from(4))));
                    assert_eq!(r.port, 2);
                }
                _ => {}
            }
        }
    }

    #[test]
    fn test_parse_session_vector() -> Result<(), SdpParserError> {
        let mut sdp_session = create_dummy_sdp_session();
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("a=sendrecv", 1)?);
        sdp_session.parse_session_vector(&mut lines)?;
        assert_eq!(sdp_session.attribute.len(), 1);
        Ok(())
    }

    #[test]
    fn test_parse_session_vector_non_session_attribute() -> Result<(), SdpParserError> {
        let mut sdp_session = create_dummy_sdp_session();
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("a=bundle-only", 2)?);
        assert!(sdp_session.parse_session_vector(&mut lines).is_err());
        assert_eq!(sdp_session.attribute.len(), 0);
        Ok(())
    }

    #[test]
    fn test_parse_session_vector_version_repeated() -> Result<(), SdpParserError> {
        let mut sdp_session = create_dummy_sdp_session();
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("v=0", 3)?);
        assert!(sdp_session.parse_session_vector(&mut lines).is_err());
        Ok(())
    }

    #[test]
    fn test_parse_session_vector_contains_media_type() -> Result<(), SdpParserError> {
        let mut sdp_session = create_dummy_sdp_session();
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("m=audio 0 UDP/TLS/RTP/SAVPF 0", 4)?);
        assert!(sdp_session.parse_session_vector(&mut lines).is_err());
        Ok(())
    }

    #[test]
    fn test_parse_sdp_vector_no_media_section() -> Result<(), SdpParserError> {
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("v=0", 1)?);
        lines.push(parse_sdp_line(
            "o=ausername 4294967296 2 IN IP4 127.0.0.1",
            1,
        )?);
        lines.push(parse_sdp_line("s=SIP Call", 1)?);
        lines.push(parse_sdp_line("t=0 0", 1)?);
        lines.push(parse_sdp_line("c=IN IP6 ::1", 1)?);
        assert!(parse_sdp_vector(&mut lines).is_ok());
        Ok(())
    }

    #[test]
    fn test_parse_sdp_vector_with_media_section() -> Result<(), SdpParserError> {
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("v=0", 1)?);
        lines.push(parse_sdp_line(
            "o=ausername 4294967296 2 IN IP4 127.0.0.1",
            1,
        )?);
        lines.push(parse_sdp_line("s=SIP Call", 1)?);
        lines.push(parse_sdp_line("t=0 0", 1)?);
        lines.push(parse_sdp_line("m=video 56436 RTP/SAVPF 120", 1)?);
        lines.push(parse_sdp_line("c=IN IP6 ::1", 1)?);
        assert!(parse_sdp_vector(&mut lines).is_ok());
        Ok(())
    }

    #[test]
    fn test_parse_sdp_vector_too_short() -> Result<(), SdpParserError> {
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("v=0", 1)?);
        assert!(parse_sdp_vector(&mut lines).is_err());
        Ok(())
    }

    #[test]
    fn test_parse_sdp_vector_missing_version() -> Result<(), SdpParserError> {
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line(
            "o=ausername 4294967296 2 IN IP4 127.0.0.1",
            1,
        )?);
        for _ in 0..3 {
            lines.push(parse_sdp_line("a=sendrecv", 1)?);
        }
        assert!(parse_sdp_vector(&mut lines).is_err());
        Ok(())
    }

    #[test]
    fn test_parse_sdp_vector_missing_origin() -> Result<(), SdpParserError> {
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("v=0", 1)?);
        for _ in 0..3 {
            lines.push(parse_sdp_line("a=sendrecv", 1)?);
        }
        assert!(parse_sdp_vector(&mut lines).is_err());
        Ok(())
    }

    #[test]
    fn test_parse_sdp_vector_missing_session() -> Result<(), SdpParserError> {
        let mut lines: Vec<SdpLine> = Vec::new();
        lines.push(parse_sdp_line("v=0", 1)?);
        lines.push(parse_sdp_line(
            "o=ausername 4294967296 2 IN IP4 127.0.0.1",
            1,
        )?);
        for _ in 0..2 {
            lines.push(parse_sdp_line("a=sendrecv", 1)?);
        }
        assert!(parse_sdp_vector(&mut lines).is_err());
        Ok(())
    }

    #[test]
    fn test_session_add_media_works() -> Result<(), SdpParserError> {
        let mut sdp_session = create_dummy_sdp_session();
        assert!(sdp_session
            .add_media(
                SdpMediaValue::Audio,
                SdpAttribute::Sendrecv,
                99,
                SdpProtocolValue::RtpSavpf,
                ExplicitlyTypedAddress::from(Ipv4Addr::new(127, 0, 0, 1))
            )
            .is_ok());
        assert!(sdp_session.get_connection().is_some());
        assert_eq!(sdp_session.attribute.len(), 0);
        assert_eq!(sdp_session.media.len(), 1);
        assert_eq!(sdp_session.media[0].get_attributes().len(), 1);
        assert!(sdp_session.media[0]
            .get_attribute(SdpAttributeType::Sendrecv)
            .is_some());
        Ok(())
    }

    #[test]
    fn test_session_add_media_invalid_attribute_fails() -> Result<(), SdpParserInternalError> {
        let mut sdp_session = create_dummy_sdp_session();
        assert!(sdp_session
            .add_media(
                SdpMediaValue::Audio,
                SdpAttribute::IceLite,
                99,
                SdpProtocolValue::RtpSavpf,
                ExplicitlyTypedAddress::try_from((AddressType::IpV4, "127.0.0.1"))?
            )
            .is_err());
        Ok(())
    }
}
