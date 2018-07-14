use std::fmt;
use {SdpType, SdpLine, SdpBandwidth, SdpConnection};
use attribute_type::{SdpAttribute, SdpAttributeType, SdpAttributeRtpmap, SdpAttributeSctpmap};
use error::{SdpParserError, SdpParserInternalError};

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpMediaLine {
    pub media: SdpMediaValue,
    pub port: u32,
    pub port_count: u32,
    pub proto: SdpProtocolValue,
    pub formats: SdpFormatList,
}

#[derive(Clone,Debug,PartialEq)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpMediaValue {
    Audio,
    Video,
    Application,
}

impl fmt::Display for SdpMediaValue {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let printable = match *self {
            SdpMediaValue::Audio => "Audio",
            SdpMediaValue::Video => "Video",
            SdpMediaValue::Application => "Application",
        };
        write!(f, "{}", printable)
    }
}

#[derive(Clone,Debug,PartialEq)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpProtocolValue {
    RtpSavpf,
    UdpTlsRtpSavpf,
    TcpTlsRtpSavpf,
    DtlsSctp,
    UdpDtlsSctp,
    TcpDtlsSctp,
}

impl fmt::Display for SdpProtocolValue {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let printable = match *self {
            SdpProtocolValue::RtpSavpf => "Rtp/Savpf",
            SdpProtocolValue::UdpTlsRtpSavpf => "Udp/Tls/Rtp/Savpf",
            SdpProtocolValue::TcpTlsRtpSavpf => "Tcp/Tls/Rtp/Savpf",
            SdpProtocolValue::DtlsSctp => "Dtls/Sctp",
            SdpProtocolValue::UdpDtlsSctp => "Udp/Dtls/Sctp",
            SdpProtocolValue::TcpDtlsSctp => "Tcp/Dtls/Sctp",
        };
        write!(f, "{}", printable)
    }
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpFormatList {
    Integers(Vec<u32>),
    Strings(Vec<String>),
}

impl fmt::Display for SdpFormatList {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SdpFormatList::Integers(ref x) => write!(f, "{:?}", x),
            SdpFormatList::Strings(ref x) => write!(f, "{:?}", x),
        }
    }
}

#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpMedia {
    media: SdpMediaLine,
    connection: Option<SdpConnection>,
    bandwidth: Vec<SdpBandwidth>,
    attribute: Vec<SdpAttribute>,
    // unsupported values:
    // information: Option<String>,
    // key: Option<String>,
}

impl SdpMedia {
    pub fn new(media: SdpMediaLine) -> SdpMedia {
        SdpMedia {
            media,
            connection: None,
            bandwidth: Vec::new(),
            attribute: Vec::new(),
        }
    }

    pub fn get_type(&self) -> &SdpMediaValue {
        &self.media.media
    }

    pub fn set_port(&mut self, port: u32) {
        self.media.port = port;
    }

    pub fn get_port(&self) -> u32 {
        self.media.port
    }

    pub fn get_port_count(&self) -> u32 {
        self.media.port_count
    }

    pub fn get_proto(&self) -> &SdpProtocolValue {
        &self.media.proto
    }

    pub fn get_formats(&self) -> &SdpFormatList {
        &self.media.formats
    }

    pub fn get_bandwidth(&self) -> &Vec<SdpBandwidth> {
        &self.bandwidth
    }

    pub fn add_bandwidth(&mut self, bw: &SdpBandwidth) {
        self.bandwidth.push(bw.clone())
    }

    pub fn get_attributes(&self) -> &Vec<SdpAttribute> {
        &self.attribute
    }

    pub fn add_attribute(&mut self, attr: &SdpAttribute) -> Result<(), SdpParserInternalError> {
        if !attr.allowed_at_media_level() {
            return Err(SdpParserInternalError::Generic(format!("{} not allowed at media level",
                                                               attr)));
        }
        Ok(self.attribute.push(attr.clone()))
    }

    pub fn get_attribute(&self, t: SdpAttributeType) -> Option<&SdpAttribute> {
        self.attribute.iter().filter(|a| SdpAttributeType::from(*a) == t).next()
    }

