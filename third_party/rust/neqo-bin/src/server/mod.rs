// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    fmt::{self, Display},
    fs, io,
    net::{SocketAddr, ToSocketAddrs},
    path::PathBuf,
    pin::Pin,
    process::exit,
    rc::Rc,
    time::{Duration, Instant},
};

use clap::Parser;
use futures::{
    future::{select, select_all, Either},
    FutureExt,
};
use neqo_common::{qdebug, qerror, qinfo, qwarn, Datagram};
use neqo_crypto::{
    constants::{TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384, TLS_CHACHA20_POLY1305_SHA256},
    init_db, AntiReplay, Cipher,
};
use neqo_transport::{Output, RandomConnectionIdGenerator, Version};
use tokio::time::Sleep;

use crate::{udp, SharedArgs};

const ANTI_REPLAY_WINDOW: Duration = Duration::from_secs(10);

mod http09;
mod http3;

#[derive(Debug)]
pub enum Error {
    ArgumentError(&'static str),
    Http3Error(neqo_http3::Error),
    IoError(io::Error),
    QlogError,
    TransportError(neqo_transport::Error),
    CryptoError(neqo_crypto::Error),
}

impl From<neqo_crypto::Error> for Error {
    fn from(err: neqo_crypto::Error) -> Self {
        Self::CryptoError(err)
    }
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Self {
        Self::IoError(err)
    }
}

impl From<neqo_http3::Error> for Error {
    fn from(err: neqo_http3::Error) -> Self {
        Self::Http3Error(err)
    }
}

impl From<qlog::Error> for Error {
    fn from(_err: qlog::Error) -> Self {
        Self::QlogError
    }
}

impl From<neqo_transport::Error> for Error {
    fn from(err: neqo_transport::Error) -> Self {
        Self::TransportError(err)
    }
}

impl Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Error: {self:?}")?;
        Ok(())
    }
}

impl std::error::Error for Error {}

type Res<T> = Result<T, Error>;

#[derive(Debug, Parser)]
#[command(author, version, about, long_about = None)]
pub struct Args {
    #[command(flatten)]
    shared: SharedArgs,

    /// List of IP:port to listen on
    #[arg(default_value = "[::]:4433")]
    hosts: Vec<String>,

    #[arg(short = 'd', long, default_value = "./test-fixture/db")]
    /// NSS database directory.
    db: PathBuf,

    #[arg(short = 'k', long, default_value = "key")]
    /// Name of key from NSS database.
    key: String,

    #[arg(name = "retry", long)]
    /// Force a retry
    retry: bool,

    #[arg(name = "ech", long)]
    /// Enable encrypted client hello (ECH).
    /// This generates a new set of ECH keys when it is invoked.
    /// The resulting configuration is printed to stdout in hexadecimal format.
    ech: bool,
}

#[cfg(feature = "bench")]
impl Default for Args {
    fn default() -> Self {
        use std::str::FromStr;
        Self {
            shared: crate::SharedArgs::default(),
            hosts: vec!["[::]:12345".to_string()],
            db: PathBuf::from_str("../test-fixture/db").unwrap(),
            key: "key".to_string(),
            retry: false,
            ech: false,
        }
    }
}

impl Args {
    fn get_ciphers(&self) -> Vec<Cipher> {
        self.shared
            .ciphers
            .iter()
            .filter_map(|c| match c.as_str() {
                "TLS_AES_128_GCM_SHA256" => Some(TLS_AES_128_GCM_SHA256),
                "TLS_AES_256_GCM_SHA384" => Some(TLS_AES_256_GCM_SHA384),
                "TLS_CHACHA20_POLY1305_SHA256" => Some(TLS_CHACHA20_POLY1305_SHA256),
                _ => None,
            })
            .collect::<Vec<_>>()
    }

