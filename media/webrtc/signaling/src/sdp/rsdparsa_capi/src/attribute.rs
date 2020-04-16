/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use libc::{c_float, size_t};
use std::ptr;
use std::slice;

use nserror::{nsresult, NS_ERROR_INVALID_ARG, NS_OK};
use rsdparsa::attribute_type::*;
use rsdparsa::SdpSession;

use network::{RustAddress, RustExplicitlyTypedAddress};
use types::StringView;

#[no_mangle]
pub unsafe extern "C" fn num_attributes(session: *const SdpSession) -> u32 {
    (*session).attribute.len() as u32
}

#[no_mangle]
pub unsafe extern "C" fn get_attribute_ptr(
    session: *const SdpSession,
    index: u32,
    ret: *mut *const SdpAttribute,
) -> nsresult {
    match (*session).attribute.get(index as usize) {
        Some(attribute) => {
            *ret = attribute as *const SdpAttribute;
            NS_OK
        }
        None => NS_ERROR_INVALID_ARG,
    }
}

fn count_attribute(attributes: &[SdpAttribute], search: SdpAttributeType) -> usize {
    let mut count = 0;
    for attribute in (*attributes).iter() {
        if SdpAttributeType::from(attribute) == search {
            count += 1;
        }
    }
    count
}

fn argsearch(attributes: &[SdpAttribute], attribute_type: SdpAttributeType) -> Option<usize> {
    for (i, attribute) in (*attributes).iter().enumerate() {
        if SdpAttributeType::from(attribute) == attribute_type {
            return Some(i);
        }
    }
    None
}

pub unsafe fn has_attribute(
    attributes: *const Vec<SdpAttribute>,
    attribute_type: SdpAttributeType,
) -> bool {
    argsearch((*attributes).as_slice(), attribute_type).is_some()
}

