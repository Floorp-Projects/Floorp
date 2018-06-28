#![cfg_attr(feature="clippy", feature(plugin))]

#[cfg(feature="serialize")]
#[macro_use]
extern crate serde_derive;
#[cfg(feature="serialize")]
extern crate serde;

use std::net::IpAddr;
use std::str::FromStr;
use std::fmt;

pub mod attribute_type;
pub mod error;
pub mod media_type;
pub mod network;
pub mod unsupported_types;

use attribute_type::{SdpAttribute, SdpSingleDirection, SdpAttributeType, parse_attribute,
                     SdpAttributeSimulcastVersion, SdpAttributeRid};
use error::{SdpParserInternalError, SdpParserError};
use media_type::{SdpMedia, SdpMediaLine, parse_media, parse_media_vector, SdpProtocolValue,
                 SdpMediaValue, SdpFormatList};
use network::{parse_addrtype, parse_nettype, parse_unicast_addr};
use unsupported_types::{parse_email, parse_information, parse_key, parse_phone, parse_repeat,
                        parse_uri, parse_zone};

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpBandwidth {
    As(u32),
    Ct(u32),
    Tias(u32),
    Unknown(String, u32),
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpConnection {
    pub addr: IpAddr,
    pub ttl: Option<u8>,
    pub amount: Option<u32>,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpOrigin {
    pub username: String,
    pub session_id: u64,
    pub session_version: u64,
    pub unicast_addr: IpAddr,
}

impl fmt::Display for SdpOrigin {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f,
               "origin: {}, {}, {}, {}",
               self.username,
               self.session_id,
               self.session_version,
               self.unicast_addr)
    }
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpTiming {
    pub start: u64,
    pub stop: u64,
}

#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpType {
    Attribute(SdpAttribute),
    Bandwidth(SdpBandwidth),
    Connection(SdpConnection),
    Email(String),
    Information(String),
    Key(String),
    Media(SdpMediaLine),
    Phone(String),
    Origin(SdpOrigin),
    Repeat(String),
    Session(String),
    Timing(SdpTiming),
    Uri(String),
    Version(u64),
    Zone(String),
}

#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpLine {
    pub line_number: usize,
    pub sdp_type: SdpType,
}

#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpSession {
    pub version: u64,
    pub origin: SdpOrigin,
    pub session: String,
    pub connection: Option<SdpConnection>,
    pub bandwidth: Vec<SdpBandwidth>,
    pub timing: Option<SdpTiming>,
    pub attribute: Vec<SdpAttribute>,
    pub media: Vec<SdpMedia>,
    pub warnings: Vec<SdpParserError>
    // unsupported values:
    // information: Option<String>,
    // uri: Option<String>,
    // email: Option<String>,
    // phone: Option<String>,
    // repeat: Option<String>,
    // zone: Option<String>,
    // key: Option<String>,
}

