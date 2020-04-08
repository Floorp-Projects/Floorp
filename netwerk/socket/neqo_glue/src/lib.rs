/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use neqo_common::Datagram;
use neqo_crypto::{init, PRErrorCode};
use neqo_http3::Error as Http3Error;
use neqo_http3::{Http3Client, Http3ClientEvent, Http3State};
use neqo_transport::Error as TransportError;
use neqo_transport::{FixedConnectionIdManager, Output};
use nserror::*;
use nsstring::*;
use std::cell::RefCell;
use std::convert::TryFrom;
use std::net::SocketAddr;
use std::ptr;
use std::rc::Rc;
use std::slice;
use std::str;
use std::time::Instant;
use thin_vec::ThinVec;
use xpcom::{interfaces::nsrefcnt, AtomicRefcnt, RefCounted, RefPtr};

#[repr(C)]
pub struct NeqoHttp3Conn {
    conn: Http3Client,
    local_addr: SocketAddr,
    remote_addr: SocketAddr,
    refcnt: AtomicRefcnt,
    packets_to_send: Vec<Datagram>,
}

impl NeqoHttp3Conn {
    fn new(
        origin: &nsACString,
        alpn: &nsACString,
        local_addr: &nsACString,
        remote_addr: &nsACString,
        max_table_size: u64,
        max_blocked_streams: u16,
    ) -> Result<RefPtr<NeqoHttp3Conn>, nsresult> {
        // Nss init.
        init();

        let origin_conv = str::from_utf8(origin).map_err(|_| NS_ERROR_INVALID_ARG)?;

        let alpn_conv = str::from_utf8(alpn).map_err(|_| NS_ERROR_INVALID_ARG)?;

        let local: SocketAddr = match str::from_utf8(local_addr) {
            Ok(s) => match s.parse() {
                Ok(addr) => addr,
                Err(_) => return Err(NS_ERROR_INVALID_ARG),
            },
            Err(_) => return Err(NS_ERROR_INVALID_ARG),
        };

        let remote: SocketAddr = match str::from_utf8(remote_addr) {
            Ok(s) => match s.parse() {
                Ok(addr) => addr,
                Err(_) => return Err(NS_ERROR_INVALID_ARG),
            },
            Err(_) => return Err(NS_ERROR_INVALID_ARG),
        };

        let conn = match Http3Client::new(
            origin_conv,
            &[alpn_conv],
            Rc::new(RefCell::new(FixedConnectionIdManager::new(3))),
            local,
            remote,
            max_table_size,
            max_blocked_streams,
        ) {
            Ok(c) => c,
            Err(_) => return Err(NS_ERROR_INVALID_ARG),
        };

        let conn = Box::into_raw(Box::new(NeqoHttp3Conn {
            conn,
            local_addr: local,
            remote_addr: remote,
            refcnt: unsafe { AtomicRefcnt::new() },
            packets_to_send: Vec::new(),
        }));
        unsafe { Ok(RefPtr::from_raw(conn).unwrap()) }
    }
}

#[no_mangle]
pub unsafe extern "C" fn neqo_http3conn_addref(conn: &NeqoHttp3Conn) -> nsrefcnt {
    conn.refcnt.inc()
}

