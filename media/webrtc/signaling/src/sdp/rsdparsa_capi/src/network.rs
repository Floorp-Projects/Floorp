use std::net::IpAddr;
use std::os::raw::c_char;
use std::ffi::{CStr, CString};

use libc::{uint8_t, uint64_t};

use rsdparsa::{SdpOrigin, SdpConnection, SdpBandwidth};

use types::StringView;

#[repr(C)]
#[derive(Clone, Copy, PartialEq)]
pub enum RustSdpAddrType {
    None,
    IP4,
    IP6
}

impl<'a> From<&'a IpAddr> for RustSdpAddrType {
    fn from(addr: &IpAddr) -> RustSdpAddrType {
        match *addr {
            IpAddr::V4(_) => RustSdpAddrType::IP4,
            IpAddr::V6(_) => RustSdpAddrType::IP6
        }
    }
}

pub fn get_octets(addr: &IpAddr) -> [u8; 16] {
    let mut octets = [0; 16];
    match *addr {
        IpAddr::V4(v4_addr) => {
            let v4_octets = v4_addr.octets();
            (&mut octets[0..4]).copy_from_slice(&v4_octets);
        },
        IpAddr::V6(v6_addr) => {
            let v6_octets = v6_addr.octets();
            octets.copy_from_slice(&v6_octets);
        }
    }
    octets
}

#[repr(C)]
pub struct RustIpAddr {
    addr_type: RustSdpAddrType,
    unicast_addr: [u8; 50]
}

impl<'a> From<&'a IpAddr> for RustIpAddr {
    fn from(addr: &IpAddr) -> Self {
        let mut c_addr = [0; 50];
        let str_addr = format!("{}", addr);
        let str_bytes = str_addr.as_bytes();
        if str_bytes.len() < 50 {
            c_addr[..str_bytes.len()].copy_from_slice(&str_bytes);
        }
        RustIpAddr {addr_type: RustSdpAddrType::from(addr),
                    unicast_addr: c_addr }
    }
}

impl<'a> From<&'a Option<IpAddr>> for RustIpAddr {
    fn from(addr: &Option<IpAddr>) -> Self {
        match *addr {
            Some(ref x) => RustIpAddr::from(x),
            None => RustIpAddr {
                addr_type: RustSdpAddrType::None,
                unicast_addr: [0; 50]
            }
        }
    }
}

#[repr(C)]
pub struct RustSdpConnection {
    pub addr: RustIpAddr,
    pub ttl: uint8_t,
    pub amount: uint64_t
}

impl<'a> From<&'a SdpConnection> for RustSdpConnection {
    fn from(sdp_connection: &SdpConnection) -> Self {
        let ttl = match sdp_connection.ttl {
            Some(x) => x as u8,
            None => 0
        };
        let amount = match sdp_connection.amount {
            Some(x) => x as u64,
            None => 0
        };
        RustSdpConnection { addr: RustIpAddr::from(&sdp_connection.addr),
                            ttl: ttl, amount: amount }
    }
}

#[repr(C)]
pub struct RustSdpOrigin {
    username: StringView,
    session_id: u64,
    session_version: u64,
    addr: RustIpAddr,
}


fn bandwidth_match(str_bw: &str, enum_bw: &SdpBandwidth) -> bool {
    match *enum_bw {
        SdpBandwidth::As(_) => str_bw == "AS",
        SdpBandwidth::Ct(_) => str_bw == "CT",
        SdpBandwidth::Tias(_) => str_bw == "TIAS",
        SdpBandwidth::Unknown(ref type_name, _) => str_bw == type_name,
    }
}

fn bandwidth_value(bandwidth: &SdpBandwidth) -> u32 {
    match *bandwidth {
        SdpBandwidth::As(x) | SdpBandwidth::Ct(x) |
        SdpBandwidth::Tias(x) => x,
        SdpBandwidth::Unknown(_, _) => 0
    }
}

pub unsafe fn get_bandwidth(bandwidths: &Vec<SdpBandwidth>,
                            bandwidth_type: *const c_char) -> u32 {
    let bw_type = match CStr::from_ptr(bandwidth_type).to_str() {
        Ok(string) => string,
        Err(_) => return 0
    };
    for bandwidth in bandwidths.iter() {
        if bandwidth_match(bw_type, bandwidth) {
            return bandwidth_value(bandwidth);
        }
    }
    0
}

#[no_mangle]
pub unsafe extern "C" fn sdp_serialize_bandwidth(bw: *const Vec<SdpBandwidth>) -> *mut c_char {
    let mut builder = String::new();
    for bandwidth in (*bw).iter() {
        match *bandwidth {
            SdpBandwidth::As(val) => {
                builder.push_str("b=AS:");
                builder.push_str(&val.to_string());
                builder.push_str("\r\n");
            },
            SdpBandwidth::Ct(val) => {
                builder.push_str("b=CT:");
                builder.push_str(&val.to_string());
                builder.push_str("\r\n");
            },
            SdpBandwidth::Tias(val) => {
                builder.push_str("b=TIAS:");
                builder.push_str(&val.to_string());
                builder.push_str("\r\n");
            },
            SdpBandwidth::Unknown(ref name, val) => {
                builder.push_str("b=");
                builder.push_str(name.as_str());
                builder.push(':');
                builder.push_str(&val.to_string());
                builder.push_str("\r\n");
            },
        }
    }
    CString::from_vec_unchecked(builder.into_bytes()).into_raw()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_free_string(s: *mut c_char) {
    drop(CString::from_raw(s));
}

pub unsafe fn origin_view_helper(origin: &SdpOrigin) -> RustSdpOrigin {
    RustSdpOrigin {
        username: StringView::from(origin.username.as_str()),
        session_id: origin.session_id,
        session_version: origin.session_version,
        addr: RustIpAddr::from(&origin.unicast_addr)
    }
}
