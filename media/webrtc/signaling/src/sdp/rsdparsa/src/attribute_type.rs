use std::net::IpAddr;
use std::str::FromStr;
use std::fmt;
use std::iter;

use SdpType;
use error::SdpParserInternalError;
use network::{parse_nettype, parse_addrtype, parse_unicast_addr};

#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpSingleDirection{
    // This is explicitly 1 and 2 to match the defines in the C++ glue code.
    Send = 1,
    Recv = 2,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributePayloadType {
    PayloadType(u8),
    Wildcard, // Wildcard means "*",
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeCandidateTransport {
    Udp,
    Tcp,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeCandidateType {
    Host,
    Srflx,
    Prflx,
    Relay,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeCandidateTcpType {
    Active,
    Passive,
    Simultaneous,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeCandidate {
    pub foundation: String,
    pub component: u32,
    pub transport: SdpAttributeCandidateTransport,
    pub priority: u64,
    pub address: IpAddr,
    pub port: u32,
    pub c_type: SdpAttributeCandidateType,
    pub raddr: Option<IpAddr>,
    pub rport: Option<u32>,
    pub tcp_type: Option<SdpAttributeCandidateTcpType>,
    pub generation: Option<u32>,
    pub ufrag: Option<String>,
    pub networkcost: Option<u32>,
}

impl SdpAttributeCandidate {
    pub fn new(foundation: String,
               component: u32,
               transport: SdpAttributeCandidateTransport,
               priority: u64,
               address: IpAddr,
               port: u32,
               c_type: SdpAttributeCandidateType)
               -> SdpAttributeCandidate {
        SdpAttributeCandidate {
            foundation,
            component,
            transport,
            priority,
            address,
            port,
            c_type,
            raddr: None,
            rport: None,
            tcp_type: None,
            generation: None,
            ufrag: None,
            networkcost: None,
        }
    }

    fn set_remote_address(&mut self, ip: IpAddr) {
        self.raddr = Some(ip)
    }

    fn set_remote_port(&mut self, p: u32) {
        self.rport = Some(p)
    }

    fn set_tcp_type(&mut self, t: SdpAttributeCandidateTcpType) {
        self.tcp_type = Some(t)
    }

    fn set_generation(&mut self, g: u32) {
        self.generation = Some(g)
    }

    fn set_ufrag(&mut self, u: String) {
        self.ufrag = Some(u)
    }

    fn set_network_cost(&mut self, n: u32) {
        self.networkcost = Some(n)
    }
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeDtlsMessage {
    Client(String),
    Server(String),
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeRemoteCandidate {
    pub component: u32,
    pub address: IpAddr,
    pub port: u32,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeSimulcastId {
    pub id: String,
    pub paused: bool,
}

impl SdpAttributeSimulcastId {
    pub fn new(idstr: &str) -> SdpAttributeSimulcastId {
        if idstr.starts_with('~') {
            SdpAttributeSimulcastId {
                id: idstr[1..].to_string(),
                paused: true,
            }
        } else {
            SdpAttributeSimulcastId {
                id: idstr.to_string(),
                paused: false,
            }
        }
    }
}

#[repr(C)]
#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeSimulcastVersion {
    pub ids: Vec<SdpAttributeSimulcastId>,
}

impl SdpAttributeSimulcastVersion {
    pub fn new(idlist: &str) -> SdpAttributeSimulcastVersion {
        SdpAttributeSimulcastVersion {
            ids: idlist
                .split(',')
                .map(SdpAttributeSimulcastId::new)
                .collect(),
        }
    }
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeSimulcast {
    pub send: Vec<SdpAttributeSimulcastVersion>,
    pub receive: Vec<SdpAttributeSimulcastVersion>,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeRtcp {
    pub port: u16,
    pub unicast_addr: Option<IpAddr>,
}

impl SdpAttributeRtcp {
    pub fn new(port: u16) -> SdpAttributeRtcp {
        SdpAttributeRtcp {
            port,
            unicast_addr: None,
        }
    }

    fn set_addr(&mut self, addr: IpAddr) {
        self.unicast_addr = Some(addr)
    }
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeRtcpFbType {
    Ack = 0,
    Ccm = 2, // This is explicitly 2 to make the conversion to the
             // enum used in the glue-code possible. The glue code has "app"
             // in the place of 1
    Nack,
    TrrInt,
    Remb,
    TransCC
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeRtcpFb {
    pub payload_type: SdpAttributePayloadType,
    pub feedback_type: SdpAttributeRtcpFbType,
    pub parameter: String,
    pub extra: String,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeDirection {
    Recvonly,
    Sendonly,
    Sendrecv,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeExtmap {
    pub id: u16,
    pub direction: Option<SdpAttributeDirection>,
    pub url: String,
    pub extension_attributes: Option<String>,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeFmtpParameters {
    // H264
    // TODO(bug 1466859): Support sprop-parameter-sets
    // pub static const max_sprop_len: u32 = 128,
    // pub sprop_parameter_sets: [u8, max_sprop_len],
    pub packetization_mode: u32,
    pub level_asymmetry_allowed: bool,
    pub profile_level_id: u32,
    pub max_fs: u32,
    pub max_cpb: u32,
    pub max_dpb: u32,
    pub max_br: u32,
    pub max_mbps: u32,

    // VP8 and VP9
    // max_fs, already defined in H264
    pub max_fr: u32,

    // Opus
    pub maxplaybackrate: u32,
    pub usedtx: bool,
    pub stereo: bool,
    pub useinbandfec: bool,
    pub cbr: bool,

    // Red
    pub encodings: Vec<u8>,

    // telephone-event
    pub dtmf_tones: String,

    // Unknown
    pub unknown_tokens: Vec<String>,
}


#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeFmtp {
    pub payload_type: u8,
    pub parameters: SdpAttributeFmtpParameters,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeFingerprint {
    // TODO turn the supported hash algorithms into an enum?
    pub hash_algorithm: String,
    pub fingerprint: String,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeImageAttrXYRange {
    Range(u32,u32,Option<u32>), // min, max, step
    DiscreteValues(Vec<u32>),
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeImageAttrSRange {
    Range(f32,f32), // min, max
    DiscreteValues(Vec<f32>),
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeImageAttrPRange {
    pub min: f32,
    pub max: f32,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeImageAttrSet {
    pub x: SdpAttributeImageAttrXYRange,
    pub y: SdpAttributeImageAttrXYRange,
    pub sar: Option<SdpAttributeImageAttrSRange>,
    pub par: Option<SdpAttributeImageAttrPRange>,
    pub q: Option<f32>,
}

#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeImageAttrSetList {
    Sets(Vec<SdpAttributeImageAttrSet>),
    Wildcard,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeImageAttr {
    pub pt: SdpAttributePayloadType,
    pub send: SdpAttributeImageAttrSetList,
    pub recv: SdpAttributeImageAttrSetList,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeSctpmap {
    pub port: u16,
    pub channels: u32,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeGroupSemantic {
    LipSynchronization,
    FlowIdentification,
    SingleReservationFlow,
    AlternateNetworkAddressType,
    ForwardErrorCorrection,
    DecodingDependency,
    Bundle,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeGroup {
    pub semantics: SdpAttributeGroupSemantic,
    pub tags: Vec<String>,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeMsid {
    pub id: String,
    pub appdata: Option<String>,
}

#[derive(Clone, Debug)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeMsidSemantic {
    pub semantic: String,
    pub msids: Vec<String>,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeRidParameters{
    pub max_width: u32,
    pub max_height: u32,
    pub max_fps: u32,
    pub max_fs: u32,
    pub max_br: u32,
    pub max_pps: u32,

    pub unknown: Vec<String>
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeRid {
    pub id: String,
    pub direction: SdpSingleDirection,
    pub formats: Vec<u16>,
    pub params: SdpAttributeRidParameters,
    pub depends: Vec<String>
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeRtpmap {
    pub payload_type: u8,
    pub codec_name: String,
    pub frequency: u32,
    pub channels: Option<u32>,
}

impl SdpAttributeRtpmap {
    pub fn new(payload_type: u8, codec_name: String, frequency: u32) -> SdpAttributeRtpmap {
        SdpAttributeRtpmap {
            payload_type,
            codec_name,
            frequency,
            channels: None,
        }
    }

    fn set_channels(&mut self, c: u32) {
        self.channels = Some(c)
    }
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttributeSetup {
    Active,
    Actpass,
    Holdconn,
    Passive,
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub struct SdpAttributeSsrc {
    pub id: u32,
    pub attribute: Option<String>,
    pub value: Option<String>,
}

impl SdpAttributeSsrc {
    pub fn new(id: u32) -> SdpAttributeSsrc {
        SdpAttributeSsrc {
            id,
            attribute: None,
            value: None,
        }
    }

    fn set_attribute(&mut self, a: &str) {
        if a.find(':') == None {
            self.attribute = Some(a.to_string());
        } else {
            let v: Vec<&str> = a.splitn(2, ':').collect();
            self.attribute = Some(v[0].to_string());
            self.value = Some(v[1].to_string());
        }
    }
}

#[derive(Clone)]
#[cfg_attr(feature="serialize", derive(Serialize))]
pub enum SdpAttribute {
    BundleOnly,
    Candidate(SdpAttributeCandidate),
    DtlsMessage(SdpAttributeDtlsMessage),
    EndOfCandidates,
    Extmap(SdpAttributeExtmap),
    Fingerprint(SdpAttributeFingerprint),
    Fmtp(SdpAttributeFmtp),
    Group(SdpAttributeGroup),
    IceLite,
    IceMismatch,
    IceOptions(Vec<String>),
    IcePwd(String),
    IceUfrag(String),
    Identity(String),
    ImageAttr(SdpAttributeImageAttr),
    Inactive,
    Label(String),
    MaxMessageSize(u64),
    MaxPtime(u64),
    Mid(String),
    Msid(SdpAttributeMsid),
    MsidSemantic(SdpAttributeMsidSemantic),
    Ptime(u64),
    Rid(SdpAttributeRid),
    Recvonly,
    RemoteCandidate(SdpAttributeRemoteCandidate),
    Rtpmap(SdpAttributeRtpmap),
    Rtcp(SdpAttributeRtcp),
    Rtcpfb(SdpAttributeRtcpFb),
    RtcpMux,
    RtcpRsize,
    Sctpmap(SdpAttributeSctpmap),
    SctpPort(u64),
    Sendonly,
    Sendrecv,
    Setup(SdpAttributeSetup),
    Simulcast(SdpAttributeSimulcast),
    Ssrc(SdpAttributeSsrc),
    SsrcGroup(String),
}

impl SdpAttribute {
    pub fn allowed_at_session_level(&self) -> bool {
        match *self {
            SdpAttribute::BundleOnly |
            SdpAttribute::Candidate(..) |
            SdpAttribute::Fmtp(..) |
            SdpAttribute::IceMismatch |
            SdpAttribute::ImageAttr(..) |
            SdpAttribute::Label(..) |
            SdpAttribute::MaxMessageSize(..) |
            SdpAttribute::MaxPtime(..) |
            SdpAttribute::Mid(..) |
            SdpAttribute::Msid(..) |
            SdpAttribute::Ptime(..) |
            SdpAttribute::Rid(..) |
            SdpAttribute::RemoteCandidate(..) |
            SdpAttribute::Rtpmap(..) |
            SdpAttribute::Rtcp(..) |
            SdpAttribute::Rtcpfb(..) |
            SdpAttribute::RtcpMux |
            SdpAttribute::RtcpRsize |
            SdpAttribute::Sctpmap(..) |
            SdpAttribute::SctpPort(..) |
            SdpAttribute::Simulcast(..) |
            SdpAttribute::Ssrc(..) |
            SdpAttribute::SsrcGroup(..) => false,

            SdpAttribute::DtlsMessage{..} |
            SdpAttribute::EndOfCandidates |
            SdpAttribute::Extmap(..) |
            SdpAttribute::Fingerprint(..) |
            SdpAttribute::Group(..) |
            SdpAttribute::IceLite |
            SdpAttribute::IceOptions(..) |
            SdpAttribute::IcePwd(..) |
            SdpAttribute::IceUfrag(..) |
            SdpAttribute::Identity(..) |
            SdpAttribute::Inactive |
            SdpAttribute::MsidSemantic(..) |
            SdpAttribute::Recvonly |
            SdpAttribute::Sendonly |
            SdpAttribute::Sendrecv |
            SdpAttribute::Setup(..) => true,
        }
    }

    pub fn allowed_at_media_level(&self) -> bool {
        match *self {
            SdpAttribute::DtlsMessage{..} |
            SdpAttribute::Group(..) |
            SdpAttribute::IceLite |
            SdpAttribute::Identity(..) |
            SdpAttribute::MsidSemantic(..) => false,

            SdpAttribute::BundleOnly |
            SdpAttribute::Candidate(..) |
            SdpAttribute::EndOfCandidates |
            SdpAttribute::Extmap(..) |
            SdpAttribute::Fingerprint(..) |
            SdpAttribute::Fmtp(..) |
            SdpAttribute::IceMismatch |
            SdpAttribute::IceOptions(..) |
            SdpAttribute::IcePwd(..) |
            SdpAttribute::IceUfrag(..) |
            SdpAttribute::ImageAttr(..) |
            SdpAttribute::Inactive |
            SdpAttribute::Label(..) |
            SdpAttribute::MaxMessageSize(..) |
            SdpAttribute::MaxPtime(..) |
            SdpAttribute::Mid(..) |
            SdpAttribute::Msid(..) |
            SdpAttribute::Ptime(..) |
            SdpAttribute::Rid(..) |
            SdpAttribute::Recvonly |
            SdpAttribute::RemoteCandidate(..) |
            SdpAttribute::Rtpmap(..) |
            SdpAttribute::Rtcp(..) |
            SdpAttribute::Rtcpfb(..) |
            SdpAttribute::RtcpMux |
            SdpAttribute::RtcpRsize |
            SdpAttribute::Sctpmap(..) |
            SdpAttribute::SctpPort(..) |
            SdpAttribute::Sendonly |
            SdpAttribute::Sendrecv |
            SdpAttribute::Setup(..) |
            SdpAttribute::Simulcast(..) |
            SdpAttribute::Ssrc(..) |
            SdpAttribute::SsrcGroup(..) => true,
        }
    }
}

impl fmt::Display for SdpAttribute {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let printable = match *self {
            SdpAttribute::BundleOnly => "bundle-only",
            SdpAttribute::Candidate(..) => "candidate",
            SdpAttribute::DtlsMessage{..} => "dtls-message",
            SdpAttribute::EndOfCandidates => "end-of-candidates",
            SdpAttribute::Extmap(..) => "extmap",
            SdpAttribute::Fingerprint(..) => "fingerprint",
            SdpAttribute::Fmtp(..) => "fmtp",
            SdpAttribute::Group(..) => "group",
            SdpAttribute::IceLite => "ice-lite",
            SdpAttribute::IceMismatch => "ice-mismatch",
            SdpAttribute::IceOptions(..) => "ice-options",
            SdpAttribute::IcePwd(..) => "ice-pwd",
            SdpAttribute::IceUfrag(..) => "ice-ufrag",
            SdpAttribute::Identity(..) => "identity",
            SdpAttribute::ImageAttr(..) => "imageattr",
            SdpAttribute::Inactive => "inactive",
            SdpAttribute::Label(..) => "label",
            SdpAttribute::MaxMessageSize(..) => "max-message-size",
            SdpAttribute::MaxPtime(..) => "max-ptime",
            SdpAttribute::Mid(..) => "mid",
            SdpAttribute::Msid(..) => "msid",
            SdpAttribute::MsidSemantic(..) => "msid-semantic",
            SdpAttribute::Ptime(..) => "ptime",
            SdpAttribute::Rid(..) => "rid",
            SdpAttribute::Recvonly => "recvonly",
            SdpAttribute::RemoteCandidate(..) => "remote-candidate",
            SdpAttribute::Rtpmap(..) => "rtpmap",
            SdpAttribute::Rtcp(..) => "rtcp",
            SdpAttribute::Rtcpfb(..) => "rtcp-fb",
            SdpAttribute::RtcpMux => "rtcp-mux",
            SdpAttribute::RtcpRsize => "rtcp-rsize",
            SdpAttribute::Sctpmap(..) => "sctpmap",
            SdpAttribute::SctpPort(..) => "sctp-port",
            SdpAttribute::Sendonly => "sendonly",
            SdpAttribute::Sendrecv => "sendrecv",
            SdpAttribute::Setup(..) => "setup",
            SdpAttribute::Simulcast(..) => "simulcast",
            SdpAttribute::Ssrc(..) => "ssrc",
            SdpAttribute::SsrcGroup(..) => "ssrc-group",
        };
        write!(f, "attribute: {}", printable)
    }
}
impl FromStr for SdpAttribute {
    type Err = SdpParserInternalError;

    fn from_str(line: &str) -> Result<Self, Self::Err> {
        let tokens: Vec<_> = line.splitn(2, ':').collect();
        let name = tokens[0].to_lowercase();
        let val = match tokens.get(1) {
            Some(x) => x.trim(),
            None => "",
        };
        if tokens.len() > 1 {
            match name.as_str() {
                "bundle-only" |
                "end-of-candidates" |
                "ice-lite" |
                "ice-mismatch" |
                "inactive" |
                "recvonly" |
                "rtcp-mux" |
                "rtcp-rsize" |
                "sendonly" |
                "sendrecv" => {
                    return Err(SdpParserInternalError::Generic(format!("{} attribute is not allowed to have a value",
                                                                       name)));
                }
                _ => (),
            }
        }
        match name.as_str() {
            "bundle-only" => Ok(SdpAttribute::BundleOnly),
            "dtls-message" => parse_dtls_message(val),
            "end-of-candidates" => Ok(SdpAttribute::EndOfCandidates),
            "ice-lite" => Ok(SdpAttribute::IceLite),
            "ice-mismatch" => Ok(SdpAttribute::IceMismatch),
            "ice-pwd" => Ok(SdpAttribute::IcePwd(string_or_empty(val)?)),
            "ice-ufrag" => Ok(SdpAttribute::IceUfrag(string_or_empty(val)?)),
            "identity" => Ok(SdpAttribute::Identity(string_or_empty(val)?)),
            "imageattr" => parse_image_attr(val),
            "inactive" => Ok(SdpAttribute::Inactive),
            "label" => Ok(SdpAttribute::Label(string_or_empty(val)?)),
            "max-message-size" => Ok(SdpAttribute::MaxMessageSize(val.parse()?)),
            "maxptime" => Ok(SdpAttribute::MaxPtime(val.parse()?)),
            "mid" => Ok(SdpAttribute::Mid(string_or_empty(val)?)),
            "msid-semantic" => parse_msid_semantic(val),
            "ptime" => Ok(SdpAttribute::Ptime(val.parse()?)),
            "rid" =>  parse_rid(val),
            "recvonly" => Ok(SdpAttribute::Recvonly),
            "rtcp-mux" => Ok(SdpAttribute::RtcpMux),
            "rtcp-rsize" => Ok(SdpAttribute::RtcpRsize),
            "sendonly" => Ok(SdpAttribute::Sendonly),
            "sendrecv" => Ok(SdpAttribute::Sendrecv),
            "ssrc-group" => Ok(SdpAttribute::SsrcGroup(string_or_empty(val)?)),
            "sctp-port" => parse_sctp_port(val),
            "candidate" => parse_candidate(val),
            "extmap" => parse_extmap(val),
            "fingerprint" => parse_fingerprint(val),
            "fmtp" => parse_fmtp(val),
            "group" => parse_group(val),
            "ice-options" => parse_ice_options(val),
            "msid" => parse_msid(val),
            "remote-candidates" => parse_remote_candidates(val),
            "rtpmap" => parse_rtpmap(val),
            "rtcp" => parse_rtcp(val),
            "rtcp-fb" => parse_rtcp_fb(val),
            "sctpmap" => parse_sctpmap(val),
            "setup" => parse_setup(val),
            "simulcast" => parse_simulcast(val),
            "ssrc" => parse_ssrc(val),
            _ => {
                Err(SdpParserInternalError::Unsupported(format!("Unknown attribute type {}", name)))
            }
        }
    }
}

#[derive(Clone, PartialEq)]
pub enum SdpAttributeType {
    BundleOnly,
    Candidate,
    DtlsMessage,
    EndOfCandidates,
    Extmap,
    Fingerprint,
    Fmtp,
    Group,
    IceLite,
    IceMismatch,
    IceOptions,
    IcePwd,
    IceUfrag,
    Identity,
    ImageAttr,
    Inactive,
    Label,
    MaxMessageSize,
    MaxPtime,
    Mid,
    Msid,
    MsidSemantic,
    Ptime,
    Rid,
    Recvonly,
    RemoteCandidate,
    Rtpmap,
    Rtcp,
    Rtcpfb,
    RtcpMux,
    RtcpRsize,
    Sctpmap,
    SctpPort,
    Sendonly,
    Sendrecv,
    Setup,
    Simulcast,
    Ssrc,
    SsrcGroup,
}

impl<'a> From<&'a SdpAttribute> for SdpAttributeType {
    fn from(other: &SdpAttribute) -> Self {
        match *other {
            SdpAttribute::BundleOnly{..} => SdpAttributeType::BundleOnly,
            SdpAttribute::Candidate{..} => SdpAttributeType::Candidate,
            SdpAttribute::DtlsMessage{..} => SdpAttributeType::DtlsMessage,
            SdpAttribute::EndOfCandidates{..} => SdpAttributeType::EndOfCandidates,
            SdpAttribute::Extmap{..} => SdpAttributeType::Extmap,
            SdpAttribute::Fingerprint{..} => SdpAttributeType::Fingerprint,
            SdpAttribute::Fmtp{..} => SdpAttributeType::Fmtp,
            SdpAttribute::Group{..} => SdpAttributeType::Group,
            SdpAttribute::IceLite{..} => SdpAttributeType::IceLite,
            SdpAttribute::IceMismatch{..} => SdpAttributeType::IceMismatch,
            SdpAttribute::IceOptions{..} => SdpAttributeType::IceOptions,
            SdpAttribute::IcePwd{..} => SdpAttributeType::IcePwd,
            SdpAttribute::IceUfrag{..} => SdpAttributeType::IceUfrag,
            SdpAttribute::Identity{..} => SdpAttributeType::Identity,
            SdpAttribute::ImageAttr{..} => SdpAttributeType::ImageAttr,
            SdpAttribute::Inactive{..} => SdpAttributeType::Inactive,
            SdpAttribute::Label{..} => SdpAttributeType::Label,
            SdpAttribute::MaxMessageSize{..} => SdpAttributeType::MaxMessageSize,
            SdpAttribute::MaxPtime{..} => SdpAttributeType::MaxPtime,
            SdpAttribute::Mid{..} => SdpAttributeType::Mid,
            SdpAttribute::Msid{..} => SdpAttributeType::Msid,
            SdpAttribute::MsidSemantic{..} => SdpAttributeType::MsidSemantic,
            SdpAttribute::Ptime{..} => SdpAttributeType::Ptime,
            SdpAttribute::Rid{..} => SdpAttributeType::Rid,
            SdpAttribute::Recvonly{..} => SdpAttributeType::Recvonly,
            SdpAttribute::RemoteCandidate{..} => SdpAttributeType::RemoteCandidate,
            SdpAttribute::Rtcp{..} => SdpAttributeType::Rtcp,
            SdpAttribute::Rtcpfb{..} => SdpAttributeType::Rtcpfb,
            SdpAttribute::RtcpMux{..} => SdpAttributeType::RtcpMux,
            SdpAttribute::RtcpRsize{..} => SdpAttributeType::RtcpRsize,
            SdpAttribute::Rtpmap{..} => SdpAttributeType::Rtpmap,
            SdpAttribute::Sctpmap{..} => SdpAttributeType::Sctpmap,
            SdpAttribute::SctpPort{..} => SdpAttributeType::SctpPort,
            SdpAttribute::Sendonly{..} => SdpAttributeType::Sendonly,
            SdpAttribute::Sendrecv{..} => SdpAttributeType::Sendrecv,
            SdpAttribute::Setup{..} => SdpAttributeType::Setup,
            SdpAttribute::Simulcast{..} => SdpAttributeType::Simulcast,
            SdpAttribute::Ssrc{..} => SdpAttributeType::Ssrc,
            SdpAttribute::SsrcGroup{..} => SdpAttributeType::SsrcGroup
        }
    }
}


fn string_or_empty(to_parse: &str) -> Result<String, SdpParserInternalError> {
    if to_parse.is_empty() {
        Err(SdpParserInternalError::Generic("This attribute is required to have a value"
                                                .to_string()))
    } else {
        Ok(to_parse.to_string())
    }
}

fn parse_payload_type(to_parse: &str) -> Result<SdpAttributePayloadType, SdpParserInternalError>
{
    Ok(match to_parse {
             "*" => SdpAttributePayloadType::Wildcard,
             _ => SdpAttributePayloadType::PayloadType(to_parse.parse::<u8>()?)
         })
}

fn parse_single_direction(to_parse: &str) -> Result<SdpSingleDirection, SdpParserInternalError> {
    match to_parse {
        "send" => Ok(SdpSingleDirection::Send),
        "recv" => Ok(SdpSingleDirection::Recv),
        x @ _ => Err(SdpParserInternalError::Generic(
            format!("Unknown direction description found: '{:}'",x).to_string()
        ))
    }
}

fn parse_sctp_port(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let port = to_parse.parse()?;
    if port > 65535 {
        return Err(SdpParserInternalError::Generic(format!("Sctpport port {} can only be a bit 16bit number",
                                                           port)));
    }
    Ok(SdpAttribute::SctpPort(port))
}

fn parse_candidate(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens: Vec<&str> = to_parse.split_whitespace().collect();
    if tokens.len() < 8 {
        return Err(SdpParserInternalError::Generic("Candidate needs to have minimum eigth tokens"
                                                       .to_string()));
    }
    let component = tokens[1].parse::<u32>()?;
    let transport = match tokens[2].to_lowercase().as_ref() {
        "udp" => SdpAttributeCandidateTransport::Udp,
        "tcp" => SdpAttributeCandidateTransport::Tcp,
        _ => {
            return Err(SdpParserInternalError::Generic("Unknonw candidate transport value"
                                                           .to_string()))
        }
    };
    let priority = tokens[3].parse::<u64>()?;
    let address = parse_unicast_addr(tokens[4])?;
    let port = tokens[5].parse::<u32>()?;
    if port > 65535 {
        return Err(SdpParserInternalError::Generic("ICE candidate port can only be a bit 16bit number".to_string()));
    }
    match tokens[6].to_lowercase().as_ref() {
        "typ" => (),
        _ => {
            return Err(SdpParserInternalError::Generic("Candidate attribute token must be 'typ'"
                                                           .to_string()))
        }
    };
    let cand_type = match tokens[7].to_lowercase().as_ref() {
        "host" => SdpAttributeCandidateType::Host,
        "srflx" => SdpAttributeCandidateType::Srflx,
        "prflx" => SdpAttributeCandidateType::Prflx,
        "relay" => SdpAttributeCandidateType::Relay,
        _ => return Err(SdpParserInternalError::Generic("Unknow candidate type value".to_string())),
    };
    let mut cand = SdpAttributeCandidate::new(tokens[0].to_string(),
                                              component,
                                              transport,
                                              priority,
                                              address,
                                              port,
                                              cand_type);
    if tokens.len() > 8 {
        let mut index = 8;
        while tokens.len() > index + 1 {
            match tokens[index].to_lowercase().as_ref() {
                "generation" => {
                    let generation = tokens[index + 1].parse::<u32>()?;
                    cand.set_generation(generation);
                    index += 2;
                }
                "network-cost" => {
                    let cost = tokens[index + 1].parse::<u32>()?;
                    cand.set_network_cost(cost);
                    index += 2;
                }
                "raddr" => {
                    let addr = parse_unicast_addr(tokens[index + 1])?;
                    cand.set_remote_address(addr);
                    index += 2;
                }
                "rport" => {
                    let port = tokens[index + 1].parse::<u32>()?;
                    if port > 65535 {
                        return Err(SdpParserInternalError::Generic( "ICE candidate rport can only be a bit 16bit number".to_string()));
                    }
                    cand.set_remote_port(port);
                    index += 2;
                }
                "tcptype" => {
                    cand.set_tcp_type(match tokens[index + 1].to_lowercase().as_ref() {
                                          "active" => SdpAttributeCandidateTcpType::Active,
                                          "passive" => SdpAttributeCandidateTcpType::Passive,
                                          "so" => SdpAttributeCandidateTcpType::Simultaneous,
                                          _ => {
                            return Err(SdpParserInternalError::Generic("Unknown tcptype value in candidate line".to_string()))
                        }
                                      });
                    index += 2;
                }
                "ufrag" => {
                    let ufrag = tokens[index + 1];
                    cand.set_ufrag(ufrag.to_string());
                    index += 2;
                }
                _ => {
                    return Err(SdpParserInternalError::Unsupported("Uknown candidate extension name"
                                                                       .to_string()))
                }
            };
        }
    }
    Ok(SdpAttribute::Candidate(cand))
}

fn parse_dtls_message(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens:Vec<&str> = to_parse.split(" ").collect();

    if tokens.len() != 2 {
        return Err(SdpParserInternalError::Generic(
            "dtls-message must have a role token and a value token.".to_string()
        ));
    }

    Ok(SdpAttribute::DtlsMessage(match tokens[0] {
        "client" => SdpAttributeDtlsMessage::Client(tokens[1].to_string()),
        "server" => SdpAttributeDtlsMessage::Server(tokens[1].to_string()),
        e @ _ => {
            return Err(SdpParserInternalError::Generic(
                format!("dtls-message has unknown role token '{}'",e).to_string()
            ));
        }
    }))
}

// ABNF for extmap is defined in RFC 5285
// https://tools.ietf.org/html/rfc5285#section-7
fn parse_extmap(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens: Vec<&str> = to_parse.split_whitespace().collect();
    if tokens.len() < 2 {
        return Err(SdpParserInternalError::Generic("Extmap needs to have at least two tokens"
                                                       .to_string()));
    }
    let id: u16;
    let mut direction: Option<SdpAttributeDirection> = None;
    if tokens[0].find('/') == None {
        id = tokens[0].parse::<u16>()?;
    } else {
        let id_dir: Vec<&str> = tokens[0].splitn(2, '/').collect();
        id = id_dir[0].parse::<u16>()?;
        direction = Some(match id_dir[1].to_lowercase().as_ref() {
                             "recvonly" => SdpAttributeDirection::Recvonly,
                             "sendonly" => SdpAttributeDirection::Sendonly,
                             "sendrecv" => SdpAttributeDirection::Sendrecv,
                             _ => {
                                 return Err(SdpParserInternalError::Generic("Unsupported direction in extmap value".to_string()))
                             }
                         })
    }
    // Consider replacing to_parse.split_whitespace() above with splitn on space. Would we want the pattern to split on any amout of any kind of whitespace?
    let extension_attributes = if tokens.len() == 2 {
        None
    } else {
        let ext_string: String = tokens[2..].join(" ");
        if !valid_byte_string(&ext_string) {
            return Err(SdpParserInternalError::Generic("Illegal character in extmap extension attributes".to_string()));
        }
        Some(ext_string)
    };
    Ok(SdpAttribute::Extmap(SdpAttributeExtmap {
                                id,
                                direction,
                                url: tokens[1].to_string(),
                                extension_attributes: extension_attributes,
                            }))
}

fn parse_fingerprint(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens: Vec<&str> = to_parse.split_whitespace().collect();
    if tokens.len() != 2 {
        return Err(SdpParserInternalError::Generic("Fingerprint needs to have two tokens"
                                                       .to_string()));
    }
    Ok(SdpAttribute::Fingerprint(SdpAttributeFingerprint {
                                     hash_algorithm: tokens[0].to_string(),
                                     fingerprint: tokens[1].to_string(),
                                 }))
}

fn parse_fmtp(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens: Vec<&str> = to_parse.splitn(2," ").collect();

    if tokens.len() != 2 {
        return Err(SdpParserInternalError::Unsupported(
            "Fmtp attributes require a payload type and a parameter block.".to_string()
        ));
    }

    let payload_token = tokens[0];
    let parameter_token = tokens[1];


    // Default initiliaze SdpAttributeFmtpParameters
    let mut parameters = SdpAttributeFmtpParameters{
        packetization_mode: 0,
        level_asymmetry_allowed: false,
        profile_level_id: 0x420010,
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
        encodings: Vec::new(),
        dtmf_tones: "".to_string(),
        unknown_tokens: Vec::new(),
    };

    if parameter_token.contains("=") {
        let parameter_tokens: Vec<&str> = parameter_token.split(";").collect();

        for parameter_token in parameter_tokens.iter() {
            let name_value_pair: Vec<&str> = parameter_token.splitn(2,"=").collect();

            if name_value_pair.len() != 2 {
                return Err(SdpParserInternalError::Generic(
                    "A fmtp parameter must be either a telephone event, a parameter list or
                                                                    a red codec list".to_string()
                ))
            }

            let parse_bool = |val: &str, param_name: &str| -> Result<bool,SdpParserInternalError> {
                match val.parse::<u8>()? {
                    0 => Ok(false),
                    1 => Ok(true),
                    _ => return Err(SdpParserInternalError::Generic(
                        format!("The fmtp parameter '{:}' must be 0 or 1", param_name)
                        .to_string()
                    ))
                }
            };

            let parameter_name = name_value_pair[0];
            let parameter_val = name_value_pair[1];

            match parameter_name.to_uppercase().as_str() {
                // H264
                "PROFILE-LEVEL-ID" => parameters.profile_level_id =
                                                match u32::from_str_radix(parameter_val,16)? {
                    x @ 0 ... 0xffffff => x,
                    _ => return Err(SdpParserInternalError::Generic(
                        "The fmtp parameter 'profile-level-id' must be in range [0,0xffffff]"
                        .to_string()
                    ))
                },
                "PACKETIZATION-MODE" => parameters.packetization_mode =
                                                   match parameter_val.parse::<u32>()? {
                    x @ 0...2 => x,
                    _ => return Err(SdpParserInternalError::Generic(
                        "The fmtp parameter 'packetization-mode' must be 0,1 or 2"
                        .to_string()
                    ))
                },
                "LEVEL-ASYMMETRY-ALLOWED" => parameters.level_asymmetry_allowed =
                                             parse_bool(parameter_val,"level-asymmetry-allowed")?,
                "MAX-MBPS" => parameters.max_mbps = parameter_val.parse::<u32>()?,
                "MAX-FS" => parameters.max_fs = parameter_val.parse::<u32>()?,
                "MAX-CPB" => parameters.max_cpb = parameter_val.parse::<u32>()?,
                "MAX-DPB" => parameters.max_dpb = parameter_val.parse::<u32>()?,
                "MAX-BR" => parameters.max_br = parameter_val.parse::<u32>()?,

                // VP8 and VP9
                "MAX-FR" => parameters.max_fr = parameter_val.parse::<u32>()?,

                //Opus
                "MAXPLAYBACKRATE" => parameters.maxplaybackrate = parameter_val.parse::<u32>()?,
                "USEDTX" => parameters.usedtx = parse_bool(parameter_val,"usedtx")?,
                "STEREO" => parameters.stereo = parse_bool(parameter_val,"stereo")?,
                "USEINBANDFEC" => parameters.useinbandfec =
                                             parse_bool(parameter_val,"useinbandfec")?,
                "CBR" => parameters.cbr = parse_bool(parameter_val,"cbr")?,
                _ => {
                    parameters.unknown_tokens.push(parameter_token.to_string())
                }
            }
        }
    } else {
        if parameter_token.contains("/") {
            let encodings: Vec<&str> = parameter_token.split("/").collect();

            for encoding in encodings {
                match encoding.parse::<u8>()? {
                    x @ 0...128 => parameters.encodings.push(x),
                    _ => return Err(SdpParserInternalError::Generic(
                        "Red codec must be in range [0,128]".to_string()
                    ))
                }
            }
        } else { // This is the case for the 'telephone-event' codec
            let dtmf_tones: Vec<&str> = parameter_token.split(",").collect();
            let mut dtmf_tone_is_ok = true;

            // This closure verifies the output of some_number_as_string.parse::<u8>().ok() like calls
            let validate_digits = |digit_option: Option<u8> | -> Option<u8> {
                match digit_option{
                    Some(x) => match x {
                        0...100 => Some(x),
                        _ => None,
                    },
                    None => None,
                }
            };

            // This loop does some sanity checking on the passed dtmf tones
            for dtmf_tone in dtmf_tones {
                let dtmf_tone_range: Vec<&str> = dtmf_tone.splitn(2,"-").collect();

                dtmf_tone_is_ok = match dtmf_tone_range.len() {
                    // In this case the dtmf tone is a range
                    2 => {
                        match validate_digits(dtmf_tone_range[0].parse::<u8>().ok()) {
                            Some(l) => match validate_digits(dtmf_tone_range[1].parse::<u8>().ok()) {
                                Some(u) => {
                                    // Check that the first part of the range is smaller than the second part
                                    l < u
                                },
                                None => false
                            },
                            None => false,
                        }
                    },
                     // In this case the dtmf tone is a single tone
                    1 => validate_digits(dtmf_tone.parse::<u8>().ok()).is_some(),
                    _ => false
                };

                if !dtmf_tone_is_ok {
                    break ;
                }
            }

            // Set the parsed dtmf tones or in case the parsing was insuccessfull, set it to the default "0-15"
            parameters.dtmf_tones = match dtmf_tone_is_ok{
                true => parameter_token.to_string(),
                false => "0-15".to_string()
            }
        }
    }

    Ok(SdpAttribute::Fmtp(SdpAttributeFmtp {
                              payload_type: payload_token.parse::<u8>()?,
                              parameters: parameters,
                          }))
}

fn parse_group(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let mut tokens = to_parse.split_whitespace();
    let semantics = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Group attribute is missing semantics token"
                                                           .to_string()))
        }
        Some(x) => {
            match x.to_uppercase().as_ref() {
                "LS" => SdpAttributeGroupSemantic::LipSynchronization,
                "FID" => SdpAttributeGroupSemantic::FlowIdentification,
                "SRF" => SdpAttributeGroupSemantic::SingleReservationFlow,
                "ANAT" => SdpAttributeGroupSemantic::AlternateNetworkAddressType,
                "FEC" => SdpAttributeGroupSemantic::ForwardErrorCorrection,
                "DDP" => SdpAttributeGroupSemantic::DecodingDependency,
                "BUNDLE" => SdpAttributeGroupSemantic::Bundle,
                unknown @ _ => {
                    return Err(SdpParserInternalError::Unsupported(
                        format!("Unknown group semantic '{:?}' found", unknown)
                    ))
                }
            }
        }
    };
    Ok(SdpAttribute::Group(SdpAttributeGroup {
                               semantics,
                               tags: tokens.map(|x| x.to_string()).collect(),
                           }))
}

fn parse_ice_options(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    if to_parse.is_empty() {
        return Err(SdpParserInternalError::Generic("ice-options is required to have a value"
                                                       .to_string()));
    }
    Ok(SdpAttribute::IceOptions(to_parse.split_whitespace().map(|x| x.to_string()).collect()))
}

fn parse_imageattr_tokens(to_parse: &str, separator: char) -> Vec<String> {
    let mut tokens = Vec::new();
    let mut open_braces_counter = 0;
    let mut current_tokens = Vec::new();

    for token in to_parse.split(separator) {
        if token.contains("[") {
            open_braces_counter+=1;
        }
        if token.contains("]") {
            open_braces_counter-=1;
        }

        current_tokens.push(token.to_string());

        if open_braces_counter == 0 {
            tokens.push(current_tokens.join(&separator.to_string()));
            current_tokens = Vec::new();
        }
    }

    tokens
}

fn parse_imagettr_braced_token(to_parse: &str) -> Option<&str> {
    if !to_parse.ends_with("]") {
        return None;
    }

    Some(&to_parse[1..to_parse.len()-1])
}

fn parse_image_attr_xyrange(to_parse: &str) -> Result<SdpAttributeImageAttrXYRange,
                                                      SdpParserInternalError> {
    if to_parse.starts_with("[") {
        let value_tokens = parse_imagettr_braced_token(to_parse).ok_or(
            SdpParserInternalError::Generic(
                "imageattr's xyrange has no closing tag ']'".to_string()
            )
        )?;

        if to_parse.contains(":") {
            // Range values
            let range_tokens:Vec<&str> = value_tokens.split(":").collect();

            if range_tokens.len() == 3 {
                Ok(SdpAttributeImageAttrXYRange::Range(
                    range_tokens[0].parse::<u32>()?,
                    range_tokens[2].parse::<u32>()?,
                    Some(range_tokens[1].parse::<u32>()?),
                ))
            } else if range_tokens.len() == 2 {
                Ok(SdpAttributeImageAttrXYRange::Range(
                    range_tokens[0].parse::<u32>()?,
                    range_tokens[1].parse::<u32>()?,
                    None
                ))
            } else {
                return Err(SdpParserInternalError::Generic(
                    "imageattr's xyrange must contain 2 or 3 fields".to_string()
                ))
            }
        } else {
            // Discrete values
            let values = value_tokens.split(",")
                                     .map(|x| x.parse::<u32>())
                                     .collect::<Result<Vec<u32>, _>>()?;

            if values.len() < 2 {
                return Err(SdpParserInternalError::Generic(
                    "imageattr's discrete value list must have at least two elements".to_string()
                ))
            }

            Ok(SdpAttributeImageAttrXYRange::DiscreteValues(values))
        }

    } else {
        Ok(SdpAttributeImageAttrXYRange::DiscreteValues(vec![to_parse.parse::<u32>()?]))
    }
}

fn parse_image_attr_set(to_parse: &str) -> Result<SdpAttributeImageAttrSet,
                                                  SdpParserInternalError> {
    let mut tokens = parse_imageattr_tokens(to_parse, ',').into_iter();

    let x_token = tokens.next().ok_or(SdpParserInternalError::Generic(
        "imageattr set is missing the 'x=' token".to_string()
    ))?;
    if !x_token.starts_with("x=") {
        return Err(SdpParserInternalError::Generic(
            "The first token in an imageattr set must begin with 'x='".to_string()
        ))
    }
    let x = parse_image_attr_xyrange(&x_token[2..])?;


    let y_token = tokens.next().ok_or(SdpParserInternalError::Generic(
        "imageattr set is missing the 'y=' token".to_string()
    ))?;
    if !y_token.starts_with("y=") {
        return Err(SdpParserInternalError::Generic(
            "The second token in an imageattr set must begin with 'y='".to_string()
        ))
    }
    let y = parse_image_attr_xyrange(&y_token[2..])?;

    let mut sar = None;
    let mut par = None;
    let mut q = None;

    let parse_ps_range = |resolution_range: &str| -> Result<(f32, f32), SdpParserInternalError> {
        let minmax_pair:Vec<&str> = resolution_range.split("-").collect();

        if minmax_pair.len() != 2 {
            return Err(SdpParserInternalError::Generic(
                "imageattr's par and sar ranges must have two components".to_string()
            ))
        }

        let min = minmax_pair[0].parse::<f32>()?;
        let max = minmax_pair[1].parse::<f32>()?;

        if min >= max {
            return Err(SdpParserInternalError::Generic(
                "In imageattr's par and sar ranges, first must be < than the second".to_string()
            ))
        }

        Ok((min,max))
    };

    while let Some(current_token) = tokens.next() {
        if current_token.starts_with("sar=") {
            let value_token = &current_token[4..];
            if value_token.starts_with("[") {
                let sar_values = parse_imagettr_braced_token(value_token).ok_or(
                    SdpParserInternalError::Generic(
                        "imageattr's sar value is missing closing tag ']'".to_string()
                    )
                )?;

                if value_token.contains("-") {
                    // Range
                    let range = parse_ps_range(sar_values)?;
                    sar = Some(SdpAttributeImageAttrSRange::Range(range.0,range.1))
                } else if value_token.contains(",") {
                    // Discrete values
                    let values = sar_values.split(",")
                                             .map(|x| x.parse::<f32>())
                                             .collect::<Result<Vec<f32>, _>>()?;

                    if values.len() < 2 {
                        return Err(SdpParserInternalError::Generic(
                            "imageattr's sar discrete value list must have at least two values"
                            .to_string()
                        ))
                    }

                    // Check that all the values are ascending
                    let mut last_value = 0.0;
                    for value in &values {
                        if last_value >= *value {
                            return Err(SdpParserInternalError::Generic(
                                "imageattr's sar discrete value list must contain ascending values"
                                .to_string()
                            ))
                        }
                        last_value = *value;
                    }
                    sar = Some(SdpAttributeImageAttrSRange::DiscreteValues(values))
                }
            } else {
                sar = Some(SdpAttributeImageAttrSRange::DiscreteValues(
                    vec![value_token.parse::<f32>()?])
                )
            }
        } else if current_token.starts_with("par=") {
            let braced_value_token = &current_token[4..];
            if !braced_value_token.starts_with("[") {
                return Err(SdpParserInternalError::Generic(
                    "imageattr's par value must start with '['".to_string()
                ))
            }

            let par_values = parse_imagettr_braced_token(braced_value_token).ok_or(
                SdpParserInternalError::Generic(
                    "imageattr's par value must be enclosed with ']'".to_string()
                )
            )?;
            let range = parse_ps_range(par_values)?;
            par = Some(SdpAttributeImageAttrPRange {
                min: range.0,
                max: range.1,
            })
        } else if current_token.starts_with("q=") {
            q = Some(current_token[2..].parse::<f32>()?);
        }
    }

    Ok(SdpAttributeImageAttrSet {
        x,
        y,
        sar,
        par,
        q
    })
}

fn parse_image_attr_set_list<I>(tokens: &mut iter::Peekable<I>)
                                -> Result<SdpAttributeImageAttrSetList, SdpParserInternalError>
                            where I: Iterator<Item = String> + Clone {
    let parse_set = |set_token:&str| -> Result<SdpAttributeImageAttrSet, SdpParserInternalError> {
        Ok(parse_image_attr_set(parse_imagettr_braced_token(set_token).ok_or(
            SdpParserInternalError::Generic(
                "imageattr sets must be enclosed by ']'".to_string()
        ))?)?)
    };

    match tokens.next().ok_or(SdpParserInternalError::Generic(
        "imageattr must have a parameter set after a direction token".to_string()
    ))?.as_str() {
        "*" => Ok(SdpAttributeImageAttrSetList::Wildcard),
        x => {
            let mut sets = vec![parse_set(x)?];
            while let Some(set_str) = tokens.clone().peek() {
                if set_str.starts_with("[") {
                    sets.push(parse_set(&tokens.next().unwrap())?);
                } else {
                    break;
                }
            }

            Ok(SdpAttributeImageAttrSetList::Sets(sets))
        }
    }
}

fn parse_image_attr(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let mut tokens = parse_imageattr_tokens(to_parse, ' ').into_iter().peekable();

    let pt = parse_payload_type(tokens.next().ok_or(SdpParserInternalError::Generic(
        "imageattr requires a payload token".to_string()
    ))?.as_str())?;
    let first_direction = parse_single_direction(tokens.next().ok_or(
        SdpParserInternalError::Generic(
            "imageattr's second token must be a direction token".to_string()
    ))?.as_str())?;

    let first_set_list = parse_image_attr_set_list(&mut tokens)?;

    let mut second_set_list = SdpAttributeImageAttrSetList::Sets(Vec::new());

    // Check if there is a second direction defined
    if let Some(direction_token) = tokens.next() {
        if parse_single_direction(direction_token.as_str())? == first_direction {
            return Err(SdpParserInternalError::Generic(
                "imageattr's second direction token must be different from the first one"
                .to_string()
            ))
        }

        second_set_list = parse_image_attr_set_list(&mut tokens)?;
    }

    if let Some(_) = tokens.next() {
        return Err(SdpParserInternalError::Generic(
            "imageattr must not contain any token after the second set list".to_string()
        ))
    }

    Ok(SdpAttribute::ImageAttr(match first_direction {
        SdpSingleDirection::Send =>
            SdpAttributeImageAttr {
                pt,
                send: first_set_list,
                recv: second_set_list,
            },
        SdpSingleDirection::Recv =>
            SdpAttributeImageAttr {
                pt,
                send: second_set_list,
                recv: first_set_list,
            }
    }))
}

fn parse_msid(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let mut tokens = to_parse.split_whitespace();
    let id = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Msid attribute is missing msid-id token"
                                                           .to_string()))
        }
        Some(x) => x.to_string(),
    };
    let appdata = match tokens.next() {
        None => None,
        Some(x) => Some(x.to_string()),
    };
    Ok(SdpAttribute::Msid(SdpAttributeMsid { id, appdata }))

}

fn parse_msid_semantic(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens: Vec<_> = to_parse.split_whitespace().collect();
    if tokens.len() < 1 {
        return Err(SdpParserInternalError::Generic("Msid-semantic attribute is missing msid-semantic token"
                                                   .to_string()));
    }
    // TODO: Should msids be checked to ensure they are non empty?
    let semantic = SdpAttributeMsidSemantic {
        semantic: tokens[0].to_string(),
        msids: tokens[1..].iter().map(|x| x.to_string()).collect(),
    };
    Ok(SdpAttribute::MsidSemantic(semantic))
}

fn parse_rid(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens: Vec<&str> = to_parse.splitn(3, " ").collect();

    if tokens.len() < 2 {
        return Err(SdpParserInternalError::Generic(
                "A rid attribute must at least have an id and a direction token.".to_string()
        ));
    }

    // Default initilize
    let mut params = SdpAttributeRidParameters {
        max_width: 0,
        max_height: 0,
        max_fps: 0,
        max_fs: 0,
        max_br: 0,
        max_pps: 0,
        unknown: Vec::new(),
    };
    let mut formats: Vec<u16> = Vec::new();
    let mut depends: Vec<String> = Vec::new();


    if let Some(param_token) = tokens.get(2) {
        let mut parameters = param_token.split(";").peekable();

        // The 'pt' parameter must be the first parameter if present, so it
        // cannot be checked along with the other parameters below
        if let Some(maybe_fmt_parameter) = parameters.clone().peek() {
            if maybe_fmt_parameter.starts_with("pt=") {
                let fmt_list = maybe_fmt_parameter[3..].split(",");
                for fmt in fmt_list {
                    formats.push(fmt.trim().parse::<u16>()?);
                }

                parameters.next();
            }
        }

        for param in parameters {
            // TODO: Bug 1225877. Add support for params without '='
            let param_value_pair: Vec<&str> = param.splitn(2,"=").collect();
            if param_value_pair.len() != 2 {
                return Err(SdpParserInternalError::Generic(
                    "A rid parameter needs to be of form 'param=value'".to_string()
                ));
            }

            match param_value_pair[0] {
                "max-width" => params.max_width = param_value_pair[1].parse::<u32>()?,
                "max-height" => params.max_height = param_value_pair[1].parse::<u32>()?,
                "max-fps" => params.max_fps = param_value_pair[1].parse::<u32>()?,
                "max-fs" => params.max_fs = param_value_pair[1].parse::<u32>()?,
                "max-br" => params.max_br = param_value_pair[1].parse::<u32>()?,
                "max-pps" => params.max_pps = param_value_pair[1].parse::<u32>()?,
                "depends" => {
                    depends.extend(param_value_pair[1].split(",").map(|x| x.to_string()));
                },
                _ => params.unknown.push(param.to_string()),
            }
        }
    }

    Ok(SdpAttribute::Rid(SdpAttributeRid{
        id: tokens[0].to_string(),
        direction: parse_single_direction(tokens[1])?,
        formats: formats,
        params: params,
        depends: depends,
    }))
}

fn parse_remote_candidates(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let mut tokens = to_parse.split_whitespace();
    let component = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                           "Remote-candidate attribute is missing component ID"
                               .to_string(),
                       ))
        }
        Some(x) => x.parse::<u32>()?,
    };
    let address = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                           "Remote-candidate attribute is missing connection address"
                               .to_string(),
                       ))
        }
        Some(x) => parse_unicast_addr(x)?,
    };
    let port = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic(
                           "Remote-candidate attribute is missing port number".to_string(),
                       ))
        }
        Some(x) => x.parse::<u32>()?,
    };
    if port > 65535 {
        return Err(SdpParserInternalError::Generic(
                       "Remote-candidate port can only be a bit 16bit number".to_string(),
                   ));
    };
    Ok(SdpAttribute::RemoteCandidate(SdpAttributeRemoteCandidate {
                                         component,
                                         address,
                                         port,
                                     }))
}

fn parse_rtpmap(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let mut tokens = to_parse.split_whitespace();
    let payload_type: u8 = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Rtpmap missing payload type".to_string()))
        }
        Some(x) => {
            let pt = x.parse::<u8>()?;
            if pt > 127 {
                return Err(SdpParserInternalError::Generic("Rtpmap payload type must be less then 127".to_string()));
            };
            pt
        }
    };
    let mut parameters = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Rtpmap missing payload type".to_string()))
        }
        Some(x) => x.split('/'),
    };
    let name = match parameters.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Rtpmap missing codec name".to_string()))
        }
        Some(x) => x.to_string(),
    };
    let frequency = match parameters.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Rtpmap missing codec name".to_string()))
        }
        Some(x) => x.parse::<u32>()?,
    };
    let mut rtpmap = SdpAttributeRtpmap::new(payload_type, name, frequency);
    match parameters.next() {
        Some(x) => rtpmap.set_channels(x.parse::<u32>()?),
        None => (),
    };
    Ok(SdpAttribute::Rtpmap(rtpmap))
}

