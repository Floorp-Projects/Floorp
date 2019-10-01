/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use byteorder::{BigEndian, WriteBytesExt};
use socket2::{Domain, Socket, Type};
use std::collections::HashMap;
use std::ffi::{c_void, CStr, CString};
use std::io;
use std::net;
use std::os::raw::c_char;
use std::sync::mpsc::channel;
use std::thread;
use std::time;
use uuid::Uuid;

#[macro_use]
extern crate log;

struct Callback {
    data: *const c_void,
    resolved: extern "C" fn(*const c_void, *const c_char, *const c_char),
}

unsafe impl Send for Callback {}

fn hostname_resolved(callback: &Callback, hostname: &str, addr: &str) {
    if let Ok(hostname) = CString::new(hostname) {
        if let Ok(addr) = CString::new(addr) {
            (callback.resolved)(callback.data, hostname.as_ptr(), addr.as_ptr());
        }
    }
}

// This code is derived from code for creating questions in the dns-parser
// crate. It would be nice to upstream this, or something similar.
fn create_answer(id: u16, answers: &Vec<(String, &[u8])>) -> Result<Vec<u8>, io::Error> {
    let mut buf = Vec::with_capacity(512);
    let head = dns_parser::Header {
        id: id,
        query: false,
        opcode: dns_parser::Opcode::StandardQuery,
        authoritative: true,
        truncated: false,
        recursion_desired: false,
        recursion_available: false,
        authenticated_data: false,
        checking_disabled: false,
        response_code: dns_parser::ResponseCode::NoError,
        questions: 0,
        answers: answers.len() as u16,
        nameservers: 0,
        additional: 0,
    };

    buf.extend([0u8; 12].iter());
    head.write(&mut buf[..12]);

    for (name, addr) in answers {
        for part in name.split('.') {
            if part.len() > 62 {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "Name part length too long",
                ));
            }
            let ln = part.len() as u8;
            buf.push(ln);
            buf.extend(part.as_bytes());
        }
        buf.push(0);

        if addr.len() == 4 {
            buf.write_u16::<BigEndian>(dns_parser::Type::A as u16)?;
        } else {
            buf.write_u16::<BigEndian>(dns_parser::Type::AAAA as u16)?;
        }
        // set cache flush bit
        buf.write_u16::<BigEndian>(dns_parser::Class::IN as u16 | (0x1 << 15))?;
        buf.write_u32::<BigEndian>(120)?;
        buf.write_u16::<BigEndian>(addr.len() as u16)?;
        buf.extend(*addr);
    }

    Ok(buf)
}

fn create_query(id: u16, queries: &Vec<String>) -> Result<Vec<u8>, io::Error> {
    let mut buf = Vec::with_capacity(512);
    let head = dns_parser::Header {
        id: id,
        query: true,
        opcode: dns_parser::Opcode::StandardQuery,
        authoritative: false,
        truncated: false,
        recursion_desired: false,
        recursion_available: false,
        authenticated_data: false,
        checking_disabled: false,
        response_code: dns_parser::ResponseCode::NoError,
        questions: queries.len() as u16,
        answers: 0,
        nameservers: 0,
        additional: 0,
    };

    buf.extend([0u8; 12].iter());
    head.write(&mut buf[..12]);

    for name in queries {
        for part in name.split('.') {
            assert!(part.len() < 63);
            let ln = part.len() as u8;
            buf.push(ln);
            buf.extend(part.as_bytes());
        }
        buf.push(0);

        buf.write_u16::<BigEndian>(dns_parser::QueryType::A as u16)?;
        buf.write_u16::<BigEndian>(dns_parser::QueryClass::IN as u16)?;
    }

    Ok(buf)
}

fn validate_hostname(hostname: &str) -> bool {
    match hostname.find(".local") {
        Some(index) => match hostname.get(0..index) {
            Some(uuid) => match uuid.get(0..36) {
                Some(initial) => match Uuid::parse_str(initial) {
                    Ok(_) => {
                        // Oddly enough, Safari does not generate valid UUIDs,
                        // the last part sometimes contains more than 12 digits.
                        match uuid.get(36..) {
                            Some(trailing) => {
                                for c in trailing.chars() {
                                    if !c.is_ascii_hexdigit() {
                                        return false;
                                    }
                                }
                                return true;
                            }
                            None => true,
                        }
                    }
                    Err(_) => {
                        return false;
                    }
                },
                None => false,
            },
            None => false,
        },
        None => false,
    }
}

enum ServiceControl {
    Register {
        hostname: String,
        address: String,
    },
    Query {
        callback: Callback,
        hostname: String,
    },
    Unregister {
        hostname: String,
    },
    Stop,
}

pub struct MDNSService {
    handle: Option<std::thread::JoinHandle<()>>,
    sender: Option<std::sync::mpsc::Sender<ServiceControl>>,
}

