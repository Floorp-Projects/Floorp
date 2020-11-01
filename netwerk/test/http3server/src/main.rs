// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(warnings)]

use neqo_common::{event::Provider, qdebug, qinfo, qtrace, Datagram};
use neqo_crypto::{init_db, AllowZeroRtt, AntiReplay};
use neqo_http3::{Error, Http3Server, Http3ServerEvent};
use neqo_qpack::QpackSettings;
use neqo_transport::server::Server;
use neqo_transport::{ConnectionEvent, FixedConnectionIdManager, Output};
use std::env;

use std::cell::RefCell;
use std::io;
use std::path::PathBuf;
use std::process::exit;
use std::rc::Rc;
use std::thread;
use std::time::{Duration, Instant};

use core::fmt::Display;
use mio::net::UdpSocket;
use mio::{Events, Poll, PollOpt, Ready, Token};
use mio_extras::timer::{Builder, Timeout, Timer};
use std::collections::HashMap;
use std::collections::HashSet;
use std::mem;
use std::net::SocketAddr;

const MAX_TABLE_SIZE: u64 = 65536;
const MAX_BLOCKED_STREAMS: u16 = 10;
const PROTOCOLS: &[&str] = &["h3-27"];
const TIMER_TOKEN: Token = Token(0xffff_ffff);

const HTTP_RESPONSE_WITH_WRONG_FRAME: &[u8] = &[
    0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x37, // headers
    0x0, 0x3, 0x61, 0x62, 0x63, // the first data frame
    0x3, 0x1, 0x5, // a cancel push frame that is not allowed
];

trait HttpServer: Display {
    fn process(&mut self, dgram: Option<Datagram>) -> Output;
    fn process_events(&mut self);
}

impl HttpServer for Http3Server {
    fn process(&mut self, dgram: Option<Datagram>) -> Output {
        self.process(dgram, Instant::now())
    }

    fn process_events(&mut self) {
        while let Some(event) = self.next_event() {
            qtrace!("Event: {:?}", event);
            match event {
                Http3ServerEvent::Headers {
                    mut request,
                    headers,
                    fin,
                } => {
                    qtrace!("Headers (request={} fin={}): {:?}", request, fin, headers);

                    let default_ret = b"Hello World".to_vec();
                    let default_headers = vec![
                        (String::from(":status"), String::from("200")),
                        (String::from("Cache-Control"), String::from("no-cache")),
                        (
                            String::from("content-length"),
                            default_ret.len().to_string(),
                        ),
                    ];

                    let path_hdr = headers
                        .iter()
                        .find(|(k, _)| k == ":path");
                    match path_hdr {
                        Some((_, path)) if !path.is_empty() => {
                            qtrace!("Serve request {}", path);
                            if path == "/Response421" {
                                let response_body = b"0123456789".to_vec();
                                request
                                    .set_response(
                                        &[
                                            (String::from(":status"), String::from("421")),
                                            (
                                                String::from("Cache-Control"),
                                                String::from("no-cache"),
                                            ),
                                            (
                                                String::from("content-length"),
                                                response_body.len().to_string(),
                                            ),
                                        ],
                                        &response_body,
                                    )
                                    .unwrap();
                            } else if path == "/RequestCancelled" {
                                request
                                    .stream_reset(Error::HttpRequestCancelled.code())
                                    .unwrap();
                            } else if path == "/VersionFallback" {
                                request
                                    .stream_reset(Error::HttpVersionFallback.code())
                                    .unwrap();
                            } else if path == "/EarlyResponse" {
                                request.stream_reset(Error::HttpNoError.code()).unwrap();
                            } else if path == "/RequestRejected" {
                                request
                                    .stream_reset(Error::HttpRequestRejected.code())
                                    .unwrap();
                            } else if path == "/.well-known/http-opportunistic" {
                                let host_hdr = headers
                                    .iter()
                                    .find(|(k, _)| k == ":authority");
                                match host_hdr {
                                    Some((_, host)) if !host.is_empty() => {
                                        let mut content = b"[\"http://".to_vec();
                                        content.extend(host.as_bytes());
                                        content.extend(b"\"]".to_vec());
                                        request
                                            .set_response(
                                                &[
                                                    (String::from(":status"), String::from("200")),
                                                    (
                                                        String::from("Cache-Control"),
                                                        String::from("no-cache"),
                                                    ),
                                                    (
                                                        String::from("Content-Type"),
                                                        String::from("application/json"),
                                                    ),
                                                    (
                                                        String::from("content-length"),
                                                        content.len().to_string(),
                                                    ),
                                                ],
                                                &content,
                                            )
                                            .unwrap();
                                    }
                                    _ => request
                                        .set_response(&default_headers, &default_ret)
                                        .unwrap(),
                                }
                            } else if path == "/no_body" {
                                request
                                    .set_response(
                                        &[
                                            (String::from(":status"), String::from("200")),
                                            (
                                                String::from("Cache-Control"),
                                                String::from("no-cache"),
                                            ),
                                        ],
                                        &[],
                                    )
                                    .unwrap();
                            } else if path == "/no_content_length" {
                                request
                                    .set_response(
                                        &[
                                            (String::from(":status"), String::from("200")),
                                            (
                                                String::from("Cache-Control"),
                                                String::from("no-cache"),
                                            ),
                                        ],
                                        &vec![b'a'; 4000],
                                    )
                                    .unwrap();
                            } else if path == "/content_length_smaller" {
                                request
                                    .set_response(
                                        &[
                                            (String::from(":status"), String::from("200")),
                                            (
                                                String::from("Cache-Control"),
                                                String::from("no-cache"),
                                            ),
                                            (String::from("content-length"), 4000.to_string()),
                                        ],
                                        &vec![b'a'; 8000],
                                    )
                                    .unwrap();
                            } else {
                                match path.trim_matches(|p| p == '/').parse::<usize>() {
                                    Ok(v) => request
                                        .set_response(
                                            &[
                                                (String::from(":status"), String::from("200")),
                                                (
                                                    String::from("Cache-Control"),
                                                    String::from("no-cache"),
                                                ),
                                                (String::from("content-length"), v.to_string()),
                                            ],
                                            &vec![b'a'; v],
                                        )
                                        .unwrap(),
                                    Err(_) => request
                                        .set_response(&default_headers, &default_ret)
                                        .unwrap(),
                                }
                            }
                        }
                        _ => {
                            request
                                .set_response(&default_headers, &default_ret)
                                .unwrap();
                        }
                    }
                }
                _ => {}
            }
        }
    }
}