    pub fn remove_attribute(&mut self, t: SdpAttributeType) {
        self.attribute.retain(|a| SdpAttributeType::from(a) != t);
    }

    pub fn set_attribute(&mut self, attr: &SdpAttribute) -> Result<(), SdpParserInternalError> {
        self.remove_attribute(SdpAttributeType::from(attr));
        self.add_attribute(attr)
    }

    pub fn remove_codecs(&mut self) {
        match self.media.formats{
            SdpFormatList::Integers(_) => self.media.formats = SdpFormatList::Integers(Vec::new()),
            SdpFormatList::Strings(_) => self.media.formats = SdpFormatList::Strings(Vec::new()),
        }

        self.attribute.retain({|x|
            match x {
                &SdpAttribute::Rtpmap(_) |
                &SdpAttribute::Fmtp(_) |
                &SdpAttribute::Rtcpfb(_) |
                &SdpAttribute::Sctpmap(_) => false,
                _ => true
            }
        });
    }

    pub fn add_codec(&mut self, rtpmap: SdpAttributeRtpmap) -> Result<(),SdpParserInternalError> {
          match self.media.formats {
             SdpFormatList::Integers(ref mut x) => x.push(rtpmap.payload_type as u32),
             SdpFormatList::Strings(ref mut x) => x.push(rtpmap.payload_type.to_string()),
         }

        self.add_attribute(&SdpAttribute::Rtpmap(rtpmap))?;
        Ok(())
    }

    pub fn get_attributes_of_type(&self, t: SdpAttributeType) -> Vec<&SdpAttribute> {
        self.attribute.iter().filter(|a| SdpAttributeType::from(*a) == t).collect()
    }

    pub fn get_connection(&self) -> &Option<SdpConnection> {
        &self.connection
    }

    pub fn set_connection(&mut self, c: &SdpConnection) -> Result<(), SdpParserInternalError> {
        if self.connection.is_some() {
            return Err(SdpParserInternalError::Generic("connection type already exists at this media level"
                               .to_string(),
                       ));
        }
        Ok(self.connection = Some(c.clone()))
    }

    pub fn add_datachannel(&mut self, name: String, port: u16, streams: u16, msg_size:u32)
                           -> Result<(),SdpParserInternalError> {
         // Only one allowed, for now. This may change as the specs (and deployments) evolve.
        match self.media.proto {
            SdpProtocolValue::UdpDtlsSctp |
            SdpProtocolValue::TcpDtlsSctp => {
                // new data channel format according to draft 21
                self.media.formats = SdpFormatList::Strings(vec![name]);
                self.set_attribute(&SdpAttribute::SctpPort(port as u64))?;
            }
            _ => {
                // old data channels format according to draft 05
                self.media.formats = SdpFormatList::Integers(vec![port as u32]);
                self.set_attribute(&SdpAttribute::Sctpmap(SdpAttributeSctpmap {
                    port,
                    channels: streams as u32,
                }))?;
            }
        }

        if msg_size > 0 {
            self.set_attribute(&SdpAttribute::MaxMessageSize(msg_size as u64))?;
        }

        Ok(())
    }
}

#[cfg(test)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub fn create_dummy_media_section() -> SdpMedia {
    let media_line = SdpMediaLine {
        media: SdpMediaValue::Audio,
        port: 9,
        port_count: 0,
        proto: SdpProtocolValue::RtpSavpf,
        formats: SdpFormatList::Integers(Vec::new()),
    };
    SdpMedia::new(media_line)
}

fn parse_media_token(value: &str) -> Result<SdpMediaValue, SdpParserInternalError> {
    Ok(match value.to_lowercase().as_ref() {
           "audio" => SdpMediaValue::Audio,
           "video" => SdpMediaValue::Video,
           "application" => SdpMediaValue::Application,
           _ => {
               return Err(SdpParserInternalError::Unsupported(format!("unsupported media value: {}",
                                                                      value)))
           }
       })
}

