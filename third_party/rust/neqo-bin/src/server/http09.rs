// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{cell::RefCell, collections::HashMap, fmt::Display, rc::Rc, time::Instant};

use neqo_common::{event::Provider, hex, qdebug, qerror, qinfo, qwarn, Datagram};
use neqo_crypto::{generate_ech_keys, random, AllowZeroRtt, AntiReplay};
use neqo_http3::Error;
use neqo_transport::{
    server::{ActiveConnectionRef, Server, ValidateAddress},
    ConnectionEvent, ConnectionIdGenerator, Output, State, StreamId,
};
use regex::Regex;

use super::{qns_read_response, Args};

#[derive(Default)]
struct HttpStreamState {
    writable: bool,
    data_to_send: Option<(Vec<u8>, usize)>,
}

pub struct HttpServer {
    server: Server,
    write_state: HashMap<StreamId, HttpStreamState>,
    read_state: HashMap<StreamId, Vec<u8>>,
    is_qns_test: bool,
    regex: Regex,
}

impl HttpServer {
    pub fn new(
        args: &Args,
        anti_replay: AntiReplay,
        cid_manager: Rc<RefCell<dyn ConnectionIdGenerator>>,
    ) -> Result<Self, Error> {
        let mut server = Server::new(
            args.now(),
            &[args.key.clone()],
            &[args.shared.alpn.clone()],
            anti_replay,
            Box::new(AllowZeroRtt {}),
            cid_manager,
            args.shared.quic_parameters.get(&args.shared.alpn),
        )?;

        server.set_ciphers(&args.get_ciphers());
        server.set_qlog_dir(args.shared.qlog_dir.clone());
        if args.retry {
            server.set_validation(ValidateAddress::Always);
        }
        if args.ech {
            let (sk, pk) = generate_ech_keys().expect("generate ECH keys");
            server
                .enable_ech(random::<1>()[0], "public.example", &sk, &pk)
                .expect("enable ECH");
            let cfg = server.ech_config();
            qinfo!("ECHConfigList: {}", hex(cfg));
        }

        let is_qns_test = args.shared.qns_test.is_some();
        Ok(Self {
            server,
            write_state: HashMap::new(),
            read_state: HashMap::new(),
            is_qns_test,
            regex: if is_qns_test {
                Regex::new(r"GET +/(\S+)(?:\r)?\n").unwrap()
            } else {
                Regex::new(r"GET +/(\d+)(?:\r)?\n").unwrap()
            },
        })
    }

    fn save_partial(
        &mut self,
        stream_id: StreamId,
        partial: Vec<u8>,
        conn: &mut ActiveConnectionRef,
    ) {
        let url_dbg = String::from_utf8(partial.clone())
            .unwrap_or_else(|_| format!("<invalid UTF-8: {}>", hex(&partial)));
        if partial.len() < 4096 {
            qdebug!("Saving partial URL: {}", url_dbg);
            self.read_state.insert(stream_id, partial);
        } else {
            qdebug!("Giving up on partial URL {}", url_dbg);
            conn.borrow_mut().stream_stop_sending(stream_id, 0).unwrap();
        }
    }

    fn write(
        &mut self,
        stream_id: StreamId,
        data: Option<Vec<u8>>,
        conn: &mut ActiveConnectionRef,
    ) {
        let resp = data.unwrap_or_else(|| Vec::from(&b"404 That request was nonsense\r\n"[..]));
        if let Some(stream_state) = self.write_state.get_mut(&stream_id) {
            match stream_state.data_to_send {
                None => stream_state.data_to_send = Some((resp, 0)),
                Some(_) => {
                    qdebug!("Data already set, doing nothing");
                }
            }
            if stream_state.writable {
                self.stream_writable(stream_id, conn);
            }
        } else {
            self.write_state.insert(
                stream_id,
                HttpStreamState {
                    writable: false,
                    data_to_send: Some((resp, 0)),
                },
            );
        }
    }