impl MDNSService {
    fn register_hostname(&mut self, hostname: &str, address: &str) {
        if let Some(sender) = &self.sender {
            if let Err(err) = sender.send(ServiceControl::Register {
                hostname: hostname.to_string(),
                address: address.to_string(),
            }) {
                warn!(
                    "Could not send register hostname {} message: {}",
                    hostname, err
                );
            }
        }
    }

    fn query_hostname(&mut self, callback: Callback, hostname: &str) {
        if let Some(sender) = &self.sender {
            if let Err(err) = sender.send(ServiceControl::Query {
                callback: callback,
                hostname: hostname.to_string(),
            }) {
                warn!(
                    "Could not send query hostname {} message: {}",
                    hostname, err
                );
            }
        }
    }

    fn unregister_hostname(&mut self, hostname: &str) {
        if let Some(sender) = &self.sender {
            if let Err(err) = sender.send(ServiceControl::Unregister {
                hostname: hostname.to_string(),
            }) {
                warn!(
                    "Could not send unregister hostname {} message: {}",
                    hostname, err
                );
            }
        }
    }

    fn start(&mut self, addr: std::net::Ipv4Addr) -> io::Result<()> {
        let (sender, receiver) = channel();
        self.sender = Some(sender);

        let port = 5353;

        let socket = Socket::new(Domain::ipv4(), Type::dgram(), None).unwrap();
        socket.set_reuse_address(true)?;

        #[cfg(not(target_os = "windows"))]
        socket.set_reuse_port(true)?;
        socket.bind(&socket2::SockAddr::from(std::net::SocketAddr::from((
            [0, 0, 0, 0],
            port,
        ))))?;

        let socket = socket.into_udp_socket();
        socket.set_multicast_loop_v4(true)?;
        socket.set_read_timeout(Some(time::Duration::from_millis(10)))?;
        socket.join_multicast_v4(&std::net::Ipv4Addr::new(224, 0, 0, 251), &addr)?;

        let builder = thread::Builder::new().name("mdns_service".to_string());
        self.handle = Some(builder.spawn(move || {
            let mdns_addr = std::net::SocketAddr::from(([224, 0, 0, 251], port));
            let mut buffer: [u8; 1024] = [0; 1024];
            let mut hosts = HashMap::new();
            let mut queries = HashMap::new();
            loop {
                match receiver.recv_timeout(time::Duration::from_millis(10)) {
                    Ok(msg) => match msg {
                        ServiceControl::Register { hostname, address } => {
                            if !validate_hostname(&hostname) {
                                warn!("Not registering invalid hostname: {}", hostname);
                                continue;
                            }
                            trace!("Registering {} for: {}", hostname, address);
                            match address.parse().and_then(|ip| {
                                Ok(match ip {
                                    net::IpAddr::V4(ip) => ip.octets().to_vec(),
                                    net::IpAddr::V6(ip) => ip.octets().to_vec(),
                                })
                            }) {
                                Ok(octets) => {
                                    let mut v = Vec::new();
                                    v.extend(octets);
                                    hosts.insert(hostname, v);
                                }
                                Err(err) => {
                                    warn!(
                                        "Could not parse address for {}: {}: {}",
                                        hostname, address, err
                                    );
                                }
                            }
                        }
                        ServiceControl::Query { callback, hostname } => {
                            trace!("Querying {}", hostname);
                            if !validate_hostname(&hostname) {
                                warn!("Not sending mDNS query for invalid hostname: {}", hostname);
                                continue;
                            }
                            queries.insert(hostname.to_string(), callback);
                            //TODO: limit pending queries
                            if let Ok(buf) = create_query(0, &vec![hostname.to_string()]) {
                                if let Err(err) = socket.send_to(&buf, &mdns_addr) {
                                    warn!("Sending mDNS query failed: {}", err);
                                }
                            }
                        }
                        ServiceControl::Unregister { hostname } => {
                            trace!("Unregistering {}", hostname);
                            hosts.remove(&hostname);
                        }
                        ServiceControl::Stop => {
                            trace!("Stopping");
                            break;
                        }
                    },
                    Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                        break;
                    }
                    _ => {}
                }
                match socket.recv_from(&mut buffer) {
                    Ok((amt, _)) => {
                        if amt > 0 {
                            let buffer = &buffer[0..amt];
                            match dns_parser::Packet::parse(&buffer) {
                                Ok(parsed) => {
                                    let mut answers: Vec<(String, &[u8])> = Vec::new();

                                    // If a packet contains both both questions and
                                    // answers, the questions should be ignored.
                                    if parsed.answers.len() == 0 {
                                        parsed
                                            .questions
                                            .iter()
                                            .filter(|question| {
                                                question.qtype == dns_parser::QueryType::A
                                            })
                                            .for_each(|question| {
                                                let qname = question.qname.to_string();
                                                trace!(
                                                    "mDNS question: {} {:?}",
                                                    qname,
                                                    question.qtype
                                                );
                                                if let Some(octets) = hosts.get(&qname) {
                                                    trace!(
                                                        "Sending mDNS answer for {}: {:?}",
                                                        qname,
                                                        octets
                                                    );
                                                    answers.push((qname, &octets));
                                                }
                                            });
                                    }
                                    for answer in parsed.answers {
                                        let hostname = answer.name.to_string();
                                        match queries.get(&hostname) {
                                            Some(callback) => {
                                                match answer.data {
                                                    dns_parser::RData::A(
                                                        dns_parser::rdata::a::Record(addr),
                                                    ) => {
                                                        let addr = addr.to_string();
                                                        trace!(
                                                            "mDNS response: {} {}",
                                                            hostname,
                                                            addr
                                                        );
                                                        hostname_resolved(
                                                            callback, &hostname, &addr,
                                                        );
                                                    }
                                                    dns_parser::RData::AAAA(
                                                        dns_parser::rdata::aaaa::Record(addr),
                                                    ) => {
                                                        let addr = addr.to_string();
                                                        trace!(
                                                            "mDNS response: {} {}",
                                                            hostname,
                                                            addr
                                                        );
                                                        hostname_resolved(
                                                            callback, &hostname, &addr,
                                                        );
                                                    }
                                                    _ => {}
                                                }
                                                queries.remove(&hostname);
                                            }
                                            None => {
                                                continue;
                                            }
                                        }
                                    }
                                    // TODO: If we did not answer every query
                                    // in this question, we should wait for a
                                    // random amount of time so as to not
                                    // collide with someone else responding to
                                    // this query.
                                    if answers.len() > 0 {
                                        if let Ok(buf) = create_answer(parsed.header.id, &answers) {
                                            if let Err(err) = socket.send_to(&buf, &mdns_addr) {
                                                warn!("Sending mDNS answer failed: {}", err);
                                            }
                                        }
                                    }
                                }
                                Err(err) => {
                                    warn!("Could not parse mDNS packet: {}", err);
                                }
                            }
                        }
                    }
                    Err(err) => {
                        if err.kind() != io::ErrorKind::WouldBlock
                            && err.kind() != io::ErrorKind::TimedOut
                        {
                            error!("Socket error: {}", err);
                            break;
                        }
                    }
                }
            }
        })?);