    fn listen_addresses(&self) -> Vec<SocketAddr> {
        self.hosts
            .iter()
            .filter_map(|host| host.to_socket_addrs().ok())
            .flatten()
            .chain(self.shared.quic_parameters.preferred_address_v4())
            .chain(self.shared.quic_parameters.preferred_address_v6())
            .collect()
    }

    fn now(&self) -> Instant {
        if self.shared.qns_test.is_some() {
            // When NSS starts its anti-replay it blocks any acceptance of 0-RTT for a
            // single period.  This ensures that an attacker that is able to force a
            // server to reboot is unable to use that to flush the anti-replay buffers
            // and have something replayed.
            //
            // However, this is a massive inconvenience for us when we are testing.
            // As we can't initialize `AntiReplay` in the past (see `neqo_common::time`
            // for why), fast forward time here so that the connections get times from
            // in the future.
            //
            // This is NOT SAFE.  Don't do this.
            Instant::now() + ANTI_REPLAY_WINDOW
        } else {
            Instant::now()
        }
    }
}

fn qns_read_response(filename: &str) -> Result<Vec<u8>, io::Error> {
    let path: PathBuf = ["/www", filename.trim_matches(|p| p == '/')]
        .iter()
        .collect();
    fs::read(path)
}

#[allow(clippy::module_name_repetitions)]
pub trait HttpServer: Display {
    fn process(&mut self, dgram: Option<&Datagram>, now: Instant) -> Output;
    fn process_events(&mut self, now: Instant);
    fn has_events(&self) -> bool;
}

#[allow(clippy::module_name_repetitions)]
pub struct ServerRunner {
    now: Box<dyn Fn() -> Instant>,
    server: Box<dyn HttpServer>,
    timeout: Option<Pin<Box<Sleep>>>,
    sockets: Vec<(SocketAddr, udp::Socket)>,
}

impl ServerRunner {
    #[must_use]
    pub fn new(
        now: Box<dyn Fn() -> Instant>,
        server: Box<dyn HttpServer>,
        sockets: Vec<(SocketAddr, udp::Socket)>,
    ) -> Self {
        Self {
            now,
            server,
            timeout: None,
            sockets,
        }
    }

    /// Tries to find a socket, but then just falls back to sending from the first.
    fn find_socket(&mut self, addr: SocketAddr) -> &mut udp::Socket {
        let ((_host, first_socket), rest) = self.sockets.split_first_mut().unwrap();
        rest.iter_mut()
            .map(|(_host, socket)| socket)
            .find(|socket| {
                socket
                    .local_addr()
                    .ok()
                    .map_or(false, |socket_addr| socket_addr == addr)
            })
            .unwrap_or(first_socket)
    }

    async fn process(&mut self, mut dgram: Option<&Datagram>) -> Result<(), io::Error> {
        loop {
            match self.server.process(dgram.take(), (self.now)()) {
                Output::Datagram(dgram) => {
                    let socket = self.find_socket(dgram.source());
                    socket.writable().await?;
                    socket.send(&dgram)?;
                }
                Output::Callback(new_timeout) => {
                    qdebug!("Setting timeout of {:?}", new_timeout);
                    self.timeout = Some(Box::pin(tokio::time::sleep(new_timeout)));
                    break;
                }
                Output::None => {
                    break;
                }
            }
        }
        Ok(())
    }

    // Wait for any of the sockets to be readable or the timeout to fire.
    async fn ready(&mut self) -> Result<Ready, io::Error> {
        let sockets_ready = select_all(
            self.sockets
                .iter()
                .map(|(_host, socket)| Box::pin(socket.readable())),
        )
        .map(|(res, inx, _)| match res {
            Ok(()) => Ok(Ready::Socket(inx)),
            Err(e) => Err(e),
        });
        let timeout_ready = self
            .timeout
            .as_mut()
            .map_or(Either::Right(futures::future::pending()), Either::Left)
            .map(|()| Ok(Ready::Timeout));
        select(sockets_ready, timeout_ready).await.factor_first().0
    }