impl SdpSession {
    pub fn new(version: u64, origin: SdpOrigin, session: String) -> SdpSession {
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

    pub fn get_session(&self) -> &String {
        &self.session
    }

    pub fn get_connection(&self) -> &Option<SdpConnection> {
        &self.connection
    }

    pub fn set_connection(&mut self, c: &SdpConnection) {
        self.connection = Some(c.clone())
    }

    pub fn add_bandwidth(&mut self, b: &SdpBandwidth) {
        self.bandwidth.push(b.clone())
    }

    pub fn set_timing(&mut self, t: &SdpTiming) {
        self.timing = Some(t.clone())
    }

    pub fn add_attribute(&mut self, a: &SdpAttribute) -> Result<(), SdpParserInternalError> {
        if !a.allowed_at_session_level() {
            return Err(SdpParserInternalError::Generic(format!("{} not allowed at session level",
                                                               a)));
        };
        Ok(self.attribute.push(a.clone()))
    }

    pub fn extend_media(&mut self, v: Vec<SdpMedia>) {
        self.media.extend(v)
    }

    pub fn has_timing(&self) -> bool {
        self.timing.is_some()
    }

    pub fn has_attributes(&self) -> bool {
        !self.attribute.is_empty()
    }

    pub fn get_attribute(&self, t: SdpAttributeType) -> Option<&SdpAttribute> {
       self.attribute.iter().filter(|a| SdpAttributeType::from(*a) == t).next()
    }

    pub fn has_media(&self) -> bool {
        !self.media.is_empty()
    }

    pub fn add_media(&mut self, media_type: SdpMediaValue, direction: SdpAttribute, port: u32,
                     protocol: SdpProtocolValue, addr: String)
                     -> Result<(),SdpParserInternalError> {
       let mut media = SdpMedia::new(SdpMediaLine {
           media: media_type,
           port,
           port_count: 1,
           proto: protocol,
           formats: SdpFormatList::Integers(Vec::new()),
       });

       media.add_attribute(&direction)?;

       media.set_connection(&SdpConnection {
           addr: IpAddr::from_str(addr.as_str())?,
           ttl: None,
           amount: None,
       })?;

       self.media.push(media);

       Ok(())
    }
}

fn parse_session(value: &str) -> Result<SdpType, SdpParserInternalError> {
    println!("session: {}", value);
    Ok(SdpType::Session(String::from(value)))
}

#[test]
fn test_session_works() {
    assert!(parse_session("topic").is_ok());
}


fn parse_version(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let ver = value.parse::<u64>()?;
    if ver != 0 {
        return Err(SdpParserInternalError::Generic(format!("version type contains unsupported value {}",
                                                           ver)));
    };
    println!("version: {}", ver);
    Ok(SdpType::Version(ver))
}

#[test]
fn test_version_works() {
    assert!(parse_version("0").is_ok());
}

#[test]
fn test_version_unsupported_input() {
    assert!(parse_version("1").is_err());
    assert!(parse_version("11").is_err());
    assert!(parse_version("a").is_err());
}

fn parse_origin(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let mut tokens = value.split_whitespace();
    let username = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Origin type is missing username token"
                                                           .to_string()))
        }
        Some(x) => x,
    };
    let session_id = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Origin type is missing session ID token"
                                                           .to_string()))
        }
        Some(x) => x.parse::<u64>()?,
    };
    let session_version = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                           "Origin type is missing session version token"
                           .to_string()))
        }
        Some(x) => x.parse::<u64>()?,
    };
    match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                           "Origin type is missing session version token".to_string(),
                       ))
        }
        Some(x) => parse_nettype(x)?,
    };
    let addrtype = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Origin type is missing address type token"
                                                           .to_string()))
        }
        Some(x) => parse_addrtype(x)?,
    };
    let unicast_addr = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Origin type is missing IP address token"
                                                           .to_string()))
        }
        Some(x) => parse_unicast_addr(x)?,
    };
    if !addrtype.same_protocol(&unicast_addr) {
        return Err(SdpParserInternalError::Generic("Origin addrtype does not match address."
                                                       .to_string()));
    }
    let o = SdpOrigin {
        username: String::from(username),
        session_id,
        session_version,
        unicast_addr,
    };
    println!("{}", o);
    Ok(SdpType::Origin(o))
}

#[test]
fn test_origin_works() {
    assert!(parse_origin("mozilla 506705521068071134 0 IN IP4 0.0.0.0").is_ok());
    assert!(parse_origin("mozilla 506705521068071134 0 IN IP6 ::1").is_ok());
}

#[test]
fn test_origin_wrong_amount_of_tokens() {
    assert!(parse_origin("a b c d e").is_err());
    assert!(parse_origin("a b c d e f g").is_err());
}

#[test]
fn test_origin_unsupported_nettype() {
    assert!(parse_origin("mozilla 506705521068071134 0 UNSUPPORTED IP4 0.0.0.0").is_err());
}

#[test]
fn test_origin_unsupported_addrtpe() {
    assert!(parse_origin("mozilla 506705521068071134 0 IN IP1 0.0.0.0").is_err());
}

#[test]
fn test_origin_broken_ip_addr() {
    assert!(parse_origin("mozilla 506705521068071134 0 IN IP4 1.1.1.256").is_err());
    assert!(parse_origin("mozilla 506705521068071134 0 IN IP6 ::g").is_err());
}