fn parse_rtcp(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let mut tokens = to_parse.split_whitespace();
    let port = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Rtcp attribute is missing port number"
                                                           .to_string()))
        }
        Some(x) => x.parse::<u16>()?,
    };
    let mut rtcp = SdpAttributeRtcp::new(port);
    match tokens.next() {
        None => (),
        Some(x) => {
            parse_nettype(x)?;
            match tokens.next() {
                None => {
                    return Err(SdpParserInternalError::Generic(
                                   "Rtcp attribute is missing address type token"
                                       .to_string(),
                               ))
                }
                Some(x) => {
                    let addrtype = parse_addrtype(x)?;
                    let addr = match tokens.next() {
                        None => {
                            return Err(SdpParserInternalError::Generic(
                                           "Rtcp attribute is missing ip address token"
                                               .to_string(),
                                       ))
                        }
                        Some(x) => {
                            let addr = parse_unicast_addr(x)?;
                            if !addrtype.same_protocol(&addr) {
                                return Err(SdpParserInternalError::Generic(
                                               "Failed to parse unicast address attribute.\
                                              addrtype does not match address."
                                                       .to_string(),
                                           ));
                            }
                            addr
                        }
                    };
                    rtcp.set_addr(addr);
                }
            };
        }
    };
    Ok(SdpAttribute::Rtcp(rtcp))
}