        Ok(())
    }

    fn stop(self) {
        if let Some(sender) = self.sender {
            if let Err(err) = sender.send(ServiceControl::Stop) {
                warn!("Could not stop mDNS Service: {}", err);
            }
            if let Some(handle) = self.handle {
                if let Err(_) = handle.join() {
                    error!("Error on thread join");
                }
            }
        }
    }

    fn new() -> MDNSService {
        MDNSService {
            handle: None,
            sender: None,
        }
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_register_hostname(
    serv: *mut MDNSService,
    hostname: *const c_char,
    address: *const c_char,
) {
    assert!(!serv.is_null());
    assert!(!hostname.is_null());
    assert!(!address.is_null());
    unsafe {
        let hostname = CStr::from_ptr(hostname).to_string_lossy();
        let address = CStr::from_ptr(address).to_string_lossy();
        (*serv).register_hostname(&hostname, &address);
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_start(ifaddr: *const c_char) -> *mut MDNSService {
    assert!(!ifaddr.is_null());
    let mut r = Box::new(MDNSService::new());
    unsafe {
        let ifaddr = CStr::from_ptr(ifaddr).to_string_lossy();
        if let Ok(addr) = ifaddr.parse() {
            if let Err(err) = r.start(addr) {
                warn!("Could not start mDNS Service: {}", err);
            }
        } else {
            warn!("Could not parse interface address: {}", ifaddr);
        }
    }
    Box::into_raw(r)
}

#[no_mangle]
pub extern "C" fn mdns_service_stop(serv: *mut MDNSService) {
    assert!(!serv.is_null());
    unsafe {
        let boxed = Box::from_raw(serv);
        boxed.stop();
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_query_hostname(
    serv: *mut MDNSService,
    data: *const c_void,
    resolved: extern "C" fn(*const c_void, *const c_char, *const c_char),
    hostname: *const c_char,
) {
    assert!(!serv.is_null());
    assert!(!data.is_null());
    assert!(!hostname.is_null());
    unsafe {
        let hostname = CStr::from_ptr(hostname).to_string_lossy();
        let callback = Callback {
            data: data,
            resolved: resolved,
        };
        (*serv).query_hostname(callback, &hostname);
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_unregister_hostname(
    serv: *mut MDNSService,
    hostname: *const c_char,
) {
    assert!(!serv.is_null());
    assert!(!hostname.is_null());
    unsafe {
        let hostname = CStr::from_ptr(hostname).to_string_lossy();
        (*serv).unregister_hostname(&hostname);
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_generate_uuid() -> *const c_char {
    let uuid = Uuid::new_v4().to_hyphenated().to_string();
    match CString::new(uuid) {
        Ok(uuid) => uuid.into_raw(),
        Err(_) => unreachable!(), // UUID should not contain 0 byte
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_free_uuid(uuid: *mut c_char) {
    assert!(!uuid.is_null());
    unsafe {
        CString::from_raw(uuid);
    }
}