#[test]
fn test_origin_addr_type_mismatch() {
    assert!(parse_origin("mozilla 506705521068071134 0 IN IP4 ::1").is_err());
}

fn parse_connection(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let cv: Vec<&str> = value.split_whitespace().collect();
    if cv.len() != 3 {
        return Err(SdpParserInternalError::Generic("connection attribute must have three tokens"
                                                       .to_string()));
    }
    parse_nettype(cv[0])?;
    let addrtype = parse_addrtype(cv[1])?;
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
    let addr = parse_unicast_addr(addr_token)?;
    if !addrtype.same_protocol(&addr) {
        return Err(SdpParserInternalError::Generic("connection addrtype does not match address."
                                                       .to_string()));
    }
    let c = SdpConnection { addr, ttl, amount };
    println!("connection: {}", c.addr);
    Ok(SdpType::Connection(c))
}

#[test]
fn connection_works() {
    assert!(parse_connection("IN IP4 127.0.0.1").is_ok());
    assert!(parse_connection("IN IP4 127.0.0.1/10/10").is_ok());
}

#[test]
fn connection_lots_of_whitespace() {
    assert!(parse_connection("IN   IP4   127.0.0.1").is_ok());
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

fn parse_bandwidth(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let bv: Vec<&str> = value.split(':').collect();
    if bv.len() != 2 {
        return Err(SdpParserInternalError::Generic("bandwidth attribute must have two tokens"
                                                       .to_string()));
    }
    let bandwidth = bv[1].parse::<u32>()?;
    let bw = match bv[0].to_uppercase().as_ref() {
        "AS" => SdpBandwidth::As(bandwidth),
        "CT" => SdpBandwidth::Ct(bandwidth),
        "TIAS" => SdpBandwidth::Tias(bandwidth),
        _ => SdpBandwidth::Unknown(String::from(bv[0]), bandwidth),
    };
    println!("bandwidth: {}, {}", bv[0], bandwidth);
    Ok(SdpType::Bandwidth(bw))
}

#[test]
fn bandwidth_works() {
    assert!(parse_bandwidth("AS:1").is_ok());
    assert!(parse_bandwidth("CT:123").is_ok());
    assert!(parse_bandwidth("TIAS:12345").is_ok());
}

#[test]
fn bandwidth_wrong_amount_of_tokens() {
    assert!(parse_bandwidth("TIAS").is_err());
    assert!(parse_bandwidth("TIAS:12345:xyz").is_err());
}

#[test]
fn bandwidth_unsupported_type() {
    assert!(parse_bandwidth("UNSUPPORTED:12345").is_ok());
}

fn parse_timing(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let tv: Vec<&str> = value.split_whitespace().collect();
    if tv.len() != 2 {
        return Err(SdpParserInternalError::Generic("timing attribute must have two tokens"
                                                       .to_string()));
    }
    let start = tv[0].parse::<u64>()?;
    let stop = tv[1].parse::<u64>()?;
    let t = SdpTiming { start, stop };
    println!("timing: {}, {}", t.start, t.stop);
    Ok(SdpType::Timing(t))
}

#[test]
fn test_timing_works() {
    assert!(parse_timing("0 0").is_ok());
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

fn parse_sdp_line(line: &str, line_number: usize) -> Result<SdpLine, SdpParserError> {
    if line.find('=') == None {
        return Err(SdpParserError::Line {
                       error: SdpParserInternalError::Generic("missing = character in line"
                                                                  .to_string()),
                       line: line.to_string(),
                       line_number: line_number,
                   });
    }
    let mut splitted_line = line.splitn(2, '=');
    let line_type = match splitted_line.next() {
        None => {
            return Err(SdpParserError::Line {
                           error: SdpParserInternalError::Generic("missing type".to_string()),
                           line: line.to_string(),
                           line_number: line_number,
                       })
        }
        Some(t) => {
            let trimmed = t.trim();
            if trimmed.len() > 1 {
                return Err(SdpParserError::Line {
                               error: SdpParserInternalError::Generic("type too long".to_string()),
                               line: line.to_string(),
                               line_number: line_number,
                           });
            }
            if trimmed.is_empty() {
                return Err(SdpParserError::Line {
                               error: SdpParserInternalError::Generic("type is empty".to_string()),
                               line: line.to_string(),
                               line_number: line_number,
                           });
            }
            trimmed
        }
    };
    let line_value = match splitted_line.next() {
        None => {
            return Err(SdpParserError::Line {
                           error: SdpParserInternalError::Generic("missing value".to_string()),
                           line: line.to_string(),
                           line_number: line_number,
                       })
        }
        Some(v) => {
            let trimmed = v.trim();
            if trimmed.is_empty() {
                return Err(SdpParserError::Line {
                               error: SdpParserInternalError::Generic("value is empty".to_string()),
                               line: line.to_string(),
                               line_number: line_number,
                           });
            }
            trimmed
        }
    };
    match line_type.to_lowercase().as_ref() {
            "a" => parse_attribute(line_value),
            "b" => parse_bandwidth(line_value),
            "c" => parse_connection(line_value),
            "e" => parse_email(line_value),
            "i" => parse_information(line_value),
            "k" => parse_key(line_value),
            "m" => parse_media(line_value),
            "o" => parse_origin(line_value),
            "p" => parse_phone(line_value),
            "r" => parse_repeat(line_value),
            "s" => parse_session(line_value),
            "t" => parse_timing(line_value),
            "u" => parse_uri(line_value),
            "v" => parse_version(line_value),
            "z" => parse_zone(line_value),
            _ => Err(SdpParserInternalError::Generic("unknown sdp type".to_string())),
        }
        .map(|sdp_type| {
                 SdpLine {
                     line_number,
                     sdp_type,
                 }
             })
        .map_err(|e| match e {
                     SdpParserInternalError::Generic(..) |
                     SdpParserInternalError::Integer(..) |
                     SdpParserInternalError::Float(..) |
                     SdpParserInternalError::Address(..) => {
                         SdpParserError::Line {
                             error: e,
                             line: line.to_string(),
                             line_number: line_number,
                         }
                     }
                     SdpParserInternalError::Unsupported(..) => {
                         SdpParserError::Unsupported {
                             error: e,
                             line: line.to_string(),
                             line_number: line_number,
                         }
                     }
                 })
}

#[test]
fn test_parse_sdp_line_works() {
    assert!(parse_sdp_line("v=0", 0).is_ok());
    assert!(parse_sdp_line("s=somesession", 0).is_ok());
}

#[test]
fn test_parse_sdp_line_empty_line() {
    assert!(parse_sdp_line("", 0).is_err());
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
    assert!(parse_sdp_line("s=", 0).is_err());
}

#[test]
fn test_parse_sdp_line_empty_name() {
    assert!(parse_sdp_line("=abc", 0).is_err());
}

#[test]
fn test_parse_sdp_line_valid_a_line() {
    assert!(parse_sdp_line("a=rtpmap:8 PCMA/8000", 0).is_ok());
}

#[test]
fn test_parse_sdp_line_invalid_a_line() {
    assert!(parse_sdp_line("a=rtpmap:200 PCMA/8000", 0).is_err());
}

fn sanity_check_sdp_session(session: &SdpSession) -> Result<(), SdpParserError> {
    let make_error = |x: &str| SdpParserError::Sequence {
        message: x.to_string(),
        line_number: 0,
    };

    if !session.has_timing() {
        return Err(SdpParserError::Sequence {
                       message: "Missing timing type".to_string(),
                       line_number: 0,
                   });
    }

    if !session.has_media() {
        return Err(SdpParserError::Sequence {
                       message: "Missing media setion".to_string(),
                       line_number: 0,
                   });
    }

    if session.get_connection().is_none() {
        for msection in &session.media {
            if !msection.has_connection() {
                return Err(SdpParserError::Sequence {
                    message: "Each media section must define a connection
                              if it is not defined on session level".to_string(),
                    line_number: 0,
                });
            }
        }
    }

    // Check that extmaps are not defined on session and media level
    if session.get_attribute(SdpAttributeType::Extmap).is_some() {
        for msection in &session.media {
            if msection.get_attribute(SdpAttributeType::Extmap).is_some() {
                return Err(SdpParserError::Sequence {
                               message: "Extmap can't be define at session and media level"
                                   .to_string(),
                               line_number: 0,
                           });
            }
        }
    }

    for msection in &session.media {
        if msection.get_attribute(SdpAttributeType::Sendonly).is_some() {
            if let Some(&SdpAttribute::Simulcast(ref x)) = msection.get_attribute(SdpAttributeType::Simulcast) {
                if x.receive.len() > 0 {
                    return Err(SdpParserError::Sequence {
                        message: "Simulcast can't define receive parameters for sendonly".to_string(),
                        line_number: 0,
                    });
                }
            }
        }
        if msection.get_attribute(SdpAttributeType::Recvonly).is_some() {
            if let Some(&SdpAttribute::Simulcast(ref x)) = msection.get_attribute(SdpAttributeType::Simulcast) {
                if x.send.len() > 0 {
                    return Err(SdpParserError::Sequence {
                        message: "Simulcast can't define send parameters for recvonly".to_string(),
                        line_number: 0,
                    });
                }
            }
        }

        let rids:Vec<&SdpAttributeRid> = msection.get_attributes().iter().filter_map(|attr| {
                                                      match attr {
                                                          &SdpAttribute::Rid(ref rid) => Some(rid),
                                                          _ => None,
                                                         }
                                                  }).collect();
        let recv_rids:Vec<&str> = rids.iter().filter_map(|rid| {
          match rid.direction {
              SdpSingleDirection::Recv => Some(rid.id.as_str()),
              _ => None,
          }
        }).collect();


        for rid_format in rids.iter().flat_map(|rid| &rid.formats) {
            match msection.get_formats() {
                &SdpFormatList::Integers(ref int_fmt) => {
                    if !int_fmt.contains(&(*rid_format as u32))  {
                        return Err(make_error("Rid pts must be declared in the media section"));
                    }
                },
                &SdpFormatList::Strings(ref str_fmt) => {
                    if !str_fmt.contains(&rid_format.to_string())  {
                        return Err(make_error("Rid pts must be declared in the media section"));
                    }
                }
            }
        }

        if let Some(&SdpAttribute::Simulcast(ref simulcast)) =
                                            msection.get_attribute(SdpAttributeType::Simulcast) {
            // This is already a closure as the next Bug 1432931 will require the same procedure
            let check_defined_rids = |simulcast_version_list: &Vec<SdpAttributeSimulcastVersion>,
                                      rid_ids: &[&str]| -> Result<(),SdpParserError> {
                for simulcast_rid in simulcast_version_list.iter().flat_map(|x| &x.ids) {
                    if !rid_ids.contains(&simulcast_rid.id.as_str()) {
                        return Err(make_error(
                                       "Simulcast RIDs must be defined in any rid attribute"));
                    }
                }
                Ok(())
            };

            check_defined_rids(&simulcast.receive, &recv_rids)?
        }
    }

    Ok(())
}

#[cfg(test)]
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
            panic!("Sdp type is not Connection")
        }
    } else {
        panic!("SdpType is not Origin");
    }
    sdp_session
}

