// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// This file implements a server that can handle multiple connections.

use neqo_common::{
    self as common, hex, qdebug, qerror, qinfo, qlog::NeqoQlog, qtrace, qwarn, timer::Timer,
    Datagram, Decoder, Encoder, Role,
};
use neqo_crypto::{
    constants::{TLS_AES_128_GCM_SHA256, TLS_VERSION_1_3},
    selfencrypt::SelfEncrypt,
    AntiReplay, ZeroRttCheckResult, ZeroRttChecker,
};

use crate::cid::{ConnectionId, ConnectionIdDecoder, ConnectionIdManager, ConnectionIdRef};
use crate::connection::{Connection, Output, State};
use crate::packet::{PacketBuilder, PacketType, PublicPacket};
use crate::{QuicVersion, Res};

use std::cell::RefCell;
use std::collections::{HashMap, HashSet, VecDeque};
use std::convert::TryFrom;
use std::fs::OpenOptions;
use std::mem;
use std::net::{IpAddr, SocketAddr};
use std::ops::{Deref, DerefMut};
use std::path::PathBuf;
use std::rc::{Rc, Weak};
use std::time::{Duration, Instant};

pub enum InitialResult {
    Accept,
    Drop,
    Retry(Vec<u8>),
}

/// MIN_INITIAL_PACKET_SIZE is the smallest packet that can be used to establish
/// a new connection across all QUIC versions this server supports.
const MIN_INITIAL_PACKET_SIZE: usize = 1200;
const TIMER_GRANULARITY: Duration = Duration::from_millis(10);
const TIMER_CAPACITY: usize = 16384;

type StateRef = Rc<RefCell<ServerConnectionState>>;
type CidMgr = Rc<RefCell<dyn ConnectionIdManager>>;
type ConnectionTableRef = Rc<RefCell<HashMap<ConnectionId, StateRef>>>;

#[derive(Debug)]
pub struct ServerConnectionState {
    c: Connection,
    active_attempt: Option<AttemptKey>,
    last_timer: Instant,
}

impl Deref for ServerConnectionState {
    type Target = Connection;
    fn deref(&self) -> &Self::Target {
        &self.c
    }
}

impl DerefMut for ServerConnectionState {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.c
    }
}

enum RetryTokenResult {
    Pass,
    Valid(ConnectionId),
    Validate,
    Invalid,
}

struct RetryToken {
    /// Whether to send a Retry.
    require_retry: bool,
    /// A self-encryption object used for protecting Retry tokens.
    self_encrypt: SelfEncrypt,
    /// When this object was created.
    start_time: Instant,
}

impl RetryToken {
    fn new(now: Instant) -> Res<Self> {
        Ok(Self {
            require_retry: false,
            self_encrypt: SelfEncrypt::new(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256)?,
            start_time: now,
        })
    }

    fn encode_peer_address(peer_address: SocketAddr) -> Vec<u8> {
        // Let's be "clever" by putting the peer's address in the AAD.
        // We don't need to encode these into the token as they should be
        // available when we need to check the token.
        let mut encoded_address = Encoder::default();
        match peer_address.ip() {
            IpAddr::V4(a) => {
                encoded_address.encode_byte(4);
                encoded_address.encode(&a.octets());
            }
            IpAddr::V6(a) => {
                encoded_address.encode_byte(6);
                encoded_address.encode(&a.octets());
            }
        }
        encoded_address.encode_uint(2, peer_address.port());
        encoded_address.into()
    }

    /// This generates a token for use with Retry.
    pub fn generate_token(
        &mut self,
        dcid: &ConnectionId,
        peer_address: SocketAddr,
        now: Instant,
    ) -> Res<Vec<u8>> {
        const EXPIRATION: Duration = Duration::from_secs(5);

        // TODO(mt) rotate keys on a fixed schedule.
        let mut token = Encoder::default();
        let end = now + EXPIRATION;
        let end_millis = u32::try_from(end.duration_since(self.start_time).as_millis())?;
        token.encode_uint(4, end_millis);
        token.encode(dcid);
        let peer_addr = Self::encode_peer_address(peer_address);
        Ok(self.self_encrypt.seal(&peer_addr, &token)?)
    }

