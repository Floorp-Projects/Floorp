/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use byteorder::{BigEndian, WriteBytesExt};
use get_if_addrs;
use socket2::{Domain, Socket, Type};
use std::collections::HashMap;
use std::ffi::CStr;
use std::io;
use std::net;
use std::os::raw::c_char;
use std::sync::mpsc::channel;
use std::thread;
use std::time;

#[macro_use]
extern crate log;

// This code is derived from code for creating questions in the dns-parser
// crate. It would be nice to upstream this, or something similar.
fn create_answer(id: u16, answers: &Vec<(String, &[u8])>) -> Result<Vec<u8>, &'static str> {
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
                return Err("Name part length too long");
            }
            let ln = part.len() as u8;
            buf.push(ln);
            buf.extend(part.as_bytes());
        }
        buf.push(0);

        if addr.len() == 4 {
            buf.write_u16::<BigEndian>(dns_parser::Type::A as u16)
                .unwrap();
        } else {
            buf.write_u16::<BigEndian>(dns_parser::Type::AAAA as u16)
                .unwrap();
        }
        // set cache flush bit
        buf.write_u16::<BigEndian>(dns_parser::Class::IN as u16 | (0x1 << 15))
            .unwrap();
        buf.write_u32::<BigEndian>(120).unwrap();
        buf.write_u16::<BigEndian>(addr.len() as u16).unwrap();
        buf.extend(*addr);
    }

    Ok(buf)
}

enum ServiceControl {
    Register { hostname: String, address: String },
    Unregister { hostname: String },
    Stop,
}

pub struct MDNSService {
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

    fn get_ipv4_interface(&self) -> Option<std::net::Ipv4Addr> {
        if let Ok(interfaces) = get_if_addrs::get_if_addrs() {
            for interface in interfaces {
                if let get_if_addrs::IfAddr::V4(addr) = &interface.addr {
                    if !interface.is_loopback() {
                        info!("Found interface {} -> {:?}", interface.name, interface.addr);
                        return Some(addr.ip);
                    }
                }
            }
        }
        None
    }

    fn start(&mut self) -> io::Result<()> {
        let addr = self.get_ipv4_interface().unwrap();

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
        builder.spawn(move || {
            let mdns_addr = std::net::SocketAddr::from(([224, 0, 0, 251], port));
            let mut buffer: [u8; 1024] = [0; 1024];
            let mut hosts = HashMap::new();
            loop {
                match receiver.recv_timeout(time::Duration::from_millis(10)) {
                    Ok(msg) => match msg {
                        ServiceControl::Register { hostname, address } => {
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
                                    parsed
                                        .questions
                                        .iter()
                                        .filter(|question| {
                                            question.qtype == dns_parser::QueryType::A
                                        })
                                        .for_each(|question| {
                                            let qname = question.qname.to_string();
                                            trace!("mDNS question: {} {:?}", qname, question.qtype);
                                            if let Some(octets) = hosts.get(&qname) {
                                                trace!(
                                                    "Sending mDNS answer for {}: {:?}",
                                                    qname,
                                                    octets
                                                );
                                                answers.push((qname, &octets));
                                            }
                                        });
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
        })?;

        Ok(())
    }

    fn stop(self) {
        if let Some(sender) = self.sender {
            if let Err(err) = sender.send(ServiceControl::Stop) {
                warn!("Could not stop mDNS Service: {}", err);
            }
        }
    }

    fn new() -> MDNSService {
        MDNSService { sender: None }
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_register_hostname(
    serv: *mut MDNSService,
    hostname: *const c_char,
    address: *const c_char,
) {
    unsafe {
        let hostname = CStr::from_ptr(hostname).to_string_lossy();
        let address = CStr::from_ptr(address).to_string_lossy();
        (*serv).register_hostname(&hostname, &address);
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_start() -> *mut MDNSService {
    let mut r = Box::new(MDNSService::new());
    if let Err(err) = r.start() {
        warn!("Could not start mDNS Service: {}", err);
    }

    Box::into_raw(r)
}

#[no_mangle]
pub extern "C" fn mdns_service_stop(serv: *mut MDNSService) {
    unsafe {
        let boxed = Box::from_raw(serv);
        boxed.stop();
    }
}

#[no_mangle]
pub extern "C" fn mdns_service_unregister_hostname(
    serv: *mut MDNSService,
    hostname: *const c_char,
) {
    unsafe {
        let hostname = CStr::from_ptr(hostname).to_string_lossy();
        (*serv).unregister_hostname(&hostname);
    }
}