fn parse_rtcp_fb(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens: Vec<&str> = to_parse.splitn(4,' ').collect();

    // Parse this in advance to use it later in the parameter switch
    let feedback_type = match tokens.get(1) {
        Some(x) => match x.as_ref(){
            "ack" => SdpAttributeRtcpFbType::Ack,
            "ccm" => SdpAttributeRtcpFbType::Ccm,
            "nack" => SdpAttributeRtcpFbType::Nack,
            "trr-int" => SdpAttributeRtcpFbType::TrrInt,
            "goog-remb" => SdpAttributeRtcpFbType::Remb,
            "transport-cc" => SdpAttributeRtcpFbType::TransCC,
            _ => {
                return Err(SdpParserInternalError::Unsupported(
                    format!("Unknown rtcpfb feedback type: {:?}",x).to_string()
                ))
            }
        },
        None => {
            return Err(SdpParserInternalError::Generic(
                           "Error parsing rtcpfb: no feedback type".to_string(),
                       ))
        }
    };

    // Parse this in advance to make the initilization block below better readable
    let parameter = match &feedback_type {
        &SdpAttributeRtcpFbType::Ack => match tokens.get(2) {
            Some(x) => match x.as_ref() {
                "rpsi" | "app"  => x.to_string(),
                _ => {
                    return Err(SdpParserInternalError::Unsupported(
                        format!("Unknown rtcpfb ack parameter: {:?}",x).to_string()
                    ))
                },
            },
            None => {
                return Err(SdpParserInternalError::Unsupported(
                    format!("The rtcpfb ack feeback type needs a parameter:").to_string()
                ))
            }
        },
        &SdpAttributeRtcpFbType::Ccm => match tokens.get(2) {
            Some(x) => match x.as_ref() {
                "fir" | "tmmbr" | "tstr" | "vbcm"  => x.to_string(),
                _ => {
                    return Err(SdpParserInternalError::Unsupported(
                        format!("Unknown rtcpfb ccm parameter: {:?}",x).to_string()
                    ))
                },
            },
            None => "".to_string(),
        },
        &SdpAttributeRtcpFbType::Nack => match tokens.get(2) {
            Some(x) => match x.as_ref() {
                "sli" | "pli" | "rpsi" | "app"  => x.to_string(),
                _ => {
                    return Err(SdpParserInternalError::Unsupported(
                        format!("Unknown rtcpfb nack parameter: {:?}",x).to_string()
                    ))
                },
            },
            None => "".to_string(),
        },
        &SdpAttributeRtcpFbType::TrrInt => match tokens.get(2) {
            Some(x) => match x {
                _ if x.parse::<u32>().is_ok() => x.to_string(),
                _ => {
                    return Err(SdpParserInternalError::Generic(
                        format!("Unknown rtcpfb trr-int parameter: {:?}",x).to_string()
                    ))
                },
            },
            None => {
                    return Err(SdpParserInternalError::Generic(
                        format!("The rtcpfb trr-int feedback type needs a parameter").to_string()
                    ))
            }
        },
        &SdpAttributeRtcpFbType::Remb => match tokens.get(2) {
            Some(x) => match x {
                _ => {
                    return Err(SdpParserInternalError::Unsupported(
                        format!("Unknown rtcpfb remb parameter: {:?}",x).to_string()
                    ))
                },
            },
            None => "".to_string(),
        },
        &SdpAttributeRtcpFbType::TransCC => match tokens.get(2) {
            Some(x) => match x {
                _ => {
                    return Err(SdpParserInternalError::Unsupported(
                        format!("Unknown rtcpfb transport-cc parameter: {:?}",x).to_string()
                    ))
                },
            },
            None => "".to_string(),
        }
    };


    Ok(SdpAttribute::Rtcpfb(SdpAttributeRtcpFb {
                                payload_type: parse_payload_type(tokens[0])?,

                                feedback_type: feedback_type,

                                parameter: parameter,

                                extra: match tokens.get(3) {
                                    Some(x) => x.to_string(),
                                    None => "".to_string(),
                                },
                            }))
}