impl HttpServer for Server {
    fn process(&mut self, dgram: Option<Datagram>) -> Output {
        self.process(dgram, Instant::now())
    }

    fn process_events(&mut self) {
        let active_conns = self.active_connections();
        for mut acr in active_conns {
            loop {
                let event = match acr.borrow_mut().next_event() {
                    None => break,
                    Some(e) => e,
                };
                match event {
                    ConnectionEvent::RecvStreamReadable { stream_id } => {
                        if stream_id % 4 == 0 {
                            // We are only interesting in request streams
                            acr.borrow_mut()
                                .stream_send(stream_id, HTTP_RESPONSE_WITH_WRONG_FRAME)
                                .expect("Read should succeed");
                        }
                    }
                    _ => {}
                }
            }
        }
    }
}

fn emit_packet(socket: &UdpSocket, out_dgram: Datagram) {
    let sent = socket
        .send_to(&out_dgram, &out_dgram.destination())
        .expect("Error sending datagram");
    if sent != out_dgram.len() {
        qinfo!("Unable to send all {} bytes of datagram", out_dgram.len());
    }
}

fn process(
    server: &mut dyn HttpServer,
    svr_timeout: &mut Option<Timeout>,
    inx: usize,
    dgram: Option<Datagram>,
    timer: &mut Timer<usize>,
    socket: &mut UdpSocket,
) -> bool {
    match server.process(dgram) {
        Output::Datagram(dgram) => {
            emit_packet(socket, dgram);
            true
        }
        Output::Callback(new_timeout) => {
            if let Some(svr_timeout) = svr_timeout {
                timer.cancel_timeout(svr_timeout);
            }

            qinfo!("Setting timeout of {:?} for {}", new_timeout, server);
            *svr_timeout = Some(timer.set_timeout(new_timeout, inx));
            false
        }
        Output::None => {
            qdebug!("Output::None");
            false
        }
    }
}

fn read_dgram(
    socket: &mut UdpSocket,
    local_address: &SocketAddr,
) -> Result<Option<Datagram>, io::Error> {
    let buf = &mut [0u8; 2048];
    let (sz, remote_addr) = match socket.recv_from(&mut buf[..]) {
        Err(ref err) if err.kind() == io::ErrorKind::WouldBlock => return Ok(None),
        Err(err) => {
            eprintln!("UDP recv error: {:?}", err);
            return Err(err);
        }
        Ok(res) => res,
    };

    if sz == buf.len() {
        eprintln!("Might have received more than {} bytes", buf.len());
    }

    if sz == 0 {
        eprintln!("zero length datagram received?");
        Ok(None)
    } else {
        Ok(Some(Datagram::new(remote_addr, *local_address, &buf[..sz])))
    }
}

struct ServersRunner {
    hosts: Vec<SocketAddr>,
    poll: Poll,
    sockets: Vec<UdpSocket>,
    servers: HashMap<SocketAddr, (Box<dyn HttpServer>, Option<Timeout>)>,
    timer: Timer<usize>,
    active_servers: HashSet<usize>,
}

impl ServersRunner {
    pub fn new() -> Result<Self, io::Error> {
        Ok(Self {
            hosts: Vec::new(),
            poll: Poll::new()?,
            sockets: Vec::new(),
            servers: HashMap::new(),
            timer: Builder::default()
                .tick_duration(Duration::from_millis(1))
                .build::<usize>(),
            active_servers: HashSet::new(),
        })
    }

    pub fn init(&mut self) {
        self.add_new_socket(0, true);
        self.add_new_socket(1, false);
        println!(
            "HTTP3 server listening on ports {} and {}",
            self.hosts[0].port(),
            self.hosts[1].port()
        );
        self.poll
            .register(&self.timer, TIMER_TOKEN, Ready::readable(), PollOpt::edge())
            .unwrap();
    }