    pub fn set_retry_required(&mut self, retry: bool) {
        self.require_retry = retry;
    }

    /// Decrypts `token` and returns the connection Id it contains.
    /// Returns `None` if the date is invalid in any way (such as it being expired or garbled).
    fn decrypt_token(
        &self,
        token: &[u8],
        peer_address: SocketAddr,
        now: Instant,
    ) -> Option<ConnectionId> {
        let peer_addr = Self::encode_peer_address(peer_address);
        let data = if let Ok(d) = self.self_encrypt.open(&peer_addr, token) {
            d
        } else {
            return None;
        };
        let mut dec = Decoder::new(&data);
        match dec.decode_uint(4) {
            Some(d) => {
                let end = self.start_time + Duration::from_millis(d);
                if end < now {
                    return None;
                }
            }
            _ => return None,
        }
        Some(ConnectionId::from(dec.decode_remainder()))
    }

    pub fn validate(
        &self,
        token: &[u8],
        peer_address: SocketAddr,
        now: Instant,
    ) -> RetryTokenResult {
        if token.is_empty() {
            if self.require_retry {
                RetryTokenResult::Validate
            } else {
                RetryTokenResult::Pass
            }
        } else if let Some(cid) = self.decrypt_token(token, peer_address, now) {
            RetryTokenResult::Valid(cid)
        } else {
            RetryTokenResult::Invalid
        }
    }
}

/// A `AttemptKey` is used to disambiguate connection attempts.
/// Multiple connection attempts with the same key won't produce multiple connections.
#[derive(Clone, Debug, Hash, PartialEq, Eq)]
struct AttemptKey {
    // Using the remote address is sufficient for disambiguation,
    // until we support multiple local socket addresses.
    remote_address: SocketAddr,
    odcid: ConnectionId,
}

/// A `ServerZeroRttChecker` is a simple wrapper around a single checker.
/// It uses `RefCell` so that the wrapped checker can be shared between
/// multiple connections created by the server.
#[derive(Clone, Debug)]
struct ServerZeroRttChecker {
    checker: Rc<RefCell<Box<dyn ZeroRttChecker>>>,
}

impl ServerZeroRttChecker {
    pub fn new(checker: Box<dyn ZeroRttChecker>) -> Self {
        Self {
            checker: Rc::new(RefCell::new(checker)),
        }
    }
}

impl ZeroRttChecker for ServerZeroRttChecker {
    fn check(&self, token: &[u8]) -> ZeroRttCheckResult {
        self.checker.borrow().check(token)
    }
}

/// `InitialDetails` holds important information for processing `Initial` packets.
struct InitialDetails {
    src_cid: ConnectionId,
    dst_cid: ConnectionId,
    token: Vec<u8>,
    quic_version: QuicVersion,
}

impl InitialDetails {
    fn new(packet: &PublicPacket) -> Self {
        Self {
            src_cid: ConnectionId::from(packet.scid()),
            dst_cid: ConnectionId::from(packet.dcid()),
            token: packet.token().to_vec(),
            quic_version: packet.version().unwrap(),
        }
    }
}

pub struct Server {
    /// The names of certificates.
    certs: Vec<String>,
    /// The ALPN values that the server supports.
    protocols: Vec<String>,
    /// Anti-replay configuration for 0-RTT.
    anti_replay: AntiReplay,
    /// A function for determining if 0-RTT can be accepted.
    zero_rtt_checker: ServerZeroRttChecker,
    /// A connection ID manager.
    cid_manager: CidMgr,
    /// Active connection attempts, keyed by `AttemptKey`.  Initial packets with
    /// the same key are routed to the connection that was first accepted.
    /// This is cleared out when the connection is closed or established.
    active_attempts: HashMap<AttemptKey, StateRef>,
    /// All connections, keyed by ConnectionId.
    connections: ConnectionTableRef,
    /// The connections that have new events.
    active: HashSet<ActiveConnectionRef>,
    /// The set of connections that need immediate processing.
    waiting: VecDeque<StateRef>,
    /// Outstanding timers for connections.
    timers: Timer<StateRef>,
    /// Whether a Retry packet will be sent in response to new
    /// Initial packets.
    retry: RetryToken,
    /// Directory to create qlog traces in
    qlog_dir: Option<PathBuf>,
}