fn parse_sctpmap(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let tokens: Vec<&str> = to_parse.split_whitespace().collect();
    if tokens.len() != 3 {
        return Err(SdpParserInternalError::Generic("Sctpmap needs to have three tokens"
                                                       .to_string()));
    }
    let port = tokens[0].parse::<u16>()?;
    if tokens[1].to_lowercase() != "webrtc-datachannel" {
        return Err(SdpParserInternalError::Generic("Unsupported sctpmap type token".to_string()));
    }
    Ok(SdpAttribute::Sctpmap(SdpAttributeSctpmap {
                                 port,
                                 channels: tokens[2].parse::<u32>()?,
                             }))
}

fn parse_setup(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    Ok(SdpAttribute::Setup(match to_parse.to_lowercase().as_ref() {
                               "active" => SdpAttributeSetup::Active,
                               "actpass" => SdpAttributeSetup::Actpass,
                               "holdconn" => SdpAttributeSetup::Holdconn,
                               "passive" => SdpAttributeSetup::Passive,
                               _ => {
                                   return Err(SdpParserInternalError::Generic(
                                                  "Unsupported setup value".to_string(),
                                              ))
                               }
                           }))
}

fn parse_simulcast_version_list(to_parse: &str) -> Result<Vec<SdpAttributeSimulcastVersion>,
                                                          SdpParserInternalError> {
    let make_version_list = |to_parse: &str| {
       to_parse.split(';').map(SdpAttributeSimulcastVersion::new).collect()
    };
    if to_parse.contains("=") {
       let mut descriptor_versionlist_pair = to_parse.splitn(2,"=");
       match descriptor_versionlist_pair.next().unwrap() {
           // TODO Bug 1470568
           "rid" => Ok(make_version_list(descriptor_versionlist_pair.next().unwrap())),
           descriptor @ _ => {
               return Err(SdpParserInternalError::Generic(
                   format!("Simulcast attribute has unknown list descriptor '{:?}'",
                           descriptor)
                   .to_string()
               ))
           }
       }
    } else {
       Ok(make_version_list(to_parse))
    }
}

