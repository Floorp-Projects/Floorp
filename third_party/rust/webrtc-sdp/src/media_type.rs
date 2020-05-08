/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anonymizer::{AnonymizingClone, StatefulSdpAnonymizer};
use attribute_type::{
    maybe_print_param, SdpAttribute, SdpAttributeRtpmap, SdpAttributeSctpmap, SdpAttributeType,
};
use error::{SdpParserError, SdpParserInternalError};
use std::fmt;
use {SdpBandwidth, SdpConnection, SdpLine, SdpType};

/*
 * RFC4566
 * media-field =         %x6d "=" media SP port ["/" integer]
 *                       SP proto 1*(SP fmt) CRLF
 */
#[derive(Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub struct SdpMediaLine {
    pub media: SdpMediaValue,
    pub port: u32,
    pub port_count: u32,
    pub proto: SdpProtocolValue,
    pub formats: SdpFormatList,
}

impl fmt::Display for SdpMediaLine {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{media} {port}{pcount} {proto} {formats}",
            media = self.media,
            port = self.port,
            pcount = maybe_print_param("/", self.port_count, 0),
            proto = self.proto,
            formats = self.formats
        )
    }
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub enum SdpMediaValue {
    Audio,
    Video,
    Application,
}

impl fmt::Display for SdpMediaValue {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SdpMediaValue::Audio => "audio",
            SdpMediaValue::Video => "video",
            SdpMediaValue::Application => "application",
        }
        .fmt(f)
    }
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub enum SdpProtocolValue {
    RtpAvp,          /* RTP/AVP [RFC4566] */
    RtpAvpf,         /* RTP/AVPF [RFC4585] */
    RtpSavp,         /* RTP/SAVP [RFC3711] */
    RtpSavpf,        /* RTP/SAVPF [RFC5124] */
    TcpDtlsRtpSavp,  /* TCP/DTLS/RTP/SAVP [RFC7850] */
    TcpDtlsRtpSavpf, /* TCP/DTLS/RTP/SAVPF [RFC7850] */
    UdpTlsRtpSavp,   /* UDP/TLS/RTP/SAVP [RFC5764] */
    UdpTlsRtpSavpf,  /* UDP/TLS/RTP/SAVPF [RFC5764] */
    DtlsSctp,        /* DTLS/SCTP [draft-ietf-mmusic-sctp-sdp-07] */
    UdpDtlsSctp,     /* UDP/DTLS/SCTP [draft-ietf-mmusic-sctp-sdp-26] */
    TcpDtlsSctp,     /* TCP/DTLS/SCTP [draft-ietf-mmusic-sctp-sdp-26] */
}

impl fmt::Display for SdpProtocolValue {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SdpProtocolValue::RtpAvp => "RTP/AVP",
            SdpProtocolValue::RtpAvpf => "RTP/AVPF",
            SdpProtocolValue::RtpSavp => "RTP/SAVP",
            SdpProtocolValue::RtpSavpf => "RTP/SAVPF",
            SdpProtocolValue::TcpDtlsRtpSavp => "TCP/DTLS/RTP/SAVP",
            SdpProtocolValue::TcpDtlsRtpSavpf => "TCP/DTLS/RTP/SAVPF",
            SdpProtocolValue::UdpTlsRtpSavp => "UDP/TLS/RTP/SAVP",
            SdpProtocolValue::UdpTlsRtpSavpf => "UDP/TLS/RTP/SAVPF",
            SdpProtocolValue::DtlsSctp => "DTLS/SCTP",
            SdpProtocolValue::UdpDtlsSctp => "UDP/DTLS/SCTP",
            SdpProtocolValue::TcpDtlsSctp => "TCP/DTLS/SCTP",
        }
        .fmt(f)
    }
}

#[derive(Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub enum SdpFormatList {
    Integers(Vec<u32>),
    Strings(Vec<String>),
}

impl fmt::Display for SdpFormatList {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            SdpFormatList::Integers(ref x) => maybe_vector_to_string!("{}", x, " "),
            SdpFormatList::Strings(ref x) => x.join(" "),
        }
        .fmt(f)
    }
}