#[cfg(test)]
use media_type::create_dummy_media_section;

#[test]
fn test_sanity_check_sdp_session_timing() {
    let mut sdp_session = create_dummy_sdp_session();
    sdp_session.extend_media(vec![create_dummy_media_section()]);

    assert!(sanity_check_sdp_session(&sdp_session).is_err());

    let t = SdpTiming { start: 0, stop: 0 };
    sdp_session.set_timing(&t);

    assert!(sanity_check_sdp_session(&sdp_session).is_ok());
}

#[test]
fn test_sanity_check_sdp_session_media() {
    let mut sdp_session = create_dummy_sdp_session();
    let t = SdpTiming { start: 0, stop: 0 };
    sdp_session.set_timing(&t);

    assert!(sanity_check_sdp_session(&sdp_session).is_err());

    sdp_session.extend_media(vec![create_dummy_media_section()]);

    assert!(sanity_check_sdp_session(&sdp_session).is_ok());
}

#[test]
fn test_sanity_check_sdp_session_extmap() {
    let mut sdp_session = create_dummy_sdp_session();
    let t = SdpTiming { start: 0, stop: 0 };
    sdp_session.set_timing(&t);
    sdp_session.extend_media(vec![create_dummy_media_section()]);

    let attribute = parse_attribute("extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",);
    assert!(attribute.is_ok());
    let extmap;
    if let SdpType::Attribute(a) = attribute.unwrap() {
        extmap = a;
    } else {
        panic!("SdpType is not Attribute");
    }
    let ret = sdp_session.add_attribute(&extmap);
    assert!(ret.is_ok());
    assert!(sdp_session.get_attribute(SdpAttributeType::Extmap).is_some());

    assert!(sanity_check_sdp_session(&sdp_session).is_ok());

    let mattribute = parse_attribute("extmap:1/sendonly urn:ietf:params:rtp-hdrext:ssrc-audio-level",);
    assert!(mattribute.is_ok());
    let mextmap;
    if let SdpType::Attribute(ma) = mattribute.unwrap() {
        mextmap = ma;
    } else {
        panic!("SdpType is not Attribute");
    }
    let mut second_media = create_dummy_media_section();
    assert!(second_media.add_attribute(&mextmap).is_ok());
    assert!(second_media.get_attribute(SdpAttributeType::Extmap).is_some());

    sdp_session.extend_media(vec![second_media]);
    assert!(sdp_session.media.len() == 2);

    assert!(sanity_check_sdp_session(&sdp_session).is_err());

    sdp_session.attribute = Vec::new();

    assert!(sanity_check_sdp_session(&sdp_session).is_ok());
}

