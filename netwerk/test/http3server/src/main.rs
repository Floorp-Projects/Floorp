// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(warnings)]

use neqo_common::{event::Provider, qdebug, qinfo, qtrace, Datagram, Header};
use neqo_crypto::{generate_ech_keys, init_db, AllowZeroRtt, AntiReplay};
use neqo_http3::{
    Error, Http3OrWebTransportStream, Http3Parameters, Http3Server, Http3ServerEvent,
};
use neqo_transport::server::Server;
use neqo_transport::{ConnectionEvent, ConnectionParameters, Output, RandomConnectionIdGenerator};
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
use std::collections::hash_map::DefaultHasher;
use std::collections::HashMap;
use std::collections::HashSet;
use std::hash::{Hash, Hasher};
use std::mem;
use std::net::SocketAddr;

extern crate base64;

const MAX_TABLE_SIZE: u64 = 65536;
const MAX_BLOCKED_STREAMS: u16 = 10;
const PROTOCOLS: &[&str] = &["h3-29", "h3"];
const TIMER_TOKEN: Token = Token(0xffff);
const ECH_CONFIG_ID: u8 = 7;
const ECH_PUBLIC_NAME: &str = "public.example";

const HTTP_RESPONSE_WITH_WRONG_FRAME: &[u8] = &[
    0x01, 0x06, 0x00, 0x00, 0xd9, 0x54, 0x01, 0x37, // headers
    0x0, 0x3, 0x61, 0x62, 0x63, // the first data frame
    0x3, 0x1, 0x5, // a cancel push frame that is not allowed
];

trait HttpServer: Display {
    fn process(&mut self, dgram: Option<Datagram>) -> Output;
    fn process_events(&mut self);
}

struct Http3TestServer {
    server: Http3Server,
    // This a map from a post request to amount of data ithas been received on the request.
    // The respons will carry the amount of data received.
    posts: HashMap<Http3OrWebTransportStream, usize>,
    responses: HashMap<Http3OrWebTransportStream, Vec<u8>>,
    current_connection_hash: u64,
}

impl ::std::fmt::Display for Http3TestServer {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{}", self.server)
    }
}

impl Http3TestServer {
    pub fn new(server: Http3Server) -> Self {
        Self {
            server,
            posts: HashMap::new(),
            responses: HashMap::new(),
            current_connection_hash: 0,
        }
    }

    fn new_response(&mut self, mut stream: Http3OrWebTransportStream, mut data: Vec<u8>) {
        if data.len() == 0 {
            stream.stream_close_send().unwrap();
            return;
        }
        let sent = stream.send_data(&data).unwrap();
        if sent < data.len() {
            self.responses.insert(stream, data.split_off(sent));
        } else {
            stream.stream_close_send().unwrap();
        }
    }

    fn handle_stream_writable(&mut self, mut stream: Http3OrWebTransportStream) {
        if let Some(data) = self.responses.get_mut(&stream) {
            match stream.send_data(&data) {
                Ok(sent) => {
                    if sent < data.len() {
                        let new_d = (*data).split_off(sent);
                        *data = new_d;
                    } else {
                        stream.stream_close_send().unwrap();
                        self.responses.remove(&stream);
                    }
                }
                Err(_) => {
                    eprintln!("Unexpected error");
                }
            }
        }
    }
}

impl HttpServer for Http3TestServer {
    fn process(&mut self, dgram: Option<Datagram>) -> Output {
        self.server.process(dgram, Instant::now())
    }