    pub async fn run(mut self) -> Res<()> {
        loop {
            self.server.process_events((self.now)());

            self.process(None).await?;

            if self.server.has_events() {
                continue;
            }

            match self.ready().await? {
                Ready::Socket(inx) => loop {
                    let (host, socket) = self.sockets.get_mut(inx).unwrap();
                    let dgrams = socket.recv(host)?;
                    if dgrams.is_empty() {
                        break;
                    }
                    for dgram in dgrams {
                        self.process(Some(&dgram)).await?;
                    }
                },
                Ready::Timeout => {
                    self.timeout = None;
                    self.process(None).await?;
                }
            }
        }
    }
}

enum Ready {
    Socket(usize),
    Timeout,
}

pub async fn server(mut args: Args) -> Res<()> {
    const HQ_INTEROP: &str = "hq-interop";

    neqo_common::log::init(
        args.shared
            .verbose
            .as_ref()
            .map(clap_verbosity_flag::Verbosity::log_level_filter),
    );
    assert!(!args.key.is_empty(), "Need at least one key");

    init_db(args.db.clone())?;

    if let Some(testcase) = args.shared.qns_test.as_ref() {
        if args.shared.quic_parameters.quic_version.is_empty() {
            // Quic Interop Runner expects the server to support `Version1`
            // only. Exceptions are testcases `versionnegotiation` (not yet
            // implemented) and `v2`.
            if testcase != "v2" {
                args.shared.quic_parameters.quic_version = vec![Version::Version1];
            }
        } else {
            qwarn!("Both -V and --qns-test were set. Ignoring testcase specific versions.");
        }

        // TODO: More options to deduplicate with client?
        match testcase.as_str() {
            "http3" => (),
            "zerortt" => {
                args.shared.use_old_http = true;
                args.shared.alpn = String::from(HQ_INTEROP);
                args.shared.quic_parameters.max_streams_bidi = 100;
            }
            "handshake" | "transfer" | "resumption" | "multiconnect" | "v2" | "ecn" => {
                args.shared.use_old_http = true;
                args.shared.alpn = String::from(HQ_INTEROP);
            }
            "chacha20" => {
                args.shared.use_old_http = true;
                args.shared.alpn = String::from(HQ_INTEROP);
                args.shared.ciphers.clear();
                args.shared
                    .ciphers
                    .extend_from_slice(&[String::from("TLS_CHACHA20_POLY1305_SHA256")]);
            }
            "retry" => {
                args.shared.use_old_http = true;
                args.shared.alpn = String::from(HQ_INTEROP);
                args.retry = true;
            }
            _ => exit(127),
        }
    }

    let hosts = args.listen_addresses();
    if hosts.is_empty() {
        qerror!("No valid hosts defined");
        Err(io::Error::new(io::ErrorKind::InvalidInput, "No hosts"))?;
    }
    let sockets = hosts
        .into_iter()
        .map(|host| {
            let socket = udp::Socket::bind(host)?;
            let local_addr = socket.local_addr()?;
            qinfo!("Server waiting for connection on: {local_addr:?}");

            Ok((host, socket))
        })
        .collect::<Result<_, io::Error>>()?;

    // Note: this is the exception to the case where we use `Args::now`.
    let anti_replay = AntiReplay::new(Instant::now(), ANTI_REPLAY_WINDOW, 7, 14)
        .expect("unable to setup anti-replay");
    let cid_mgr = Rc::new(RefCell::new(RandomConnectionIdGenerator::new(10)));

    let server: Box<dyn HttpServer> = if args.shared.use_old_http {
        Box::new(
            http09::HttpServer::new(&args, anti_replay, cid_mgr).expect("We cannot make a server!"),
        )
    } else {
        Box::new(http3::HttpServer::new(&args, anti_replay, cid_mgr))
    };

    ServerRunner::new(Box::new(move || args.now()), server, sockets)
        .run()
        .await
}
