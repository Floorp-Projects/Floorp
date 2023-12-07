/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#[cfg(not(windows))]
use libc::{AF_INET, AF_INET6};
use neqo_common::event::Provider;
use neqo_common::{self as common, qlog::NeqoQlog, qwarn, Datagram, Header, Role};
use neqo_crypto::{init, PRErrorCode};
use neqo_http3::{
    features::extended_connect::SessionCloseReason, Error as Http3Error, Http3Client,
    Http3ClientEvent, Http3Parameters, Http3State, Priority, WebTransportEvent,
};
use neqo_transport::{
    stream_id::StreamType, CongestionControlAlgorithm, ConnectionParameters,
    Error as TransportError, Output, RandomConnectionIdGenerator, StreamId, Version,
};
use nserror::*;
use nsstring::*;
use qlog::streamer::QlogStreamer;
use std::borrow::Cow;
use std::cell::RefCell;
use std::cmp::{max, min};
use std::convert::TryFrom;
use std::convert::TryInto;
use std::ffi::c_void;
use std::fs::OpenOptions;
use std::net::SocketAddr;
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};
use std::path::PathBuf;
use std::ptr;
use std::rc::Rc;
use std::slice;
use std::str;
#[cfg(feature = "fuzzing")]
use std::time::Duration;
use std::time::{Duration, Instant};
use thin_vec::ThinVec;
use uuid::Uuid;
#[cfg(windows)]
use winapi::shared::ws2def::{AF_INET, AF_INET6};
use xpcom::{AtomicRefcnt, RefCounted, RefPtr};

#[repr(C)]
pub struct NeqoHttp3Conn {
    conn: Http3Client,
    local_addr: SocketAddr,
    refcnt: AtomicRefcnt,
    last_output_time: Instant,
    max_accumlated_time: Duration,
}

// Opaque interface to mozilla::net::NetAddr defined in DNS.h
#[repr(C)]
pub union NetAddr {
    private: [u8; 0],
}

extern "C" {
    pub fn moz_netaddr_get_family(arg: *const NetAddr) -> u16;
    pub fn moz_netaddr_get_network_order_ip(arg: *const NetAddr) -> u32;
    pub fn moz_netaddr_get_ipv6(arg: *const NetAddr) -> *const u8;
    pub fn moz_netaddr_get_network_order_port(arg: *const NetAddr) -> u16;
}

fn netaddr_to_socket_addr(arg: *const NetAddr) -> Result<SocketAddr, nsresult> {
    if arg == ptr::null() {
        return Err(NS_ERROR_INVALID_ARG);
    }

    unsafe {
        let family = moz_netaddr_get_family(arg) as i32;
        if family == AF_INET {
            let port = u16::from_be(moz_netaddr_get_network_order_port(arg));
            let ipv4 = Ipv4Addr::from(u32::from_be(moz_netaddr_get_network_order_ip(arg)));
            return Ok(SocketAddr::new(IpAddr::V4(ipv4), port));
        }

        if family == AF_INET6 {
            let port = u16::from_be(moz_netaddr_get_network_order_port(arg));
            let ipv6_slice: [u8; 16] = slice::from_raw_parts(moz_netaddr_get_ipv6(arg), 16)
                .try_into()
                .expect("slice with incorrect length");
            let ipv6 = Ipv6Addr::from(ipv6_slice);
            return Ok(SocketAddr::new(IpAddr::V6(ipv6), port));
        }
    }

    Err(NS_ERROR_UNEXPECTED)
}

fn get_current_or_last_output_time(last_output_time: &Instant) -> Instant {
    max(*last_output_time, Instant::now())
}

type SendFunc = extern "C" fn(
    context: *mut c_void,
    addr_family: u16,
    addr: *const u8,
    port: u16,
    data: *const u8,
    size: u32,
) -> nsresult;

type SetTimerFunc = extern "C" fn(context: *mut c_void, timeout: u64);