    fn process_events(&mut self) {
        while let Some(event) = self.server.next_event() {
            qtrace!("Event: {:?}", event);
            match event {
                Http3ServerEvent::Headers {
                    mut stream,
                    headers,
                    fin,
                } => {
                    qtrace!("Headers (request={} fin={}): {:?}", stream, fin, headers);

                    // Some responses do not have content-type. This is on purpose to exercise
                    // UnknownDecoder code.
                    let default_ret = b"Hello World".to_vec();
                    let default_headers = vec![
                        Header::new(":status", "200"),
                        Header::new("cache-control", "no-cache"),
                        Header::new("content-length", default_ret.len().to_string()),
                        Header::new(
                            "x-http3-conn-hash",
                            self.current_connection_hash.to_string(),
                        ),
                    ];

                    let path_hdr = headers.iter().find(|&h| h.name() == ":path");
                    match path_hdr {
                        Some(ph) if !ph.value().is_empty() => {
                            let path = ph.value();
                            qtrace!("Serve request {}", path);
                            if path == "/Response421" {
                                let response_body = b"0123456789".to_vec();
                                stream
                                    .send_headers(&[
                                        Header::new(":status", "421"),
                                        Header::new("cache-control", "no-cache"),
                                        Header::new("content-type", "text/plain"),
                                        Header::new(
                                            "content-length",
                                            response_body.len().to_string(),
                                        ),
                                    ])
                                    .unwrap();
                                self.new_response(stream, response_body);
                            } else if path == "/RequestCancelled" {
                                stream
                                    .stream_stop_sending(Error::HttpRequestCancelled.code())
                                    .unwrap();
                                stream
                                    .stream_reset_send(Error::HttpRequestCancelled.code())
                                    .unwrap();
                            } else if path == "/VersionFallback" {
                                stream
                                    .stream_stop_sending(Error::HttpVersionFallback.code())
                                    .unwrap();
                                stream
                                    .stream_reset_send(Error::HttpVersionFallback.code())
                                    .unwrap();
                            } else if path == "/EarlyResponse" {
                                stream
                                    .stream_stop_sending(Error::HttpNoError.code())
                                    .unwrap();
                            } else if path == "/RequestRejected" {
                                stream
                                    .stream_stop_sending(Error::HttpRequestRejected.code())
                                    .unwrap();
                                stream
                                    .stream_reset_send(Error::HttpRequestRejected.code())
                                    .unwrap();
                            } else if path == "/.well-known/http-opportunistic" {
                                let host_hdr = headers.iter().find(|&h| h.name() == ":authority");
                                match host_hdr {
                                    Some(host) if !host.value().is_empty() => {
                                        let mut content = b"[\"http://".to_vec();
                                        content.extend(host.value().as_bytes());
                                        content.extend(b"\"]".to_vec());
                                        stream
                                            .send_headers(&[
                                                Header::new(":status", "200"),
                                                Header::new("cache-control", "no-cache"),
                                                Header::new("content-type", "application/json"),
                                                Header::new(
                                                    "content-length",
                                                    content.len().to_string(),
                                                ),
                                            ])
                                            .unwrap();
                                        self.new_response(stream, content);
                                    }
                                    _ => {
                                        stream.send_headers(&default_headers).unwrap();
                                        self.new_response(stream, default_ret);
                                    }
                                }
                            } else if path == "/no_body" {
                                stream
                                    .send_headers(&[
                                        Header::new(":status", "200"),
                                        Header::new("cache-control", "no-cache"),
                                    ])
                                    .unwrap();
                                stream.stream_close_send().unwrap();
                            } else if path == "/no_content_length" {
                                stream
                                    .send_headers(&[
                                        Header::new(":status", "200"),
                                        Header::new("cache-control", "no-cache"),
                                    ])
                                    .unwrap();
                                self.new_response(stream, vec![b'a'; 4000]);
                            } else if path == "/content_length_smaller" {
                                stream
                                    .send_headers(&[
                                        Header::new(":status", "200"),
                                        Header::new("cache-control", "no-cache"),
                                        Header::new("content-type", "text/plain"),
                                        Header::new("content-length", 4000.to_string()),
                                    ])
                                    .unwrap();
                                self.new_response(stream, vec![b'a'; 8000]);
                            } else if path == "/post" {
                                // Read all data before responding.
                                self.posts.insert(stream, 0);
                            } else if path == "/priority_mirror" {
                                if let Some(priority) =
                                    headers.iter().find(|h| h.name() == "priority")
                                {
                                    stream
                                        .send_headers(&[
                                            Header::new(":status", "200"),
                                            Header::new("cache-control", "no-cache"),
                                            Header::new("content-type", "text/plain"),
                                            Header::new("priority-mirror", priority.value()),
                                            Header::new(
                                                "content-length",
                                                priority.value().len().to_string(),
                                            ),
                                        ])
                                        .unwrap();
                                    self.new_response(stream, priority.value().as_bytes().to_vec());
                                } else {
                                    stream
                                        .send_headers(&[
                                            Header::new(":status", "200"),
                                            Header::new("cache-control", "no-cache"),
                                        ])
                                        .unwrap();
                                    stream.stream_close_send().unwrap();
                                }
                            } else if path == "/103_response" {
                                if let Some(early_hint) =
                                    headers.iter().find(|h| h.name() == "link-to-set")
                                {
                                    for l in early_hint.value().split(',') {
                                        stream
                                            .send_headers(&[
                                                Header::new(":status", "103"),
                                                Header::new("link", l),
                                            ])
                                            .unwrap();
                                    }
                                }
                                stream
                                    .send_headers(&[
                                        Header::new(":status", "200"),
                                        Header::new("cache-control", "no-cache"),
                                        Header::new("content-length", "0"),
                                    ])
                                    .unwrap();
                                stream.stream_close_send().unwrap();
                            } else {
                                match path.trim_matches(|p| p == '/').parse::<usize>() {
                                    Ok(v) => {
                                        stream
                                            .send_headers(&[
                                                Header::new(":status", "200"),
                                                Header::new("cache-control", "no-cache"),
                                                Header::new("content-type", "text/plain"),
                                                Header::new("content-length", v.to_string()),
                                            ])
                                            .unwrap();
                                        self.new_response(stream, vec![b'a'; v]);
                                    }
                                    Err(_) => {
                                        stream.send_headers(&default_headers).unwrap();
                                        self.new_response(stream, default_ret);
                                    }
                                }
                            }
                        }
                        _ => {
                            stream.send_headers(&default_headers).unwrap();
                            self.new_response(stream, default_ret);
                        }
                    }
                }
                Http3ServerEvent::Data {
                    mut stream,
                    data,
                    fin,
                } => {
                    if let Some(r) = self.posts.get_mut(&stream) {
                        *r += data.len();
                    }
                    if fin {
                        if let Some(r) = self.posts.remove(&stream) {
                            let default_ret = b"Hello World".to_vec();
                            stream
                                .send_headers(&[
                                    Header::new(":status", "200"),
                                    Header::new("cache-control", "no-cache"),
                                    Header::new("x-data-received-length", r.to_string()),
                                    Header::new("content-length", default_ret.len().to_string()),
                                ])
                                .unwrap();
                            self.new_response(stream, default_ret);
                        }
                    }
                }
                Http3ServerEvent::DataWritable { stream } => self.handle_stream_writable(stream),
                Http3ServerEvent::StateChange { conn, state } => {
                    if matches!(state, neqo_http3::Http3State::Connected) {
                        let mut h = DefaultHasher::new();
                        conn.hash(&mut h);
                        self.current_connection_hash = h.finish();
                    }
                }
                Http3ServerEvent::PriorityUpdate { .. }
                | Http3ServerEvent::StreamReset { .. }
                | Http3ServerEvent::StreamStopSending { .. }
                | Http3ServerEvent::WebTransport(_) => {}
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
                        if stream_id.is_bidi() && stream_id.is_client_initiated() {
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

#[derive(Default)]
struct NonRespondingServer {}

impl ::std::fmt::Display for NonRespondingServer {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "NonRespondingServer")
    }
}

impl HttpServer for NonRespondingServer {
    fn process(&mut self, _dgram: Option<Datagram>) -> Output {
        Output::None
    }

    fn process_events(&mut self) {}
}

fn emit_packet(socket: &UdpSocket, out_dgram: Datagram) {
    let res = match socket.send_to(&out_dgram, &out_dgram.destination()) {
        Err(ref err) if err.kind() == io::ErrorKind::WouldBlock => 0,
        Err(err) => {
            eprintln!("UDP send error: {:?}", err);
            exit(1);
        }
        Ok(res) => res,
    };
    if res != out_dgram.len() {
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

enum ServerType {
    Http3,
    Http3Fail,
    Http3NoResponse,
    Http3Ech,
}

struct ServersRunner {
    hosts: Vec<SocketAddr>,
    poll: Poll,
    sockets: Vec<UdpSocket>,
    servers: HashMap<SocketAddr, (Box<dyn HttpServer>, Option<Timeout>)>,
    timer: Timer<usize>,
    active_servers: HashSet<usize>,
    ech_config: Vec<u8>,
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
            ech_config: Vec::new(),
        })
    }

    pub fn init(&mut self) {
        self.add_new_socket(0, ServerType::Http3);
        self.add_new_socket(1, ServerType::Http3Fail);
        self.add_new_socket(2, ServerType::Http3Ech);
        self.add_new_socket(4, ServerType::Http3NoResponse);

        println!(
            "HTTP3 server listening on ports {}, {}, {} and {}. EchConfig is @{}@",
            self.hosts[0].port(),
            self.hosts[1].port(),
            self.hosts[2].port(),
            self.hosts[3].port(),
            base64::encode(&self.ech_config)
        );
        self.poll
            .register(&self.timer, TIMER_TOKEN, Ready::readable(), PollOpt::edge())
            .unwrap();
    }

    fn add_new_socket(&mut self, count: usize, server_type: ServerType) -> u16 {
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
        let server = self.create_server(server_type);
        self.servers.insert(local_addr, (server, None));
        local_addr.port()
    }

    fn create_server(&mut self, server_type: ServerType) -> Box<dyn HttpServer> {
        let anti_replay = AntiReplay::new(Instant::now(), Duration::from_secs(10), 7, 14)
            .expect("unable to setup anti-replay");
        let cid_mgr = Rc::new(RefCell::new(RandomConnectionIdGenerator::new(10)));

        match server_type {
            ServerType::Http3 => Box::new(Http3TestServer::new(
                Http3Server::new(
                    Instant::now(),
                    &[" HTTP2 Test Cert"],
                    PROTOCOLS,
                    anti_replay,
                    cid_mgr,
                    Http3Parameters::default()
                        .max_table_size_encoder(MAX_TABLE_SIZE)
                        .max_table_size_decoder(MAX_TABLE_SIZE)
                        .max_blocked_streams(MAX_BLOCKED_STREAMS),
                    None,
                )
                .expect("We cannot make a server!"),
            )),
            ServerType::Http3Fail => Box::new(
                Server::new(
                    Instant::now(),
                    &[" HTTP2 Test Cert"],
                    PROTOCOLS,
                    anti_replay,
                    Box::new(AllowZeroRtt {}),
                    cid_mgr,
                    ConnectionParameters::default(),
                )
                .expect("We cannot make a server!"),
            ),
            ServerType::Http3NoResponse => Box::new(NonRespondingServer::default()),
            ServerType::Http3Ech => {
                let mut server = Box::new(Http3TestServer::new(
                    Http3Server::new(
                        Instant::now(),
                        &[" HTTP2 Test Cert"],
                        PROTOCOLS,
                        anti_replay,
                        cid_mgr,
                        Http3Parameters::default()
                            .max_table_size_encoder(MAX_TABLE_SIZE)
                            .max_table_size_decoder(MAX_TABLE_SIZE)
                            .max_blocked_streams(MAX_BLOCKED_STREAMS),
                        None,
                    )
                    .expect("We cannot make a server!"),
                ));
                let ref mut unboxed_server = (*server).server;
                let (sk, pk) = generate_ech_keys().unwrap();
                unboxed_server
                    .enable_ech(ECH_CONFIG_ID, ECH_PUBLIC_NAME, &sk, &pk)
                    .expect("unable to enable ech");
                self.ech_config = Vec::from(unboxed_server.ech_config());
                server
            }
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