fn parse_simulcast(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    // TODO: Bug 1225877: Stop accepting all kinds of whitespace here, and only accept SP
    let mut tokens = to_parse.trim().split_whitespace();
    let first_direction = match tokens.next() {
        Some(x) => parse_single_direction(x)?,
        None => {
            return Err(SdpParserInternalError::Generic(
                           "Simulcast attribute is missing send/recv value".to_string(),
                       ))
        }
    };

    let first_version_list = match tokens.next() {
        Some(x) => {
            parse_simulcast_version_list(x)?
        },
        None => {
            return Err(SdpParserInternalError::Generic(
                    "Simulcast attribute must have an alternatives list after the direction token"
                    .to_string(),
            ));
        }
    };

    let mut second_version_list = Vec::new();
    if let Some(x) = tokens.next() {
        if parse_single_direction(x)? == first_direction {
            return Err(SdpParserInternalError::Generic(
                "Simulcast attribute has defined two times the same direction".to_string()
            ));
        }

        second_version_list = match tokens.next() {
            Some(x) => {
                parse_simulcast_version_list(x)?
            },
            None => {
                return Err(SdpParserInternalError::Generic(
                    format!("{:?}{:?}",
                            "Simulcast has defined a second direction but",
                            "no second list of simulcast stream versions")
                    .to_string()
                ));
            }
        }
    }

    Ok(SdpAttribute::Simulcast(match first_direction {
        SdpSingleDirection::Send => SdpAttributeSimulcast {
            send: first_version_list,
            receive: second_version_list,
        },
        SdpSingleDirection::Recv => SdpAttributeSimulcast {
            send: second_version_list,
            receive: first_version_list,
        },
    }))
}

