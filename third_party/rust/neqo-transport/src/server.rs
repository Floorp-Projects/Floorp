// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// This file implements a server that can handle multiple connections.

use neqo_common::{
    hex, matches, qerror, qinfo, qtrace, qwarn, timer::Timer, Datagram, Decoder, Encoder,
};
use neqo_crypto::{
    constants::{TLS_AES_128_GCM_SHA256, TLS_VERSION_1_3},
    selfencrypt::SelfEncrypt,
    AntiReplay,
};

use crate::cid::{ConnectionId, ConnectionIdDecoder, ConnectionIdManager, ConnectionIdRef};
use crate::connection::{Connection, Output, State};
use crate::packet::{PacketBuilder, PacketType, PublicPacket};
use crate::Res;

use std::cell::RefCell;
use std::collections::{HashMap, HashSet, VecDeque};
use std::convert::TryFrom;
use std::mem;
use std::net::{IpAddr, SocketAddr};
use std::ops::{Deref, DerefMut};
use std::rc::Rc;
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
        // TODO(mt) rotate keys on a fixed schedule.
        let mut token = Encoder::default();
        const EXPIRATION: Duration = Duration::from_secs(5);
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

pub struct Server {
    /// The names of certificates.
    certs: Vec<String>,
    /// The ALPN values that the server supports.
    protocols: Vec<String>,
    anti_replay: AntiReplay,
    /// A connection ID manager.
    cid_manager: CidMgr,
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
}

impl Server {
    /// Construct a new server.
    /// `now` is the time that the server is instantiated.
    /// `cid_manager` is responsible for generating connection IDs and parsing them;
    /// connection IDs produced by the manager cannot be zero-length.
    /// `certs` is a list of the certificates that should be configured.
    /// `protocols` is the preference list of ALPN values.
    /// `anti_replay` is an anti-replay context.
    pub fn new(
        now: Instant,
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        anti_replay: AntiReplay,
        cid_manager: CidMgr,
    ) -> Res<Self> {
        Ok(Self {
            certs: certs.iter().map(|x| String::from(x.as_ref())).collect(),
            protocols: protocols.iter().map(|x| String::from(x.as_ref())).collect(),
            anti_replay,
            cid_manager,
            connections: Rc::default(),
            active: HashSet::default(),
            waiting: VecDeque::default(),
            timers: Timer::new(now, TIMER_GRANULARITY, TIMER_CAPACITY),
            retry: RetryToken::new(now)?,
        })
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
                self.waiting.push_back(c.clone());
            }
            Output::Callback(delay) => {
                let next = now + delay;
                if next != c.borrow().last_timer {
                    qtrace!([self], "Change timer to {:?}", next);
                    self.remove_timer(&c);
                    c.borrow_mut().last_timer = next;
                    self.timers.add(next, c.clone());
                }
            }
            _ => {
                self.remove_timer(&c);
            }
        }
        if c.borrow().has_events() {
            qtrace!([self], "Connection active: {:?}", c);
            self.active.insert(ActiveConnectionRef { c: c.clone() });
        }
        if matches!(c.borrow().state(), State::Closed(_)) {
            self.connections
                .borrow_mut()
                .retain(|_, v| !Rc::ptr_eq(v, &c));
        }
        out.dgram()
    }

    fn connection(&self, cid: &ConnectionIdRef) -> Option<StateRef> {
        if let Some(c) = self.connections.borrow().get(&cid[..]) {
            Some(c.clone())
        } else {
            None
        }
    }

    fn handle_initial(
        &mut self,
        dcid: ConnectionId,
        scid: ConnectionId,
        token: Vec<u8>,
        dgram: Datagram,
        now: Instant,
    ) -> Option<Datagram> {
        match self.retry.validate(&token, dgram.source(), now) {
            RetryTokenResult::Invalid => None,
            RetryTokenResult::Pass => self.accept_connection(None, dgram, now),
            RetryTokenResult::Valid(dcid) => self.accept_connection(Some(dcid), dgram, now),
            RetryTokenResult::Validate => {
                qinfo!([self], "Send retry for {:?}", dcid);

                let res = self.retry.generate_token(&dcid, dgram.source(), now);
                let token = if let Ok(t) = res {
                    t
                } else {
                    qerror!([self], "unable to generate token, dropping packet");
                    return None;
                };
                let new_dcid = self.cid_manager.borrow_mut().generate_cid();
                let packet = PacketBuilder::retry(&scid, &new_dcid, &token, &dcid);
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

    fn accept_connection(
        &mut self,
        odcid: Option<ConnectionId>,
        dgram: Datagram,
        now: Instant,
    ) -> Option<Datagram> {
        qinfo!([self], "Accept connection");
        // The internal connection ID manager that we use is not used directly.
        // Instead, wrap it so that we can save connection IDs.
        let cid_mgr = Rc::new(RefCell::new(ServerConnectionIdManager {
            c: None,
            cid_manager: self.cid_manager.clone(),
            connections: self.connections.clone(),
        }));
        let sconn = Connection::new_server(
            &self.certs,
            &self.protocols,
            &self.anti_replay,
            cid_mgr.clone(),
        );
        if let Ok(mut c) = sconn {
            if let Some(odcid) = odcid {
                c.original_connection_id(&odcid);
            }
            let c = Rc::new(RefCell::new(ServerConnectionState { c, last_timer: now }));
            cid_mgr.borrow_mut().c = Some(c.clone());
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
        if packet.packet_type() == PacketType::OtherVersion {
            let vn = PacketBuilder::version_negotiation(packet.scid(), packet.dcid());
            return Some(Datagram::new(dgram.destination(), dgram.source(), vn));
        }

        // Copy values from `packet` because they are currently still borrowing from `dgram`.
        let dcid = ConnectionId::from(packet.dcid());
        let scid = ConnectionId::from(packet.scid());
        let token = packet.token().to_vec();
        self.handle_initial(dcid, scid, token, dgram, now)
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
        mem::replace(&mut self.active, Default::default())
            .into_iter()
            .collect()
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
        self.c.clone()
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
    c: Option<StateRef>,
    connections: ConnectionTableRef,
    cid_manager: CidMgr,
}

impl ConnectionIdDecoder for ServerConnectionIdManager {
    fn decode_cid<'a>(&self, dec: &mut Decoder<'a>) -> Option<ConnectionIdRef<'a>> {
        self.cid_manager.borrow_mut().decode_cid(dec)
    }
}
impl ConnectionIdManager for ServerConnectionIdManager {
    fn generate_cid(&mut self) -> ConnectionId {
        let cid = self.cid_manager.borrow_mut().generate_cid();
        assert!(!cid.is_empty());
        let v = self
            .connections
            .borrow_mut()
            .insert(cid.clone(), self.c.as_ref().unwrap().clone());
        if let Some(v) = v {
            debug_assert!(Rc::ptr_eq(&v, self.c.as_ref().unwrap()));
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
