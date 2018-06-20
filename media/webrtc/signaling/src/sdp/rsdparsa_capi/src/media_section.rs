use std::ptr;
use std::os::raw::c_char;

use libc::{size_t, uint32_t};

use nserror::{nsresult, NS_OK, NS_ERROR_INVALID_ARG};
use rsdparsa::{SdpBandwidth, SdpSession};
use rsdparsa::media_type::{SdpMedia, SdpMediaValue, SdpProtocolValue,
                           SdpFormatList};
use rsdparsa::attribute_type::{SdpAttribute, SdpAttributeRtpmap};

use network::{get_bandwidth, RustSdpConnection};
use types::StringView;

#[no_mangle]
pub unsafe extern "C" fn sdp_get_media_section(session: *const SdpSession,
                                               index: size_t) -> *const SdpMedia {
    return match (*session).media.get(index) {
        Some(m) => m,
        None => ptr::null()
    };
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum RustSdpMediaValue {
    Audio,
    Video,
    Application,
}

impl<'a> From<&'a SdpMediaValue> for RustSdpMediaValue {
    fn from(val: &SdpMediaValue) -> Self {
        match *val {
            SdpMediaValue::Audio => RustSdpMediaValue::Audio,
            SdpMediaValue::Video => RustSdpMediaValue::Video,
            SdpMediaValue::Application => RustSdpMediaValue::Application
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_rust_get_media_type(sdp_media: *const SdpMedia) -> RustSdpMediaValue {
    RustSdpMediaValue::from((*sdp_media).get_type())
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum RustSdpProtocolValue {
    RtpSavpf,
    UdpTlsRtpSavpf,
    TcpTlsRtpSavpf,
    DtlsSctp,
    UdpDtlsSctp,
    TcpDtlsSctp,
}

impl<'a> From<&'a SdpProtocolValue> for RustSdpProtocolValue {
    fn from(val: &SdpProtocolValue) -> Self {
        match *val {
            SdpProtocolValue::RtpSavpf => RustSdpProtocolValue::RtpSavpf,
            SdpProtocolValue::UdpTlsRtpSavpf => RustSdpProtocolValue::UdpTlsRtpSavpf,
            SdpProtocolValue::TcpTlsRtpSavpf => RustSdpProtocolValue::TcpTlsRtpSavpf,
            SdpProtocolValue::DtlsSctp => RustSdpProtocolValue::DtlsSctp,
            SdpProtocolValue::UdpDtlsSctp => RustSdpProtocolValue::UdpDtlsSctp,
            SdpProtocolValue::TcpDtlsSctp => RustSdpProtocolValue::TcpDtlsSctp,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_media_protocol(sdp_media: *const SdpMedia) ->  RustSdpProtocolValue {
    RustSdpProtocolValue::from((*sdp_media).get_proto())
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum RustSdpFormatType {
    Integers,
    Strings
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_format_type(sdp_media: *const SdpMedia) ->  RustSdpFormatType {
    match *(*sdp_media).get_formats() {
        SdpFormatList::Integers(_) => RustSdpFormatType::Integers,
        SdpFormatList::Strings(_) => RustSdpFormatType::Strings
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_format_string_vec(sdp_media: *const SdpMedia) -> *const Vec<String> {
    if let SdpFormatList::Strings(ref formats) = *(*sdp_media).get_formats() {
        formats
    } else {
        ptr::null()
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_format_u32_vec(sdp_media: *const SdpMedia) -> *const Vec<u32> {
    if let SdpFormatList::Integers(ref formats) = *(*sdp_media).get_formats() {
        formats
    } else {
        ptr::null()
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_set_media_port(sdp_media: *mut SdpMedia, port: uint32_t) {
    (*sdp_media).set_port(port);
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_media_port(sdp_media: *const SdpMedia) -> uint32_t {
    (*sdp_media).get_port()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_media_port_count(sdp_media: *const SdpMedia) -> uint32_t {
    (*sdp_media).get_port_count()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_media_bandwidth(sdp_media: *const SdpMedia,
                                                bandwidth_type: *const c_char) -> uint32_t {
    get_bandwidth((*sdp_media).get_bandwidth(), bandwidth_type)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_media_bandwidth_vec(sdp_media: *const SdpMedia) -> *const Vec<SdpBandwidth> {
    (*sdp_media).get_bandwidth()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_media_has_connection(sdp_media: *const SdpMedia) -> bool {
    (*sdp_media).has_connection()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_media_connection(sdp_media: *const SdpMedia, ret: *mut RustSdpConnection) -> nsresult {
    if let &Some(ref connection) = (*sdp_media).get_connection() {
        *ret = RustSdpConnection::from(connection);
        return NS_OK;
    }
    NS_ERROR_INVALID_ARG
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_media_attribute_list(sdp_media: *const SdpMedia) -> *const Vec<SdpAttribute> {
    (*sdp_media).get_attributes()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_media_clear_codecs(sdp_media: *mut SdpMedia) {
    (*sdp_media).remove_codecs()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_media_add_codec(sdp_media: *mut SdpMedia, pt: u8,
                                             codec_name: StringView, clockrate: u32,
                                             channels: u16) -> nsresult {
     let rtpmap = SdpAttributeRtpmap{
                     payload_type: pt,
                     codec_name: match codec_name.into() {
                         Ok(x) => x,
                         Err(boxed_error) => {
                             println!("Error while pasing string, description: {:?}", (*boxed_error).description());
                             return NS_ERROR_INVALID_ARG;
                         }
                     },
                     frequency: clockrate,
                     channels: Some(channels as u32),
     };

    match (*sdp_media).add_codec(rtpmap) {
        Ok(_) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_media_add_datachannel(sdp_media: *mut SdpMedia, name: StringView,
                                                    port: u16, streams: u16, message_size: u32)
                                                    -> nsresult {
    let name_str = match name.into() {
        Ok(x) => x,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    match (*sdp_media).add_datachannel(name_str, port, streams, message_size){
        Ok(_) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG
    }
}