#[no_mangle]
pub unsafe extern "C" fn neqo_http3conn_release(conn: &NeqoHttp3Conn) -> nsrefcnt {
    let rc = conn.refcnt.dec();
    if rc == 0 {
        Box::from_raw(conn as *const _ as *mut NeqoHttp3Conn);
    }
    rc
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
    local_addr: &nsACString,
    remote_addr: &nsACString,
    max_table_size: u64,
    max_blocked_streams: u16,
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
pub extern "C" fn neqo_http3conn_process_input(
    conn: &mut NeqoHttp3Conn,
    packet: *const u8,
    len: u32,
) {
    let array = unsafe { slice::from_raw_parts(packet, len as usize) };
    conn.conn.process_input(
        Datagram::new(conn.remote_addr, conn.local_addr, array.to_vec()),
        Instant::now(),
    );
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_process_http3(conn: &mut NeqoHttp3Conn) {
    conn.conn.process_http3(Instant::now());
}

/* Process output and store data to be sent into conn.packets_to_send.
 * neqo_http3conn_get_data_to_send will be called to pick up this data.
 */
#[no_mangle]
pub extern "C" fn neqo_http3conn_process_output(conn: &mut NeqoHttp3Conn) -> u64 {
    loop {
        let out = conn.conn.process_output(Instant::now());
        match out {
            Output::Datagram(dg) => {
                conn.packets_to_send.push(dg);
            }
            Output::Callback(to) => {
                let timeout = to.as_millis() as u64;
                break timeout;
            }
            Output::None => break std::u64::MAX,
        }
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_process_timer(conn: &mut NeqoHttp3Conn) {
    conn.conn.process_timer(Instant::now());
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_has_data_to_send(conn: &mut NeqoHttp3Conn) -> bool {
    !conn.packets_to_send.is_empty()
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_get_data_to_send(
    conn: &mut NeqoHttp3Conn,
    packet: &mut ThinVec<u8>,
) -> nsresult {
    match conn.packets_to_send.pop() {
        None => NS_BASE_STREAM_WOULD_BLOCK,
        Some(d) => {
            packet.extend_from_slice(&d);
            NS_OK
        }
    }
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_close(conn: &mut NeqoHttp3Conn, error: u64) {
    conn.conn.close(Instant::now(), error, "");
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

#[no_mangle]
pub extern "C" fn neqo_http3conn_fetch(
    conn: &mut NeqoHttp3Conn,
    method: &nsACString,
    scheme: &nsACString,
    host: &nsACString,
    path: &nsACString,
    headers: &nsACString,
    stream_id: &mut u64,
) -> nsresult {
    let mut hdrs = Vec::new();
    // this is only used for headers built by Firefox.
    // Firefox supplies all headers already prepared for sending over http1.
    // They need to be split into (String, String) pairs.
    match str::from_utf8(headers) {
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
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

                hdrs.push((name, value));
            }
        }
    }

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
    match conn
        .conn
        .fetch(method_tmp, scheme_tmp, host_tmp, path_tmp, &hdrs)
    {
        Ok(id) => {
            *stream_id = id;
            NS_OK
        }
        Err(Http3Error::TransportError(TransportError::StreamLimitError)) => {
            NS_BASE_STREAM_WOULD_BLOCK
        }
        Err(_) => NS_ERROR_UNEXPECTED,
    }
}

#[no_mangle]
pub extern "C" fn neqo_htttp3conn_send_request_body(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    buf: *const u8,
    len: u32,
    read: &mut u32,
) -> nsresult {
    let array = unsafe { slice::from_raw_parts(buf, len as usize) };
    match conn.conn.send_request_body(stream_id, array) {
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

// This is only used for telemetry. Therefore we only return error code
// numbers and do not label them. Recording telemetry is easier with a
// number.
#[repr(C)]
pub enum CloseError {
    QuicTransportError(u64),
    Http3AppError(u64),
}

impl From<neqo_transport::CloseError> for CloseError {
    fn from(error: neqo_transport::CloseError) -> CloseError {
        match error {
            neqo_transport::CloseError::Transport(c) => CloseError::QuicTransportError(c),
            neqo_transport::CloseError::Application(c) => CloseError::Http3AppError(c),
        }
    }
}

// Reset a stream with streamId.
#[no_mangle]
pub extern "C" fn neqo_http3conn_reset_stream(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    error: u64,
) -> nsresult {
    match conn.conn.stream_reset(stream_id, error) {
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
    match conn.conn.stream_close_send(stream_id) {
        Ok(()) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

#[repr(C)]
pub enum Http3Event {
    /// A request stream has space for more data to be send.
    DataWritable {
        stream_id: u64,
    },
    /// A server has send STOP_SENDING frame.
    StopSending {
        stream_id: u64,
    },
    HeaderReady {
        stream_id: u64,
    },
    /// New bytes available for reading.
    DataReadable {
        stream_id: u64,
    },
    /// Peer reset the stream.
    Reset {
        stream_id: u64,
        error: u64,
    },
    /// A new push stream
    NewPushStream {
        stream_id: u64,
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
    NoEvent,
}

#[no_mangle]
pub extern "C" fn neqo_http3conn_event(conn: &mut NeqoHttp3Conn) -> Http3Event {
    loop {
        match conn.conn.next_event() {
            None => break Http3Event::NoEvent,
            Some(e) => {
                let fe: Http3Event = e.into();
                match fe {
                    Http3Event::NoEvent => {}
                    _ => break fe,
                };
            }
        }
    }
}

impl From<Http3ClientEvent> for Http3Event {
    fn from(event: Http3ClientEvent) -> Self {
        match event {
            Http3ClientEvent::DataWritable { stream_id } => Http3Event::DataWritable { stream_id },
            Http3ClientEvent::StopSending { stream_id, .. } => {
                Http3Event::StopSending { stream_id }
            }
            Http3ClientEvent::HeaderReady { stream_id } => Http3Event::HeaderReady { stream_id },
            Http3ClientEvent::DataReadable { stream_id } => Http3Event::DataReadable { stream_id },
            Http3ClientEvent::Reset { stream_id, error } => Http3Event::Reset { stream_id, error },
            Http3ClientEvent::NewPushStream { stream_id } => {
                Http3Event::NewPushStream { stream_id }
            }
            Http3ClientEvent::RequestsCreatable => Http3Event::RequestsCreatable,
            Http3ClientEvent::AuthenticationNeeded => Http3Event::AuthenticationNeeded,
            Http3ClientEvent::ZeroRttRejected => Http3Event::ZeroRttRejected,
            Http3ClientEvent::GoawayReceived => Http3Event::GoawayReceived,
            Http3ClientEvent::StateChange(state) => match state {
                Http3State::Connected => Http3Event::ConnectionConnected,
                Http3State::Closing(error_code) => Http3Event::ConnectionClosing {
                    error: error_code.into(),
                },
                Http3State::Closed(error_code) => Http3Event::ConnectionClosed {
                    error: error_code.into(),
                },
                _ => Http3Event::NoEvent,
            },
        }
    }
}

// Read response headers.
// Firefox needs these headers to look like http1 heeaders, so we are
// building that here.
#[no_mangle]
pub extern "C" fn neqo_http3conn_read_response_headers(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    headers: &mut ThinVec<u8>,
    fin: &mut bool,
) -> nsresult {
    match conn.conn.read_response_headers(stream_id) {
        Ok((h, fin_recvd)) => {
            let status_element: Vec<_> = h.iter().filter(|elem| elem.0 == ":status").collect();
            if status_element.len() != 1 {
                return NS_ERROR_ILLEGAL_VALUE;
            }
            headers.extend_from_slice("HTTP/3 ".as_bytes());
            headers.extend_from_slice(status_element[0].1.as_bytes());
            headers.extend_from_slice("\r\n".as_bytes());

            for elem in h.iter().filter(|elem| elem.0 != ":status") {
                headers.extend_from_slice(&elem.0.as_bytes());
                headers.extend_from_slice(": ".as_bytes());
                headers.extend_from_slice(&elem.1.as_bytes());
                headers.extend_from_slice("\r\n".as_bytes());
            }
            headers.extend_from_slice("\r\n".as_bytes());
            *fin = fin_recvd;
            NS_OK
        }
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

// Read response data into buf.
#[no_mangle]
pub extern "C" fn neqo_http3conn_read_response_data(
    conn: &mut NeqoHttp3Conn,
    stream_id: u64,
    buf: *mut u8,
    len: u32,
    read: &mut u32,
    fin: &mut bool,
) -> nsresult {
    let array = unsafe { slice::from_raw_parts_mut(buf, len as usize) };
    match conn
        .conn
        .read_response_data(Instant::now(), stream_id, &mut array[..])
    {
        Ok((amount, fin_recvd)) => {
            *read = u32::try_from(amount).unwrap();
            *fin = fin_recvd;
            NS_OK
        }
        Err(Http3Error::TransportError(TransportError::InvalidStreamId))
        | Err(Http3Error::TransportError(TransportError::NoMoreData)) => NS_ERROR_INVALID_ARG,
        Err(Http3Error::HttpFrameError) => NS_ERROR_ABORT,
        Err(_) => NS_ERROR_UNEXPECTED,
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
    conn.conn.authenticated(error.into(), Instant::now());
}