/*
 * RFC4566
 * media-descriptions =  *( media-field
 *                       information-field
 *                       *connection-field
 *                       bandwidth-fields
 *                       key-field
 *                       attribute-fields )
 */
#[derive(Clone)]
#[cfg_attr(feature = "serialize", derive(Serialize))]
pub struct SdpMedia {
    media: SdpMediaLine,
    connection: Option<SdpConnection>,
    bandwidth: Vec<SdpBandwidth>,
    attribute: Vec<SdpAttribute>,
    // unsupported values:
    // information: Option<String>,
    // key: Option<String>,
}

impl fmt::Display for SdpMedia {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "m={mline}\r\n{bw}{connection}{attributes}",
            mline = self.media,
            bw = maybe_vector_to_string!("b={}\r\n", self.bandwidth, "\r\nb="),
            connection = option_to_string!("c={}\r\n", self.connection),
            attributes = maybe_vector_to_string!("a={}\r\n", self.attribute, "\r\na=")
        )
    }
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

    pub fn add_bandwidth(&mut self, bw: SdpBandwidth) {
        self.bandwidth.push(bw)
    }

    pub fn get_attributes(&self) -> &Vec<SdpAttribute> {
        &self.attribute
    }

    pub fn add_attribute(&mut self, attr: SdpAttribute) -> Result<(), SdpParserInternalError> {
        if !attr.allowed_at_media_level() {
            return Err(SdpParserInternalError::Generic(format!(
                "{} not allowed at media level",
                attr
            )));
        }
        self.attribute.push(attr);
        Ok(())
    }

    pub fn get_attribute(&self, t: SdpAttributeType) -> Option<&SdpAttribute> {
        self.attribute
            .iter()
            .find(|a| SdpAttributeType::from(*a) == t)
    }

    pub fn remove_attribute(&mut self, t: SdpAttributeType) {
        self.attribute.retain(|a| SdpAttributeType::from(a) != t);
    }

    pub fn set_attribute(&mut self, attr: SdpAttribute) -> Result<(), SdpParserInternalError> {
        self.remove_attribute(SdpAttributeType::from(&attr));
        self.add_attribute(attr)
    }

    pub fn remove_codecs(&mut self) {
        match self.media.formats {
            SdpFormatList::Integers(_) => self.media.formats = SdpFormatList::Integers(Vec::new()),
            SdpFormatList::Strings(_) => self.media.formats = SdpFormatList::Strings(Vec::new()),
        }

        self.attribute.retain({
            |x| match *x {
                SdpAttribute::Rtpmap(_)
                | SdpAttribute::Fmtp(_)
                | SdpAttribute::Rtcpfb(_)
                | SdpAttribute::Sctpmap(_)
                | SdpAttribute::SctpPort(_) => false,
                _ => true,
            }
        });
    }

    pub fn add_codec(&mut self, rtpmap: SdpAttributeRtpmap) -> Result<(), SdpParserInternalError> {
        match self.media.formats {
            SdpFormatList::Integers(ref mut x) => x.push(u32::from(rtpmap.payload_type)),
            SdpFormatList::Strings(ref mut x) => x.push(rtpmap.payload_type.to_string()),
        }

        self.add_attribute(SdpAttribute::Rtpmap(rtpmap))?;
        Ok(())
    }

    pub fn get_attributes_of_type(&self, t: SdpAttributeType) -> Vec<&SdpAttribute> {
        self.attribute
            .iter()
            .filter(|a| SdpAttributeType::from(*a) == t)
            .collect()
    }

    pub fn get_connection(&self) -> &Option<SdpConnection> {
        &self.connection
    }

    pub fn set_connection(&mut self, c: SdpConnection) -> Result<(), SdpParserInternalError> {
        if self.connection.is_some() {
            return Err(SdpParserInternalError::Generic(
                "connection type already exists at this media level".to_string(),
            ));
        }
        self.connection = Some(c);
        Ok(())
    }

    pub fn add_datachannel(
        &mut self,
        name: String,
        port: u16,
        streams: u16,
        msg_size: u32,
    ) -> Result<(), SdpParserInternalError> {
        // Only one allowed, for now. This may change as the specs (and deployments) evolve.
        match self.media.proto {
            SdpProtocolValue::UdpDtlsSctp | SdpProtocolValue::TcpDtlsSctp => {
                // new data channel format according to draft 21
                self.media.formats = SdpFormatList::Strings(vec![name]);
                self.set_attribute(SdpAttribute::SctpPort(u64::from(port)))?;
            }
            _ => {
                // old data channels format according to draft 05
                self.media.formats = SdpFormatList::Integers(vec![u32::from(port)]);
                self.set_attribute(SdpAttribute::Sctpmap(SdpAttributeSctpmap {
                    port,
                    channels: u32::from(streams),
                }))?;
            }
        }
        if msg_size > 0 {
            self.set_attribute(SdpAttribute::MaxMessageSize(u64::from(msg_size)))?;
        }
        self.media.media = SdpMediaValue::Application;

        Ok(())
    }
}