fn get_attribute(
    attributes: &[SdpAttribute],
    attribute_type: SdpAttributeType,
) -> Option<&SdpAttribute> {
    argsearch(attributes, attribute_type).map(|i| &attributes[i])
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum RustSdpAttributeDtlsMessageType {
    Client,
    Server,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeDtlsMessage {
    pub role: u8,
    pub value: StringView,
}

impl<'a> From<&'a SdpAttributeDtlsMessage> for RustSdpAttributeDtlsMessage {
    fn from(other: &SdpAttributeDtlsMessage) -> Self {
        match other {
            &SdpAttributeDtlsMessage::Client(ref x) => RustSdpAttributeDtlsMessage {
                role: RustSdpAttributeDtlsMessageType::Client as u8,
                value: StringView::from(x.as_str()),
            },
            &SdpAttributeDtlsMessage::Server(ref x) => RustSdpAttributeDtlsMessage {
                role: RustSdpAttributeDtlsMessageType::Server as u8,
                value: StringView::from(x.as_str()),
            },
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_dtls_message(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut RustSdpAttributeDtlsMessage,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::DtlsMessage);
    if let Some(&SdpAttribute::DtlsMessage(ref dtls_message)) = attr {
        *ret = RustSdpAttributeDtlsMessage::from(dtls_message);
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_iceufrag(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut StringView,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::IceUfrag);
    if let Some(&SdpAttribute::IceUfrag(ref string)) = attr {
        *ret = StringView::from(string.as_str());
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_icepwd(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut StringView,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::IcePwd);
    if let Some(&SdpAttribute::IcePwd(ref string)) = attr {
        *ret = StringView::from(string.as_str());
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_identity(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut StringView,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::Identity);
    if let Some(&SdpAttribute::Identity(ref string)) = attr {
        *ret = StringView::from(string.as_str());
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_iceoptions(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut *const Vec<String>,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::IceOptions);
    if let Some(&SdpAttribute::IceOptions(ref options)) = attr {
        *ret = options;
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_maxptime(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut u64,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::MaxPtime);
    if let Some(&SdpAttribute::MaxPtime(ref max_ptime)) = attr {
        *ret = *max_ptime;
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeFingerprint {
    hash_algorithm: u16,
    fingerprint: *const Vec<u8>,
}

impl<'a> From<&'a SdpAttributeFingerprint> for RustSdpAttributeFingerprint {
    fn from(other: &SdpAttributeFingerprint) -> Self {
        RustSdpAttributeFingerprint {
            hash_algorithm: other.hash_algorithm as u16,
            fingerprint: &other.fingerprint,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_fingerprint_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Fingerprint)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_fingerprints(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_fingerprints: *mut RustSdpAttributeFingerprint,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Fingerprint(ref data) = *x {
                Some(RustSdpAttributeFingerprint::from(data))
            } else {
                None
            }
        })
        .collect();
    let fingerprints = slice::from_raw_parts_mut(ret_fingerprints, ret_size);
    fingerprints.copy_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone)]
pub enum RustSdpAttributeSetup {
    Active,
    Actpass,
    Holdconn,
    Passive,
}

impl<'a> From<&'a SdpAttributeSetup> for RustSdpAttributeSetup {
    fn from(other: &SdpAttributeSetup) -> Self {
        match *other {
            SdpAttributeSetup::Active => RustSdpAttributeSetup::Active,
            SdpAttributeSetup::Actpass => RustSdpAttributeSetup::Actpass,
            SdpAttributeSetup::Holdconn => RustSdpAttributeSetup::Holdconn,
            SdpAttributeSetup::Passive => RustSdpAttributeSetup::Passive,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_setup(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut RustSdpAttributeSetup,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::Setup);
    if let Some(&SdpAttribute::Setup(ref setup)) = attr {
        *ret = RustSdpAttributeSetup::from(setup);
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeSsrc {
    pub id: u32,
    pub attribute: StringView,
    pub value: StringView,
}

impl<'a> From<&'a SdpAttributeSsrc> for RustSdpAttributeSsrc {
    fn from(other: &SdpAttributeSsrc) -> Self {
        RustSdpAttributeSsrc {
            id: other.id,
            attribute: StringView::from(&other.attribute),
            value: StringView::from(&other.value),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_ssrc_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Ssrc)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_ssrcs(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_ssrcs: *mut RustSdpAttributeSsrc,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Ssrc(ref data) = *x {
                Some(RustSdpAttributeSsrc::from(data))
            } else {
                None
            }
        })
        .collect();
    let ssrcs = slice::from_raw_parts_mut(ret_ssrcs, ret_size);
    ssrcs.copy_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum RustSdpSsrcGroupSemantic {
    Duplication,
    FlowIdentification,
    ForwardErrorCorrection,
    ForwardErrorCorrectionFR,
    SIM,
}

impl<'a> From<&'a SdpSsrcGroupSemantic> for RustSdpSsrcGroupSemantic {
    fn from(other: &SdpSsrcGroupSemantic) -> Self {
        match *other {
            SdpSsrcGroupSemantic::Duplication => RustSdpSsrcGroupSemantic::Duplication,
            SdpSsrcGroupSemantic::FlowIdentification => {
                RustSdpSsrcGroupSemantic::FlowIdentification
            }
            SdpSsrcGroupSemantic::ForwardErrorCorrection => {
                RustSdpSsrcGroupSemantic::ForwardErrorCorrection
            }
            SdpSsrcGroupSemantic::ForwardErrorCorrectionFR => {
                RustSdpSsrcGroupSemantic::ForwardErrorCorrectionFR
            }
            SdpSsrcGroupSemantic::SIM => RustSdpSsrcGroupSemantic::SIM,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpSsrcGroup {
    pub semantic: RustSdpSsrcGroupSemantic,
    pub ssrcs: *const Vec<SdpAttributeSsrc>,
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_ssrc_group_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::SsrcGroup)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_ssrc_groups(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_ssrc_groups: *mut RustSdpSsrcGroup,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::SsrcGroup(ref semantic, ref ssrcs) = *x {
                Some(RustSdpSsrcGroup {
                    semantic: RustSdpSsrcGroupSemantic::from(semantic),
                    ssrcs: ssrcs,
                })
            } else {
                None
            }
        })
        .collect();
    let ssrc_groups = slice::from_raw_parts_mut(ret_ssrc_groups, ret_size);
    ssrc_groups.copy_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeRtpmap {
    pub payload_type: u8,
    pub codec_name: StringView,
    pub frequency: u32,
    pub channels: u32,
}

impl<'a> From<&'a SdpAttributeRtpmap> for RustSdpAttributeRtpmap {
    fn from(other: &SdpAttributeRtpmap) -> Self {
        RustSdpAttributeRtpmap {
            payload_type: other.payload_type as u8,
            codec_name: StringView::from(other.codec_name.as_str()),
            frequency: other.frequency as u32,
            channels: other.channels.unwrap_or(0),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_rtpmap_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Rtpmap)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_rtpmaps(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_rtpmaps: *mut RustSdpAttributeRtpmap,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Rtpmap(ref data) = *x {
                Some(RustSdpAttributeRtpmap::from(data))
            } else {
                None
            }
        })
        .collect();
    let rtpmaps = slice::from_raw_parts_mut(ret_rtpmaps, ret_size);
    rtpmaps.copy_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustRtxFmtpParameters {
    pub apt: u8,
    pub has_rtx_time: bool,
    pub rtx_time: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeFmtpParameters {
    // H264
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

    // telephone-event
    pub dtmf_tones: StringView,

    // RTX
    pub rtx: RustRtxFmtpParameters,

    // Red
    pub encodings: *const Vec<u8>,

    // Unknown
    pub unknown_tokens: *const Vec<String>,
}

impl<'a> From<&'a SdpAttributeFmtpParameters> for RustSdpAttributeFmtpParameters {
    fn from(other: &SdpAttributeFmtpParameters) -> Self {
        let rtx = if let Some(rtx) = other.rtx {
            RustRtxFmtpParameters {
                apt: rtx.apt,
                has_rtx_time: rtx.rtx_time.is_some(),
                rtx_time: rtx.rtx_time.unwrap_or(0),
            }
        } else {
            RustRtxFmtpParameters {
                apt: 0,
                has_rtx_time: false,
                rtx_time: 0,
            }
        };

        RustSdpAttributeFmtpParameters {
            packetization_mode: other.packetization_mode,
            level_asymmetry_allowed: other.level_asymmetry_allowed,
            profile_level_id: other.profile_level_id,
            max_fs: other.max_fs,
            max_cpb: other.max_cpb,
            max_dpb: other.max_dpb,
            max_br: other.max_br,
            max_mbps: other.max_mbps,
            usedtx: other.usedtx,
            stereo: other.stereo,
            useinbandfec: other.useinbandfec,
            cbr: other.cbr,
            max_fr: other.max_fr,
            maxplaybackrate: other.maxplaybackrate,
            dtmf_tones: StringView::from(other.dtmf_tones.as_str()),
            rtx,
            encodings: &other.encodings,
            unknown_tokens: &other.unknown_tokens,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeFmtp {
    pub payload_type: u8,
    pub codec_name: StringView,
    pub parameters: RustSdpAttributeFmtpParameters,
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_fmtp_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Fmtp)
}

fn find_payload_type(attributes: &[SdpAttribute], payload_type: u8) -> Option<&SdpAttributeRtpmap> {
    attributes
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Rtpmap(ref data) = *x {
                if data.payload_type == payload_type {
                    Some(data)
                } else {
                    None
                }
            } else {
                None
            }
        })
        .next()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_fmtp(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_fmtp: *mut RustSdpAttributeFmtp,
) -> size_t {
    let fmtps = (*attributes).iter().filter_map(|x| {
        if let SdpAttribute::Fmtp(ref data) = *x {
            Some(data)
        } else {
            None
        }
    });
    let mut rust_fmtps = Vec::new();
    for fmtp in fmtps {
        if let Some(rtpmap) = find_payload_type((*attributes).as_slice(), fmtp.payload_type) {
            rust_fmtps.push(RustSdpAttributeFmtp {
                payload_type: fmtp.payload_type as u8,
                codec_name: StringView::from(rtpmap.codec_name.as_str()),
                parameters: RustSdpAttributeFmtpParameters::from(&fmtp.parameters),
            });
        }
    }
    let fmtps = if ret_size <= rust_fmtps.len() {
        slice::from_raw_parts_mut(ret_fmtp, ret_size)
    } else {
        slice::from_raw_parts_mut(ret_fmtp, rust_fmtps.len())
    };
    fmtps.copy_from_slice(rust_fmtps.as_slice());
    fmtps.len()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_ptime(attributes: *const Vec<SdpAttribute>) -> i64 {
    for attribute in (*attributes).iter() {
        if let SdpAttribute::Ptime(time) = *attribute {
            return time as i64;
        }
    }
    -1
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_max_msg_size(attributes: *const Vec<SdpAttribute>) -> i64 {
    for attribute in (*attributes).iter() {
        if let SdpAttribute::MaxMessageSize(max_msg_size) = *attribute {
            return max_msg_size as i64;
        }
    }
    -1
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_sctp_port(attributes: *const Vec<SdpAttribute>) -> i64 {
    for attribute in (*attributes).iter() {
        if let SdpAttribute::SctpPort(port) = *attribute {
            return port as i64;
        }
    }
    -1
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeFlags {
    pub ice_lite: bool,
    pub rtcp_mux: bool,
    pub rtcp_rsize: bool,
    pub bundle_only: bool,
    pub end_of_candidates: bool,
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_attribute_flags(
    attributes: *const Vec<SdpAttribute>,
) -> RustSdpAttributeFlags {
    let mut ret = RustSdpAttributeFlags {
        ice_lite: false,
        rtcp_mux: false,
        rtcp_rsize: false,
        bundle_only: false,
        end_of_candidates: false,
    };
    for attribute in (*attributes).iter() {
        if let SdpAttribute::IceLite = *attribute {
            ret.ice_lite = true;
        } else if let SdpAttribute::RtcpMux = *attribute {
            ret.rtcp_mux = true;
        } else if let SdpAttribute::RtcpRsize = *attribute {
            ret.rtcp_rsize = true;
        } else if let SdpAttribute::BundleOnly = *attribute {
            ret.bundle_only = true;
        } else if let SdpAttribute::EndOfCandidates = *attribute {
            ret.end_of_candidates = true;
        }
    }
    ret
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_mid(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut StringView,
) -> nsresult {
    for attribute in (*attributes).iter() {
        if let SdpAttribute::Mid(ref data) = *attribute {
            *ret = StringView::from(data.as_str());
            return NS_OK;
        }
    }
    NS_ERROR_INVALID_ARG
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeMsid {
    id: StringView,
    appdata: StringView,
}

impl<'a> From<&'a SdpAttributeMsid> for RustSdpAttributeMsid {
    fn from(other: &SdpAttributeMsid) -> Self {
        RustSdpAttributeMsid {
            id: StringView::from(other.id.as_str()),
            appdata: StringView::from(&other.appdata),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_msid_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Msid)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_msids(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_msids: *mut RustSdpAttributeMsid,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Msid(ref data) = *x {
                Some(RustSdpAttributeMsid::from(data))
            } else {
                None
            }
        })
        .collect();
    let msids = slice::from_raw_parts_mut(ret_msids, ret_size);
    msids.copy_from_slice(attrs.as_slice());
}

// TODO: Finish msid attributes once parsing is changed upstream.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeMsidSemantic {
    pub semantic: StringView,
    pub msids: *const Vec<String>,
}

impl<'a> From<&'a SdpAttributeMsidSemantic> for RustSdpAttributeMsidSemantic {
    fn from(other: &SdpAttributeMsidSemantic) -> Self {
        RustSdpAttributeMsidSemantic {
            semantic: StringView::from(other.semantic.as_str()),
            msids: &other.msids,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_msid_semantic_count(
    attributes: *const Vec<SdpAttribute>,
) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::MsidSemantic)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_msid_semantics(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_msid_semantics: *mut RustSdpAttributeMsidSemantic,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::MsidSemantic(ref data) = *x {
                Some(RustSdpAttributeMsidSemantic::from(data))
            } else {
                None
            }
        })
        .collect();
    let msid_semantics = slice::from_raw_parts_mut(ret_msid_semantics, ret_size);
    msid_semantics.copy_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum RustSdpAttributeGroupSemantic {
    LipSynchronization,
    FlowIdentification,
    SingleReservationFlow,
    AlternateNetworkAddressType,
    ForwardErrorCorrection,
    DecodingDependency,
    Bundle,
}

impl<'a> From<&'a SdpAttributeGroupSemantic> for RustSdpAttributeGroupSemantic {
    fn from(other: &SdpAttributeGroupSemantic) -> Self {
        match *other {
            SdpAttributeGroupSemantic::LipSynchronization => {
                RustSdpAttributeGroupSemantic::LipSynchronization
            }
            SdpAttributeGroupSemantic::FlowIdentification => {
                RustSdpAttributeGroupSemantic::FlowIdentification
            }
            SdpAttributeGroupSemantic::SingleReservationFlow => {
                RustSdpAttributeGroupSemantic::SingleReservationFlow
            }
            SdpAttributeGroupSemantic::AlternateNetworkAddressType => {
                RustSdpAttributeGroupSemantic::AlternateNetworkAddressType
            }
            SdpAttributeGroupSemantic::ForwardErrorCorrection => {
                RustSdpAttributeGroupSemantic::ForwardErrorCorrection
            }
            SdpAttributeGroupSemantic::DecodingDependency => {
                RustSdpAttributeGroupSemantic::DecodingDependency
            }
            SdpAttributeGroupSemantic::Bundle => RustSdpAttributeGroupSemantic::Bundle,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeGroup {
    pub semantic: RustSdpAttributeGroupSemantic,
    pub tags: *const Vec<String>,
}

impl<'a> From<&'a SdpAttributeGroup> for RustSdpAttributeGroup {
    fn from(other: &SdpAttributeGroup) -> Self {
        RustSdpAttributeGroup {
            semantic: RustSdpAttributeGroupSemantic::from(&other.semantics),
            tags: &other.tags,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_group_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Group)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_groups(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_groups: *mut RustSdpAttributeGroup,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Group(ref data) = *x {
                Some(RustSdpAttributeGroup::from(data))
            } else {
                None
            }
        })
        .collect();
    let groups = slice::from_raw_parts_mut(ret_groups, ret_size);
    groups.copy_from_slice(attrs.as_slice());
}

#[repr(C)]
pub struct RustSdpAttributeRtcp {
    pub port: u32,
    pub unicast_addr: RustExplicitlyTypedAddress,
    pub has_address: bool,
}

impl<'a> From<&'a SdpAttributeRtcp> for RustSdpAttributeRtcp {
    fn from(other: &SdpAttributeRtcp) -> Self {
        match other.unicast_addr {
            Some(ref address) => RustSdpAttributeRtcp {
                port: other.port as u32,
                unicast_addr: address.into(),
                has_address: true,
            },
            None => RustSdpAttributeRtcp {
                port: other.port as u32,
                unicast_addr: RustExplicitlyTypedAddress::default(),
                has_address: false,
            },
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_rtcp(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut RustSdpAttributeRtcp,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::Rtcp);
    if let Some(&SdpAttribute::Rtcp(ref data)) = attr {
        *ret = RustSdpAttributeRtcp::from(data);
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeRtcpFb {
    pub payload_type: u32,
    pub feedback_type: u32,
    pub parameter: StringView,
    pub extra: StringView,
}

impl<'a> From<&'a SdpAttributeRtcpFb> for RustSdpAttributeRtcpFb {
    fn from(other: &SdpAttributeRtcpFb) -> Self {
        RustSdpAttributeRtcpFb {
            payload_type: match other.payload_type {
                SdpAttributePayloadType::Wildcard => u32::max_value(),
                SdpAttributePayloadType::PayloadType(x) => x as u32,
            },
            feedback_type: other.feedback_type.clone() as u32,
            parameter: StringView::from(other.parameter.as_str()),
            extra: StringView::from(other.extra.as_str()),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_rtcpfb_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Rtcpfb)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_rtcpfbs(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_rtcpfbs: *mut RustSdpAttributeRtcpFb,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Rtcpfb(ref data) = *x {
                Some(RustSdpAttributeRtcpFb::from(data))
            } else {
                None
            }
        })
        .collect();
    let rtcpfbs = slice::from_raw_parts_mut(ret_rtcpfbs, ret_size);
    rtcpfbs.clone_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeImageAttrXYRange {
    // range
    pub min: u32,
    pub max: u32,
    pub step: u32,

    // discrete values
    pub discrete_values: *const Vec<u32>,
}

impl<'a> From<&'a SdpAttributeImageAttrXYRange> for RustSdpAttributeImageAttrXYRange {
    fn from(other: &SdpAttributeImageAttrXYRange) -> Self {
        match other {
            &SdpAttributeImageAttrXYRange::Range(min, max, step) => {
                RustSdpAttributeImageAttrXYRange {
                    min,
                    max,
                    step: step.unwrap_or(1),
                    discrete_values: ptr::null(),
                }
            }
            &SdpAttributeImageAttrXYRange::DiscreteValues(ref discrete_values) => {
                RustSdpAttributeImageAttrXYRange {
                    min: 0,
                    max: 1,
                    step: 1,
                    discrete_values,
                }
            }
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeImageAttrSRange {
    // range
    pub min: c_float,
    pub max: c_float,

    // discrete values
    pub discrete_values: *const Vec<c_float>,
}

impl<'a> From<&'a SdpAttributeImageAttrSRange> for RustSdpAttributeImageAttrSRange {
    fn from(other: &SdpAttributeImageAttrSRange) -> Self {
        match other {
            &SdpAttributeImageAttrSRange::Range(min, max) => RustSdpAttributeImageAttrSRange {
                min,
                max,
                discrete_values: ptr::null(),
            },
            &SdpAttributeImageAttrSRange::DiscreteValues(ref discrete_values) => {
                RustSdpAttributeImageAttrSRange {
                    min: 0.0,
                    max: 1.0,
                    discrete_values,
                }
            }
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeImageAttrPRange {
    pub min: c_float,
    pub max: c_float,
}

impl<'a> From<&'a SdpAttributeImageAttrPRange> for RustSdpAttributeImageAttrPRange {
    fn from(other: &SdpAttributeImageAttrPRange) -> Self {
        RustSdpAttributeImageAttrPRange {
            min: other.min,
            max: other.max,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeImageAttrSet {
    pub x: RustSdpAttributeImageAttrXYRange,
    pub y: RustSdpAttributeImageAttrXYRange,

    pub has_sar: bool,
    pub sar: RustSdpAttributeImageAttrSRange,

    pub has_par: bool,
    pub par: RustSdpAttributeImageAttrPRange,

    pub q: c_float,
}

impl<'a> From<&'a SdpAttributeImageAttrSet> for RustSdpAttributeImageAttrSet {
    fn from(other: &SdpAttributeImageAttrSet) -> Self {
        RustSdpAttributeImageAttrSet {
            x: RustSdpAttributeImageAttrXYRange::from(&other.x),
            y: RustSdpAttributeImageAttrXYRange::from(&other.y),

            has_sar: other.sar.is_some(),
            sar: match other.sar {
                Some(ref x) => RustSdpAttributeImageAttrSRange::from(x),
                // This is just any valid value accepted by rust,
                // it might as well by uninitilized
                None => RustSdpAttributeImageAttrSRange::from(
                    &SdpAttributeImageAttrSRange::DiscreteValues(vec![]),
                ),
            },

            has_par: other.par.is_some(),
            par: match other.par {
                Some(ref x) => RustSdpAttributeImageAttrPRange::from(x),
                // This is just any valid value accepted by rust,
                // it might as well by uninitilized
                None => RustSdpAttributeImageAttrPRange { min: 0.0, max: 1.0 },
            },

            q: other.q.unwrap_or(0.5),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeImageAttrSetList {
    pub sets: *const Vec<SdpAttributeImageAttrSet>,
}

impl<'a> From<&'a SdpAttributeImageAttrSetList> for RustSdpAttributeImageAttrSetList {
    fn from(other: &SdpAttributeImageAttrSetList) -> Self {
        match other {
            &SdpAttributeImageAttrSetList::Wildcard => {
                RustSdpAttributeImageAttrSetList { sets: ptr::null() }
            }
            &SdpAttributeImageAttrSetList::Sets(ref sets) => {
                RustSdpAttributeImageAttrSetList { sets: sets }
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_imageattr_get_set_count(
    sets: *const Vec<SdpAttributeImageAttrSet>,
) -> size_t {
    (*sets).len()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_imageattr_get_sets(
    sets: *const Vec<SdpAttributeImageAttrSet>,
    ret_size: size_t,
    ret: *mut RustSdpAttributeImageAttrSet,
) {
    let rust_sets: Vec<_> = (*sets)
        .iter()
        .map(RustSdpAttributeImageAttrSet::from)
        .collect();
    let sets = slice::from_raw_parts_mut(ret, ret_size);
    sets.clone_from_slice(rust_sets.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeImageAttr {
    pub pt: u32,
    pub send: RustSdpAttributeImageAttrSetList,
    pub recv: RustSdpAttributeImageAttrSetList,
}

impl<'a> From<&'a SdpAttributeImageAttr> for RustSdpAttributeImageAttr {
    fn from(other: &SdpAttributeImageAttr) -> Self {
        RustSdpAttributeImageAttr {
            pt: match other.pt {
                SdpAttributePayloadType::Wildcard => u32::max_value(),
                SdpAttributePayloadType::PayloadType(x) => x as u32,
            },
            send: RustSdpAttributeImageAttrSetList::from(&other.send),
            recv: RustSdpAttributeImageAttrSetList::from(&other.recv),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_imageattr_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::ImageAttr)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_imageattrs(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_attrs: *mut RustSdpAttributeImageAttr,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::ImageAttr(ref data) = *x {
                Some(RustSdpAttributeImageAttr::from(data))
            } else {
                None
            }
        })
        .collect();
    let imageattrs = slice::from_raw_parts_mut(ret_attrs, ret_size);
    imageattrs.copy_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeSctpmap {
    pub port: u32,
    pub channels: u32,
}

impl<'a> From<&'a SdpAttributeSctpmap> for RustSdpAttributeSctpmap {
    fn from(other: &SdpAttributeSctpmap) -> Self {
        RustSdpAttributeSctpmap {
            port: other.port as u32,
            channels: other.channels,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_sctpmap_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Sctpmap)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_sctpmaps(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_sctpmaps: *mut RustSdpAttributeSctpmap,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Sctpmap(ref data) = *x {
                Some(RustSdpAttributeSctpmap::from(data))
            } else {
                None
            }
        })
        .collect();
    let sctpmaps = slice::from_raw_parts_mut(ret_sctpmaps, ret_size);
    sctpmaps.copy_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeSimulcastId {
    pub id: StringView,
    pub paused: bool,
}

impl<'a> From<&'a SdpAttributeSimulcastId> for RustSdpAttributeSimulcastId {
    fn from(other: &SdpAttributeSimulcastId) -> Self {
        RustSdpAttributeSimulcastId {
            id: StringView::from(other.id.as_str()),
            paused: other.paused,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeSimulcastVersion {
    pub ids: *const Vec<SdpAttributeSimulcastId>,
}

impl<'a> From<&'a SdpAttributeSimulcastVersion> for RustSdpAttributeSimulcastVersion {
    fn from(other: &SdpAttributeSimulcastVersion) -> Self {
        RustSdpAttributeSimulcastVersion { ids: &other.ids }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_simulcast_get_ids_count(
    ids: *const Vec<SdpAttributeSimulcastId>,
) -> size_t {
    (*ids).len()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_simulcast_get_ids(
    ids: *const Vec<SdpAttributeSimulcastId>,
    ret_size: size_t,
    ret: *mut RustSdpAttributeSimulcastId,
) {
    let rust_ids: Vec<_> = (*ids)
        .iter()
        .map(RustSdpAttributeSimulcastId::from)
        .collect();
    let ids = slice::from_raw_parts_mut(ret, ret_size);
    ids.clone_from_slice(rust_ids.as_slice());
}

#[repr(C)]
pub struct RustSdpAttributeSimulcast {
    pub send: *const Vec<SdpAttributeSimulcastVersion>,
    pub receive: *const Vec<SdpAttributeSimulcastVersion>,
}

impl<'a> From<&'a SdpAttributeSimulcast> for RustSdpAttributeSimulcast {
    fn from(other: &SdpAttributeSimulcast) -> Self {
        RustSdpAttributeSimulcast {
            send: &other.send,
            receive: &other.receive,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_simulcast_get_version_count(
    version_list: *const Vec<SdpAttributeSimulcastVersion>,
) -> size_t {
    (*version_list).len()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_simulcast_get_versions(
    version_list: *const Vec<SdpAttributeSimulcastVersion>,
    ret_size: size_t,
    ret: *mut RustSdpAttributeSimulcastVersion,
) {
    let rust_versions_list: Vec<_> = (*version_list)
        .iter()
        .map(RustSdpAttributeSimulcastVersion::from)
        .collect();
    let versions = slice::from_raw_parts_mut(ret, ret_size);
    versions.clone_from_slice(rust_versions_list.as_slice())
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_simulcast(
    attributes: *const Vec<SdpAttribute>,
    ret: *mut RustSdpAttributeSimulcast,
) -> nsresult {
    let attr = get_attribute((*attributes).as_slice(), SdpAttributeType::Simulcast);
    if let Some(&SdpAttribute::Simulcast(ref data)) = attr {
        *ret = RustSdpAttributeSimulcast::from(data);
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum RustDirection {
    Recvonly,
    Sendonly,
    Sendrecv,
    Inactive,
}

impl<'a> From<&'a Option<SdpAttributeDirection>> for RustDirection {
    fn from(other: &Option<SdpAttributeDirection>) -> Self {
        match *other {
            Some(ref direction) => match *direction {
                SdpAttributeDirection::Recvonly => RustDirection::Recvonly,
                SdpAttributeDirection::Sendonly => RustDirection::Sendonly,
                SdpAttributeDirection::Sendrecv => RustDirection::Sendrecv,
            },
            None => RustDirection::Inactive,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_direction(attributes: *const Vec<SdpAttribute>) -> RustDirection {
    for attribute in (*attributes).iter() {
        match *attribute {
            SdpAttribute::Recvonly => {
                return RustDirection::Recvonly;
            }
            SdpAttribute::Sendonly => {
                return RustDirection::Sendonly;
            }
            SdpAttribute::Sendrecv => {
                return RustDirection::Sendrecv;
            }
            SdpAttribute::Inactive => {
                return RustDirection::Inactive;
            }
            _ => (),
        }
    }
    RustDirection::Sendrecv
}

#[repr(C)]
pub struct RustSdpAttributeRemoteCandidate {
    pub component: u32,
    pub address: RustAddress,
    pub port: u32,
}

impl<'a> From<&'a SdpAttributeRemoteCandidate> for RustSdpAttributeRemoteCandidate {
    fn from(other: &SdpAttributeRemoteCandidate) -> Self {
        RustSdpAttributeRemoteCandidate {
            component: other.component,
            address: RustAddress::from(&other.address),
            port: other.port,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_remote_candidate_count(
    attributes: *const Vec<SdpAttribute>,
) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::RemoteCandidate)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_remote_candidates(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_candidates: *mut RustSdpAttributeRemoteCandidate,
) {
    let attrs = (*attributes).iter().filter_map(|x| {
        if let SdpAttribute::RemoteCandidate(ref data) = *x {
            Some(RustSdpAttributeRemoteCandidate::from(data))
        } else {
            None
        }
    });
    let candidates = slice::from_raw_parts_mut(ret_candidates, ret_size);
    for (source, destination) in attrs.zip(candidates) {
        *destination = source
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_candidate_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Candidate)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_candidates(
    attributes: *const Vec<SdpAttribute>,
    _ret_size: size_t,
    ret: *mut *const Vec<String>,
) {
    let attr_strings: Vec<String> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Candidate(ref attr) = *x {
                // The serialized attribute starts with "candidate:...", this needs to be removed
                Some(attr.to_string())
            } else {
                None
            }
        })
        .collect();

    *ret = Box::into_raw(Box::from(attr_strings));
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeRidParameters {
    pub max_width: u32,
    pub max_height: u32,
    pub max_fps: u32,
    pub max_fs: u32,
    pub max_br: u32,
    pub max_pps: u32,
    pub unknown: *const Vec<String>,
}

impl<'a> From<&'a SdpAttributeRidParameters> for RustSdpAttributeRidParameters {
    fn from(other: &SdpAttributeRidParameters) -> Self {
        RustSdpAttributeRidParameters {
            max_width: other.max_width,
            max_height: other.max_height,
            max_fps: other.max_fps,
            max_fs: other.max_fs,
            max_br: other.max_br,
            max_pps: other.max_pps,

            unknown: &other.unknown,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeRid {
    pub id: StringView,
    pub direction: u32,
    pub formats: *const Vec<u16>,
    pub params: RustSdpAttributeRidParameters,
    pub depends: *const Vec<String>,
}

impl<'a> From<&'a SdpAttributeRid> for RustSdpAttributeRid {
    fn from(other: &SdpAttributeRid) -> Self {
        RustSdpAttributeRid {
            id: StringView::from(other.id.as_str()),
            direction: other.direction.clone() as u32,
            formats: &other.formats,
            params: RustSdpAttributeRidParameters::from(&other.params),
            depends: &other.depends,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_rid_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Rid)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_rids(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_rids: *mut RustSdpAttributeRid,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Rid(ref data) = *x {
                Some(RustSdpAttributeRid::from(data))
            } else {
                None
            }
        })
        .collect();
    let rids = slice::from_raw_parts_mut(ret_rids, ret_size);
    rids.clone_from_slice(attrs.as_slice());
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RustSdpAttributeExtmap {
    pub id: u16,
    pub direction_specified: bool,
    pub direction: RustDirection,
    pub url: StringView,
    pub extension_attributes: StringView,
}

impl<'a> From<&'a SdpAttributeExtmap> for RustSdpAttributeExtmap {
    fn from(other: &SdpAttributeExtmap) -> Self {
        let dir = if other.direction.is_some() {
            RustDirection::from(&other.direction)
        } else {
            RustDirection::from(&Some(SdpAttributeDirection::Sendrecv))
        };
        RustSdpAttributeExtmap {
            id: other.id as u16,
            direction_specified: other.direction.is_some(),
            direction: dir,
            url: StringView::from(other.url.as_str()),
            extension_attributes: StringView::from(&other.extension_attributes),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_extmap_count(attributes: *const Vec<SdpAttribute>) -> size_t {
    count_attribute((*attributes).as_slice(), SdpAttributeType::Extmap)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_extmaps(
    attributes: *const Vec<SdpAttribute>,
    ret_size: size_t,
    ret_rids: *mut RustSdpAttributeExtmap,
) {
    let attrs: Vec<_> = (*attributes)
        .iter()
        .filter_map(|x| {
            if let SdpAttribute::Extmap(ref data) = *x {
                Some(RustSdpAttributeExtmap::from(data))
            } else {
                None
            }
        })
        .collect();
    let extmaps = slice::from_raw_parts_mut(ret_rids, ret_size);
    extmaps.copy_from_slice(attrs.as_slice());
}