impl NeqoHttp3Conn {
    fn new(
        origin: &nsACString,
        alpn: &nsACString,
        local_addr: *const NetAddr,
        remote_addr: *const NetAddr,
        max_table_size: u64,
        max_blocked_streams: u16,
        max_data: u64,
        max_stream_data: u64,
        version_negotiation: bool,
        webtransport: bool,
        qlog_dir: &nsACString,
        webtransport_datagram_size: u32,
        max_accumlated_time_ms: u32,
    ) -> Result<RefPtr<NeqoHttp3Conn>, nsresult> {
        // Nss init.
        init();

        let origin_conv = str::from_utf8(origin).map_err(|_| NS_ERROR_INVALID_ARG)?;

        let alpn_conv = str::from_utf8(alpn).map_err(|_| NS_ERROR_INVALID_ARG)?;

        let local: SocketAddr = netaddr_to_socket_addr(local_addr)?;

        let remote: SocketAddr = netaddr_to_socket_addr(remote_addr)?;

        let quic_version = match alpn_conv {
            "h3-32" => Version::Draft32,
            "h3-31" => Version::Draft31,
            "h3-30" => Version::Draft30,
            "h3-29" => Version::Draft29,
            "h3" => Version::Version1,
            _ => return Err(NS_ERROR_INVALID_ARG),
        };

        let version_list = if version_negotiation {
            Version::all()
        } else {
            vec![quic_version]
        };

        let cc_algorithm = match static_prefs::pref!("network.http.http3.cc_algorithm") {
            0 => CongestionControlAlgorithm::NewReno,
            1 => CongestionControlAlgorithm::Cubic,
            _ => {
                // Unknown preferences; default to Cubic
                CongestionControlAlgorithm::Cubic
            }
        };

        #[allow(unused_mut)]
        let mut params = ConnectionParameters::default()
            .versions(quic_version, version_list)
            .cc_algorithm(cc_algorithm)
            .max_data(max_data)
            .max_stream_data(StreamType::BiDi, false, max_stream_data)
            .grease(static_prefs::pref!("security.tls.grease_http3_enable"));

        // Set a short timeout when fuzzing.
        #[cfg(feature = "fuzzing")]
        if static_prefs::pref!("fuzzing.necko.http3") {
            params = params.idle_timeout(Duration::from_millis(10));
        }

        if webtransport_datagram_size > 0 {
            params = params.datagram_size(webtransport_datagram_size.into());
        }

        let http3_settings = Http3Parameters::default()
            .max_table_size_encoder(max_table_size)
            .max_table_size_decoder(max_table_size)
            .max_blocked_streams(max_blocked_streams)
            .max_concurrent_push_streams(0)
            .connection_parameters(params)
            .webtransport(webtransport)
            .http3_datagram(webtransport);

        let mut conn = match Http3Client::new(
            origin_conv,
            Rc::new(RefCell::new(RandomConnectionIdGenerator::new(3))),
            local,
            remote,
            http3_settings,
            Instant::now(),
        ) {
            Ok(c) => c,
            Err(_) => return Err(NS_ERROR_INVALID_ARG),
        };

        if !qlog_dir.is_empty() {
            let qlog_dir_conv = str::from_utf8(qlog_dir).map_err(|_| NS_ERROR_INVALID_ARG)?;
            let mut qlog_path = PathBuf::from(qlog_dir_conv);
            qlog_path.push(format!("{}_{}.qlog", origin, Uuid::new_v4()));

            // Emit warnings but to not return an error if qlog initialization
            // fails.
            match OpenOptions::new()
                .write(true)
                .create(true)
                .truncate(true)
                .open(&qlog_path)
            {
                Err(_) => qwarn!("Could not open qlog path: {}", qlog_path.display()),
                Ok(f) => {
                    let streamer = QlogStreamer::new(
                        qlog::QLOG_VERSION.to_string(),
                        Some("Firefox Client qlog".to_string()),
                        Some("Firefox Client qlog".to_string()),
                        None,
                        std::time::Instant::now(),
                        common::qlog::new_trace(Role::Client),
                        qlog::events::EventImportance::Base,
                        Box::new(f),
                    );

                    match NeqoQlog::enabled(streamer, &qlog_path) {
                        Err(_) => qwarn!("Could not write to qlog path: {}", qlog_path.display()),
                        Ok(nq) => conn.set_qlog(nq),
                    }
                }
            }
        }

        let conn = Box::into_raw(Box::new(NeqoHttp3Conn {
            conn,
            local_addr: local,
            refcnt: unsafe { AtomicRefcnt::new() },
            last_output_time: Instant::now(),
            max_accumlated_time: Duration::from_millis(max_accumlated_time_ms.into()),
        }));
        unsafe { Ok(RefPtr::from_raw(conn).unwrap()) }
    }
}

#[no_mangle]
pub unsafe extern "C" fn neqo_http3conn_addref(conn: &NeqoHttp3Conn) {
    conn.refcnt.inc();
}

#[no_mangle]
pub unsafe extern "C" fn neqo_http3conn_release(conn: &NeqoHttp3Conn) {
    let rc = conn.refcnt.dec();
    if rc == 0 {
        std::mem::drop(Box::from_raw(conn as *const _ as *mut NeqoHttp3Conn));
    }
}

// xpcom::RefPtr support
unsafe impl RefCounted for NeqoHttp3Conn {
    unsafe fn addref(&self) {
        neqo_http3conn_addref(self);
    }
    unsafe fn release(&self) {
        neqo_http3conn_release(self);
    }
}

// Allocate a new NeqoHttp3Conn object.
#[no_mangle]
pub extern "C" fn neqo_http3conn_new(
    origin: &nsACString,
    alpn: &nsACString,
    local_addr: *const NetAddr,
    remote_addr: *const NetAddr,
    max_table_size: u64,
    max_blocked_streams: u16,
    max_data: u64,
    max_stream_data: u64,
    version_negotiation: bool,
    webtransport: bool,
    qlog_dir: &nsACString,
    webtransport_datagram_size: u32,
    max_accumlated_time_ms: u32,
    result: &mut *const NeqoHttp3Conn,
) -> nsresult {
    *result = ptr::null_mut();

    match NeqoHttp3Conn::new(
        origin,
        alpn,
        local_addr,
        remote_addr,
        max_table_size,
        max_blocked_streams,
        max_data,
        max_stream_data,
        version_negotiation,
        webtransport,
        qlog_dir,
        webtransport_datagram_size,
        max_accumlated_time_ms,
    ) {
        Ok(http3_conn) => {
            http3_conn.forget(result);
            NS_OK
        }
        Err(e) => e,
    }
}

/* Process a packet.
 * packet holds packet data.
 */