fn parse_ssrc(to_parse: &str) -> Result<SdpAttribute, SdpParserInternalError> {
    let mut tokens = to_parse.splitn(2,' ');
    let ssrc_id = match tokens.next() {
        None => {
            return Err(SdpParserInternalError::Generic("Ssrc attribute is missing ssrc-id value"
                                                           .to_string()))
        }
        Some(x) => x.parse::<u32>()?,
    };
    let mut ssrc = SdpAttributeSsrc::new(ssrc_id);
    match tokens.next() {
        None => (),
        Some(x) => ssrc.set_attribute(x),
    };
    Ok(SdpAttribute::Ssrc(ssrc))
}

pub fn parse_attribute(value: &str) -> Result<SdpType, SdpParserInternalError> {
    Ok(SdpType::Attribute(value.trim().parse()?))
}

#[test]
fn test_parse_attribute_candidate() {
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ host").is_ok());
    assert!(parse_attribute("candidate:foo 1 UDP 2122252543 172.16.156.106 49760 typ host")
                .is_ok());
    assert!(parse_attribute("candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host").is_ok());
    assert!(parse_attribute("candidate:0 1 TCP 2122252543 ::1 49760 typ host").is_ok());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ srflx").is_ok());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ prflx").is_ok());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ relay").is_ok());
    assert!(
        parse_attribute(
            "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host tcptype active"
        ).is_ok()
    );
    assert!(
        parse_attribute(
            "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host tcptype passive"
        ).is_ok()
    );
    assert!(
        parse_attribute("candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host tcptype so")
            .is_ok()
    );
    assert!(
        parse_attribute("candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host ufrag foobar")
            .is_ok()
    );
    assert!(
        parse_attribute(
            "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host network-cost 50"
        ).is_ok()
    );
    assert!(parse_attribute("candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 generation 0").is_ok());
    assert!(parse_attribute("candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665").is_ok());
    assert!(parse_attribute("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive").is_ok());
    assert!(parse_attribute("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive generation 1").is_ok());
    assert!(parse_attribute("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive generation 1 ufrag +DGd").is_ok());
    assert!(parse_attribute("candidate:1 1 TCP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 tcptype passive generation 1 ufrag +DGd network-cost 1").is_ok());

    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ").is_err());
    assert!(parse_attribute("candidate:0 foo UDP 2122252543 172.16.156.106 49760 typ host")
                .is_err());
    assert!(parse_attribute("candidate:0 1 FOO 2122252543 172.16.156.106 49760 typ host").is_err());
    assert!(parse_attribute("candidate:0 1 UDP foo 172.16.156.106 49760 typ host").is_err());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156 49760 typ host").is_err());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 70000 typ host").is_err());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 type host")
                .is_err());
    assert!(parse_attribute("candidate:0 1 UDP 2122252543 172.16.156.106 49760 typ fost").is_err());
    // FIXME this should fail without the extra 'foobar' at the end
    assert!(
        parse_attribute(
            "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host unsupported foobar"
        ).is_err()
    );
    assert!(parse_attribute("candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 61665 generation B").is_err());
    assert!(
        parse_attribute(
            "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host network-cost C"
        ).is_err()
    );
    assert!(
        parse_attribute(
            "candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1 rport 61665"
        ).is_err()
    );
    assert!(
        parse_attribute(
            "candidate:0 1 TCP 2122252543 172.16.156.106 49760 typ host tcptype foobar"
        ).is_err()
    );
    assert!(
        parse_attribute(
            "candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1 rport 61665"
        ).is_err()
    );
    assert!(parse_attribute("candidate:1 1 UDP 1685987071 24.23.204.141 54609 typ srflx raddr 192.168.1.4 rport 70000").is_err());
}

#[test]
fn test_parse_dtls_message() {
    let check_parse = |x| -> SdpAttributeDtlsMessage {
        if let Ok(SdpType::Attribute(SdpAttribute::DtlsMessage(x))) = parse_attribute(x) {
            x
        } else {
            unreachable!();
        }
    };

    assert!(parse_attribute("dtls-message:client SGVsbG8gV29ybGQ=").is_ok());
    assert!(parse_attribute("dtls-message:server SGVsbG8gV29ybGQ=").is_ok());
    assert!(parse_attribute("dtls-message:client IGlzdCBl/W4gUeiBtaXQg+JSB1bmQCAkJJkSNEQ=").is_ok());
    assert!(parse_attribute("dtls-message:server IGlzdCBl/W4gUeiBtaXQg+JSB1bmQCAkJJkSNEQ=").is_ok());

    let mut dtls_message = check_parse("dtls-message:client SGVsbG8gV29ybGQ=");
    match dtls_message {
        SdpAttributeDtlsMessage::Client(x) => {
            assert_eq!(x, "SGVsbG8gV29ybGQ=");
        },
        _ => { unreachable!(); }
    }

    dtls_message = check_parse("dtls-message:server SGVsbG8gV29ybGQ=");
    match dtls_message {
        SdpAttributeDtlsMessage::Server(x) => {
            assert_eq!(x, "SGVsbG8gV29ybGQ=");
        },
        _ => { unreachable!(); }
    }


    assert!(parse_attribute("dtls-message:client").is_err());
    assert!(parse_attribute("dtls-message:server").is_err());
}

#[test]
fn test_parse_attribute_end_of_candidates() {
    assert!(parse_attribute("end-of-candidates").is_ok());
    assert!(parse_attribute("end-of-candidates foobar").is_err());
}