#[test]
fn test_parse_media_token() {
    let audio = parse_media_token("audio");
    assert!(audio.is_ok());
    assert_eq!(audio.unwrap(), SdpMediaValue::Audio);
    let video = parse_media_token("VIDEO");
    assert!(video.is_ok());
    assert_eq!(video.unwrap(), SdpMediaValue::Video);
    let app = parse_media_token("aPplIcatIOn");
    assert!(app.is_ok());
    assert_eq!(app.unwrap(), SdpMediaValue::Application);

    assert!(parse_media_token("").is_err());
    assert!(parse_media_token("foobar").is_err());
}


fn parse_protocol_token(value: &str) -> Result<SdpProtocolValue, SdpParserInternalError> {
    Ok(match value.to_uppercase().as_ref() {
           "RTP/SAVPF" => SdpProtocolValue::RtpSavpf,
           "UDP/TLS/RTP/SAVPF" => SdpProtocolValue::UdpTlsRtpSavpf,
           "TCP/TLS/RTP/SAVPF" => SdpProtocolValue::TcpTlsRtpSavpf,
           "DTLS/SCTP" => SdpProtocolValue::DtlsSctp,
           "UDP/DTLS/SCTP" => SdpProtocolValue::UdpDtlsSctp,
           "TCP/DTLS/SCTP" => SdpProtocolValue::TcpDtlsSctp,
           _ => {
               return Err(SdpParserInternalError::Unsupported(format!("unsupported protocol value: {}",
                                                                      value)))
           }
       })
}

#[test]
fn test_parse_protocol_token() {
    let rtps = parse_protocol_token("rtp/savpf");
    assert!(rtps.is_ok());
    assert_eq!(rtps.unwrap(), SdpProtocolValue::RtpSavpf);
    let udps = parse_protocol_token("udp/tls/rtp/savpf");
    assert!(udps.is_ok());
    assert_eq!(udps.unwrap(), SdpProtocolValue::UdpTlsRtpSavpf);
    let tcps = parse_protocol_token("TCP/tls/rtp/savpf");
    assert!(tcps.is_ok());
    assert_eq!(tcps.unwrap(), SdpProtocolValue::TcpTlsRtpSavpf);
    let dtls = parse_protocol_token("dtLs/ScTP");
    assert!(dtls.is_ok());
    assert_eq!(dtls.unwrap(), SdpProtocolValue::DtlsSctp);
    let usctp = parse_protocol_token("udp/DTLS/sctp");
    assert!(usctp.is_ok());
    assert_eq!(usctp.unwrap(), SdpProtocolValue::UdpDtlsSctp);
    let tsctp = parse_protocol_token("tcp/dtls/SCTP");
    assert!(tsctp.is_ok());
    assert_eq!(tsctp.unwrap(), SdpProtocolValue::TcpDtlsSctp);

    assert!(parse_protocol_token("").is_err());
    assert!(parse_protocol_token("foobar").is_err());
}

pub fn parse_media(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let mv: Vec<&str> = value.split_whitespace().collect();
    if mv.len() < 4 {
        return Err(SdpParserInternalError::Generic("media attribute must have at least four tokens"
                                                       .to_string()));
    }
    let media = parse_media_token(mv[0])?;
    let mut ptokens = mv[1].split('/');
    let port = match ptokens.next() {
        None => return Err(SdpParserInternalError::Generic("missing port token".to_string())),
        Some(p) => p.parse::<u32>()?,
    };
    if port > 65535 {
        return Err(SdpParserInternalError::Generic("media port token is too big".to_string()));
    }
    let port_count = match ptokens.next() {
        None => 0,
        Some(c) => c.parse::<u32>()?,
    };
    let proto = parse_protocol_token(mv[2])?;
    let fmt_slice: &[&str] = &mv[3..];
    let formats = match media {
        SdpMediaValue::Audio | SdpMediaValue::Video => {
            let mut fmt_vec: Vec<u32> = vec![];
            for num in fmt_slice {
                let fmt_num = num.parse::<u32>()?;
                match fmt_num {
                    0  |  // PCMU
                    8  |  // PCMA
                    9  |  // G722
                    13 |  // Comfort Noise
                    96 ... 127 => (),  // dynamic range
                    _ => return Err(SdpParserInternalError::Generic(
                          "format number in media line is out of range".to_string()))
                };
                fmt_vec.push(fmt_num);
            }
            SdpFormatList::Integers(fmt_vec)
        }
        SdpMediaValue::Application => {
            let mut fmt_vec: Vec<String> = vec![];
            // TODO enforce length == 1 and content 'webrtc-datachannel' only?
            for token in fmt_slice {
                fmt_vec.push(String::from(*token));
            }
            SdpFormatList::Strings(fmt_vec)
        }
    };
    let m = SdpMediaLine {
        media,
        port,
        port_count,
        proto,
        formats,
    };
    trace!("media: {}, {}, {}, {}", m.media, m.port, m.proto, m.formats);
    Ok(SdpType::Media(m))
}