impl AnonymizingClone for SdpMedia {
    fn masked_clone(&self, anon: &mut StatefulSdpAnonymizer) -> Self {
        let mut masked = SdpMedia {
            media: self.media.clone(),
            bandwidth: self.bandwidth.clone(),
            connection: self.connection.clone(),
            attribute: Vec::new(),
        };
        for i in &self.attribute {
            masked.attribute.push(i.masked_clone(anon));
        }
        masked
    }
}

fn parse_media_token(value: &str) -> Result<SdpMediaValue, SdpParserInternalError> {
    Ok(match value.to_lowercase().as_ref() {
        "audio" => SdpMediaValue::Audio,
        "video" => SdpMediaValue::Video,
        "application" => SdpMediaValue::Application,
        _ => {
            return Err(SdpParserInternalError::Unsupported(format!(
                "unsupported media value: {}",
                value
            )));
        }
    })
}

fn parse_protocol_token(value: &str) -> Result<SdpProtocolValue, SdpParserInternalError> {
    Ok(match value.to_uppercase().as_ref() {
        "RTP/AVP" => SdpProtocolValue::RtpAvp,
        "RTP/AVPF" => SdpProtocolValue::RtpAvpf,
        "RTP/SAVP" => SdpProtocolValue::RtpSavp,
        "RTP/SAVPF" => SdpProtocolValue::RtpSavpf,
        "TCP/DTLS/RTP/SAVP" => SdpProtocolValue::TcpDtlsRtpSavp,
        "TCP/DTLS/RTP/SAVPF" => SdpProtocolValue::TcpDtlsRtpSavpf,
        "UDP/TLS/RTP/SAVP" => SdpProtocolValue::UdpTlsRtpSavp,
        "UDP/TLS/RTP/SAVPF" => SdpProtocolValue::UdpTlsRtpSavpf,
        "DTLS/SCTP" => SdpProtocolValue::DtlsSctp,
        "UDP/DTLS/SCTP" => SdpProtocolValue::UdpDtlsSctp,
        "TCP/DTLS/SCTP" => SdpProtocolValue::TcpDtlsSctp,
        _ => {
            return Err(SdpParserInternalError::Unsupported(format!(
                "unsupported protocol value: {}",
                value
            )));
        }
    })
}