#[test]
fn test_parse_attribute_extmap() {
    assert!(parse_attribute("extmap:1/sendonly urn:ietf:params:rtp-hdrext:ssrc-audio-level")
                .is_ok());
    assert!(parse_attribute("extmap:2/sendrecv urn:ietf:params:rtp-hdrext:ssrc-audio-level")
                .is_ok());
    assert!(parse_attribute("extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time")
                .is_ok());

    assert!(parse_attribute("extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time ext_attributes")
            .is_ok());

    assert!(parse_attribute("extmap:a/sendrecv urn:ietf:params:rtp-hdrext:ssrc-audio-level")
                .is_err());
    assert!(parse_attribute("extmap:4/unsupported urn:ietf:params:rtp-hdrext:ssrc-audio-level")
                .is_err());

    let mut bad_char = String::from("extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time ",);
    bad_char.push(0x00 as char);
    assert!(parse_attribute(&bad_char).is_err());
}

#[test]
fn test_parse_attribute_fingerprint() {
    assert!(parse_attribute("fingerprint:sha-256 CD:34:D1:62:16:95:7B:B7:EB:74:E2:39:27:97:EB:0B:23:73:AC:BC:BF:2F:E3:91:CB:57:A9:9D:4A:A2:0B:40").is_ok())
}

#[test]
fn test_parse_attribute_fmtp() {
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000;stereo=1;useinbandfec=1").is_ok());
    assert!(parse_attribute("fmtp:66 0-15").is_ok());
    assert!(parse_attribute("fmtp:109 0-15,66").is_ok());
    assert!(parse_attribute("fmtp:66 111/115").is_ok());
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000;stereo=1;useinbandfec=1").is_ok());
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000; stereo=1; useinbandfec=1").is_ok());
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000; stereo=1;useinbandfec=1").is_ok());
    assert!(parse_attribute("fmtp:8 maxplaybackrate=48000").is_ok());

    assert!(parse_attribute("fmtp:77 ").is_err());
    assert!(parse_attribute("fmtp:109 maxplaybackrate=48000stereo=1;").is_err());
    assert!(parse_attribute("fmtp:8 ;maxplaybackrate=48000").is_err());
}

#[test]
fn test_parse_attribute_group() {
    assert!(parse_attribute("group:LS").is_ok());
    assert!(parse_attribute("group:LS 1 2").is_ok());
    assert!(parse_attribute("group:BUNDLE sdparta_0 sdparta_1 sdparta_2").is_ok());

    assert!(parse_attribute("group:").is_err());
    assert!(match parse_attribute("group:NEVER_SUPPORTED_SEMANTICS") {
        Err(SdpParserInternalError::Unsupported(_)) => true,
        _ => false
    })
}

#[test]
fn test_parse_attribute_bundle_only() {
    assert!(parse_attribute("bundle-only").is_ok());
    assert!(parse_attribute("bundle-only foobar").is_err());
}

#[test]
fn test_parse_attribute_ice_lite() {
    assert!(parse_attribute("ice-lite").is_ok());

    assert!(parse_attribute("ice-lite foobar").is_err());
}

#[test]
fn test_parse_attribute_ice_mismatch() {
    assert!(parse_attribute("ice-mismatch").is_ok());

    assert!(parse_attribute("ice-mismatch foobar").is_err());
}

#[test]
fn test_parse_attribute_ice_options() {
    assert!(parse_attribute("ice-options:trickle").is_ok());

    assert!(parse_attribute("ice-options:").is_err());
}

#[test]
fn test_parse_attribute_ice_pwd() {
    assert!(parse_attribute("ice-pwd:e3baa26dd2fa5030d881d385f1e36cce").is_ok());

    assert!(parse_attribute("ice-pwd:").is_err());
}

#[test]
fn test_parse_attribute_ice_ufrag() {
    assert!(parse_attribute("ice-ufrag:58b99ead").is_ok());

    assert!(parse_attribute("ice-ufrag:").is_err());
}

#[test]
fn test_parse_attribute_identity() {
    assert!(parse_attribute("identity:eyJpZHAiOnsiZG9tYWluIjoiZXhhbXBsZS5vcmciLCJwcm90b2NvbCI6ImJvZ3VzIn0sImFzc2VydGlvbiI6IntcImlkZW50aXR5XCI6XCJib2JAZXhhbXBsZS5vcmdcIixcImNvbnRlbnRzXCI6XCJhYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3l6XCIsXCJzaWduYXR1cmVcIjpcIjAxMDIwMzA0MDUwNlwifSJ9").is_ok());

    assert!(parse_attribute("identity:").is_err());
}

#[test]
fn test_parse_attribute_imageattr() {
    let check_parse = |x| -> SdpAttributeImageAttr {
        if let Ok(SdpType::Attribute(SdpAttribute::ImageAttr(x))) = parse_attribute(x) {
            x
        } else {
            unreachable!();
        }
    };

    assert!(parse_attribute("imageattr:120 send * recv *").is_ok());
    assert!(parse_attribute("imageattr:99 send [x=320,y=240] recv [x=320,y=240]").is_ok());
    assert!(parse_attribute("imageattr:97 send [x=800,y=640,sar=1.1,q=0.6] [x=480,y=320] recv [x=330,y=250]").is_ok());
    assert!(parse_attribute("imageattr:97 recv [x=800,y=640,sar=1.1] send [x=330,y=250]").is_ok());
    assert!(parse_attribute("imageattr:97 send [x=[480:16:800],y=[320:16:640],par=[1.2-1.3],q=0.6] [x=[176:8:208],y=[144:8:176],par=[1.2-1.3]] recv *").is_ok());

    let mut imageattr = check_parse("imageattr:* recv [x=800,y=[50,80,30],sar=1.1] send [x=330,y=250,sar=[1.1,1.3,1.9],q=0.1]");
    assert_eq!(imageattr.pt, SdpAttributePayloadType::Wildcard);
    match imageattr.recv {
        SdpAttributeImageAttrSetList::Sets(sets) => {
            assert_eq!(sets.len(), 1);

            let set = &sets[0];
            assert_eq!(set.x, SdpAttributeImageAttrXYRange::DiscreteValues(vec![800]));
            assert_eq!(set.y, SdpAttributeImageAttrXYRange::DiscreteValues(vec![50,80,30]));
            assert_eq!(set.par, None);
            assert_eq!(set.sar, Some(SdpAttributeImageAttrSRange::DiscreteValues(vec![1.1])));
            assert_eq!(set.q, None);

        },
        _ => { unreachable!(); }
    }
    match imageattr.send {
        SdpAttributeImageAttrSetList::Sets(sets) => {
            assert_eq!(sets.len(), 1);

            let set = &sets[0];
            assert_eq!(set.x, SdpAttributeImageAttrXYRange::DiscreteValues(vec![330]));
            assert_eq!(set.y, SdpAttributeImageAttrXYRange::DiscreteValues(vec![250]));
            assert_eq!(set.par, None);
            assert_eq!(set.sar, Some(SdpAttributeImageAttrSRange::DiscreteValues(vec![1.1,1.3,1.9])));
            assert_eq!(set.q, Some(0.1));
        },
        _ => { unreachable!(); }
    }

    imageattr = check_parse("imageattr:97 send [x=[480:16:800],y=[100,200,300],par=[1.2-1.3],q=0.6] [x=1080,y=[144:176],sar=[0.5-0.7]] recv *");
    assert_eq!(imageattr.pt, SdpAttributePayloadType::PayloadType(97));
    match imageattr.send {
        SdpAttributeImageAttrSetList::Sets(sets) => {
            assert_eq!(sets.len(), 2);

            let first_set = &sets[0];
            assert_eq!(first_set.x, SdpAttributeImageAttrXYRange::Range(480, 800, Some(16)));
            assert_eq!(first_set.y, SdpAttributeImageAttrXYRange::DiscreteValues(vec![100, 200, 300]));
            assert_eq!(first_set.par, Some(SdpAttributeImageAttrPRange {
                min: 1.2,
                max: 1.3
            }));
            assert_eq!(first_set.sar, None);
            assert_eq!(first_set.q, Some(0.6));

            let second_set = &sets[1];
            assert_eq!(second_set.x, SdpAttributeImageAttrXYRange::DiscreteValues(vec![1080]));
            assert_eq!(second_set.y, SdpAttributeImageAttrXYRange::Range(144, 176, None));
            assert_eq!(second_set.par, None);
            assert_eq!(second_set.sar, Some(SdpAttributeImageAttrSRange::Range(0.5, 0.7)));
            assert_eq!(second_set.q, None);
        },
        _ => { unreachable!(); }
    }
    assert_eq!(imageattr.recv, SdpAttributeImageAttrSetList::Wildcard);

    assert!(parse_attribute("imageattr:99 send [x=320,y=240]").is_ok());
    assert!(parse_attribute("imageattr:100 recv [x=320,y=240]").is_ok());
    assert!(parse_attribute("imageattr:97 recv [x=800,y=640,sar=1.1,foo=[123,456],q=0.5] send [x=330,y=250,bar=foo,sar=[20-40]]").is_ok());
    assert!(parse_attribute("imageattr:97 recv [x=800,y=640,sar=1.1,foo=abc xyz,q=0.5] send [x=330,y=250,bar=foo,sar=[20-40]]").is_ok());

    assert!(parse_attribute("imageattr:").is_err());
    assert!(parse_attribute("imageattr:100").is_err());
    assert!(parse_attribute("imageattr:120 send * recv * send *").is_err());
    assert!(parse_attribute("imageattr:97 send [x=800,y=640,sar=1.1] send [x=330,y=250]").is_err());
}

#[test]
fn test_parse_attribute_inactive() {
    assert!(parse_attribute("inactive").is_ok());
    assert!(parse_attribute("inactive foobar").is_err());
}

#[test]
fn test_parse_attribute_label() {
    assert!(parse_attribute("label:1").is_ok());
    assert!(parse_attribute("label:foobar").is_ok());
    assert!(parse_attribute("label:foobar barfoo").is_ok());

    assert!(parse_attribute("label:").is_err());
}

#[test]
fn test_parse_attribute_maxptime() {
    assert!(parse_attribute("maxptime:60").is_ok());

    assert!(parse_attribute("maxptime:").is_err());
}

#[test]
fn test_parse_attribute_mid() {
    assert!(parse_attribute("mid:sdparta_0").is_ok());
    assert!(parse_attribute("mid:sdparta_0 sdparta_1 sdparta_2").is_ok());

    assert!(parse_attribute("mid:").is_err());
}

#[test]
fn test_parse_attribute_msid() {
    assert!(parse_attribute("msid:{5a990edd-0568-ac40-8d97-310fc33f3411}").is_ok());
    assert!(
        parse_attribute(
            "msid:{5a990edd-0568-ac40-8d97-310fc33f3411} {218cfa1c-617d-2249-9997-60929ce4c405}"
        ).is_ok()
    );

    assert!(parse_attribute("msid:").is_err());
}

#[test]
fn test_parse_attribute_msid_semantics() {
    assert!(parse_attribute("msid-semantic:WMS *").is_ok())
}

#[test]
fn test_parse_attribute_ptime() {
    assert!(parse_attribute("ptime:30").is_ok());

    assert!(parse_attribute("ptime:").is_err());
}