#[test]
fn test_sanity_check_sdp_session_simulcast() {
    let mut sdp_session = create_dummy_sdp_session();
    let t = SdpTiming { start: 0, stop: 0 };
    sdp_session.set_timing(&t);
    sdp_session.extend_media(vec![create_dummy_media_section()]);

    assert!(sanity_check_sdp_session(&sdp_session).is_ok());
}

// TODO add unit tests
fn parse_sdp_vector(lines: &[SdpLine]) -> Result<SdpSession, SdpParserError> {
    if lines.len() < 5 {
        return Err(SdpParserError::Sequence {
                       message: "SDP neeeds at least 5 lines".to_string(),
                       line_number: 0,
                   });
    }

    // TODO are these mataches really the only way to verify the types?
    let version: u64 = match lines[0].sdp_type {
        SdpType::Version(v) => v,
        _ => {
            return Err(SdpParserError::Sequence {
                           message: "first line needs to be version number".to_string(),
                           line_number: lines[0].line_number,
                       })
        }
    };
    let origin: SdpOrigin = match lines[1].sdp_type {
        SdpType::Origin(ref v) => v.clone(),
        _ => {
            return Err(SdpParserError::Sequence {
                           message: "second line needs to be origin".to_string(),
                           line_number: lines[1].line_number,
                       })
        }
    };
    let session: String = match lines[2].sdp_type {
        SdpType::Session(ref v) => v.clone(),
        _ => {
            return Err(SdpParserError::Sequence {
                           message: "third line needs to be session".to_string(),
                           line_number: lines[2].line_number,
                       })
        }
    };
    let mut sdp_session = SdpSession::new(version, origin, session);
    for (index, line) in lines.iter().enumerate().skip(3) {
        match line.sdp_type {
            SdpType::Attribute(ref a) => {
                sdp_session
                    .add_attribute(a)
                    .map_err(|e: SdpParserInternalError| {
                                 SdpParserError::Sequence {
                                     message: format!("{}", e),
                                     line_number: line.line_number,
                                 }
                             })?
            }
            SdpType::Bandwidth(ref b) => sdp_session.add_bandwidth(b),
            SdpType::Timing(ref t) => sdp_session.set_timing(t),
            SdpType::Connection(ref c) => sdp_session.set_connection(c),
            SdpType::Media(_) => sdp_session.extend_media(parse_media_vector(&lines[index..])?),
            SdpType::Origin(_) |
            SdpType::Session(_) |
            SdpType::Version(_) => {
                return Err(SdpParserError::Sequence {
                               message: "version, origin or session at wrong level".to_string(),
                               line_number: line.line_number,
                           })
            }
            // the line parsers throw unsupported errors for these already
            SdpType::Email(_) |
            SdpType::Information(_) |
            SdpType::Key(_) |
            SdpType::Phone(_) |
            SdpType::Repeat(_) |
            SdpType::Uri(_) |
            SdpType::Zone(_) => (),
        };
        if sdp_session.has_media() {
            break;
        };
    }
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
    if sdp.len() < 62 {
        return Err(SdpParserError::Line {
                       error: SdpParserInternalError::Generic("string to short to be valid SDP"
                                                                  .to_string()),
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
        match parse_sdp_line(stripped_line, line_number) {
            Ok(n) => {
                sdp_lines.push(n);
            }
            Err(e) => {
                match e {
                    // FIXME is this really a good way to accomplish this?
                    SdpParserError::Line {
                        error,
                        line,
                        line_number,
                    } => {
                        errors.push(SdpParserError::Line {
                                        error,
                                        line,
                                        line_number,
                                    })
                    }
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
                    } => {
                        errors.push(SdpParserError::Sequence {
                                        message,
                                        line_number,
                                    })
                    }
                }
            }
        };
    }

    if fail_on_warning && (warnings.len() > 0) {
        return Err(warnings[0].clone());
    }

    // We just return the last of the errors here
    if let Some(e) = errors.pop() {
        return Err(e);
    };

    let mut session = parse_sdp_vector(&sdp_lines)?;
    session.warnings = warnings;

    for warning in &session.warnings {
        println!("Warning: {}", &warning);
    }

    Ok(session)
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
fn test_parse_sdp_line_error() {
    assert!(parse_sdp("v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
t=0 foobar\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n",
                      true)
                    .is_err());
}

#[test]
fn test_parse_sdp_unsupported_error() {
    assert!(parse_sdp("v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
t=0 0\r\n
m=foobar 0 UDP/TLS/RTP/SAVPF 0\r\n",
                      true)
                    .is_err());
}

#[test]
fn test_parse_sdp_unsupported_warning() {
    assert!(parse_sdp("v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
c=IN IP4 198.51.100.7\r\n
t=0 0\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n
a=unsupported\r\n",
                      false)
                    .is_ok());
}

#[test]
fn test_parse_sdp_sequence_error() {
    assert!(parse_sdp("v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
t=0 0\r\n
s=-\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n",
                      true)
                    .is_err());
}

#[test]
fn test_parse_sdp_integer_error() {
    assert!(parse_sdp("v=0\r\n
o=- 0 0 IN IP4 0.0.0.0\r\n
s=-\r\n
t=0 0\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n
a=rtcp:34er21\r\n",
                      true)
                    .is_err());
}

#[test]
fn test_parse_sdp_ipaddr_error() {
    assert!(parse_sdp("v=0\r\n
o=- 0 0 IN IP4 0.a.b.0\r\n
s=-\r\n
t=0 0\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n",
                      true)
                    .is_err());
}

#[test]
fn test_parse_sdp_invalid_session_attribute() {
    assert!(parse_sdp("v=0\r\n
o=- 0 0 IN IP4 0.a.b.0\r\n
s=-\r\n
t=0 0\r\n
a=bundle-only\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n",
                      true)
                    .is_err());
}

#[test]
fn test_parse_sdp_invalid_media_attribute() {
    assert!(parse_sdp("v=0\r\n
o=- 0 0 IN IP4 0.a.b.0\r\n
s=-\r\n
t=0 0\r\n
m=audio 0 UDP/TLS/RTP/SAVPF 0\r\n
a=ice-lite\r\n",
                      true)
                    .is_err());
}