#[test]
fn test_media_works() {
    assert!(parse_media("audio 9 UDP/TLS/RTP/SAVPF 109").is_ok());
    assert!(parse_media("video 9 UDP/TLS/RTP/SAVPF 126").is_ok());
    assert!(parse_media("application 9 DTLS/SCTP 5000").is_ok());
    assert!(parse_media("application 9 UDP/DTLS/SCTP webrtc-datachannel").is_ok());

    assert!(parse_media("audio 9 UDP/TLS/RTP/SAVPF 109 9 0 8").is_ok());
    assert!(parse_media("audio 0 UDP/TLS/RTP/SAVPF 8").is_ok());
    assert!(parse_media("audio 9/2 UDP/TLS/RTP/SAVPF 8").is_ok());
}

#[test]
fn test_media_missing_token() {
    assert!(parse_media("video 9 UDP/TLS/RTP/SAVPF").is_err());
}

#[test]
fn test_media_invalid_port_number() {
    assert!(parse_media("video 75123 UDP/TLS/RTP/SAVPF 8").is_err());
}

#[test]
fn test_media_invalid_type() {
    assert!(parse_media("invalid 9 UDP/TLS/RTP/SAVPF 8").is_err());
}

#[test]
fn test_media_invalid_port() {
    assert!(parse_media("audio / UDP/TLS/RTP/SAVPF 8").is_err());
}

#[test]
fn test_media_invalid_transport() {
    assert!(parse_media("audio 9 invalid/invalid 8").is_err());
}

#[test]
fn test_media_invalid_payload() {
    assert!(parse_media("audio 9 UDP/TLS/RTP/SAVPF 300").is_err());
}

pub fn parse_media_vector(lines: &[SdpLine]) -> Result<Vec<SdpMedia>, SdpParserError> {
    let mut media_sections: Vec<SdpMedia> = Vec::new();
    let mut sdp_media = match lines[0].sdp_type {
        SdpType::Media(ref v) => SdpMedia::new(v.clone()),
        _ => {
            return Err(SdpParserError::Sequence {
                           message: "first line in media section needs to be a media line"
                               .to_string(),
                           line_number: lines[0].line_number,
                       })
        }
    };

    for line in lines.iter().skip(1) {
        match line.sdp_type {
            SdpType::Connection(ref c) => {
                sdp_media
                    .set_connection(c)
                    .map_err(|e: SdpParserInternalError| {
                                 SdpParserError::Sequence {
                                     message: format!("{}", e),
                                     line_number: line.line_number,
                                 }
                             })?
            }
            SdpType::Bandwidth(ref b) => sdp_media.add_bandwidth(b),
            SdpType::Attribute(ref a) => {
                match a {
                    &SdpAttribute::Rtpmap(ref rtpmap) => {
                        sdp_media.add_attribute(&SdpAttribute::Rtpmap(
                            SdpAttributeRtpmap {
                                payload_type: rtpmap.payload_type,
                                codec_name: rtpmap.codec_name.clone(),
                                frequency: rtpmap.frequency,
                                channels: match sdp_media.media.media {
                                    SdpMediaValue::Video => Some(0),
                                    _ => rtpmap.channels
                                },
                            }
                        ))
                    },
                    _ => {
                        sdp_media.add_attribute(a)
                    }
                }.map_err(|e: SdpParserInternalError| {
                             SdpParserError::Sequence {
                                 message: format!("{}", e),
                                 line_number: line.line_number,
                             }
                         })?
            }
            SdpType::Media(ref v) => {
                media_sections.push(sdp_media);
                sdp_media = SdpMedia::new(v.clone());
            }

            SdpType::Email(_) |
            SdpType::Phone(_) |
            SdpType::Origin(_) |
            SdpType::Repeat(_) |
            SdpType::Session(_) |
            SdpType::Timing(_) |
            SdpType::Uri(_) |
            SdpType::Version(_) |
            SdpType::Zone(_) => {
                return Err(SdpParserError::Sequence {
                               message: "invalid type in media section".to_string(),
                               line_number: line.line_number,
                           })
            }

            // the line parsers throw unsupported errors for these already
            SdpType::Information(_) |
            SdpType::Key(_) => (),
        };
    }

    media_sections.push(sdp_media);

    Ok(media_sections)
}