#[test]
fn test_parse_attribute_rid() {

    let check_parse = |x| -> SdpAttributeRid {
        if let Ok(SdpType::Attribute(SdpAttribute::Rid(x))) = parse_attribute(x) {
            x
        } else {
            unreachable!();
        }
    };

    // assert!(parse_attribute("rid:foo send").is_ok());
    let mut rid = check_parse("rid:foo send");
    assert_eq!(rid.id, "foo");
    assert_eq!(rid.direction, SdpSingleDirection::Send);


    // assert!(parse_attribute("rid:110 send pt=9").is_ok());
    rid = check_parse("rid:110 send pt=9");
    assert_eq!(rid.id, "110");
    assert_eq!(rid.direction, SdpSingleDirection::Send);
    assert_eq!(rid.formats, vec![9]);

    assert!(parse_attribute("rid:foo send pt=10").is_ok());
    assert!(parse_attribute("rid:110 send pt=9,10").is_ok());
    assert!(parse_attribute("rid:110 send pt=9,10;max-fs=10").is_ok());
    assert!(parse_attribute("rid:110 send pt=9,10;max-width=10;depends=1,2,3").is_ok());

    // assert!(parse_attribute("rid:110 send pt=9,10;max-fs=10;UNKNOWN=100;depends=1,2,3").is_ok());
    rid = check_parse("rid:110 send pt=9,10;max-fs=10;UNKNOWN=100;depends=1,2,3");
    assert_eq!(rid.id, "110");
    assert_eq!(rid.direction, SdpSingleDirection::Send);
    assert_eq!(rid.formats, vec![9,10]);
    assert_eq!(rid.params.max_fs, 10);
    assert_eq!(rid.params.unknown, vec!["UNKNOWN=100"]);
    assert_eq!(rid.depends, vec!["1","2","3"]);

    assert!(parse_attribute("rid:110 send pt=9, 10;max-fs=10;UNKNOWN=100; depends=1, 2, 3").is_ok());
    assert!(parse_attribute("rid:110 send max-fs=10").is_ok());
    assert!(parse_attribute("rid:110 recv max-width=1920;max-height=1080").is_ok());

    // assert!(parse_attribute("rid:110 recv max-fps=42;max-fs=10;max-br=3;max-pps=1000").is_ok());
    rid = check_parse("rid:110 recv max-fps=42;max-fs=10;max-br=3;max-pps=1000");
    assert_eq!(rid.id, "110");
    assert_eq!(rid.direction, SdpSingleDirection::Recv);
    assert_eq!(rid.params.max_fps, 42);
    assert_eq!(rid.params.max_fs, 10);
    assert_eq!(rid.params.max_br, 3);
    assert_eq!(rid.params.max_pps, 1000);

    assert!(parse_attribute("rid:110 recv max-mbps=420;max-cpb=3;max-dpb=3").is_ok());
    assert!(parse_attribute("rid:110 recv scale-down-by=1.35;depends=1,2,3").is_ok());
    assert!(parse_attribute("rid:110 recv max-width=10;depends=1,2,3").is_ok());
    assert!(parse_attribute("rid:110 recv max-fs=10;UNKNOWN=100;depends=1,2,3").is_ok());

    assert!(parse_attribute("rid:").is_err());
    assert!(parse_attribute("rid:120 send pt=").is_err());
    assert!(parse_attribute("rid:120 send pt=;max-width=10").is_err());
    assert!(parse_attribute("rid:120 send pt=9;max-width=").is_err());
    assert!(parse_attribute("rid:120 send pt=9;max-width=;max-width=10").is_err());
}

#[test]
fn test_parse_attribute_recvonly() {
    assert!(parse_attribute("recvonly").is_ok());
    assert!(parse_attribute("recvonly foobar").is_err());
}

#[test]
fn test_parse_attribute_remote_candidate() {
    assert!(parse_attribute("remote-candidates:0 10.0.0.1 5555").is_ok());
    assert!(parse_attribute("remote-candidates:12345 ::1 5555").is_ok());

    assert!(parse_attribute("remote-candidates:abc 10.0.0.1 5555").is_err());
    assert!(parse_attribute("remote-candidates:0 10.a.0.1 5555").is_err());
    assert!(parse_attribute("remote-candidates:0 10.0.0.1 70000").is_err());
    assert!(parse_attribute("remote-candidates:0 10.0.0.1").is_err());
    assert!(parse_attribute("remote-candidates:0").is_err());
    assert!(parse_attribute("remote-candidates:").is_err());
}

#[test]
fn test_parse_attribute_sendonly() {
    assert!(parse_attribute("sendonly").is_ok());
    assert!(parse_attribute("sendonly foobar").is_err());
}

#[test]
fn test_parse_attribute_sendrecv() {
    assert!(parse_attribute("sendrecv").is_ok());
    assert!(parse_attribute("sendrecv foobar").is_err());
}

#[test]
fn test_parse_attribute_setup() {
    assert!(parse_attribute("setup:active").is_ok());
    assert!(parse_attribute("setup:passive").is_ok());
    assert!(parse_attribute("setup:actpass").is_ok());
    assert!(parse_attribute("setup:holdconn").is_ok());

    assert!(parse_attribute("setup:").is_err());
    assert!(parse_attribute("setup:foobar").is_err());
}

#[test]
fn test_parse_attribute_rtcp() {
    assert!(parse_attribute("rtcp:5000").is_ok());
    assert!(parse_attribute("rtcp:9 IN IP4 0.0.0.0").is_ok());

    assert!(parse_attribute("rtcp:").is_err());
    assert!(parse_attribute("rtcp:70000").is_err());
    assert!(parse_attribute("rtcp:9 IN").is_err());
    assert!(parse_attribute("rtcp:9 IN IP4").is_err());
    assert!(parse_attribute("rtcp:9 IN IP4 ::1").is_err());
}

#[test]
fn test_parse_attribute_rtcp_fb() {
    assert!(parse_attribute("rtcp-fb:101 ack rpsi").is_ok());
    assert!(parse_attribute("rtcp-fb:101 ack app").is_ok());
    assert!(parse_attribute("rtcp-fb:101 ccm").is_ok());
    assert!(parse_attribute("rtcp-fb:101 ccm fir").is_ok());
    assert!(parse_attribute("rtcp-fb:101 ccm tmmbr").is_ok());
    assert!(parse_attribute("rtcp-fb:101 ccm tstr").is_ok());
    assert!(parse_attribute("rtcp-fb:101 ccm vbcm").is_ok());
    assert!(parse_attribute("rtcp-fb:101 nack").is_ok());
    assert!(parse_attribute("rtcp-fb:101 nack sli").is_ok());
    assert!(parse_attribute("rtcp-fb:101 nack pli").is_ok());
    assert!(parse_attribute("rtcp-fb:101 nack rpsi").is_ok());
    assert!(parse_attribute("rtcp-fb:101 nack app").is_ok());
    assert!(parse_attribute("rtcp-fb:101 trr-int 1").is_ok());
    assert!(parse_attribute("rtcp-fb:101 goog-remb").is_ok());
    assert!(parse_attribute("rtcp-fb:101 transport-cc").is_ok());

    assert!(parse_attribute("rtcp-fb:101 unknown").is_err());
    assert!(parse_attribute("rtcp-fb:101 ack").is_err());
    assert!(parse_attribute("rtcp-fb:101 ccm unknwon").is_err());
    assert!(parse_attribute("rtcp-fb:101 nack unknown").is_err());
    assert!(parse_attribute("rtcp-fb:101 trr-int").is_err());
    assert!(parse_attribute("rtcp-fb:101 trr-int a").is_err());
    assert!(parse_attribute("rtcp-fb:101 goog-remb unknown").is_err());
    assert!(parse_attribute("rtcp-fb:101 transport-cc unknown").is_err());
}

#[test]
fn test_parse_attribute_rtcp_mux() {
    assert!(parse_attribute("rtcp-mux").is_ok());
    assert!(parse_attribute("rtcp-mux foobar").is_err());
}

#[test]
fn test_parse_attribute_rtcp_rsize() {
    assert!(parse_attribute("rtcp-rsize").is_ok());
    assert!(parse_attribute("rtcp-rsize foobar").is_err());
}

#[test]
fn test_parse_attribute_rtpmap() {
    assert!(parse_attribute("rtpmap:109 opus/48000").is_ok());
    assert!(parse_attribute("rtpmap:109 opus/48000/2").is_ok());

    assert!(parse_attribute("rtpmap:109 ").is_err());
    assert!(parse_attribute("rtpmap:109 opus").is_err());
    assert!(parse_attribute("rtpmap:128 opus/48000").is_err());
}

#[test]
fn test_parse_attribute_sctpmap() {
    assert!(parse_attribute("sctpmap:5000 webrtc-datachannel 256").is_ok());

    assert!(parse_attribute("sctpmap:70000 webrtc-datachannel 256").is_err());
    assert!(parse_attribute("sctpmap:5000 unsupported 256").is_err());
    assert!(parse_attribute("sctpmap:5000 webrtc-datachannel 2a").is_err());
}

#[test]
fn test_parse_attribute_sctp_port() {
    assert!(parse_attribute("sctp-port:5000").is_ok());

    assert!(parse_attribute("sctp-port:").is_err());
    assert!(parse_attribute("sctp-port:70000").is_err());
}

#[test]
fn test_parse_attribute_max_message_size() {
    assert!(parse_attribute("max-message-size:1").is_ok());
    assert!(parse_attribute("max-message-size:100000").is_ok());
    assert!(parse_attribute("max-message-size:4294967297").is_ok());
    assert!(parse_attribute("max-message-size:0").is_ok());

    assert!(parse_attribute("max-message-size:").is_err());
    assert!(parse_attribute("max-message-size:abc").is_err());
}

#[test]
fn test_parse_attribute_simulcast() {
    assert!(parse_attribute("simulcast:send 1").is_ok());
    assert!(parse_attribute("simulcast:recv test").is_ok());
    assert!(parse_attribute("simulcast:recv ~test").is_ok());
    assert!(parse_attribute("simulcast:recv test;foo").is_ok());
    assert!(parse_attribute("simulcast:recv foo,bar").is_ok());
    assert!(parse_attribute("simulcast:recv foo,bar;test").is_ok());
    assert!(parse_attribute("simulcast:recv 1;4,5 send 6;7").is_ok());
    assert!(parse_attribute("simulcast:send 1,2,3;~4,~5 recv 6;~7,~8").is_ok());
    // old draft 03 notation used by Firefox 55
    assert!(parse_attribute("simulcast: send rid=foo;bar").is_ok());

    assert!(parse_attribute("simulcast:").is_err());
    assert!(parse_attribute("simulcast:send").is_err());
    assert!(parse_attribute("simulcast:foobar 1").is_err());
    assert!(parse_attribute("simulcast:send 1 foobar 2").is_err());
    // old draft 03 notation used by Firefox 55
    assert!(parse_attribute("simulcast: send foo=8;10").is_err());
}

#[test]
fn test_parse_attribute_ssrc() {
    assert!(parse_attribute("ssrc:2655508255").is_ok());
    assert!(parse_attribute("ssrc:2655508255 foo").is_ok());
    assert!(parse_attribute("ssrc:2655508255 cname:{735484ea-4f6c-f74a-bd66-7425f8476c2e}")
                .is_ok());
    assert!(parse_attribute("ssrc:2082260239 msid:1d0cdb4e-5934-4f0f-9f88-40392cb60d31 315b086a-5cb6-4221-89de-caf0b038c79d")
                .is_ok());

    assert!(parse_attribute("ssrc:").is_err());
    assert!(parse_attribute("ssrc:foo").is_err());
}

#[test]
fn test_parse_attribute_ssrc_group() {
    assert!(parse_attribute("ssrc-group:FID 3156517279 2673335628").is_ok())
}

#[test]
fn test_parse_unknown_attribute() {
    assert!(parse_attribute("unknown").is_err())
}

// Returns true if valid byte-string as defined by RFC 4566
// https://tools.ietf.org/html/rfc4566
fn valid_byte_string(input: &str) -> bool {
    !(input.contains(0x00 as char) || input.contains(0x0A as char) || input.contains(0x0D as char))
}