pub fn parse_media(value: &str) -> Result<SdpType, SdpParserInternalError> {
    let mv: Vec<&str> = value.split_whitespace().collect();
    if mv.len() < 4 {
        return Err(SdpParserInternalError::Generic(
            "media attribute must have at least four tokens".to_string(),
        ));
    }
    let media = parse_media_token(mv[0])?;
    let mut ptokens = mv[1].split('/');
    let port = match ptokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                "missing port token".to_string(),
            ));
        }
        Some(p) => p.parse::<u32>()?,
    };
    if port > 65535 {
        return Err(SdpParserInternalError::Generic(
            "media port token is too big".to_string(),
        ));
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
                    96 ..= 127 => (),  // dynamic range
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

pub fn parse_media_vector(lines: &mut Vec<SdpLine>) -> Result<Vec<SdpMedia>, SdpParserError> {
    let mut media_sections: Vec<SdpMedia> = Vec::new();

    let media_line = lines.remove(0);
    let mut sdp_media = match media_line.sdp_type {
        SdpType::Media(v) => SdpMedia::new(v),
        _ => {
            return Err(SdpParserError::Sequence {
                message: "first line in media section needs to be a media line".to_string(),
                line_number: media_line.line_number,
            });
        }
    };

    while !lines.is_empty() {
        let line = lines.remove(0);
        let _line_number = line.line_number;
        match line.sdp_type {
            SdpType::Connection(c) => {
                sdp_media
                    .set_connection(c)
                    .map_err(|e: SdpParserInternalError| SdpParserError::Sequence {
                        message: format!("{}", e),
                        line_number: _line_number,
                    })?
            }
            SdpType::Bandwidth(b) => sdp_media.add_bandwidth(b),
            SdpType::Attribute(a) => {
                match a {
                    SdpAttribute::DtlsMessage(_) => {
                        // Ignore this attribute on media level
                        Ok(())
                    }
                    SdpAttribute::Rtpmap(rtpmap) => {
                        sdp_media.add_attribute(SdpAttribute::Rtpmap(SdpAttributeRtpmap {
                            payload_type: rtpmap.payload_type,
                            codec_name: rtpmap.codec_name.clone(),
                            frequency: rtpmap.frequency,
                            channels: rtpmap.channels,
                        }))
                    }
                    _ => sdp_media.add_attribute(a),
                }
                .map_err(|e: SdpParserInternalError| SdpParserError::Sequence {
                    message: format!("{}", e),
                    line_number: _line_number,
                })?
            }
            SdpType::Media(v) => {
                media_sections.push(sdp_media);
                sdp_media = SdpMedia::new(v);
            }

            SdpType::Origin(_) | SdpType::Session(_) | SdpType::Timing(_) | SdpType::Version(_) => {
                return Err(SdpParserError::Sequence {
                    message: "invalid type in media section".to_string(),
                    line_number: line.line_number,
                });
            }
        };
    }

    media_sections.push(sdp_media);

    Ok(media_sections)
}

#[cfg(test)]
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

#[cfg(test)]
mod tests {
    use super::*;
    use address::{AddressType, ExplicitlyTypedAddress};
    use attribute_type::{
        SdpAttributeFmtp, SdpAttributeFmtpParameters, SdpAttributePayloadType, SdpAttributeRtcpFb,
        SdpAttributeRtcpFbType,
    };
    use std::convert::TryFrom;

    // TODO is this useful outside of tests?
    impl SdpFormatList {
        fn len(&self) -> usize {
            match *self {
                SdpFormatList::Integers(ref x) => x.len(),
                SdpFormatList::Strings(ref x) => x.len(),
            }
        }
    }

    pub fn add_dummy_attributes(media: &mut SdpMedia) {
        assert!(media
            .add_attribute(SdpAttribute::Rtcpfb(SdpAttributeRtcpFb {
                payload_type: SdpAttributePayloadType::Wildcard,
                feedback_type: SdpAttributeRtcpFbType::Ack,
                parameter: "".to_string(),
                extra: "".to_string(),
            },))
            .is_ok());
        assert!(media
            .add_attribute(SdpAttribute::Fmtp(SdpAttributeFmtp {
                payload_type: 1,
                parameters: SdpAttributeFmtpParameters {
                    packetization_mode: 0,
                    level_asymmetry_allowed: false,
                    profile_level_id: 0x0042_0010,
                    max_fs: 0,
                    max_cpb: 0,
                    max_dpb: 0,
                    max_br: 0,
                    max_mbps: 0,
                    usedtx: false,
                    stereo: false,
                    useinbandfec: false,
                    cbr: false,
                    max_fr: 0,
                    maxplaybackrate: 48000,
                    maxaveragebitrate: 0,
                    ptime: 0,
                    minptime: 0,
                    maxptime: 0,
                    encodings: Vec::new(),
                    dtmf_tones: "".to_string(),
                    rtx: None,
                    unknown_tokens: Vec::new()
                }
            },))
            .is_ok());
        assert!(media
            .add_attribute(SdpAttribute::Sctpmap(SdpAttributeSctpmap {
                port: 5000,
                channels: 2,
            }))
            .is_ok());
        assert!(media.add_attribute(SdpAttribute::BundleOnly).is_ok());
        assert!(media.add_attribute(SdpAttribute::SctpPort(5000)).is_ok());

        assert!(media.get_attribute(SdpAttributeType::Rtpmap).is_some());
        assert!(media.get_attribute(SdpAttributeType::Rtcpfb).is_some());
        assert!(media.get_attribute(SdpAttributeType::Fmtp).is_some());
        assert!(media.get_attribute(SdpAttributeType::Sctpmap).is_some());
        assert!(media.get_attribute(SdpAttributeType::SctpPort).is_some());
        assert!(media.get_attribute(SdpAttributeType::BundleOnly).is_some());
    }

    fn check_parse(media_line_str: &str) -> SdpMediaLine {
        if let Ok(SdpType::Media(media_line)) = parse_media(media_line_str) {
            media_line
        } else {
            unreachable!()
        }
    }

    fn check_parse_and_serialize(media_line_str: &str) {
        let parsed = check_parse(media_line_str);
        assert_eq!(parsed.to_string(), media_line_str.to_string());
    }

    #[test]
    fn test_get_set_port() {
        let mut msection = create_dummy_media_section();
        assert_eq!(msection.get_port(), 9);
        msection.set_port(2048);
        assert_eq!(msection.get_port(), 2048);
    }

    #[test]
    fn test_add_codec() -> Result<(), SdpParserInternalError> {
        let mut msection = create_dummy_media_section();
        msection.add_codec(SdpAttributeRtpmap::new(96, "foobar".to_string(), 1000))?;
        assert_eq!(msection.get_formats().len(), 1);
        assert!(msection.get_attribute(SdpAttributeType::Rtpmap).is_some());

        let mut msection = create_dummy_media_section();
        msection.media.formats = SdpFormatList::Strings(Vec::new());
        msection.add_codec(SdpAttributeRtpmap::new(97, "boofar".to_string(), 1001))?;
        assert_eq!(msection.get_formats().len(), 1);
        assert!(msection.get_attribute(SdpAttributeType::Rtpmap).is_some());
        Ok(())
    }

    #[test]
    fn test_remove_codecs() -> Result<(), SdpParserInternalError> {
        let mut msection = create_dummy_media_section();
        msection.add_codec(SdpAttributeRtpmap::new(96, "foobar".to_string(), 1000))?;
        assert_eq!(msection.get_formats().len(), 1);
        assert!(msection.get_attribute(SdpAttributeType::Rtpmap).is_some());
        msection.remove_codecs();
        assert_eq!(msection.get_formats().len(), 0);
        assert!(msection.get_attribute(SdpAttributeType::Rtpmap).is_none());

        let mut msection = create_dummy_media_section();
        msection.media.formats = SdpFormatList::Strings(Vec::new());
        msection.add_codec(SdpAttributeRtpmap::new(97, "boofar".to_string(), 1001))?;
        assert_eq!(msection.get_formats().len(), 1);

        add_dummy_attributes(&mut msection);

        msection.remove_codecs();
        assert_eq!(msection.get_formats().len(), 0);
        assert!(msection.get_attribute(SdpAttributeType::Rtpmap).is_none());
        assert!(msection.get_attribute(SdpAttributeType::Rtcpfb).is_none());
        assert!(msection.get_attribute(SdpAttributeType::Fmtp).is_none());
        assert!(msection.get_attribute(SdpAttributeType::Sctpmap).is_none());
        assert!(msection.get_attribute(SdpAttributeType::SctpPort).is_none());
        Ok(())
    }

    #[test]
    fn test_add_datachannel() -> Result<(), SdpParserInternalError> {
        let mut msection = create_dummy_media_section();
        msection.add_datachannel("foo".to_string(), 5000, 256, 0)?;
        assert_eq!(*msection.get_type(), SdpMediaValue::Application);
        assert!(msection.get_attribute(SdpAttributeType::SctpPort).is_none());
        assert!(msection
            .get_attribute(SdpAttributeType::MaxMessageSize)
            .is_none());
        assert!(msection.get_attribute(SdpAttributeType::Sctpmap).is_some());
        match *msection.get_attribute(SdpAttributeType::Sctpmap).unwrap() {
            SdpAttribute::Sctpmap(ref s) => {
                assert_eq!(s.port, 5000);
                assert_eq!(s.channels, 256);
            }
            _ => unreachable!(),
        }

        let mut msection = create_dummy_media_section();
        msection.add_datachannel("foo".to_string(), 5000, 256, 1234)?;
        assert_eq!(*msection.get_type(), SdpMediaValue::Application);
        assert!(msection.get_attribute(SdpAttributeType::SctpPort).is_none());
        assert!(msection
            .get_attribute(SdpAttributeType::MaxMessageSize)
            .is_some());
        match *msection
            .get_attribute(SdpAttributeType::MaxMessageSize)
            .unwrap()
        {
            SdpAttribute::MaxMessageSize(m) => {
                assert_eq!(m, 1234);
            }
            _ => unreachable!(),
        }

        let mut msection = create_dummy_media_section();
        msection.media.proto = SdpProtocolValue::UdpDtlsSctp;
        msection.add_datachannel("foo".to_string(), 5000, 256, 5678)?;
        assert_eq!(*msection.get_type(), SdpMediaValue::Application);
        assert!(msection.get_attribute(SdpAttributeType::Sctpmap).is_none());
        assert!(msection.get_attribute(SdpAttributeType::SctpPort).is_some());
        match *msection.get_attribute(SdpAttributeType::SctpPort).unwrap() {
            SdpAttribute::SctpPort(s) => {
                assert_eq!(s, 5000);
            }
            _ => unreachable!(),
        }
        assert!(msection
            .get_attribute(SdpAttributeType::MaxMessageSize)
            .is_some());
        match *msection
            .get_attribute(SdpAttributeType::MaxMessageSize)
            .unwrap()
        {
            SdpAttribute::MaxMessageSize(m) => {
                assert_eq!(m, 5678);
            }
            _ => unreachable!(),
        }
        Ok(())
    }

    #[test]
    fn test_parse_media_token() -> Result<(), SdpParserInternalError> {
        let audio = parse_media_token("audio")?;
        assert_eq!(audio, SdpMediaValue::Audio);
        let video = parse_media_token("VIDEO")?;
        assert_eq!(video, SdpMediaValue::Video);
        let app = parse_media_token("aPplIcatIOn")?;
        assert_eq!(app, SdpMediaValue::Application);

        assert!(parse_media_token("").is_err());
        assert!(parse_media_token("foobar").is_err());
        Ok(())
    }

    #[test]
    fn test_parse_protocol_rtp_token() -> Result<(), SdpParserInternalError> {
        fn parse_and_serialize_protocol_token(
            token: &str,
            result: SdpProtocolValue,
        ) -> Result<(), SdpParserInternalError> {
            let rtps = parse_protocol_token(token)?;
            assert_eq!(rtps, result);
            assert_eq!(rtps.to_string(), token.to_uppercase());
            Ok(())
        }
        parse_and_serialize_protocol_token("rtp/avp", SdpProtocolValue::RtpAvp)?;
        parse_and_serialize_protocol_token("rtp/avpf", SdpProtocolValue::RtpAvpf)?;
        parse_and_serialize_protocol_token("rtp/savp", SdpProtocolValue::RtpSavp)?;
        parse_and_serialize_protocol_token("rtp/savpf", SdpProtocolValue::RtpSavpf)?;
        parse_and_serialize_protocol_token("udp/tls/rtp/savp", SdpProtocolValue::UdpTlsRtpSavp)?;
        parse_and_serialize_protocol_token("udp/tls/rtp/savpf", SdpProtocolValue::UdpTlsRtpSavpf)?;
        parse_and_serialize_protocol_token("TCP/dtls/rtp/savp", SdpProtocolValue::TcpDtlsRtpSavp)?;
        parse_and_serialize_protocol_token(
            "tcp/DTLS/rtp/savpf",
            SdpProtocolValue::TcpDtlsRtpSavpf,
        )?;

        assert!(parse_protocol_token("").is_err());
        assert!(parse_protocol_token("foobar").is_err());
        Ok(())
    }

    #[test]
    fn test_parse_protocol_sctp_token() -> Result<(), SdpParserInternalError> {
        fn parse_and_serialize_protocol_token(
            token: &str,
            result: SdpProtocolValue,
        ) -> Result<(), SdpParserInternalError> {
            let rtps = parse_protocol_token(token)?;
            assert_eq!(rtps, result);
            assert_eq!(rtps.to_string(), token.to_uppercase());
            Ok(())
        }
        parse_and_serialize_protocol_token("dtLs/ScTP", SdpProtocolValue::DtlsSctp)?;
        parse_and_serialize_protocol_token("udp/DTLS/sctp", SdpProtocolValue::UdpDtlsSctp)?;
        parse_and_serialize_protocol_token("tcp/dtls/SCTP", SdpProtocolValue::TcpDtlsSctp)?;
        Ok(())
    }

    #[test]
    fn test_media_works() {
        check_parse_and_serialize("audio 9 UDP/TLS/RTP/SAVPF 109");
        check_parse_and_serialize("video 9 UDP/TLS/RTP/SAVPF 126");
        check_parse_and_serialize("application 9 DTLS/SCTP 5000");
        check_parse_and_serialize("application 9 UDP/DTLS/SCTP webrtc-datachannel");

        check_parse_and_serialize("audio 9 UDP/TLS/RTP/SAVPF 109 9 0 8");
        check_parse_and_serialize("audio 0 UDP/TLS/RTP/SAVPF 8");
        check_parse_and_serialize("audio 9/2 UDP/TLS/RTP/SAVPF 8");
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

    #[test]
    fn test_media_vector_first_line_failure() {
        let mut sdp_lines: Vec<SdpLine> = Vec::new();
        let line = SdpLine {
            line_number: 0,
            sdp_type: SdpType::Session("hello".to_string()),
            text: "".to_owned(),
        };
        sdp_lines.push(line);
        assert!(parse_media_vector(&mut sdp_lines).is_err());
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
            sdp_type: SdpType::Media(media_line),
            text: "".to_owned(),
        };
        sdp_lines.push(media);
        let c = SdpConnection {
            address: ExplicitlyTypedAddress::try_from((AddressType::IpV4, "127.0.0.1")).unwrap(),
            ttl: None,
            amount: None,
        };
        let c1 = SdpLine {
            line_number: 1,
            sdp_type: SdpType::Connection(c.clone()),
            text: "".to_owned(),
        };
        sdp_lines.push(c1);
        let c2 = SdpLine {
            line_number: 2,
            sdp_type: SdpType::Connection(c),
            text: "".to_owned(),
        };
        sdp_lines.push(c2);
        assert!(parse_media_vector(&mut sdp_lines).is_err());
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
            sdp_type: SdpType::Media(media_line),
            text: "".to_owned(),
        };
        sdp_lines.push(media);
        use SdpTiming;
        let t = SdpTiming { start: 0, stop: 0 };
        let tline = SdpLine {
            line_number: 1,
            sdp_type: SdpType::Timing(t),
            text: "".to_owned(),
        };
        sdp_lines.push(tline);
        assert!(parse_media_vector(&mut sdp_lines).is_err());
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
            sdp_type: SdpType::Media(media_line),
            text: "".to_owned(),
        };
        sdp_lines.push(media);
        let a = SdpAttribute::IceLite;
        let aline = SdpLine {
            line_number: 1,
            sdp_type: SdpType::Attribute(a),
            text: "".to_owned(),
        };
        sdp_lines.push(aline);
        assert!(parse_media_vector(&mut sdp_lines).is_err());
    }
}