    fn stream_readable(&mut self, stream_id: StreamId, conn: &mut ActiveConnectionRef) {
        if !stream_id.is_client_initiated() || !stream_id.is_bidi() {
            qdebug!("Stream {} not client-initiated bidi, ignoring", stream_id);
            return;
        }
        let mut data = vec![0; 4000];
        let (sz, fin) = conn
            .borrow_mut()
            .stream_recv(stream_id, &mut data)
            .expect("Read should succeed");

        if sz == 0 {
            if !fin {
                qdebug!("size 0 but !fin");
            }
            return;
        }

        data.truncate(sz);
        let buf = if let Some(mut existing) = self.read_state.remove(&stream_id) {
            existing.append(&mut data);
            existing
        } else {
            data
        };

        let Ok(msg) = std::str::from_utf8(&buf[..]) else {
            self.save_partial(stream_id, buf, conn);
            return;
        };

        let m = self.regex.captures(msg);
        let Some(path) = m.and_then(|m| m.get(1)) else {
            self.save_partial(stream_id, buf, conn);
            return;
        };

        let resp = {
            let path = path.as_str();
            qdebug!("Path = '{path}'");
            if self.is_qns_test {
                match qns_read_response(path) {
                    Ok(data) => Some(data),
                    Err(e) => {
                        qerror!("Failed to read {path}: {e}");
                        Some(b"404".to_vec())
                    }
                }
            } else {
                let count = path.parse().unwrap();
                Some(vec![b'a'; count])
            }
        };
        self.write(stream_id, resp, conn);
    }

    fn stream_writable(&mut self, stream_id: StreamId, conn: &mut ActiveConnectionRef) {
        match self.write_state.get_mut(&stream_id) {
            None => {
                qwarn!("Unknown stream {stream_id}, ignoring event");
            }
            Some(stream_state) => {
                stream_state.writable = true;
                if let Some((data, ref mut offset)) = &mut stream_state.data_to_send {
                    let sent = conn
                        .borrow_mut()
                        .stream_send(stream_id, &data[*offset..])
                        .unwrap();
                    qdebug!("Wrote {}", sent);
                    *offset += sent;
                    self.server.add_to_waiting(conn);
                    if *offset == data.len() {
                        qinfo!("Sent {sent} on {stream_id}, closing");
                        conn.borrow_mut().stream_close_send(stream_id).unwrap();
                        self.write_state.remove(&stream_id);
                    } else {
                        stream_state.writable = false;
                    }
                }
            }
        }
    }
}

impl super::HttpServer for HttpServer {
    fn process(&mut self, dgram: Option<&Datagram>, now: Instant) -> Output {
        self.server.process(dgram, now)
    }

    fn process_events(&mut self, now: Instant) {
        let active_conns = self.server.active_connections();
        for mut acr in active_conns {
            loop {
                let event = match acr.borrow_mut().next_event() {
                    None => break,
                    Some(e) => e,
                };
                match event {
                    ConnectionEvent::NewStream { stream_id } => {
                        self.write_state
                            .insert(stream_id, HttpStreamState::default());
                    }
                    ConnectionEvent::RecvStreamReadable { stream_id } => {
                        self.stream_readable(stream_id, &mut acr);
                    }
                    ConnectionEvent::SendStreamWritable { stream_id } => {
                        self.stream_writable(stream_id, &mut acr);
                    }
                    ConnectionEvent::StateChange(State::Connected) => {
                        acr.connection()
                            .borrow_mut()
                            .send_ticket(now, b"hi!")
                            .unwrap();
                    }
                    ConnectionEvent::StateChange(_)
                    | ConnectionEvent::SendStreamCreatable { .. }
                    | ConnectionEvent::SendStreamComplete { .. } => (),
                    e => qwarn!("unhandled event {e:?}"),
                }
            }
        }
    }

    fn has_events(&self) -> bool {
        self.server.has_active_connections()
    }
}

impl Display for HttpServer {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Http 0.9 server ")
    }
}