impl Server {
    /// Construct a new server.
    /// * `now` is the time that the server is instantiated.
    /// * `certs` is a list of the certificates that should be configured.
    /// * `protocols` is the preference list of ALPN values.
    /// * `anti_replay` is an anti-replay context.
    /// * `zero_rtt_checker` determines whether 0-RTT should be accepted. This
    ///   will be passed the value of the `extra` argument that was passed to
    ///   `Connection::send_ticket` to see if it is OK.
    /// * `cid_manager` is responsible for generating connection IDs and parsing them;
    ///   connection IDs produced by the manager cannot be zero-length.
    pub fn new(
        now: Instant,
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        anti_replay: AntiReplay,
        zero_rtt_checker: Box<dyn ZeroRttChecker>,
        cid_manager: CidMgr,
    ) -> Res<Self> {
        Ok(Self {
            certs: certs.iter().map(|x| String::from(x.as_ref())).collect(),
            protocols: protocols.iter().map(|x| String::from(x.as_ref())).collect(),
            anti_replay,
            zero_rtt_checker: ServerZeroRttChecker::new(zero_rtt_checker),
            cid_manager,
            active_attempts: HashMap::default(),
            connections: Rc::default(),
            active: HashSet::default(),
            waiting: VecDeque::default(),
            timers: Timer::new(now, TIMER_GRANULARITY, TIMER_CAPACITY),
            retry: RetryToken::new(now)?,
            qlog_dir: None,
        })
    }

    /// Set or clear directory to create logs of connection events in QLOG format.
    pub fn set_qlog_dir(&mut self, dir: Option<PathBuf>) {
        self.qlog_dir = dir;
    }

    pub fn set_retry_required(&mut self, require_retry: bool) {
        self.retry.set_retry_required(require_retry);
    }

    fn remove_timer(&mut self, c: &StateRef) {
        let last = c.borrow().last_timer;
        self.timers.remove(last, |t| Rc::ptr_eq(t, c));
    }

    fn process_connection(
        &mut self,
        c: StateRef,
        dgram: Option<Datagram>,
        now: Instant,
    ) -> Option<Datagram> {
        qtrace!([self], "Process connection {:?}", c);
        let out = c.borrow_mut().process(dgram, now);
        match out {
            Output::Datagram(_) => {
                qtrace!([self], "Sending packet, added to waiting connections");
                self.waiting.push_back(Rc::clone(&c));
            }
            Output::Callback(delay) => {
                let next = now + delay;
                if next != c.borrow().last_timer {
                    qtrace!([self], "Change timer to {:?}", next);
                    self.remove_timer(&c);
                    c.borrow_mut().last_timer = next;
                    self.timers.add(next, Rc::clone(&c));
                }
            }
            _ => {
                self.remove_timer(&c);
            }
        }
        if c.borrow().has_events() {
            qtrace!([self], "Connection active: {:?}", c);
            self.active.insert(ActiveConnectionRef { c: Rc::clone(&c) });
        }

        if *c.borrow().state() > State::Handshaking {
            // Remove any active connection attempt now that this is no longer handshaking.
            if let Some(k) = c.borrow_mut().active_attempt.take() {
                self.active_attempts.remove(&k);
            }
        }

        if matches!(c.borrow().state(), State::Closed(_)) {
            c.borrow_mut().set_qlog(NeqoQlog::disabled());
            self.connections
                .borrow_mut()
                .retain(|_, v| !Rc::ptr_eq(v, &c));
        }
        out.dgram()
    }