#[no_mangle]
pub unsafe extern "C" fn neqo_http3conn_process_input(
    conn: &mut NeqoHttp3Conn,
    remote_addr: *const NetAddr,
    packet: *const ThinVec<u8>,
) -> nsresult {
    let remote = match netaddr_to_socket_addr(remote_addr) {
        Ok(addr) => addr,
        Err(result) => return result,
    };
    conn.conn.process_input(
        Datagram::new(remote, conn.local_addr, (*packet).to_vec()),
        get_current_or_last_output_time(&conn.last_output_time),
    );
    return NS_OK;
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_process_output_and_send(
    conn: &mut NeqoHttp3Conn,
    context: *mut c_void,
    send_func: SendFunc,
    set_timer_func: SetTimerFunc,
) -> nsresult {
    let now = Instant::now();
    if conn.last_output_time > now {
        // The timer fired too early, so reschedule it.
        // The 1ms of extra delay is not ideal, but this is a fail
        set_timer_func(
            context,
            u64::try_from((conn.last_output_time - now + conn.max_accumlated_time).as_millis())
                .unwrap(),
        );
        return NS_OK;
    }

    let mut accumulated_time = Duration::from_nanos(0);
    loop {
        conn.last_output_time = if accumulated_time.is_zero() {
            Instant::now()
        } else {
            now + accumulated_time
        };
        match conn.conn.process_output(conn.last_output_time) {
            Output::Datagram(dg) => {
                let rv = match dg.destination().ip() {
                    IpAddr::V4(v4) => send_func(
                        context,
                        u16::try_from(AF_INET).unwrap(),
                        v4.octets().as_ptr(),
                        dg.destination().port(),
                        dg.as_ptr(),
                        u32::try_from(dg.len()).unwrap(),
                    ),
                    IpAddr::V6(v6) => send_func(
                        context,
                        u16::try_from(AF_INET6).unwrap(),
                        v6.octets().as_ptr(),
                        dg.destination().port(),
                        dg.as_ptr(),
                        u32::try_from(dg.len()).unwrap(),
                    ),
                };
                if rv != NS_OK {
                    return rv;
                }
            }
            Output::Callback(to) => {
                if to.is_zero() {
                    set_timer_func(context, 1);
                    break;
                }

                let timeout = min(to, Duration::from_nanos(u64::MAX - 1));
                accumulated_time += timeout;
                if accumulated_time >= conn.max_accumlated_time {
                    let mut timeout = accumulated_time.as_millis() as u64;
                    if timeout == 0 {
                        timeout = 1;
                    }
                    set_timer_func(context, timeout);
                    break;
                }
            }
            Output::None => {
                set_timer_func(context, std::u64::MAX);
                break;
            }
        }
    }
    NS_OK
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_close(conn: &mut NeqoHttp3Conn, error: u64) {
    conn.conn.close(
        get_current_or_last_output_time(&conn.last_output_time),
        error,
        "",
    );
}

fn is_excluded_header(name: &str) -> bool {
    if (name == "connection")
        || (name == "host")
        || (name == "keep-alive")
        || (name == "proxy-connection")
        || (name == "te")
        || (name == "transfer-encoding")
        || (name == "upgrade")
        || (name == "sec-websocket-key")
    {
        true
    } else {
        false
    }
}

fn parse_headers(headers: &nsACString) -> Result<Vec<Header>, nsresult> {
    let mut hdrs = Vec::new();
    // this is only used for headers built by Firefox.
    // Firefox supplies all headers already prepared for sending over http1.
    // They need to be split into (String, String) pairs.
    match str::from_utf8(headers) {
        Err(_) => {
            return Err(NS_ERROR_INVALID_ARG);
        }
        Ok(h) => {
            for elem in h.split("\r\n").skip(1) {
                if elem.starts_with(':') {
                    // colon headers are for http/2 and 3 and this is http/1
                    // input, so that is probably a smuggling attack of some
                    // kind.
                    continue;
                }
                if elem.len() == 0 {
                    continue;
                }
                let hdr_str: Vec<_> = elem.splitn(2, ":").collect();
                let name = hdr_str[0].trim().to_lowercase();
                if is_excluded_header(&name) {
                    continue;
                }
                let value = if hdr_str.len() > 1 {
                    String::from(hdr_str[1].trim())
                } else {
                    String::new()
                };

                hdrs.push(Header::new(name, value));
            }
        }
    }
    Ok(hdrs)
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_fetch(
    conn: &mut NeqoHttp3Conn,
    method: &nsACString,
    scheme: &nsACString,
    host: &nsACString,
    path: &nsACString,
    headers: &nsACString,
    stream_id: &mut u64,
    urgency: u8,
    incremental: bool,
) -> nsresult {
    let hdrs = match parse_headers(headers) {
        Err(e) => {
            return e;
        }
        Ok(h) => h,
    };
    let method_tmp = match str::from_utf8(method) {
        Ok(m) => m,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    let scheme_tmp = match str::from_utf8(scheme) {
        Ok(s) => s,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    let host_tmp = match str::from_utf8(host) {
        Ok(h) => h,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    let path_tmp = match str::from_utf8(path) {
        Ok(p) => p,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    if urgency >= 8 {
        return NS_ERROR_INVALID_ARG;
    }
    let priority = Priority::new(urgency, incremental);
    match conn.conn.fetch(
        get_current_or_last_output_time(&conn.last_output_time),
        method_tmp,
        &(scheme_tmp, host_tmp, path_tmp),
        &hdrs,
        priority,
    ) {
        Ok(id) => {
            *stream_id = id.as_u64();
            NS_OK
        }
        Err(Http3Error::StreamLimitError) => NS_BASE_STREAM_WOULD_BLOCK,
        Err(_) => NS_ERROR_UNEXPECTED,
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_priority_update(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    urgency: u8,
    incremental: bool,
) -> nsresult {
    if urgency >= 8 {
        return NS_ERROR_INVALID_ARG;
    }
    let priority = Priority::new(urgency, incremental);
    match conn
        .conn
        .priority_update(StreamId::from(stream_id), priority)
    {
        Ok(_) => NS_OK,
        Err(_) => NS_ERROR_UNEXPECTED,
    }
}

#[no_mangle]
pub unsafe extern "C" fn neqo_htttp3conn_send_request_body(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    buf: *const u8,
    len: u32,
    read: &mut u32,
) -> nsresult {
    let array = slice::from_raw_parts(buf, len as usize);
    match conn.conn.send_data(StreamId::from(stream_id), array) {
        Ok(amount) => {
            *read = u32::try_from(amount).unwrap();
            if amount == 0 {
                NS_BASE_STREAM_WOULD_BLOCK
            } else {
                NS_OK
            }
        }
        Err(_) => NS_ERROR_UNEXPECTED,
    }
}

fn crypto_error_code(err: neqo_crypto::Error) -> u64 {
    match err {
        neqo_crypto::Error::AeadError => 1,
        neqo_crypto::Error::CertificateLoading => 2,
        neqo_crypto::Error::CreateSslSocket => 3,
        neqo_crypto::Error::HkdfError => 4,
        neqo_crypto::Error::InternalError => 5,
        neqo_crypto::Error::IntegerOverflow => 6,
        neqo_crypto::Error::InvalidEpoch => 7,
        neqo_crypto::Error::MixedHandshakeMethod => 8,
        neqo_crypto::Error::NoDataAvailable => 9,
        neqo_crypto::Error::NssError { .. } => 10,
        neqo_crypto::Error::OverrunError => 11,
        neqo_crypto::Error::SelfEncryptFailure => 12,
        neqo_crypto::Error::TimeTravelError => 13,
        neqo_crypto::Error::UnsupportedCipher => 14,
        neqo_crypto::Error::UnsupportedVersion => 15,
        neqo_crypto::Error::StringError => 16,
        neqo_crypto::Error::EchRetry(_) => 17,
        neqo_crypto::Error::CipherInitFailure => 18,
    }
}

// This is only used for telemetry. Therefore we only return error code
// numbers and do not label them. Recording telemetry is easier with a
// number.
#[repr(C)]
pub enum CloseError {
    TransportInternalError(u16),
    TransportInternalErrorOther(u16),
    TransportError(u64),
    CryptoError(u64),
    CryptoAlert(u8),
    PeerAppError(u64),
    PeerError(u64),
    AppError(u64),
    EchRetry,
}

impl From<TransportError> for CloseError {
    fn from(error: TransportError) -> CloseError {
        match error {
            TransportError::InternalError(c) => CloseError::TransportInternalError(c),
            TransportError::CryptoError(neqo_crypto::Error::EchRetry(_)) => CloseError::EchRetry,
            TransportError::CryptoError(c) => CloseError::CryptoError(crypto_error_code(c)),
            TransportError::CryptoAlert(c) => CloseError::CryptoAlert(c),
            TransportError::PeerApplicationError(c) => CloseError::PeerAppError(c),
            TransportError::PeerError(c) => CloseError::PeerError(c),
            TransportError::NoError
            | TransportError::IdleTimeout
            | TransportError::ConnectionRefused
            | TransportError::FlowControlError
            | TransportError::StreamLimitError
            | TransportError::StreamStateError
            | TransportError::FinalSizeError
            | TransportError::FrameEncodingError
            | TransportError::TransportParameterError
            | TransportError::ProtocolViolation
            | TransportError::InvalidToken
            | TransportError::KeysExhausted
            | TransportError::ApplicationError
            | TransportError::NoAvailablePath => CloseError::TransportError(error.code()),
            TransportError::EchRetry(_) => CloseError::EchRetry,
            TransportError::AckedUnsentPacket => CloseError::TransportInternalErrorOther(0),
            TransportError::ConnectionIdLimitExceeded => CloseError::TransportInternalErrorOther(1),
            TransportError::ConnectionIdsExhausted => CloseError::TransportInternalErrorOther(2),
            TransportError::ConnectionState => CloseError::TransportInternalErrorOther(3),
            TransportError::DecodingFrame => CloseError::TransportInternalErrorOther(4),
            TransportError::DecryptError => CloseError::TransportInternalErrorOther(5),
            TransportError::HandshakeFailed => CloseError::TransportInternalErrorOther(6),
            TransportError::IntegerOverflow => CloseError::TransportInternalErrorOther(7),
            TransportError::InvalidInput => CloseError::TransportInternalErrorOther(8),
            TransportError::InvalidMigration => CloseError::TransportInternalErrorOther(9),
            TransportError::InvalidPacket => CloseError::TransportInternalErrorOther(10),
            TransportError::InvalidResumptionToken => CloseError::TransportInternalErrorOther(11),
            TransportError::InvalidRetry => CloseError::TransportInternalErrorOther(12),
            TransportError::InvalidStreamId => CloseError::TransportInternalErrorOther(13),
            TransportError::KeysDiscarded(_) => CloseError::TransportInternalErrorOther(14),
            TransportError::KeysPending(_) => CloseError::TransportInternalErrorOther(15),
            TransportError::KeyUpdateBlocked => CloseError::TransportInternalErrorOther(16),
            TransportError::NoMoreData => CloseError::TransportInternalErrorOther(17),
            TransportError::NotConnected => CloseError::TransportInternalErrorOther(18),
            TransportError::PacketNumberOverlap => CloseError::TransportInternalErrorOther(19),
            TransportError::StatelessReset => CloseError::TransportInternalErrorOther(20),
            TransportError::TooMuchData => CloseError::TransportInternalErrorOther(21),
            TransportError::UnexpectedMessage => CloseError::TransportInternalErrorOther(22),
            TransportError::UnknownConnectionId => CloseError::TransportInternalErrorOther(23),
            TransportError::UnknownFrameType => CloseError::TransportInternalErrorOther(24),
            TransportError::VersionNegotiation => CloseError::TransportInternalErrorOther(25),
            TransportError::WrongRole => CloseError::TransportInternalErrorOther(26),
            TransportError::QlogError => CloseError::TransportInternalErrorOther(27),
            TransportError::NotAvailable => CloseError::TransportInternalErrorOther(28),
            TransportError::DisabledVersion => CloseError::TransportInternalErrorOther(29),
        }
    }
}

impl From<neqo_transport::ConnectionError> for CloseError {
    fn from(error: neqo_transport::ConnectionError) -> CloseError {
        match error {
            neqo_transport::ConnectionError::Transport(c) => c.into(),
            neqo_transport::ConnectionError::Application(c) => CloseError::AppError(c),
        }
    }
}

// Reset a stream with streamId.
#[no_mangle]
pub extern "C" fn neqo_http3conn_cancel_fetch(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    error: u64,
) -> nsresult {
    match conn.conn.cancel_fetch(StreamId::from(stream_id), error) {
        Ok(()) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

// Reset a stream with streamId.
#[no_mangle]
pub extern "C" fn neqo_http3conn_reset_stream(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    error: u64,
) -> nsresult {
    match conn
        .conn
        .stream_reset_send(StreamId::from(stream_id), error)
    {
        Ok(()) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_stream_stop_sending(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    error: u64,
) -> nsresult {
    match conn
        .conn
        .stream_stop_sending(StreamId::from(stream_id), error)
    {
        Ok(()) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

// Close sending side of a stream with stream_id
#[no_mangle]
pub extern "C" fn neqo_http3conn_close_stream(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
) -> nsresult {
    match conn.conn.stream_close_send(StreamId::from(stream_id)) {
        Ok(()) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

// WebTransport streams can be unidirectional and bidirectional.
// It is mapped to and from neqo's StreamType enum.
#[repr(C)]
pub enum WebTransportStreamType {
    BiDi,
    UniDi,
}

impl From<StreamType> for WebTransportStreamType {
    fn from(t: StreamType) -> WebTransportStreamType {
        match t {
            StreamType::BiDi => WebTransportStreamType::BiDi,
            StreamType::UniDi => WebTransportStreamType::UniDi,
        }
    }
}

impl From<WebTransportStreamType> for StreamType {
    fn from(t: WebTransportStreamType) -> StreamType {
        match t {
            WebTransportStreamType::BiDi => StreamType::BiDi,
            WebTransportStreamType::UniDi => StreamType::UniDi,
        }
    }
}

#[repr(C)]
pub enum SessionCloseReasonExternal {
    Error(u64),
    Status(u16),
    Clean(u32),
}

impl SessionCloseReasonExternal {
    fn new(reason: SessionCloseReason, data: &mut ThinVec<u8>) -> SessionCloseReasonExternal {
        match reason {
            SessionCloseReason::Error(e) => SessionCloseReasonExternal::Error(e),
            SessionCloseReason::Status(s) => SessionCloseReasonExternal::Status(s),
            SessionCloseReason::Clean { error, message } => {
                data.extend_from_slice(message.as_ref());
                SessionCloseReasonExternal::Clean(error)
            }
        }
    }
}

#[repr(C)]
pub enum WebTransportEventExternal {
    Negotiated(bool),
    Session(u64),
    SessionClosed {
        stream_id: u64,
        reason: SessionCloseReasonExternal,
    },
    NewStream {
        stream_id: u64,
        stream_type: WebTransportStreamType,
        session_id: u64,
    },
    Datagram {
        session_id: u64,
    },
}

impl WebTransportEventExternal {
    fn new(event: WebTransportEvent, data: &mut ThinVec<u8>) -> WebTransportEventExternal {
        match event {
            WebTransportEvent::Negotiated(n) => WebTransportEventExternal::Negotiated(n),
            WebTransportEvent::Session {
                stream_id,
                status,
                headers: _,
            } => {
                data.extend_from_slice(b"HTTP/3 ");
                data.extend_from_slice(&status.to_string().as_bytes());
                data.extend_from_slice(b"\r\n\r\n");
                WebTransportEventExternal::Session(stream_id.as_u64())
            }
            WebTransportEvent::SessionClosed {
                stream_id,
                reason,
                headers: _,
            } => match reason {
                SessionCloseReason::Status(status) => {
                    data.extend_from_slice(b"HTTP/3 ");
                    data.extend_from_slice(&status.to_string().as_bytes());
                    data.extend_from_slice(b"\r\n\r\n");
                    WebTransportEventExternal::Session(stream_id.as_u64())
                }
                _ => WebTransportEventExternal::SessionClosed {
                    stream_id: stream_id.as_u64(),
                    reason: SessionCloseReasonExternal::new(reason, data),
                },
            },
            WebTransportEvent::NewStream {
                stream_id,
                session_id,
            } => WebTransportEventExternal::NewStream {
                stream_id: stream_id.as_u64(),
                stream_type: stream_id.stream_type().into(),
                session_id: session_id.as_u64(),
            },
            WebTransportEvent::Datagram {
                session_id,
                datagram,
            } => {
                data.extend_from_slice(datagram.as_ref());
                WebTransportEventExternal::Datagram {
                    session_id: session_id.as_u64(),
                }
            }
        }
    }
}

#[repr(C)]
pub enum Http3Event {
    /// A request stream has space for more data to be sent.
    DataWritable {
        stream_id: u64,
    },
    /// A server has send STOP_SENDING frame.
    StopSending {
        stream_id: u64,
        error: u64,
    },
    HeaderReady {
        stream_id: u64,
        fin: bool,
        interim: bool,
    },
    /// New bytes available for reading.
    DataReadable {
        stream_id: u64,
    },
    /// Peer reset the stream.
    Reset {
        stream_id: u64,
        error: u64,
        local: bool,
    },
    /// A PushPromise
    PushPromise {
        push_id: u64,
        request_stream_id: u64,
    },
    /// A push response headers are ready.
    PushHeaderReady {
        push_id: u64,
        fin: bool,
    },
    /// New bytes are available on a push stream for reading.
    PushDataReadable {
        push_id: u64,
    },
    /// A push has been canceled.
    PushCanceled {
        push_id: u64,
    },
    PushReset {
        push_id: u64,
        error: u64,
    },
    RequestsCreatable,
    AuthenticationNeeded,
    ZeroRttRejected,
    ConnectionConnected,
    GoawayReceived,
    ConnectionClosing {
        error: CloseError,
    },
    ConnectionClosed {
        error: CloseError,
    },
    ResumptionToken {
        expire_in: u64, // microseconds
    },
    EchFallbackAuthenticationNeeded,
    WebTransport(WebTransportEventExternal),
    NoEvent,
}

fn sanitize_header(mut y: Cow<[u8]>) -> Cow<[u8]> {
    for i in 0..y.len() {
        if matches!(y[i], b'\n' | b'\r' | b'\0') {
            y.to_mut()[i] = b' ';
        }
    }
    y
}

fn convert_h3_to_h1_headers(headers: Vec<Header>, ret_headers: &mut ThinVec<u8>) -> nsresult {
    if headers.iter().filter(|&h| h.name() == ":status").count() != 1 {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    let status_val = headers
        .iter()
        .find(|&h| h.name() == ":status")
        .expect("must be one")
        .value();

    ret_headers.extend_from_slice(b"HTTP/3 ");
    ret_headers.extend_from_slice(status_val.as_bytes());
    ret_headers.extend_from_slice(b"\r\n");

    for hdr in headers.iter().filter(|&h| h.name() != ":status") {
        ret_headers.extend_from_slice(&sanitize_header(Cow::from(hdr.name().as_bytes())));
        ret_headers.extend_from_slice(b": ");
        ret_headers.extend_from_slice(&sanitize_header(Cow::from(hdr.value().as_bytes())));
        ret_headers.extend_from_slice(b"\r\n");
    }
    ret_headers.extend_from_slice(b"\r\n");
    return NS_OK;
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_event(
    conn: &mut NeqoHttp3Conn,
    ret_event: &mut Http3Event,
    data: &mut ThinVec<u8>,
) -> nsresult {
    while let Some(evt) = conn.conn.next_event() {
        let fe = match evt {
            Http3ClientEvent::DataWritable { stream_id } => Http3Event::DataWritable {
                stream_id: stream_id.as_u64(),
            },
            Http3ClientEvent::StopSending { stream_id, error } => Http3Event::StopSending {
                stream_id: stream_id.as_u64(),
                error,
            },
            Http3ClientEvent::HeaderReady {
                stream_id,
                headers,
                fin,
                interim,
            } => {
                let res = convert_h3_to_h1_headers(headers, data);
                if res != NS_OK {
                    return res;
                }
                Http3Event::HeaderReady {
                    stream_id: stream_id.as_u64(),
                    fin,
                    interim,
                }
            }
            Http3ClientEvent::DataReadable { stream_id } => Http3Event::DataReadable {
                stream_id: stream_id.as_u64(),
            },
            Http3ClientEvent::Reset {
                stream_id,
                error,
                local,
            } => Http3Event::Reset {
                stream_id: stream_id.as_u64(),
                error,
                local,
            },
            Http3ClientEvent::PushPromise {
                push_id,
                request_stream_id,
                headers,
            } => {
                let res = convert_h3_to_h1_headers(headers, data);
                if res != NS_OK {
                    return res;
                }
                Http3Event::PushPromise {
                    push_id,
                    request_stream_id: request_stream_id.as_u64(),
                }
            }
            Http3ClientEvent::PushHeaderReady {
                push_id,
                headers,
                fin,
                interim,
            } => {
                if interim {
                    Http3Event::NoEvent
                } else {
                    let res = convert_h3_to_h1_headers(headers, data);
                    if res != NS_OK {
                        return res;
                    }
                    Http3Event::PushHeaderReady { push_id, fin }
                }
            }
            Http3ClientEvent::PushDataReadable { push_id } => {
                Http3Event::PushDataReadable { push_id }
            }
            Http3ClientEvent::PushCanceled { push_id } => Http3Event::PushCanceled { push_id },
            Http3ClientEvent::PushReset { push_id, error } => {
                Http3Event::PushReset { push_id, error }
            }
            Http3ClientEvent::RequestsCreatable => Http3Event::RequestsCreatable,
            Http3ClientEvent::AuthenticationNeeded => Http3Event::AuthenticationNeeded,
            Http3ClientEvent::ZeroRttRejected => Http3Event::ZeroRttRejected,
            Http3ClientEvent::ResumptionToken(token) => {
                // expiration_time time is Instant, transform it into microseconds it will
                // be valid for. Necko code will add the value to PR_Now() to get the expiration
                // time in PRTime.
                if token.expiration_time() > Instant::now() {
                    let e = (token.expiration_time() - Instant::now()).as_micros();
                    if let Ok(expire_in) = u64::try_from(e) {
                        data.extend_from_slice(token.as_ref());
                        Http3Event::ResumptionToken { expire_in }
                    } else {
                        Http3Event::NoEvent
                    }
                } else {
                    Http3Event::NoEvent
                }
            }
            Http3ClientEvent::GoawayReceived => Http3Event::GoawayReceived,
            Http3ClientEvent::StateChange(state) => match state {
                Http3State::Connected => Http3Event::ConnectionConnected,
                Http3State::Closing(error_code) => {
                    match error_code {
                        neqo_transport::ConnectionError::Transport(
                            TransportError::CryptoError(neqo_crypto::Error::EchRetry(ref c)),
                        )
                        | neqo_transport::ConnectionError::Transport(TransportError::EchRetry(
                            ref c,
                        )) => {
                            data.extend_from_slice(c.as_ref());
                        }
                        _ => {}
                    }
                    Http3Event::ConnectionClosing {
                        error: error_code.into(),
                    }
                }
                Http3State::Closed(error_code) => {
                    match error_code {
                        neqo_transport::ConnectionError::Transport(
                            TransportError::CryptoError(neqo_crypto::Error::EchRetry(ref c)),
                        )
                        | neqo_transport::ConnectionError::Transport(TransportError::EchRetry(
                            ref c,
                        )) => {
                            data.extend_from_slice(c.as_ref());
                        }
                        _ => {}
                    }
                    Http3Event::ConnectionClosed {
                        error: error_code.into(),
                    }
                }
                _ => Http3Event::NoEvent,
            },
            Http3ClientEvent::EchFallbackAuthenticationNeeded { public_name } => {
                data.extend_from_slice(public_name.as_ref());
                Http3Event::EchFallbackAuthenticationNeeded
            }
            Http3ClientEvent::WebTransport(e) => {
                Http3Event::WebTransport(WebTransportEventExternal::new(e, data))
            }
        };

        if !matches!(fe, Http3Event::NoEvent) {
            *ret_event = fe;
            return NS_OK;
        }
    }

    *ret_event = Http3Event::NoEvent;
    NS_OK
}

// Read response data into buf.
#[no_mangle]
pub unsafe extern "C" fn neqo_http3conn_read_response_data(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    buf: *mut u8,
    len: u32,
    read: &mut u32,
    fin: &mut bool,
) -> nsresult {
    let array = slice::from_raw_parts_mut(buf, len as usize);
    match conn.conn.read_data(
        get_current_or_last_output_time(&conn.last_output_time),
        StreamId::from(stream_id),
        &mut array[..],
    ) {
        Ok((amount, fin_recvd)) => {
            *read = u32::try_from(amount).unwrap();
            *fin = fin_recvd;
            if (amount == 0) && !fin_recvd {
                NS_BASE_STREAM_WOULD_BLOCK
            } else {
                NS_OK
            }
        }
        Err(Http3Error::InvalidStreamId)
        | Err(Http3Error::TransportError(TransportError::NoMoreData)) => NS_ERROR_INVALID_ARG,
        Err(_) => NS_ERROR_NET_HTTP3_PROTOCOL_ERROR,
    }
}

#[repr(C)]
pub struct NeqoSecretInfo {
    set: bool,
    version: u16,
    cipher: u16,
    group: u16,
    resumed: bool,
    early_data: bool,
    alpn: nsCString,
    signature_scheme: u16,
    ech_accepted: bool,
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_tls_info(
    conn: &mut NeqoHttp3Conn,
    sec_info: &mut NeqoSecretInfo,
) -> nsresult {
    match conn.conn.tls_info() {
        Some(info) => {
            sec_info.set = true;
            sec_info.version = info.version();
            sec_info.cipher = info.cipher_suite();
            sec_info.group = info.key_exchange();
            sec_info.resumed = info.resumed();
            sec_info.early_data = info.early_data_accepted();
            sec_info.alpn = match info.alpn() {
                Some(a) => nsCString::from(a),
                None => nsCString::new(),
            };
            sec_info.signature_scheme = info.signature_scheme();
            sec_info.ech_accepted = info.ech_accepted();
            NS_OK
        }
        None => NS_ERROR_NOT_AVAILABLE,
    }
}

#[repr(C)]
pub struct NeqoCertificateInfo {
    certs: ThinVec<ThinVec<u8>>,
    stapled_ocsp_responses_present: bool,
    stapled_ocsp_responses: ThinVec<ThinVec<u8>>,
    signed_cert_timestamp_present: bool,
    signed_cert_timestamp: ThinVec<u8>,
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_peer_certificate_info(
    conn: &mut NeqoHttp3Conn,
    neqo_certs_info: &mut NeqoCertificateInfo,
) -> nsresult {
    let mut certs_info = match conn.conn.peer_certificate() {
        Some(certs) => certs,
        None => return NS_ERROR_NOT_AVAILABLE,
    };

    neqo_certs_info.certs = certs_info
        .map(|cert| cert.iter().cloned().collect())
        .collect();

    match &mut certs_info.stapled_ocsp_responses() {
        Some(ocsp_val) => {
            neqo_certs_info.stapled_ocsp_responses_present = true;
            neqo_certs_info.stapled_ocsp_responses = ocsp_val
                .iter()
                .map(|ocsp| ocsp.iter().cloned().collect())
                .collect();
        }
        None => {
            neqo_certs_info.stapled_ocsp_responses_present = false;
        }
    };

    match certs_info.signed_cert_timestamp() {
        Some(sct_val) => {
            neqo_certs_info.signed_cert_timestamp_present = true;
            neqo_certs_info
                .signed_cert_timestamp
                .extend_from_slice(sct_val);
        }
        None => {
            neqo_certs_info.signed_cert_timestamp_present = false;
        }
    };

    NS_OK
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_authenticated(conn: &mut NeqoHttp3Conn, error: PRErrorCode) {
    conn.conn.authenticated(
        error.into(),
        get_current_or_last_output_time(&conn.last_output_time),
    );
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_set_resumption_token(
    conn: &mut NeqoHttp3Conn,
    token: &mut ThinVec<u8>,
) {
    let _ = conn.conn.enable_resumption(
        get_current_or_last_output_time(&conn.last_output_time),
        token,
    );
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_set_ech_config(
    conn: &mut NeqoHttp3Conn,
    ech_config: &mut ThinVec<u8>,
) {
    let _ = conn.conn.enable_ech(ech_config);
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_is_zero_rtt(conn: &mut NeqoHttp3Conn) -> bool {
    conn.conn.state() == Http3State::ZeroRtt
}

#[repr(C)]
#[derive(Default)]
pub struct Http3Stats {
    /// Total packets received, including all the bad ones.
    pub packets_rx: usize,
    /// Duplicate packets received.
    pub dups_rx: usize,
    /// Dropped packets or dropped garbage.
    pub dropped_rx: usize,
    /// The number of packet that were saved for later processing.
    pub saved_datagrams: usize,
    /// Total packets sent.
    pub packets_tx: usize,
    /// Total number of packets that are declared lost.
    pub lost: usize,
    /// Late acknowledgments, for packets that were declared lost already.
    pub late_ack: usize,
    /// Acknowledgments for packets that contained data that was marked
    /// for retransmission when the PTO timer popped.
    pub pto_ack: usize,
    /// Count PTOs. Single PTOs, 2 PTOs in a row, 3 PTOs in row, etc. are counted
    /// separately.
    pub pto_counts: [usize; 16],
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_get_stats(conn: &mut NeqoHttp3Conn, stats: &mut Http3Stats) {
    let t_stats = conn.conn.transport_stats();
    stats.packets_rx = t_stats.packets_rx;
    stats.dups_rx = t_stats.dups_rx;
    stats.dropped_rx = t_stats.dropped_rx;
    stats.saved_datagrams = t_stats.saved_datagrams;
    stats.packets_tx = t_stats.packets_tx;
    stats.lost = t_stats.lost;
    stats.late_ack = t_stats.late_ack;
    stats.pto_ack = t_stats.pto_ack;
    stats.pto_counts = t_stats.pto_counts;
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_webtransport_create_session(
    conn: &mut NeqoHttp3Conn,
    host: &nsACString,
    path: &nsACString,
    headers: &nsACString,
    stream_id: &mut u64,
) -> nsresult {
    let hdrs = match parse_headers(headers) {
        Err(e) => {
            return e;
        }
        Ok(h) => h,
    };
    let host_tmp = match str::from_utf8(host) {
        Ok(h) => h,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    let path_tmp = match str::from_utf8(path) {
        Ok(p) => p,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };

    match conn.conn.webtransport_create_session(
        get_current_or_last_output_time(&conn.last_output_time),
        &("https", host_tmp, path_tmp),
        &hdrs,
    ) {
        Ok(id) => {
            *stream_id = id.as_u64();
            NS_OK
        }
        Err(Http3Error::StreamLimitError) => NS_BASE_STREAM_WOULD_BLOCK,
        Err(_) => NS_ERROR_UNEXPECTED,
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_webtransport_close_session(
    conn: &mut NeqoHttp3Conn,
    session_id: u64,
    error: u32,
    message: &nsACString,
) -> nsresult {
    let message_tmp = match str::from_utf8(message) {
        Ok(p) => p,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    match conn
        .conn
        .webtransport_close_session(StreamId::from(session_id), error, message_tmp)
    {
        Ok(()) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_webtransport_create_stream(
    conn: &mut NeqoHttp3Conn,
    session_id: u64,
    stream_type: WebTransportStreamType,
    stream_id: &mut u64,
) -> nsresult {
    match conn
        .conn
        .webtransport_create_stream(StreamId::from(session_id), stream_type.into())
    {
        Ok(id) => {
            *stream_id = id.as_u64();
            NS_OK
        }
        Err(Http3Error::StreamLimitError) => NS_BASE_STREAM_WOULD_BLOCK,
        Err(_) => NS_ERROR_UNEXPECTED,
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_webtransport_send_datagram(
    conn: &mut NeqoHttp3Conn,
    session_id: u64,
    data: &mut ThinVec<u8>,
    tracking_id: u64,
) -> nsresult {
    let id = if tracking_id == 0 {
        None
    } else {
        Some(tracking_id)
    };
    match conn
        .conn
        .webtransport_send_datagram(StreamId::from(session_id), data, id)
    {
        Ok(()) => NS_OK,
        Err(Http3Error::TransportError(TransportError::TooMuchData)) => NS_ERROR_NOT_AVAILABLE,
        Err(_) => NS_ERROR_UNEXPECTED,
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_webtransport_max_datagram_size(
    conn: &mut NeqoHttp3Conn,
    session_id: u64,
    result: &mut u64,
) -> nsresult {
    match conn
        .conn
        .webtransport_max_datagram_size(StreamId::from(session_id))
    {
        Ok(size) => {
            *result = size;
            NS_OK
        }
        Err(_) => NS_ERROR_UNEXPECTED,
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_webtransport_set_sendorder(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    sendorder: *const i64,
) -> nsresult {
    unsafe {
        match conn
            .conn
            .webtransport_set_sendorder(StreamId::from(stream_id), sendorder.as_ref().copied())
        {
            Ok(()) => NS_OK,
            Err(_) => NS_ERROR_UNEXPECTED,
        }
    }
}
