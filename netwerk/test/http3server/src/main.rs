// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(warnings)]

use neqo_common::{qdebug, qerror, qinfo, qtrace, Datagram};
use neqo_crypto::{init_db, AntiReplay};
use neqo_http3::{Error, Http3Server, Http3ServerEvent};
use neqo_transport::{FixedConnectionIdManager, Output};
use std::env;

use std::cell::RefCell;
use std::io;
use std::path::PathBuf;
use std::process::exit;
use std::rc::Rc;
use std::thread;
use std::time::{Duration, Instant};

use mio::net::UdpSocket;
use mio::{Events, Poll, PollOpt, Ready, Token};
use mio_extras::timer::{Builder, Timeout, Timer};

const PROTOCOLS: &[&str] = &["h3-27"];
const TIMER_TOKEN: Token = Token(0xffff_ffff);

fn process_events(server: &mut Http3Server) {
    while let Some(event) = server.next_event() {
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

                let path_hdr = headers.iter().find(|(k, _)| k == ":path");
                match path_hdr {
                    Some((_, path)) if !path.is_empty() => {
                        qtrace!("Serve request {}", path);
                        if path == "/RequestCancelled" {
                            request
                                .stream_reset(Error::HttpRequestCancelled.code())
                                .unwrap();
                        } else if path == "/VersionFallback" {
                            request
                                .stream_reset(Error::HttpVersionFallback.code())
                                .unwrap();
                        } else if path == "/EarlyResponse" {
                            request
                                .stream_reset(Error::HttpEarlyResponse.code())
                                .unwrap();
                        } else if path == "/RequestRejected" {
                            request
                                .stream_reset(Error::HttpRequestRejected.code())
                                .unwrap();
                        } else if path == "/.well-known/http-opportunistic" {
                            let host_hdr = headers.iter().find(|(k, _)| k == ":authority");
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
                                            content,
                                        )
                                        .unwrap();
                                }
                                _ => request.set_response(&default_headers, default_ret).unwrap(),
                            }
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
                                        vec![b'a'; v],
                                    )
                                    .unwrap(),
                                Err(_) => {
                                    request.set_response(&default_headers, default_ret).unwrap()
                                }
                            }
                        }
                    }
                    _ => {
                        request.set_response(&default_headers, default_ret).unwrap();
                    }
                }
            }
            _ => {}
        }
    }
}

fn emit_packets(socket: &UdpSocket, out_dgrams: &[Datagram]) {
    for d in out_dgrams {
        let sent = socket
            .send_to(d, &d.destination())
            .expect("Error sending datagram");
        if sent != d.len() {
            qinfo!("Unable to send all {} bytes of datagram", d.len());
        }
    }
}

fn process(
    server: &mut Http3Server,
    svr_timeout: &mut Option<Timeout>,
    inx: usize,
    mut dgram: Option<Datagram>,
    out_dgrams: &mut Vec<Datagram>,
    timer: &mut Timer<usize>,
) {
    loop {
        match server.process(dgram, Instant::now()) {
            Output::Datagram(dgram) => out_dgrams.push(dgram),
            Output::Callback(new_timeout) => {
                if let Some(svr_timeout) = svr_timeout {
                    timer.cancel_timeout(svr_timeout);
                }

                qinfo!("Setting timeout of {:?} for {}", new_timeout, server);
                *svr_timeout = Some(timer.set_timeout(new_timeout, inx));
                break;
            }
            Output::None => {
                qdebug!("Output::None");
                break;
            }
        };
        dgram = None;
    }
}

fn main() -> Result<(), io::Error> {
    const MAX_TABLE_SIZE: u32 = 65536;
    const MAX_BLOCKED_STREAMS: u16 = 10;

    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Wrong arguments.");
        exit(1)
    }

    // Read data from stdin and terminate the server if EOF is detected, which means that
    // runxpcshelltests.py ended without shutting down the server.
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

    let poll = Poll::new()?;

    let addr = "127.0.0.1:0".parse().unwrap();

    let mut timer = Builder::default().build::<usize>();
    poll.register(&timer, TIMER_TOKEN, Ready::readable(), PollOpt::edge())?;

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

    println!("HTTP3 server listening on port {}", local_addr.port());
    poll.register(
        &socket,
        Token(0),
        Ready::readable() | Ready::writable(),
        PollOpt::edge(),
    )?;

    let mut svr_timeout = None;
    let mut server = Http3Server::new(
        Instant::now(),
        &[" HTTP2 Test Cert"],
        PROTOCOLS,
        AntiReplay::new(Instant::now(), Duration::from_secs(10), 7, 14)
            .expect("unable to setup anti-replay"),
        Rc::new(RefCell::new(FixedConnectionIdManager::new(5))),
        MAX_TABLE_SIZE,
        MAX_BLOCKED_STREAMS,
    )
    .expect("We cannot make a server!");

    let buf = &mut [0u8; 2048];

    let mut events = Events::with_capacity(1024);

    loop {
        poll.poll(&mut events, None)?;
        let mut out_dgrams = Vec::new();
        for event in &events {
            if event.token() == TIMER_TOKEN {
                while let Some(remote_addr) = timer.poll() {
                    qinfo!("Timer expired for {:?}", remote_addr);
                    process(
                        &mut server,
                        &mut svr_timeout,
                        remote_addr,
                        None,
                        &mut out_dgrams,
                        &mut timer,
                    );
                }
            } else {
                if !event.readiness().is_readable() {
                    continue;
                }

                // Read all datagrams and group by remote host
                loop {
                    let (sz, remote_addr) = match socket.recv_from(&mut buf[..]) {
                        Err(ref err) if err.kind() == io::ErrorKind::WouldBlock => break,
                        Err(err) => {
                            qerror!("UDP recv error: {:?}", err);
                            exit(1);
                        }
                        Ok(res) => res,
                    };

                    if sz == buf.len() {
                        qerror!("Might have received more than {} bytes", buf.len());
                    }

                    if sz == 0 {
                        qerror!("zero length datagram received?");
                    } else {
                        process(
                            &mut server,
                            &mut svr_timeout,
                            event.token().0,
                            Some(Datagram::new(remote_addr, local_addr, &buf[..sz])),
                            &mut out_dgrams,
                            &mut timer,
                        );
                        process_events(&mut server);
                        process(
                            &mut server,
                            &mut svr_timeout,
                            event.token().0,
                            None,
                            &mut out_dgrams,
                            &mut timer,
                        );
                    }
                }
            }
        }

        emit_packets(&socket, &out_dgrams);
    }
}