    fn connection(&self, cid: &ConnectionIdRef) -> Option<StateRef> {
        if let Some(c) = self.connections.borrow().get(&cid[..]) {
            Some(Rc::clone(&c))
        } else {
            None
        }
    }

    fn handle_initial(
        &mut self,
        initial: InitialDetails,
        dgram: Datagram,
        now: Instant,
    ) -> Option<Datagram> {
        qdebug!([self], "Handle initial");
        match self.retry.validate(&initial.token, dgram.source(), now) {
            RetryTokenResult::Invalid => None,
            RetryTokenResult::Pass => self.connection_attempt(initial, dgram, None, now),
            RetryTokenResult::Valid(orig_dcid) => {
                self.connection_attempt(initial, dgram, Some(orig_dcid), now)
            }
            RetryTokenResult::Validate => {
                qinfo!([self], "Send retry for {:?}", initial.dst_cid);

                let res = self
                    .retry
                    .generate_token(&initial.dst_cid, dgram.source(), now);
                let token = if let Ok(t) = res {
                    t
                } else {
                    qerror!([self], "unable to generate token, dropping packet");
                    return None;
                };
                let new_dcid = self.cid_manager.borrow_mut().generate_cid();
                let packet = PacketBuilder::retry(
                    initial.quic_version,
                    &initial.src_cid,
                    &new_dcid,
                    &token,
                    &initial.dst_cid,
                );
                if let Ok(p) = packet {
                    let retry = Datagram::new(dgram.destination(), dgram.source(), p);
                    Some(retry)
                } else {
                    qerror!([self], "unable to encode retry, dropping packet");
                    None
                }
            }
        }
    }

    fn connection_attempt(
        &mut self,
        initial: InitialDetails,
        dgram: Datagram,
        orig_dcid: Option<ConnectionId>,
        now: Instant,
    ) -> Option<Datagram> {
        let attempt_key = AttemptKey {
            remote_address: dgram.source(),
            odcid: orig_dcid.as_ref().unwrap_or(&initial.dst_cid).clone(),
        };
        if let Some(c) = self.active_attempts.get(&attempt_key) {
            qdebug!(
                [self],
                "Handle Initial for existing connection attempt {:?}",
                attempt_key
            );
            let c = Rc::clone(c);
            self.process_connection(c, Some(dgram), now)
        } else {
            self.accept_connection(attempt_key, initial, dgram, orig_dcid, now)
        }
    }

    fn create_qlog_trace(&self, attempt_key: &AttemptKey) -> NeqoQlog {
        if let Some(qlog_dir) = &self.qlog_dir {
            let mut qlog_path = qlog_dir.to_path_buf();

            qlog_path.push(format!("{}.qlog", attempt_key.odcid));

            // The original DCID is chosen by the client. Using create_new()
            // prevents attackers from overwriting existing logs.
            match OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(&qlog_path)
            {
                Ok(f) => {
                    qinfo!("Qlog output to {}", qlog_path.display());

                    let streamer = ::qlog::QlogStreamer::new(
                        qlog::QLOG_VERSION.to_string(),
                        Some("Neqo server qlog".to_string()),
                        Some("Neqo server qlog".to_string()),
                        None,
                        std::time::Instant::now(),
                        common::qlog::new_trace(Role::Server),
                        Box::new(f),
                    );
                    let n_qlog = NeqoQlog::enabled(streamer, qlog_path);
                    match n_qlog {
                        Ok(nql) => nql,
                        Err(e) => {
                            // Keep going but w/o qlogging
                            qerror!("NeqoQlog error: {}", e);
                            NeqoQlog::disabled()
                        }
                    }
                }
                Err(e) => {
                    qerror!(
                        "Could not open file {} for qlog output: {}",
                        qlog_path.display(),
                        e
                    );
                    NeqoQlog::disabled()
                }
            }
        } else {
            NeqoQlog::disabled()
        }
    }