    fn add_new_socket(&mut self, count: usize, http3: bool) -> u16 {
        let addr = "127.0.0.1:0".parse().unwrap();

        let socket = match UdpSocket::bind(&addr) {
            Err(err) => {
                eprintln!("Unable to bind UDP socket: {}", err);
                exit(1)
            }
            Ok(s) => s,
        };

        let local_addr = match socket.local_addr() {
            Err(err) => {
                eprintln!("Socket local address not bound: {}", err);
                exit(1)
            }
            Ok(s) => s,
        };

        self.hosts.push(local_addr);

        self.poll
            .register(
                &socket,
                Token(count),
                Ready::readable() | Ready::writable(),
                PollOpt::edge(),
            )
            .unwrap();

        self.sockets.push(socket);
        self.servers
            .insert(local_addr, (self.create_server(http3), None));
        local_addr.port()
    }

    fn create_server(&self, http3: bool) -> Box<dyn HttpServer> {
        let anti_replay = AntiReplay::new(Instant::now(), Duration::from_secs(10), 7, 14)
            .expect("unable to setup anti-replay");
        let cid_mgr = Rc::new(RefCell::new(FixedConnectionIdManager::new(10)));

        if http3 {
            Box::new(
                Http3Server::new(
                    Instant::now(),
                    &[" HTTP2 Test Cert"],
                    PROTOCOLS,
                    anti_replay,
                    cid_mgr,
                    QpackSettings {
                        max_table_size_encoder: MAX_TABLE_SIZE,
                        max_table_size_decoder: MAX_TABLE_SIZE,
                        max_blocked_streams: MAX_BLOCKED_STREAMS,
                    },
                )
                .expect("We cannot make a server!"),
            )
        } else {
            Box::new(
                Server::new(
                    Instant::now(),
                    &[" HTTP2 Test Cert"],
                    PROTOCOLS,
                    anti_replay,
                    Box::new(AllowZeroRtt {}),
                    cid_mgr,
                )
                .expect("We cannot make a server!"),
            )
        }
    }

    fn process_datagrams_and_events(
        &mut self,
        inx: usize,
        read_socket: bool,
    ) -> Result<(), io::Error> {
        if let Some(socket) = self.sockets.get_mut(inx) {
            if let Some((ref mut server, svr_timeout)) =
                self.servers.get_mut(&socket.local_addr().unwrap())
            {
                if read_socket {
                    loop {
                        let dgram = read_dgram(socket, &self.hosts[inx])?;
                        if dgram.is_none() {
                            break;
                        }
                        let _ = process(
                            &mut **server,
                            svr_timeout,
                            inx,
                            dgram,
                            &mut self.timer,
                            socket,
                        );
                    }
                } else {
                    let _ = process(
                        &mut **server,
                        svr_timeout,
                        inx,
                        None,
                        &mut self.timer,
                        socket,
                    );
                }
                server.process_events();
                if process(
                    &mut **server,
                    svr_timeout,
                    inx,
                    None,
                    &mut self.timer,
                    socket,
                ) {
                    self.active_servers.insert(inx);
                }
            }
        }
        Ok(())
    }

    fn process_active_conns(&mut self) -> Result<(), io::Error> {
        let curr_active = mem::take(&mut self.active_servers);
        for inx in curr_active {
            self.process_datagrams_and_events(inx, false)?;
        }
        Ok(())
    }

    fn process_timeout(&mut self) -> Result<(), io::Error> {
        while let Some(inx) = self.timer.poll() {
            qinfo!("Timer expired for {:?}", inx);
            self.process_datagrams_and_events(inx, false)?;
        }
        Ok(())
    }

    pub fn run(&mut self) -> Result<(), io::Error> {
        let mut events = Events::with_capacity(1024);
        loop {
            // If there are active servers do not block in poll.
            self.poll.poll(
                &mut events,
                if self.active_servers.is_empty() {
                    None
                } else {
                    Some(Duration::from_millis(0))
                },
            )?;

            for event in &events {
                if event.token() == TIMER_TOKEN {
                    self.process_timeout()?;
                } else {
                    if !event.readiness().is_readable() {
                        continue;
                    }
                    self.process_datagrams_and_events(event.token().0, true)?;
                }
            }
            self.process_active_conns()?;
        }
    }
}

fn main() -> Result<(), io::Error> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Wrong arguments.");
        exit(1)
    }

    // Read data from stdin and terminate the server if EOF is detected, which
    // means that runxpcshelltests.py ended without shutting down the server.
    thread::spawn(|| loop {
        let mut buffer = String::new();
        match io::stdin().read_line(&mut buffer) {
            Ok(n) => {
                if n == 0 {
                    exit(0);
                }
            }
            Err(_) => {
                exit(0);
            }
        }
    });

    init_db(PathBuf::from(args[1].clone()));

    let mut servers_runner = ServersRunner::new()?;
    servers_runner.init();
    servers_runner.run()
}