#[test]
fn test_media_vector_first_line_failure() {
    let mut sdp_lines: Vec<SdpLine> = Vec::new();
    let line = SdpLine {
        line_number: 0,
        sdp_type: SdpType::Session("hello".to_string())
    };
    sdp_lines.push(line);
    assert!(parse_media_vector(&sdp_lines).is_err());
}

#[test]
fn test_media_vector_multiple_connections() {
    let mut sdp_lines: Vec<SdpLine> = Vec::new();
    let media_line = SdpMediaLine {
        media: SdpMediaValue::Audio,
        port: 9,
        port_count: 0,
        proto: SdpProtocolValue::RtpSavpf,
        formats: SdpFormatList::Integers(Vec::new()),
    };
    let media = SdpLine {
        line_number: 0,
        sdp_type: SdpType::Media(media_line)
    };
    sdp_lines.push(media);
    use network::{parse_unicast_addr};
    let addr = parse_unicast_addr("127.0.0.1").unwrap();
    let c = SdpConnection {
        addr,
        ttl: None,
        amount: None };
    let c1 = SdpLine {
        line_number: 1,
        sdp_type: SdpType::Connection(c.clone())
    };
    sdp_lines.push(c1);
    let c2 = SdpLine {
        line_number: 2,
        sdp_type: SdpType::Connection(c)
    };
    sdp_lines.push(c2);
    assert!(parse_media_vector(&sdp_lines).is_err());
}

#[test]
fn test_media_vector_invalid_types() {
    let mut sdp_lines: Vec<SdpLine> = Vec::new();
    let media_line = SdpMediaLine {
        media: SdpMediaValue::Audio,
        port: 9,
        port_count: 0,
        proto: SdpProtocolValue::RtpSavpf,
        formats: SdpFormatList::Integers(Vec::new()),
    };
    let media = SdpLine {
        line_number: 0,
        sdp_type: SdpType::Media(media_line)
    };
    sdp_lines.push(media);
    use {SdpTiming};
    let t = SdpTiming { start: 0, stop: 0 };
    let tline = SdpLine {
        line_number: 1,
        sdp_type: SdpType::Timing(t)
    };
    sdp_lines.push(tline);
    assert!(parse_media_vector(&sdp_lines).is_err());
}

#[test]
fn test_media_vector_invalid_media_level_attribute() {
    let mut sdp_lines: Vec<SdpLine> = Vec::new();
    let media_line = SdpMediaLine {
        media: SdpMediaValue::Audio,
        port: 9,
        port_count: 0,
        proto: SdpProtocolValue::RtpSavpf,
        formats: SdpFormatList::Integers(Vec::new()),
    };
    let media = SdpLine {
        line_number: 0,
        sdp_type: SdpType::Media(media_line)
    };
    sdp_lines.push(media);
    let a = SdpAttribute::IceLite;
    let aline = SdpLine {
        line_number: 1,
        sdp_type: SdpType::Attribute(a)
    };
    sdp_lines.push(aline);
    assert!(parse_media_vector(&sdp_lines).is_err());
}