    fn accept_connection(
        &mut self,
        attempt_key: AttemptKey,
        initial: InitialDetails,
        dgram: Datagram,
        orig_dcid: Option<ConnectionId>,
        now: Instant,
    ) -> Option<Datagram> {
        qinfo!([self], "Accept connection {:?}", attempt_key);
        // The internal connection ID manager that we use is not used directly.
        // Instead, wrap it so that we can save connection IDs.

        let cid_mgr = Rc::new(RefCell::new(ServerConnectionIdManager {
            c: Weak::new(),
            cid_manager: Rc::clone(&self.cid_manager),
            connections: Rc::clone(&self.connections),
            saved_cids: Vec::new(),
        }));

        let sconn = Connection::new_server(
            &self.certs,
            &self.protocols,
            Rc::clone(&cid_mgr) as _,
            initial.quic_version,
        );

        if let Ok(mut c) = sconn {
            let zcheck = self.zero_rtt_checker.clone();
            if c.server_enable_0rtt(&self.anti_replay, zcheck).is_err() {
                qwarn!([self], "Unable to enable 0-RTT");
            }
            if let Some(odcid) = orig_dcid {
                // There was a retry, so set the connection IDs for.
                c.set_retry_cids(odcid, initial.src_cid, initial.dst_cid);
            }
            c.set_qlog(self.create_qlog_trace(&attempt_key));
            let c = Rc::new(RefCell::new(ServerConnectionState {
                c,
                last_timer: now,
                active_attempt: Some(attempt_key.clone()),
            }));
            cid_mgr.borrow_mut().set_connection(Rc::clone(&c));
            let previous_attempt = self.active_attempts.insert(attempt_key, Rc::clone(&c));
            debug_assert!(previous_attempt.is_none());
            self.process_connection(c, Some(dgram), now)
        } else {
            qwarn!([self], "Unable to create connection");
            None
        }
    }

    fn process_input(&mut self, dgram: Datagram, now: Instant) -> Option<Datagram> {
        qtrace!("Process datagram: {}", hex(&dgram[..]));

        // This is only looking at the first packet header in the datagram.
        // All packets in the datagram are routed to the same connection.
        let res = PublicPacket::decode(&dgram[..], self.cid_manager.borrow().as_decoder());
        let (packet, _remainder) = match res {
            Ok(res) => res,
            _ => {
                qtrace!([self], "Discarding {:?}", dgram);
                return None;
            }
        };

        // Finding an existing connection. Should be the most common case.
        if let Some(c) = self.connection(packet.dcid()) {
            return self.process_connection(c, Some(dgram), now);
        }

        if packet.packet_type() == PacketType::Short {
            // TODO send a stateless reset here.
            qtrace!([self], "Short header packet for an unknown connection");
            return None;
        }

        if dgram.len() < MIN_INITIAL_PACKET_SIZE {
            qtrace!([self], "Bogus packet: too short");
            return None;
        }
        match packet.packet_type() {
            PacketType::Initial => {
                // Copy values from `packet` because they are currently still borrowing from `dgram`.
                let initial = InitialDetails::new(&packet);
                self.handle_initial(initial, dgram, now)
            }
            PacketType::OtherVersion => {
                let vn = PacketBuilder::version_negotiation(packet.scid(), packet.dcid());
                Some(Datagram::new(dgram.destination(), dgram.source(), vn))
            }
            _ => {
                qtrace!([self], "Not an initial packet");
                None
            }
        }
    }

    /// Iterate through the pending connections looking for any that might want
    /// to send a datagram.  Stop at the first one that does.
    fn process_next_output(&mut self, now: Instant) -> Option<Datagram> {
        qtrace!([self], "No packet to send, look at waiting connections");
        while let Some(c) = self.waiting.pop_front() {
            if let Some(d) = self.process_connection(c, None, now) {
                return Some(d);
            }
        }
        qtrace!([self], "No packet to send still, run timers");
        while let Some(c) = self.timers.take_next(now) {
            if let Some(d) = self.process_connection(c, None, now) {
                return Some(d);
            }
        }
        None
    }

    fn next_time(&mut self, now: Instant) -> Option<Duration> {
        if self.waiting.is_empty() {
            self.timers.next_time().map(|x| x - now)
        } else {
            Some(Duration::new(0, 0))
        }
    }

    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        let out = if let Some(d) = dgram {
            self.process_input(d, now)
        } else {
            None
        };
        let out = out.or_else(|| self.process_next_output(now));
        match out {
            Some(d) => {
                qtrace!([self], "Send packet: {:?}", d);
                Output::Datagram(d)
            }
            _ => match self.next_time(now) {
                Some(delay) => {
                    qtrace!([self], "Wait: {:?}", delay);
                    Output::Callback(delay)
                }
                _ => {
                    qtrace!([self], "Go dormant");
                    Output::None
                }
            },
        }
    }

    /// This lists the connections that have received new events
    /// as a result of calling `process()`.
    pub fn active_connections(&mut self) -> Vec<ActiveConnectionRef> {
        mem::take(&mut self.active).into_iter().collect()
    }

    pub fn add_to_waiting(&mut self, c: ActiveConnectionRef) {
        self.waiting.push_back(c.connection());
    }
}

#[derive(Clone, Debug)]
pub struct ActiveConnectionRef {
    c: StateRef,
}

impl ActiveConnectionRef {
    pub fn borrow<'a>(&'a self) -> impl Deref<Target = Connection> + 'a {
        std::cell::Ref::map(self.c.borrow(), |c| &c.c)
    }

    pub fn borrow_mut<'a>(&'a mut self) -> impl DerefMut<Target = Connection> + 'a {
        std::cell::RefMut::map(self.c.borrow_mut(), |c| &mut c.c)
    }

    pub fn connection(&self) -> StateRef {
        Rc::clone(&self.c)
    }
}

impl std::hash::Hash for ActiveConnectionRef {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        let ptr: *const _ = self.c.as_ref();
        ptr.hash(state)
    }
}

impl PartialEq for ActiveConnectionRef {
    fn eq(&self, other: &Self) -> bool {
        Rc::ptr_eq(&self.c, &other.c)
    }
}

impl Eq for ActiveConnectionRef {}

struct ServerConnectionIdManager {
    c: Weak<RefCell<ServerConnectionState>>,
    connections: ConnectionTableRef,
    cid_manager: CidMgr,
    saved_cids: Vec<ConnectionId>,
}

impl ServerConnectionIdManager {
    pub fn set_connection(&mut self, c: StateRef) {
        let saved = std::mem::replace(&mut self.saved_cids, Vec::with_capacity(0));
        for cid in saved {
            qtrace!("ServerConnectionIdManager inserting saved cid {}", cid);
            self.insert_cid(cid, Rc::clone(&c));
        }
        self.c = Rc::downgrade(&c);
    }

    fn insert_cid(&mut self, cid: ConnectionId, rc: StateRef) {
        debug_assert!(!cid.is_empty());
        self.connections.borrow_mut().insert(cid, rc);
    }
}

impl ConnectionIdDecoder for ServerConnectionIdManager {
    fn decode_cid<'a>(&self, dec: &mut Decoder<'a>) -> Option<ConnectionIdRef<'a>> {
        self.cid_manager.borrow_mut().decode_cid(dec)
    }
}

impl ConnectionIdManager for ServerConnectionIdManager {
    fn generate_cid(&mut self) -> ConnectionId {
        let cid = self.cid_manager.borrow_mut().generate_cid();
        if let Some(rc) = self.c.upgrade() {
            self.insert_cid(cid.clone(), rc);
        } else {
            // This function can be called before the connection is set.
            // So save any connection IDs until that hookup happens.
            qtrace!("ServerConnectionIdManager saving cid {}", cid);
            self.saved_cids.push(cid.clone());
        }
        cid
    }

    fn as_decoder(&self) -> &dyn ConnectionIdDecoder {
        self
    }
}

impl ::std::fmt::Display for Server {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Server")
    }
}
