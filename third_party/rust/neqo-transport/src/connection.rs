// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// The class implementing a QUIC connection.

use std::cell::RefCell;
use std::cmp::{max, min, Ordering};
use std::collections::HashMap;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::fmt::{self, Debug};
use std::net::SocketAddr;
use std::rc::Rc;
use std::time::{Duration, Instant};

use smallvec::SmallVec;

use neqo_common::{hex, matches, qdebug, qerror, qinfo, qtrace, qwarn, Datagram, Decoder, Encoder};
use neqo_crypto::agent::CertificateInfo;
use neqo_crypto::{
    Agent, AntiReplay, AuthenticationStatus, Client, Epoch, HandshakeState, Record,
    SecretAgentInfo, Server,
};

use crate::crypto::{Crypto, CryptoDxDirection, CryptoDxState, CryptoState};
use crate::dump::*;
use crate::events::{ConnectionEvent, ConnectionEvents};
use crate::flow_mgr::FlowMgr;
use crate::frame::{decode_frame, AckRange, Frame, FrameType, StreamType, TxMode};
use crate::packet::{
    decode_packet_hdr, decrypt_packet, encode_packet, ConnectionId, ConnectionIdDecoder, PacketHdr,
    PacketNumberDecoder, PacketType,
};
use crate::recovery::{
    LossRecovery, LossRecoveryMode, LossRecoveryState, RecoveryToken, SentPacket,
};
use crate::recv_stream::{RecvStream, RecvStreams, RX_STREAM_DATA_WINDOW};
use crate::send_stream::{SendStream, SendStreams};
use crate::stats::Stats;
use crate::stream_id::{StreamId, StreamIndex, StreamIndexes};
use crate::tparams::{
    tp_constants, TransportParameter, TransportParameters, TransportParametersHandler,
};
use crate::tracking::{AckTracker, PNSpace};
use crate::QUIC_VERSION;
use crate::{AppError, ConnectionError, Error, Res};

#[derive(Debug, Default)]
struct Packet(Vec<u8>);

const NUM_EPOCHS: Epoch = 4;

pub const LOCAL_STREAM_LIMIT_BIDI: u64 = 16;
pub const LOCAL_STREAM_LIMIT_UNI: u64 = 16;

const LOCAL_MAX_DATA: u64 = 0x3FFF_FFFF_FFFF_FFFF; // 2^62-1

const LOCAL_IDLE_TIMEOUT: Duration = Duration::from_secs(60); // 1 minute

#[derive(Debug, PartialEq, Copy, Clone)]
/// Client or Server.
pub enum Role {
    Client,
    Server,
}

impl Role {
    pub fn remote(self) -> Self {
        match self {
            Role::Client => Role::Server,
            Role::Server => Role::Client,
        }
    }
}

impl ::std::fmt::Display for Role {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{:?}", self)
    }
}

#[derive(Clone, Debug, PartialEq, Ord, Eq)]
/// The state of the Connection.
pub enum State {
    Init,
    WaitInitial,
    Handshaking,
    Connected,
    Closing {
        error: ConnectionError,
        frame_type: FrameType,
        msg: String,
        timeout: Instant,
    },
    Closed(ConnectionError),
}

// Implement Ord so that we can enforce monotonic state progression.
impl PartialOrd for State {
    #[allow(clippy::match_same_arms)] // Lint bug: rust-lang/rust-clippy#860
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        if std::mem::discriminant(self) == std::mem::discriminant(other) {
            return Some(Ordering::Equal);
        }
        Some(match (self, other) {
            (State::Init, _) => Ordering::Less,
            (_, State::Init) => Ordering::Greater,
            (State::WaitInitial, _) => Ordering::Less,
            (_, State::WaitInitial) => Ordering::Greater,
            (State::Handshaking, _) => Ordering::Less,
            (_, State::Handshaking) => Ordering::Greater,
            (State::Connected, _) => Ordering::Less,
            (_, State::Connected) => Ordering::Greater,
            (State::Closing { .. }, _) => Ordering::Less,
            (_, State::Closing { .. }) => Ordering::Greater,
            (State::Closed(_), _) => unreachable!(),
        })
    }
}

#[derive(Debug)]
enum ZeroRttState {
    Init,
    Sending(CryptoDxState),
    AcceptedClient,
    AcceptedServer(CryptoDxState),
    Rejected,
}

#[derive(Clone, Debug, PartialEq)]
struct Path {
    local: SocketAddr,
    remote: SocketAddr,
    local_cids: Vec<ConnectionId>,
    remote_cid: ConnectionId,
}

impl Path {
    // Used to create a path when receiving a packet.
    pub fn new(d: &Datagram, remote_cid: ConnectionId) -> Self {
        Self {
            local: d.destination(),
            remote: d.source(),
            local_cids: Vec::new(),
            remote_cid,
        }
    }

    pub fn received_on(&self, d: &Datagram) -> bool {
        self.local == d.destination() && self.remote == d.source()
    }

    fn mtu(&self) -> usize {
        if self.local.is_ipv4() {
            1252
        } else {
            1232 // IPv6
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
/// Type returned from process() and `process_output()`. Users are required to
/// call these repeatedly until `Callback` or `None` is returned.
pub enum Output {
    /// Connection requires no action.
    None,
    /// Connection requires the datagram be sent.
    Datagram(Datagram),
    /// Connection requires `process_input()` be called when the `Duration`
    /// elapses.
    Callback(Duration),
}

impl Output {
    /// Convert into an `Option<Datagram>`.
    pub fn dgram(self) -> Option<Datagram> {
        match self {
            Output::Datagram(dg) => Some(dg),
            _ => None,
        }
    }

    /// Get a reference to the Datagram, if any.
    pub fn as_dgram_ref(&self) -> Option<&Datagram> {
        match self {
            Output::Datagram(dg) => Some(dg),
            _ => None,
        }
    }
}

pub trait ConnectionIdManager: ConnectionIdDecoder {
    fn generate_cid(&mut self) -> ConnectionId;
    fn as_decoder(&self) -> &dyn ConnectionIdDecoder;
}
/// Alias the common form for ConnectionIdManager.
type CidMgr = Rc<RefCell<dyn ConnectionIdManager>>;

/// An FixedConnectionIdManager produces random connection IDs of a fixed length.
pub struct FixedConnectionIdManager {
    len: usize,
}
impl FixedConnectionIdManager {
    pub fn new(len: usize) -> Self {
        Self { len }
    }
}
impl ConnectionIdDecoder for FixedConnectionIdManager {
    fn decode_cid(&self, dec: &mut Decoder) -> Option<ConnectionId> {
        dec.decode(self.len).map(ConnectionId::from)
    }
}
impl ConnectionIdManager for FixedConnectionIdManager {
    fn generate_cid(&mut self) -> ConnectionId {
        ConnectionId::generate(self.len)
    }
    fn as_decoder(&self) -> &dyn ConnectionIdDecoder {
        self
    }
}

struct RetryInfo {
    token: Vec<u8>,
    odcid: ConnectionId,
}

#[derive(Debug, Clone)]
/// There's a little bit of different behavior for resetting idle timeout. See
/// -transport 10.2 ("Idle Timeout").
enum IdleTimeout {
    Init,
    PacketReceived(Instant),
    AckElicitingPacketSent(Instant),
}

impl Default for IdleTimeout {
    fn default() -> Self {
        IdleTimeout::Init
    }
}

impl IdleTimeout {
    pub fn as_instant(&self) -> Option<Instant> {
        match self {
            IdleTimeout::Init => None,
            IdleTimeout::PacketReceived(t) | IdleTimeout::AckElicitingPacketSent(t) => Some(*t),
        }
    }

    fn on_packet_sent(&mut self, now: Instant) {
        // Only reset idle timeout if we've received a packet since the last
        // time we reset the timeout here.
        match self {
            IdleTimeout::AckElicitingPacketSent(_) => {}
            IdleTimeout::Init | IdleTimeout::PacketReceived(_) => {
                *self = IdleTimeout::AckElicitingPacketSent(now + LOCAL_IDLE_TIMEOUT);
            }
        }
    }

    fn on_packet_received(&mut self, now: Instant) {
        *self = IdleTimeout::PacketReceived(now + LOCAL_IDLE_TIMEOUT);
    }

    pub fn expired(&self, now: Instant) -> bool {
        if let Some(timeout) = self.as_instant() {
            now >= timeout
        } else {
            false
        }
    }
}

/// A QUIC Connection
///
/// First, create a new connection using `new_client()` or `new_server()`.
///
/// For the life of the connection, handle activity in the following manner:
/// 1. Perform operations using the `stream_*()` methods.
/// 1. Call `process_input()` when a datagram is received or the timer
/// expires. Obtain information on connection state changes by checking
/// `events()`.
/// 1. Having completed handling current activity, repeatedly call
/// `process_output()` for packets to send, until it returns `Output::Callback`
/// or `Output::None`.
///
/// After the connection is closed (either by calling `close()` or by the
/// remote) continue processing until `state()` returns `Closed`.
pub struct Connection {
    version: crate::packet::Version,
    role: Role,
    state: State,
    tps: Rc<RefCell<TransportParametersHandler>>,
    /// What we are doing with 0-RTT.
    zero_rtt_state: ZeroRttState,
    /// This object will generate connection IDs for the connection.
    cid_manager: CidMgr,
    /// Network paths.  Right now, this tracks at most one path, so it uses `Option`.
    path: Option<Path>,
    /// The connection IDs that we will accept.
    /// This includes any we advertise in NEW_CONNECTION_ID that haven't been bound to a path yet.
    /// During the handshake at the server, it also includes the randomized DCID pick by the client.
    valid_cids: Vec<ConnectionId>,
    retry_info: Option<RetryInfo>,
    pub(crate) crypto: Crypto,
    pub(crate) acks: AckTracker,
    idle_timeout: IdleTimeout,
    pub(crate) indexes: StreamIndexes,
    connection_ids: HashMap<u64, (Vec<u8>, [u8; 16])>, // (sequence number, (connection id, reset token))
    pub(crate) send_streams: SendStreams,
    pub(crate) recv_streams: RecvStreams,
    pub(crate) flow_mgr: Rc<RefCell<FlowMgr>>,
    loss_recovery: LossRecovery,
    loss_recovery_state: LossRecoveryState,
    events: ConnectionEvents,
    token: Option<Vec<u8>>,
    stats: Stats,
    tx_mode: TxMode,
}

impl Debug for Connection {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_fmt(format_args!(
            "{:?} Connection: {:?} {:?}",
            self.role, self.state, self.path
        ))
    }
}

impl Connection {
    /// Create a new QUIC connection with Client role.
    pub fn new_client(
        server_name: &str,
        protocols: &[impl AsRef<str>],
        cid_manager: CidMgr,
        local_addr: SocketAddr,
        remote_addr: SocketAddr,
    ) -> Res<Self> {
        let dcid = ConnectionId::generate_initial();
        let local_cids = vec![cid_manager.borrow_mut().generate_cid()];
        let mut c = Self::new(
            Role::Client,
            Client::new(server_name)?.into(),
            cid_manager,
            None,
            protocols,
            Some(Path {
                local: local_addr,
                remote: remote_addr,
                local_cids,
                remote_cid: dcid.clone(),
            }),
        );
        c.crypto.create_initial_state(Role::Client, &dcid);
        Ok(c)
    }

    /// Create a new QUIC connection with Server role.
    pub fn new_server(
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        anti_replay: &AntiReplay,
        cid_manager: CidMgr,
    ) -> Res<Self> {
        Ok(Self::new(
            Role::Server,
            Server::new(certs)?.into(),
            cid_manager,
            Some(anti_replay),
            protocols,
            None,
        ))
    }

    fn set_tp_defaults(tps: &mut TransportParameters) {
        tps.set_integer(
            tp_constants::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL,
            RX_STREAM_DATA_WINDOW,
        );
        tps.set_integer(
            tp_constants::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
            RX_STREAM_DATA_WINDOW,
        );
        tps.set_integer(
            tp_constants::INITIAL_MAX_STREAM_DATA_UNI,
            RX_STREAM_DATA_WINDOW,
        );
        tps.set_integer(
            tp_constants::INITIAL_MAX_STREAMS_BIDI,
            LOCAL_STREAM_LIMIT_BIDI,
        );
        tps.set_integer(
            tp_constants::INITIAL_MAX_STREAMS_UNI,
            LOCAL_STREAM_LIMIT_UNI,
        );
        tps.set_integer(tp_constants::INITIAL_MAX_DATA, LOCAL_MAX_DATA);
        tps.set_integer(
            tp_constants::IDLE_TIMEOUT,
            LOCAL_IDLE_TIMEOUT.as_millis().try_into().unwrap(),
        );
        tps.set_empty(tp_constants::DISABLE_MIGRATION);
    }

    fn new(
        r: Role,
        agent: Agent,
        cid_manager: CidMgr,
        anti_replay: Option<&AntiReplay>,
        protocols: &[impl AsRef<str>],
        path: Option<Path>,
    ) -> Self {
        let tphandler = Rc::new(RefCell::new(TransportParametersHandler::default()));
        Self::set_tp_defaults(&mut tphandler.borrow_mut().local);
        let crypto = Crypto::new(agent, protocols, tphandler.clone(), anti_replay)
            .expect("TLS should be configured successfully");

        Self {
            version: QUIC_VERSION,
            role: r,
            state: match r {
                Role::Client => State::Init,
                Role::Server => State::WaitInitial,
            },
            cid_manager,
            path,
            valid_cids: Vec::new(),
            tps: tphandler,
            zero_rtt_state: ZeroRttState::Init,
            retry_info: None,
            crypto,
            acks: AckTracker::default(),
            idle_timeout: IdleTimeout::default(),
            indexes: StreamIndexes::new(),
            connection_ids: HashMap::new(),
            send_streams: SendStreams::default(),
            recv_streams: RecvStreams::default(),
            flow_mgr: Rc::new(RefCell::new(FlowMgr::default())),
            loss_recovery: LossRecovery::new(),
            loss_recovery_state: LossRecoveryState::default(),
            events: ConnectionEvents::default(),
            token: None,
            stats: Stats::default(),
            tx_mode: TxMode::Normal,
        }
    }

    /// Set a local transport parameter, possibly overriding a default value.
    pub fn set_local_tparam(&self, key: u16, value: TransportParameter) -> Res<()> {
        if matches!(
            (self.role(), self.state()),
            (Role::Client, State::Init) | (Role::Server, State::WaitInitial)
        ) {
            self.tps.borrow_mut().local.set(key, value);
            Ok(())
        } else {
            qerror!("Current state: {:?}", self.state());
            qerror!("Cannot set local tparam when not in an initial connection state.");
            Err(Error::ConnectionState)
        }
    }

    /// Set the connection ID that was originally chosen by the client.
    pub(crate) fn original_connection_id(&mut self, odcid: &ConnectionId) {
        assert_eq!(self.role, Role::Server);
        self.tps
            .borrow_mut()
            .local
            .set_bytes(tp_constants::ORIGINAL_CONNECTION_ID, odcid.to_vec());
    }

    /// Set ALPN preferences. Strings that appear earlier in the list are given
    /// higher preference.
    pub fn set_alpn(&mut self, protocols: &[impl AsRef<str>]) -> Res<()> {
        self.crypto.tls.set_alpn(protocols)?;
        Ok(())
    }

    /// Access the latest resumption token on the connection.
    pub fn resumption_token(&self) -> Option<Vec<u8>> {
        if self.state != State::Connected {
            return None;
        }
        match self.crypto.tls {
            Agent::Client(ref c) => match c.resumption_token() {
                Some(ref t) => {
                    qtrace!("TLS token {}", hex(&t));
                    let mut enc = Encoder::default();
                    enc.encode_vvec_with(|enc_inner| {
                        self.tps
                            .borrow()
                            .remote
                            .as_ref()
                            .expect("should have transport parameters")
                            .encode(enc_inner);
                    });
                    enc.encode(&t[..]);
                    qinfo!("resumption token {}", hex(&enc[..]));
                    Some(enc.into())
                }
                None => None,
            },
            Agent::Server(_) => None,
        }
    }

    /// Enable resumption, using a token previously provided.
    /// This can only be called once and only on the client.
    /// After calling the function, it should be possible to attempt 0-RTT
    /// if the token supports that.
    pub fn set_resumption_token(&mut self, now: Instant, token: &[u8]) -> Res<()> {
        if self.state != State::Init {
            qerror!([self], "set token in state {:?}", self.state);
            return Err(Error::ConnectionState);
        }
        qinfo!([self], "resumption token {}", hex(token));
        let mut dec = Decoder::from(token);
        let tp_slice = match dec.decode_vvec() {
            Some(v) => v,
            _ => return Err(Error::InvalidResumptionToken),
        };
        qtrace!([self], "  transport parameters {}", hex(&tp_slice));
        let mut dec_tp = Decoder::from(tp_slice);
        let tp = TransportParameters::decode(&mut dec_tp)?;

        let tok = dec.decode_remainder();
        qtrace!([self], "  TLS token {}", hex(&tok));
        match self.crypto.tls {
            Agent::Client(ref mut c) => c.set_resumption_token(&tok)?,
            Agent::Server(_) => return Err(Error::WrongRole),
        }

        self.tps.borrow_mut().remote_0rtt = Some(tp);
        self.set_initial_limits();
        // Start up TLS, which has the effect of setting up all the necessary
        // state for 0-RTT.  This only stages the CRYPTO frames.
        self.client_start(now)
    }

    /// Send a TLS session ticket.
    pub fn send_ticket(&mut self, now: Instant, extra: &[u8]) -> Res<()> {
        let tps = &self.tps;
        match self.crypto.tls {
            Agent::Server(ref mut s) => {
                let mut enc = Encoder::default();
                enc.encode_vvec_with(|mut enc_inner| {
                    tps.borrow().local.encode(&mut enc_inner);
                });
                enc.encode(extra);
                let records = s.send_ticket(now, &enc)?;
                qinfo!([self], "send session ticket {}", hex(&enc));
                self.crypto.buffer_records(records);
                Ok(())
            }
            Agent::Client(_) => Err(Error::WrongRole),
        }
    }

    pub fn tls_info(&self) -> Option<&SecretAgentInfo> {
        self.crypto.tls.info()
    }

    /// Get the peer's certificate chain and other info.
    pub fn peer_certificate(&self) -> Option<CertificateInfo> {
        self.crypto.tls.peer_certificate()
    }

    /// Call by application when the peer cert has been verified
    pub fn authenticated(&mut self, status: AuthenticationStatus, now: Instant) {
        self.crypto.tls.authenticated(status);
        let res = self.handshake(now, 0, None);
        self.absorb_error(now, res);
    }

    /// Get the role of the connection.
    pub fn role(&self) -> Role {
        self.role
    }

    /// Get the state of the connection.
    pub fn state(&self) -> &State {
        &self.state
    }

    /// Get collected statistics.
    pub fn stats(&self) -> &Stats {
        &self.stats
    }

    // This function wraps a call to another function and sets the connection state
    // properly if that call fails.
    fn capture_error<T>(&mut self, now: Instant, frame_type: FrameType, res: Res<T>) -> Res<T> {
        if let Err(v) = &res {
            #[cfg(debug_assertions)]
            let msg = format!("{:?}", v);
            #[cfg(not(debug_assertions))]
            let msg = String::from("");
            if let State::Closed(err) | State::Closing { error: err, .. } = &self.state {
                qwarn!([self], "Closing again after error {:?}", err);
            } else {
                self.set_state(State::Closing {
                    error: ConnectionError::Transport(v.clone()),
                    frame_type,
                    msg,
                    timeout: self.get_closing_period_time(now),
                });
            }
        }
        res
    }

    /// For use with process_input(). Errors there can be ignored, but this
    /// needs to ensure that the state is updated.
    fn absorb_error<T>(&mut self, now: Instant, res: Res<T>) -> Option<T> {
        self.capture_error(now, 0, res).ok()
    }

    pub fn process_timer(&mut self, now: Instant) {
        if matches!(self.state(), State::Closing{..} | State::Closed{..}) {
            qinfo!("Timer fired while closing/closed");
            return;
        }

        if self.idle_timeout.expired(now) {
            qinfo!("idle timeout expired");
            self.set_state(State::Closed(ConnectionError::Transport(
                Error::IdleTimeout,
            )));
        } else {
            self.check_loss_detection_timeout(now);
        }
    }

    /// Call in to process activity on the connection. Either new packets have
    /// arrived or a timeout has expired (or both).
    pub fn process_input(&mut self, dgram: Datagram, now: Instant) {
        let res = self.input(dgram, now);
        self.absorb_error(now, res);
        self.cleanup_streams();
    }

    /// Just like above but returns frames parsed from the datagram
    #[cfg(test)]
    pub fn test_process_input(&mut self, dgram: Datagram, now: Instant) -> Vec<(Frame, Epoch)> {
        let res = self.input(dgram, now);
        let frames = self.absorb_error(now, res).unwrap_or_default();
        self.cleanup_streams();
        frames
    }

    /// Get the time that we next need to be called back, relative to `now`.
    fn next_delay(&mut self, now: Instant) -> Duration {
        self.loss_recovery_state = self.loss_recovery.get_timer();

        let mut delays = SmallVec::<[_; 4]>::new();

        if let Some(lr_time) = self.loss_recovery_state.callback_time() {
            delays.push(lr_time);
        }

        if let Some(ack_time) = self.acks.ack_time() {
            delays.push(ack_time);
        }

        if let Some(idle_time) = self.idle_timeout.as_instant() {
            delays.push(idle_time);
        }

        // Should always at least have idle timeout, once connected
        assert!(!delays.is_empty());
        let earliest = delays.into_iter().min().unwrap();

        // TODO(agrover, mt) - need to analyze and fix #47
        // rather than just clamping to zero here.
        max(now, earliest).duration_since(now)
    }

    /// Get output packets, as a result of receiving packets, or actions taken
    /// by the application.
    /// Returns datagrams to send, and how long to wait before calling again
    /// even if no incoming packets.
    pub fn process_output(&mut self, now: Instant) -> Output {
        let pkt = match &self.state {
            State::Init => {
                let res = self.client_start(now);
                self.absorb_error(now, res);
                self.output(now)
            }
            State::Closing { error, timeout, .. } => {
                if *timeout > now {
                    self.output(now)
                } else {
                    // Close timeout expired, move to Closed
                    let st = State::Closed(error.clone());
                    self.set_state(st);
                    None
                }
            }
            State::Closed(..) => None,
            _ => self.output(now),
        };

        match pkt {
            Some(pkt) => Output::Datagram(pkt),
            None => match self.state {
                State::Closed(_) => Output::None,
                State::Closing { timeout, .. } => Output::Callback(timeout - now),
                _ => Output::Callback(self.next_delay(now)),
            },
        }
    }

    /// Process input and generate output.
    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        if let Some(d) = dgram {
            self.process_input(d, now);
        }
        self.process_timer(now);
        self.process_output(now)
    }

    fn is_valid_cid(&self, cid: &ConnectionId) -> bool {
        self.valid_cids.contains(cid) || self.path.iter().any(|p| p.local_cids.contains(cid))
    }

    fn is_valid_initial(&self, hdr: &PacketHdr) -> bool {
        if let PacketType::Initial(token) = &hdr.tipe {
            // Server checks the token, so if we have one,
            // assume that the DCID is OK.
            if hdr.dcid.len() < 8 {
                if token.is_empty() {
                    qinfo!([self], "Drop Initial with short DCID");
                    false
                } else {
                    qinfo!([self], "Initial received with token, assuming OK");
                    true
                }
            } else {
                // This is the normal path. Don't log.
                true
            }
        } else {
            qdebug!([self], "Dropping non-Initial packet");
            false
        }
    }

    fn handle_retry(&mut self, scid: &ConnectionId, odcid: &ConnectionId, token: &[u8]) -> Res<()> {
        qdebug!([self], "received Retry");
        if self.retry_info.is_some() {
            qinfo!([self], "Dropping extra Retry");
            return Ok(());
        }
        if token.is_empty() {
            qinfo!([self], "Dropping Retry without a token");
            return Ok(());
        }
        match self.path.iter_mut().find(|p| p.remote_cid == *odcid) {
            None => {
                qinfo!([self], "Ignoring Retry with mismatched ODCID");
                return Ok(());
            }
            Some(path) => {
                path.remote_cid = scid.clone();
            }
        }
        qinfo!(
            [self],
            "Valid Retry received, restarting with provided token"
        );
        self.retry_info = Some(RetryInfo {
            token: token.to_vec(),
            odcid: odcid.clone(),
        });
        let lost_packets = self.loss_recovery.retry();
        self.handle_lost_packets(&lost_packets);

        // Switching crypto state here might not happen eventually.
        // https://github.com/quicwg/base-drafts/issues/2823
        self.crypto.create_initial_state(self.role, scid);
        Ok(())
    }

    fn input(&mut self, d: Datagram, now: Instant) -> Res<Vec<(Frame, Epoch)>> {
        let mut slc = &d[..];
        let mut frames = Vec::new();

        qdebug!([self], "input {}", hex(&**d));

        // Handle each packet in the datagram
        while !slc.is_empty() {
            let res = decode_packet_hdr(self.cid_manager.borrow().as_decoder(), slc);
            let mut hdr = match res {
                Ok(h) => h,
                Err(e) => {
                    qinfo!(
                        [self],
                        "Received indecipherable packet header {} {}",
                        hex(slc),
                        e
                    );
                    return Ok(frames); // Drop the remainder of the datagram.
                }
            };
            self.stats.packets_rx += 1;
            match (&hdr.tipe, &self.state, &self.role) {
                (PacketType::VN(_), State::WaitInitial, Role::Client) => {
                    self.set_state(State::Closed(ConnectionError::Transport(
                        Error::VersionNegotiation,
                    )));
                    return Err(Error::VersionNegotiation);
                }
                (PacketType::Retry { odcid, token }, State::WaitInitial, Role::Client) => {
                    self.handle_retry(hdr.scid.as_ref().unwrap(), odcid, token)?;
                    return Ok(frames);
                }
                (PacketType::VN(_), ..) | (PacketType::Retry { .. }, ..) => {
                    qwarn!("dropping {:?}", hdr.tipe);
                    return Ok(frames);
                }
                _ => {}
            };

            if let Some(version) = hdr.version {
                if version != self.version {
                    qwarn!(
                        "Dropping packet from version {:x} (self.version={:x})",
                        hdr.version.unwrap(),
                        self.version,
                    );
                    return Ok(frames);
                }
            }

            match self.state {
                State::Init => {
                    qinfo!([self], "Received message while in Init state");
                    return Ok(frames);
                }
                State::WaitInitial => {
                    qinfo!([self], "Received packet in WaitInitial");
                    if self.role == Role::Server {
                        if !self.is_valid_initial(&hdr) {
                            return Ok(frames);
                        }
                        self.crypto.create_initial_state(self.role, &hdr.dcid);
                    }
                }
                State::Handshaking | State::Connected => {
                    if !self.is_valid_cid(&hdr.dcid) {
                        qinfo!([self], "Ignoring packet with CID {:?}", hdr.dcid);
                        return Ok(frames);
                    }
                }
                State::Closing { .. } => {
                    // Don't bother processing the packet. Instead ask to get a
                    // new close frame.
                    self.flow_mgr.borrow_mut().set_need_close_frame(true);
                    return Ok(frames);
                }
                State::Closed(..) => {
                    // Do nothing.
                    return Ok(frames);
                }
            }

            qdebug!([self], "Received unverified packet {:?}", hdr);

            let body = self.decrypt_body(&mut hdr, slc);
            slc = &slc[hdr.hdr_len + hdr.body_len()..];
            if let Some(body) = body {
                // TODO(ekr@rtfm.com): Have the server blow away the initial
                // crypto state if this fails? Otherwise, we will get a panic
                // on the assert for doesn't exist.
                // OK, we have a valid packet.
                self.idle_timeout.on_packet_received(now);
                dump_packet(self, "-> RX", &hdr, &body);
                frames.extend(self.process_packet(&hdr, body, now)?);
                if matches!(self.state, State::WaitInitial) {
                    self.start_handshake(hdr, &d)?;
                }
                self.process_migrations(&d)?;
            }
        }
        Ok(frames)
    }

    fn obtain_epoch_rx_crypto_state(&mut self, epoch: Epoch) -> Option<&mut CryptoDxState> {
        if (self.state == State::Handshaking) && (epoch == 3) && (self.role() == Role::Server) {
            // We got a packet for epoch 3 but the connection is still in the Handshaking
            // state -> discharge the packet.
            // On the server side we have keys for epoch 3 before we enter the epoch,
            // but we still need to discharge the packet.
            None
        } else if epoch != 1 {
            match self
                .crypto
                .states
                .obtain(self.role, epoch, &self.crypto.tls)
            {
                Ok(CryptoState { rx, .. }) => rx.as_mut(),
                _ => None,
            }
        } else if self.role == Role::Server {
            if let ZeroRttState::AcceptedServer(rx) = &mut self.zero_rtt_state {
                return Some(rx);
            }
            None
        } else {
            None
        }
    }

    fn decrypt_body(&mut self, mut hdr: &mut PacketHdr, slc: &[u8]) -> Option<Vec<u8>> {
        // Decryption failure, or not having keys is not fatal.
        // If the state isn't available, or we can't decrypt the packet, drop
        // the rest of the datagram on the floor, but don't generate an error.
        let largest_acknowledged = self
            .loss_recovery
            .largest_acknowledged_pn(PNSpace::from(hdr.epoch));
        match self.obtain_epoch_rx_crypto_state(hdr.epoch) {
            Some(rx) => {
                let pn_decoder = PacketNumberDecoder::new(largest_acknowledged);
                decrypt_packet(rx, pn_decoder, &mut hdr, slc).ok()
            }
            _ => None,
        }
    }

    /// Ok(true) if the packet is a duplicate
    fn process_packet(
        &mut self,
        hdr: &PacketHdr,
        body: Vec<u8>,
        now: Instant,
    ) -> Res<Vec<(Frame, Epoch)>> {
        // TODO(ekr@rtfm.com): Have the server blow away the initial
        // crypto state if this fails? Otherwise, we will get a panic
        // on the assert for doesn't exist.
        // OK, we have a valid packet.

        // TODO(ekr@rtfm.com): Filter for valid for this epoch.

        let space = PNSpace::from(hdr.epoch);
        if self.acks[space].is_duplicate(hdr.pn) {
            qdebug!(
                [self],
                "Received duplicate packet epoch={} pn={}",
                hdr.epoch,
                hdr.pn
            );
            self.stats.dups_rx += 1;
            return Ok(vec![]);
        }

        let mut ack_eliciting = false;
        let mut d = Decoder::from(&body[..]);
        #[allow(unused_mut)]
        let mut frames = Vec::new();
        while d.remaining() > 0 {
            let f = decode_frame(&mut d)?;
            if cfg!(test) {
                frames.push((f.clone(), hdr.epoch));
            }
            ack_eliciting |= f.ack_eliciting();
            let t = f.get_type();
            let res = self.input_frame(hdr.epoch, f, now);
            self.capture_error(now, t, res)?;
        }
        self.acks[space].set_received(now, hdr.pn, ack_eliciting);

        Ok(frames)
    }

    fn get_zero_rtt_crypto(&mut self) -> Option<CryptoDxState> {
        match self.crypto.tls.preinfo() {
            Err(_) => None,
            Ok(preinfo) => {
                match preinfo.early_data_cipher() {
                    Some(cipher) => {
                        match self.role {
                            Role::Client => self.crypto.tls.write_secret(1).map(|ws| {
                                CryptoDxState::new(CryptoDxDirection::Write, 1, ws, cipher)
                            }),
                            Role::Server => self.crypto.tls.read_secret(1).map(|rs| {
                                CryptoDxState::new(CryptoDxDirection::Read, 1, rs, cipher)
                            }),
                        }
                    }
                    None => None,
                }
            }
        }
    }

    fn start_handshake(&mut self, hdr: PacketHdr, d: &Datagram) -> Res<()> {
        if self.role == Role::Server {
            assert!(matches!(hdr.tipe, PacketType::Initial(..)));
            // A server needs to accept the client's selected CID during the handshake.
            self.valid_cids.push(hdr.dcid.clone());
            // Install a path.
            assert!(self.path.is_none());
            let mut p = Path::new(&d, hdr.scid.unwrap());
            p.local_cids
                .push(self.cid_manager.borrow_mut().generate_cid());
            self.path = Some(p);

            // SecretAgentPreinfo::early_data() always returns false for a server,
            // but a non-zero maximum tells us if we are accepting 0-RTT.
            self.zero_rtt_state = if self.crypto.tls.preinfo()?.max_early_data() > 0 {
                match self.get_zero_rtt_crypto() {
                    Some(cs) => ZeroRttState::AcceptedServer(cs),
                    None => {
                        debug_assert!(false, "We must have zero-rtt keys.");
                        ZeroRttState::Rejected
                    }
                }
            } else {
                ZeroRttState::Rejected
            };
        } else {
            qdebug!(
                [self],
                "Changing to use Server CID={}",
                hdr.scid.as_ref().unwrap()
            );
            let p = self
                .path
                .iter_mut()
                .find(|p| p.received_on(&d))
                .expect("should have a path for sending Initial");
            p.remote_cid = hdr.scid.unwrap();
        }
        self.set_state(State::Handshaking);
        Ok(())
    }

    fn process_migrations(&self, d: &Datagram) -> Res<()> {
        if self.path.iter().any(|p| p.received_on(&d)) {
            Ok(())
        } else {
            // Right now, we don't support any form of migration.
            // So generate an error if a packet is received on a new path.
            Err(Error::InvalidMigration)
        }
    }

    fn output(&mut self, now: Instant) -> Option<Datagram> {
        let mut out = None;
        if self.path.is_some() {
            match self.output_pkt_for_path(now) {
                Ok(res) => {
                    out = res;
                }
                Err(e) => {
                    if !matches!(self.state, State::Closing{..}) {
                        // An error here causes us to transition to closing.
                        let err: Result<Option<Datagram>, Error> = Err(e);
                        self.absorb_error(now, err);
                        // Rerun to give a chance to send a CONNECTION_CLOSE.
                        out = match self.output_pkt_for_path(now) {
                            Ok(x) => x,
                            Err(e) => {
                                qwarn!([self], "two output_path errors in a row: {:?}", e);
                                None
                            }
                        };
                    }
                }
            };
        }
        out
    }

    #[allow(clippy::cognitive_complexity)]
    #[allow(clippy::useless_let_if_seq)]
    /// Build a datagram, possibly from multiple packets (for different PN
    /// spaces) and each containing 1+ frames.
    fn output_pkt_for_path(&mut self, now: Instant) -> Res<Option<Datagram>> {
        let mut out_bytes = Vec::new();
        let mut needs_padding = false;
        let mut close_sent = false;
        let path = self
            .path
            .take()
            .expect("we know we have a path because calling fn checked");

        // Frames for different epochs must go in different packets, but then these
        // packets can go in a single datagram
        for epoch in 0..NUM_EPOCHS {
            let space = PNSpace::from(epoch);
            let mut encoder = Encoder::default();
            let mut tokens = Vec::new();

            // Ensure we have tx crypto state for this epoch, or skip it.
            let tx = if epoch == 1 && self.role == Role::Server {
                continue;
            } else if epoch == 1 {
                match &mut self.zero_rtt_state {
                    ZeroRttState::Sending(tx) => tx,
                    _ => continue,
                }
            } else {
                match self
                    .crypto
                    .states
                    .obtain(self.role, epoch, &self.crypto.tls)
                {
                    Ok(CryptoState { tx: Some(tx), .. }) => tx,
                    _ => continue,
                }
            };

            let hdr = PacketHdr::new(
                0,
                match epoch {
                    0 => {
                        let token = match &self.retry_info {
                            Some(v) => v.token.clone(),
                            _ => Vec::new(),
                        };
                        PacketType::Initial(token)
                    }
                    1 => PacketType::ZeroRTT,
                    2 => PacketType::Handshake,
                    3 => PacketType::Short,
                    _ => unimplemented!(), // TODO(ekr@rtfm.com): Key Update.
                },
                Some(self.version),
                path.remote_cid.clone(),
                path.local_cids.first().cloned(),
                self.loss_recovery.next_pn(space),
                epoch,
            );

            let mut ack_eliciting = false;
            let mut has_padding = false;
            let cong_avail = match self.tx_mode {
                TxMode::Normal => usize::try_from(self.loss_recovery.cwnd_avail()).unwrap(),
                TxMode::Pto => path.mtu(), // send one packet
            };
            let tx_mode = self.tx_mode;

            match &self.state {
                State::Init | State::WaitInitial | State::Handshaking | State::Connected => {
                    loop {
                        let used =
                            out_bytes.len() + encoder.len() + hdr.overhead(&tx.aead, path.mtu());
                        let remaining = min(
                            path.mtu().saturating_sub(used),
                            cong_avail.saturating_sub(used),
                        );
                        if remaining < 2 {
                            // All useful frames are at least 2 bytes.
                            break;
                        }

                        // Try to get a frame from frame sources
                        let mut frame = None;
                        if self.tx_mode == TxMode::Normal {
                            frame = self.acks.get_frame(now, epoch);
                        }
                        if frame.is_none() {
                            frame = self.crypto.streams.get_frame(epoch, tx_mode, remaining)
                        }
                        if frame.is_none() && self.tx_mode == TxMode::Normal {
                            frame = self.flow_mgr.borrow_mut().get_frame(epoch, remaining);
                        }
                        if frame.is_none() {
                            frame = self.send_streams.get_frame(epoch, tx_mode, remaining)
                        }
                        if frame.is_none() && self.tx_mode == TxMode::Pto {
                            frame = Some((Frame::Ping, None));
                        }

                        if let Some((frame, token)) = frame {
                            ack_eliciting |= frame.ack_eliciting();
                            if let Frame::Padding = frame {
                                has_padding |= true;
                            }
                            frame.marshal(&mut encoder);
                            if let Some(t) = token {
                                tokens.push(t);
                            }

                            // Pto only ever sends one frame, but it ALWAYS
                            // sends one
                            if self.tx_mode == TxMode::Pto {
                                break;
                            }
                        } else {
                            // No more frames to send.
                            assert_eq!(self.tx_mode, TxMode::Normal);
                            break;
                        }
                    }
                }
                State::Closing {
                    error,
                    frame_type,
                    msg,
                    ..
                } => {
                    if self.flow_mgr.borrow().need_close_frame() {
                        // ConnectionClose frame not allowed for 0RTT
                        if epoch == 1 {
                            continue;
                        }
                        // ConnectionError::Application only allowed at 1RTT
                        if epoch != 3 && matches!(error, ConnectionError::Application(_)) {
                            continue;
                        }
                        let frame = Frame::ConnectionClose {
                            error_code: error.clone().into(),
                            frame_type: *frame_type,
                            reason_phrase: Vec::from(msg.clone()),
                        };
                        frame.marshal(&mut encoder);
                        close_sent = true;
                    }
                }
                State::Closed { .. } => unimplemented!(),
            }

            assert!(encoder.len() <= path.mtu());
            if encoder.len() == 0 {
                continue;
            }

            qdebug!("Need to send a packet");
            match epoch {
                // Packets containing Initial packets need padding.
                0 => needs_padding = true,
                1 => (),
                // ...unless they include higher epochs.
                _ => needs_padding = false,
            }

            self.stats.packets_tx += 1;
            self.loss_recovery.inc_pn(space);

            let mut packet = encode_packet(tx, &hdr, &encoder);

            if self.tx_mode != TxMode::Pto && ack_eliciting {
                self.idle_timeout.on_packet_sent(now);
            }

            let in_flight = match self.tx_mode {
                TxMode::Pto => false,
                TxMode::Normal => ack_eliciting || has_padding,
            };

            self.loss_recovery.on_packet_sent(
                space,
                hdr.pn,
                SentPacket::new(now, ack_eliciting, tokens, packet.len(), in_flight),
            );

            dump_packet(self, "TX ->", &hdr, &encoder);

            out_bytes.append(&mut packet);
        }

        if close_sent {
            self.flow_mgr.borrow_mut().set_need_close_frame(false);
        }

        // Sent a probe pkt. Another timeout will re-engage ProbeTimeout mode,
        // but otherwise return to honoring CC.
        if self.tx_mode == TxMode::Pto {
            self.tx_mode = TxMode::Normal;
        }

        if out_bytes.is_empty() {
            assert!(self.tx_mode != TxMode::Pto);
            self.path = Some(path);
            Ok(None)
        } else {
            // Pad Initial packets sent by the client to mtu bytes.
            if self.role == Role::Client && needs_padding {
                qdebug!([self], "pad Initial to max_datagram_size");
                out_bytes.resize(path.mtu(), 0);
            }
            let ret = Ok(Some(Datagram::new(path.local, path.remote, out_bytes)));
            self.path = Some(path);
            ret
        }
    }

    fn client_start(&mut self, now: Instant) -> Res<()> {
        qinfo!([self], "client_start");
        self.handshake(now, 0, None)?;
        self.set_state(State::WaitInitial);
        if self.crypto.tls.preinfo()?.early_data() {
            qdebug!([self], "Enabling 0-RTT");
            self.zero_rtt_state = match self.get_zero_rtt_crypto() {
                Some(cs) => ZeroRttState::Sending(cs),
                None => {
                    debug_assert!(false, "We must have zero-rtt keys.");
                    ZeroRttState::Rejected
                }
            };
        }
        Ok(())
    }

    fn get_closing_period_time(&self, now: Instant) -> Instant {
        // Spec says close time should be at least PTO times 3.
        now + (self.loss_recovery.pto() * 3)
    }

    /// Close the connection.
    pub fn close(&mut self, now: Instant, error: AppError, msg: &str) {
        self.set_state(State::Closing {
            error: ConnectionError::Application(error),
            frame_type: 0,
            msg: msg.into(),
            timeout: self.get_closing_period_time(now),
        });
    }

    fn set_initial_limits(&mut self) {
        let tps = self.tps.borrow();
        let remote = tps.remote();
        self.indexes.remote_max_stream_bidi =
            StreamIndex::new(remote.get_integer(tp_constants::INITIAL_MAX_STREAMS_BIDI));
        self.indexes.remote_max_stream_uni =
            StreamIndex::new(remote.get_integer(tp_constants::INITIAL_MAX_STREAMS_UNI));
        self.flow_mgr
            .borrow_mut()
            .conn_increase_max_credit(remote.get_integer(tp_constants::INITIAL_MAX_DATA));
    }

    fn validate_odcid(&self) -> Res<()> {
        if let Some(info) = &self.retry_info {
            let tph = self.tps.borrow();
            let tp = tph.remote().get_bytes(tp_constants::ORIGINAL_CONNECTION_ID);
            if let Some(odcid_tp) = tp {
                if odcid_tp[..] == info.odcid[..] {
                    Ok(())
                } else {
                    Err(Error::InvalidRetry)
                }
            } else {
                Err(Error::InvalidRetry)
            }
        } else {
            Ok(())
        }
    }

    fn handshake(&mut self, now: Instant, epoch: u16, data: Option<&[u8]>) -> Res<()> {
        qdebug!("Handshake epoch={} data={:0x?}", epoch, data);

        let rec = data
            .map(|d| {
                qdebug!([self], "Handshake received {:0x?} ", d);
                Some(Record {
                    ct: 22, // TODO(ekr@rtfm.com): Symbolic constants for CT. This is handshake.
                    epoch,
                    data: d.to_vec(),
                })
            })
            .unwrap_or(None);

        match self.crypto.tls.handshake_raw(now, rec) {
            Err(e) => {
                qwarn!([self], "Handshake failed");
                return Err(match self.crypto.tls.alert() {
                    Some(a) => Error::CryptoAlert(*a),
                    _ => Error::CryptoError(e),
                });
            }
            Ok(msgs) => self.crypto.buffer_records(msgs),
        }

        if *self.crypto.tls.state() == HandshakeState::AuthenticationPending {
            self.events.authentication_needed();
        } else if matches!(self.crypto.tls.state(), HandshakeState::Complete(_)) {
            qinfo!([self], "TLS handshake completed");

            if self.crypto.tls.info().map(SecretAgentInfo::alpn).is_none() {
                qwarn!([self], "No ALPN. Closing connection.");
                // 120 = no_application_protocol
                return Err(Error::CryptoAlert(120));
            }

            self.validate_odcid()?;
            self.set_state(State::Connected);
            self.set_initial_limits();
        }
        Ok(())
    }

    fn handle_max_data(&mut self, maximum_data: u64) {
        let conn_was_blocked = self.flow_mgr.borrow().conn_credit_avail() == 0;
        let conn_credit_increased = self
            .flow_mgr
            .borrow_mut()
            .conn_increase_max_credit(maximum_data);

        if conn_was_blocked && conn_credit_increased {
            for (id, ss) in &mut self.send_streams {
                if ss.avail() > 0 {
                    // These may not actually all be writable if one
                    // uses up all the conn credit. Not our fault.
                    self.events.send_stream_writable(*id)
                }
            }
        }
    }

    fn input_frame(&mut self, epoch: Epoch, frame: Frame, now: Instant) -> Res<()> {
        if !frame.is_allowed(epoch) {
            return Err(Error::ProtocolViolation);
        }
        match frame {
            Frame::Padding => {
                // Ignore
            }
            Frame::Ping => {
                // Ack elicited with no further handling needed
            }
            Frame::Ack {
                largest_acknowledged,
                ack_delay,
                first_ack_range,
                ack_ranges,
            } => {
                self.handle_ack(
                    epoch,
                    largest_acknowledged,
                    ack_delay,
                    first_ack_range,
                    ack_ranges,
                    now,
                )?;
            }
            Frame::ResetStream {
                stream_id,
                application_error_code,
                ..
            } => {
                // TODO(agrover@mozilla.com): use final_size for connection MaxData calc
                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    rs.reset(application_error_code);
                }
            }
            Frame::StopSending {
                stream_id,
                application_error_code,
            } => {
                self.events
                    .send_stream_stop_sending(stream_id, application_error_code);
                if let (Some(ss), _) = self.obtain_stream(stream_id)? {
                    ss.reset(application_error_code);
                }
            }
            Frame::Crypto { offset, data } => {
                qdebug!(
                    [self],
                    "Crypto frame on epoch={} offset={}, data={:0x?}",
                    epoch,
                    offset,
                    &data
                );
                self.crypto.streams.inbound_frame(epoch, offset, data)?;
                if self.crypto.streams.data_ready(epoch) {
                    let mut buf = Vec::new();
                    let read = self.crypto.streams.read_to_end(epoch, &mut buf)?;
                    qdebug!("Read {} bytes", read);
                    self.handshake(now, epoch, Some(&buf))?;
                }
            }
            Frame::NewToken { token } => self.token = Some(token),
            Frame::Stream {
                fin,
                stream_id,
                offset,
                data,
                ..
            } => {
                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    rs.inbound_stream_frame(fin, offset, data)?;
                }
            }
            Frame::MaxData { maximum_data } => self.handle_max_data(maximum_data),
            Frame::MaxStreamData {
                stream_id,
                maximum_stream_data,
            } => {
                if let (Some(ss), _) = self.obtain_stream(stream_id)? {
                    ss.set_max_stream_data(maximum_stream_data);
                }
            }
            Frame::MaxStreams {
                stream_type,
                maximum_streams,
            } => {
                let remote_max = match stream_type {
                    StreamType::BiDi => &mut self.indexes.remote_max_stream_bidi,
                    StreamType::UniDi => &mut self.indexes.remote_max_stream_uni,
                };

                if maximum_streams > *remote_max {
                    *remote_max = maximum_streams;
                    self.events.send_stream_creatable(stream_type);
                }
            }
            Frame::DataBlocked { data_limit } => {
                // Should never happen since we set data limit to max
                qwarn!(
                    [self],
                    "Received DataBlocked with data limit {}",
                    data_limit
                );
                // But if it does, open it up all the way
                self.flow_mgr.borrow_mut().max_data(LOCAL_MAX_DATA);
            }
            Frame::StreamDataBlocked { stream_id, .. } => {
                // TODO(agrover@mozilla.com): how should we be using
                // currently-unused stream_data_limit?

                // Terminate connection with STREAM_STATE_ERROR if send-only
                // stream (-transport 19.13)
                if stream_id.is_send_only(self.role()) {
                    return Err(Error::StreamStateError);
                }

                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    rs.maybe_send_flowc_update();
                }
            }
            Frame::StreamsBlocked { stream_type, .. } => {
                let local_max = match stream_type {
                    StreamType::BiDi => &mut self.indexes.local_max_stream_bidi,
                    StreamType::UniDi => &mut self.indexes.local_max_stream_uni,
                };

                self.flow_mgr
                    .borrow_mut()
                    .max_streams(*local_max, stream_type)
            }
            Frame::NewConnectionId {
                sequence_number,
                connection_id,
                stateless_reset_token,
                ..
            } => {
                self.connection_ids
                    .insert(sequence_number, (connection_id, stateless_reset_token));
            }
            Frame::RetireConnectionId { sequence_number } => {
                self.connection_ids.remove(&sequence_number);
            }
            Frame::PathChallenge { data } => self.flow_mgr.borrow_mut().path_response(data),
            Frame::PathResponse { .. } => {
                // Should never see this, we don't support migration atm and
                // do not send path challenges
                qwarn!([self], "Received Path Response");
            }
            Frame::ConnectionClose {
                error_code,
                frame_type,
                reason_phrase,
            } => {
                let reason_phrase = String::from_utf8_lossy(&reason_phrase);
                qinfo!(
                    [self],
                    "ConnectionClose received. Error code: {:?} frame type {:x} reason {}",
                    error_code,
                    frame_type,
                    reason_phrase
                );
                self.set_state(State::Closed(error_code.into()));
            }
        };

        Ok(())
    }

    fn handle_lost_packets(&mut self, lost_packets: &[SentPacket]) {
        for lost in lost_packets {
            for token in &lost.tokens {
                qdebug!([self], "Lost: {:?}", token);
                match token {
                    RecoveryToken::Ack(_) => {}
                    RecoveryToken::Stream(st) => self.send_streams.lost(&st),
                    RecoveryToken::Crypto(ct) => self.crypto.lost(&ct),
                    RecoveryToken::Flow(ft) => self.flow_mgr.borrow_mut().lost(
                        &ft,
                        &mut self.send_streams,
                        &mut self.recv_streams,
                        &mut self.indexes,
                    ),
                }
            }
        }
    }

    fn handle_ack(
        &mut self,
        epoch: Epoch,
        largest_acknowledged: u64,
        ack_delay: u64,
        first_ack_range: u64,
        ack_ranges: Vec<AckRange>,
        now: Instant,
    ) -> Res<()> {
        qinfo!(
            [self],
            "Rx ACK epoch={}, largest_acked={}, first_ack_range={}, ranges={:?}",
            epoch,
            largest_acknowledged,
            first_ack_range,
            ack_ranges
        );

        let acked_ranges =
            Frame::decode_ack_frame(largest_acknowledged, first_ack_range, ack_ranges)?;
        let (acked_packets, lost_packets) = self.loss_recovery.on_ack_received(
            PNSpace::from(epoch),
            largest_acknowledged,
            acked_ranges,
            Duration::from_millis(ack_delay),
            now,
        );
        for acked in acked_packets {
            for token in acked.tokens {
                match token {
                    RecoveryToken::Ack(at) => self.acks.acked(&at),
                    RecoveryToken::Stream(st) => self.send_streams.acked(&st),
                    RecoveryToken::Crypto(ct) => self.crypto.acked(ct),
                    RecoveryToken::Flow(ft) => {
                        self.flow_mgr.borrow_mut().acked(ft, &mut self.send_streams)
                    }
                }
            }
        }
        self.handle_lost_packets(&lost_packets);
        Ok(())
    }

    /// When the server rejects 0-RTT we need to drop a bunch of stuff.
    fn client_0rtt_rejected(&mut self) {
        if !matches!(self.zero_rtt_state, ZeroRttState::Sending(..)) {
            return;
        }

        // Tell 0-RTT packets that they were "lost".
        // TODO(mt) remove these from "bytes in flight" when we
        // have a congestion controller.
        for dropped in self.loss_recovery.drop_0rtt() {
            for token in dropped.tokens {
                match token {
                    RecoveryToken::Ack(_) => {}
                    RecoveryToken::Stream(st) => self.send_streams.lost(&st),
                    RecoveryToken::Crypto(ct) => self.crypto.lost(&ct),
                    RecoveryToken::Flow(ft) => self.flow_mgr.borrow_mut().lost(
                        &ft,
                        &mut self.send_streams,
                        &mut self.recv_streams,
                        &mut self.indexes,
                    ),
                }
            }
        }
        self.send_streams.clear();
        self.recv_streams.clear();
        self.indexes = StreamIndexes::new();
        self.events.client_0rtt_rejected();
    }

    fn set_state(&mut self, state: State) {
        if state > self.state {
            qinfo!([self], "State change from {:?} -> {:?}", self.state, state);
            self.state = state.clone();
            match &self.state {
                State::Connected => {
                    if self.role == Role::Server {
                        // Remove the randomized client CID from the list of acceptable CIDs.
                        assert_eq!(1, self.valid_cids.len());
                        self.valid_cids.clear();
                    } else {
                        self.zero_rtt_state =
                            if self.crypto.tls.info().unwrap().early_data_accepted() {
                                ZeroRttState::AcceptedClient
                            } else {
                                self.client_0rtt_rejected();
                                ZeroRttState::Rejected
                            }
                    }
                }
                State::Closing { .. } => {
                    self.send_streams.clear();
                    self.recv_streams.clear();
                    self.flow_mgr.borrow_mut().set_need_close_frame(true);
                }
                State::Closed(..) => {
                    // Equivalent to spec's "draining" state -- never send anything.
                    self.send_streams.clear();
                    self.recv_streams.clear();
                }
                _ => {}
            }
            self.events.connection_state_change(state);
        } else {
            assert_eq!(state, self.state);
        }
    }

    fn cleanup_streams(&mut self) {
        let recv_to_remove = self
            .recv_streams
            .iter()
            .filter(|(_, stream)| stream.is_terminal())
            .map(|(id, _)| *id)
            .collect::<Vec<_>>();

        let mut removed_bidi = 0;
        let mut removed_uni = 0;
        for id in &recv_to_remove {
            self.recv_streams.remove(&id);
            if id.is_remote_initiated(self.role()) {
                if id.is_bidi() {
                    removed_bidi += 1;
                } else {
                    removed_uni += 1;
                }
            }
        }

        // Send max_streams updates if we removed remote-initiated recv streams.
        if removed_bidi > 0 {
            self.indexes.local_max_stream_bidi += removed_bidi;
            self.flow_mgr
                .borrow_mut()
                .max_streams(self.indexes.local_max_stream_bidi, StreamType::BiDi)
        }
        if removed_uni > 0 {
            self.indexes.local_max_stream_uni += removed_uni;
            self.flow_mgr
                .borrow_mut()
                .max_streams(self.indexes.local_max_stream_uni, StreamType::UniDi)
        }

        self.send_streams.clear_terminal();
    }

    /// Get or make a stream, and implicitly open additional streams as
    /// indicated by its stream id.
    fn obtain_stream(
        &mut self,
        stream_id: StreamId,
    ) -> Res<(Option<&mut SendStream>, Option<&mut RecvStream>)> {
        match (&self.state, &self.zero_rtt_state) {
            (State::Connected, _) | (State::Handshaking, ZeroRttState::AcceptedServer(..)) => (),
            _ => return Err(Error::ConnectionState),
        }

        // May require creating new stream(s)
        if stream_id.is_remote_initiated(self.role()) {
            let next_stream_idx = if stream_id.is_bidi() {
                &mut self.indexes.local_next_stream_bidi
            } else {
                &mut self.indexes.local_next_stream_uni
            };
            let stream_idx: StreamIndex = stream_id.into();

            if stream_idx >= *next_stream_idx {
                let recv_initial_max_stream_data = if stream_id.is_bidi() {
                    if stream_idx > self.indexes.local_max_stream_bidi {
                        qwarn!(
                            [self],
                            "remote bidi stream create blocked, next={:?} max={:?}",
                            stream_idx,
                            self.indexes.local_max_stream_bidi
                        );
                        return Err(Error::StreamLimitError);
                    }
                    // From the local perspective, this is a remote- originated BiDi stream. From
                    // the remote perspective, this is a local-originated BiDi stream. Therefore,
                    // look at the local transport parameters for the
                    // INITIAL_MAX_STREAM_DATA_BIDI_REMOTE value to decide how much this endpoint
                    // will allow its peer to send.
                    self.tps
                        .borrow()
                        .local
                        .get_integer(tp_constants::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE)
                } else {
                    if stream_idx > self.indexes.local_max_stream_uni {
                        qwarn!(
                            [self],
                            "remote uni stream create blocked, next={:?} max={:?}",
                            stream_idx,
                            self.indexes.local_max_stream_uni
                        );
                        return Err(Error::StreamLimitError);
                    }
                    self.tps
                        .borrow()
                        .local
                        .get_integer(tp_constants::INITIAL_MAX_STREAM_DATA_UNI)
                };

                loop {
                    let next_stream_id =
                        next_stream_idx.to_stream_id(stream_id.stream_type(), stream_id.role());
                    self.recv_streams.insert(
                        next_stream_id,
                        RecvStream::new(
                            next_stream_id,
                            recv_initial_max_stream_data,
                            self.flow_mgr.clone(),
                            self.events.clone(),
                        ),
                    );

                    if next_stream_id.is_uni() {
                        self.events.new_stream(next_stream_id);
                    } else {
                        // From the local perspective, this is a remote- originated BiDi stream.
                        // From the remote perspective, this is a local-originated BiDi stream.
                        // Therefore, look at the remote's transport parameters for the
                        // INITIAL_MAX_STREAM_DATA_BIDI_LOCAL value to decide how much this endpoint
                        // is allowed to send its peer.
                        let send_initial_max_stream_data = self
                            .tps
                            .borrow()
                            .remote()
                            .get_integer(tp_constants::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL);
                        self.send_streams.insert(
                            next_stream_id,
                            SendStream::new(
                                next_stream_id,
                                send_initial_max_stream_data,
                                self.flow_mgr.clone(),
                                self.events.clone(),
                            ),
                        );
                        self.events.new_stream(next_stream_id);
                    }

                    *next_stream_idx += 1;
                    if *next_stream_idx > stream_idx {
                        break;
                    }
                }
            }
        }

        Ok((
            self.send_streams.get_mut(stream_id).ok(),
            self.recv_streams.get_mut(&stream_id),
        ))
    }

    /// Create a stream.
    // Returns new stream id
    pub fn stream_create(&mut self, st: StreamType) -> Res<u64> {
        // Can't make streams while closing, otherwise rely on the stream limits.
        match self.state {
            State::Closing { .. } | State::Closed { .. } => return Err(Error::ConnectionState),
            State::WaitInitial | State::Handshaking => {
                if !matches!(self.zero_rtt_state, ZeroRttState::Sending(..)) {
                    return Err(Error::ConnectionState);
                }
            }
            _ => (),
        }
        if self.tps.borrow().remote.is_none() && self.tps.borrow().remote_0rtt.is_none() {
            return Err(Error::ConnectionState);
        }

        Ok(match st {
            StreamType::UniDi => {
                if self.indexes.remote_next_stream_uni >= self.indexes.remote_max_stream_uni {
                    self.flow_mgr
                        .borrow_mut()
                        .streams_blocked(self.indexes.remote_max_stream_uni, StreamType::UniDi);
                    qwarn!(
                        [self],
                        "local uni stream create blocked, next={:?} max={:?}",
                        self.indexes.remote_next_stream_uni,
                        self.indexes.remote_max_stream_uni
                    );
                    return Err(Error::StreamLimitError);
                }
                let new_id = self
                    .indexes
                    .remote_next_stream_uni
                    .to_stream_id(StreamType::UniDi, self.role);
                self.indexes.remote_next_stream_uni += 1;
                let initial_max_stream_data = self
                    .tps
                    .borrow()
                    .remote()
                    .get_integer(tp_constants::INITIAL_MAX_STREAM_DATA_UNI);

                self.send_streams.insert(
                    new_id,
                    SendStream::new(
                        new_id,
                        initial_max_stream_data,
                        self.flow_mgr.clone(),
                        self.events.clone(),
                    ),
                );
                new_id.as_u64()
            }
            StreamType::BiDi => {
                if self.indexes.remote_next_stream_bidi >= self.indexes.remote_max_stream_bidi {
                    self.flow_mgr
                        .borrow_mut()
                        .streams_blocked(self.indexes.remote_max_stream_bidi, StreamType::BiDi);
                    qwarn!(
                        [self],
                        "local bidi stream create blocked, next={:?} max={:?}",
                        self.indexes.remote_next_stream_bidi,
                        self.indexes.remote_max_stream_bidi
                    );
                    return Err(Error::StreamLimitError);
                }
                let new_id = self
                    .indexes
                    .remote_next_stream_bidi
                    .to_stream_id(StreamType::BiDi, self.role);
                self.indexes.remote_next_stream_bidi += 1;
                // From the local perspective, this is a local- originated BiDi stream. From the
                // remote perspective, this is a remote-originated BiDi stream. Therefore, look at
                // the remote transport parameters for the INITIAL_MAX_STREAM_DATA_BIDI_REMOTE value
                // to decide how much this endpoint is allowed to send its peer.
                let send_initial_max_stream_data = self
                    .tps
                    .borrow()
                    .remote()
                    .get_integer(tp_constants::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE);

                self.send_streams.insert(
                    new_id,
                    SendStream::new(
                        new_id,
                        send_initial_max_stream_data,
                        self.flow_mgr.clone(),
                        self.events.clone(),
                    ),
                );
                // From the local perspective, this is a local- originated BiDi stream. From the
                // remote perspective, this is a remote-originated BiDi stream. Therefore, look at
                // the local transport parameters for the INITIAL_MAX_STREAM_DATA_BIDI_LOCAL value
                // to decide how much this endpoint will allow its peer to send.
                let recv_initial_max_stream_data = self
                    .tps
                    .borrow()
                    .local
                    .get_integer(tp_constants::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL);

                self.recv_streams.insert(
                    new_id,
                    RecvStream::new(
                        new_id,
                        recv_initial_max_stream_data,
                        self.flow_mgr.clone(),
                        self.events.clone(),
                    ),
                );
                new_id.as_u64()
            }
        })
    }

    /// Send data on a stream.
    /// Returns how many bytes were successfully sent. Could be less
    /// than total, based on receiver credit space available, etc.
    pub fn stream_send(&mut self, stream_id: u64, data: &[u8]) -> Res<usize> {
        self.send_streams.get_mut(stream_id.into())?.send(data)
    }

    /// Bytes that stream_send() is guaranteed to accept for sending.
    /// i.e. that will not be blocked by flow credits or send buffer max
    /// capacity.
    pub fn stream_avail_send_space(&self, stream_id: u64) -> Res<u64> {
        Ok(self.send_streams.get(stream_id.into())?.avail())
    }

    /// Close the stream. Enqueued data will be sent.
    pub fn stream_close_send(&mut self, stream_id: u64) -> Res<()> {
        self.send_streams.get_mut(stream_id.into())?.close();
        Ok(())
    }

    /// Abandon transmission of in-flight and future stream data.
    pub fn stream_reset_send(&mut self, stream_id: u64, err: AppError) -> Res<()> {
        self.send_streams.get_mut(stream_id.into())?.reset(err);
        Ok(())
    }

    /// Read buffered data from stream. bool says whether read bytes includes
    /// the final data on stream.
    pub fn stream_recv(&mut self, stream_id: u64, data: &mut [u8]) -> Res<(usize, bool)> {
        let stream = self
            .recv_streams
            .get_mut(&stream_id.into())
            .ok_or_else(|| Error::InvalidStreamId)?;

        let rb = stream.read(data)?;
        Ok((rb.0 as usize, rb.1))
    }

    /// Application is no longer interested in this stream.
    pub fn stream_stop_sending(&mut self, stream_id: u64, err: AppError) -> Res<()> {
        let stream = self
            .recv_streams
            .get_mut(&stream_id.into())
            .ok_or_else(|| Error::InvalidStreamId)?;

        stream.stop_sending(err);
        Ok(())
    }

    /// Get all current events. Best used just in debug/testing code, use
    /// next_event() instead.
    pub fn events(&mut self) -> impl Iterator<Item = ConnectionEvent> {
        self.events.events()
    }

    /// Return true if there are outstanding events.
    pub fn has_events(&self) -> bool {
        self.events.has_events()
    }

    /// Get events that indicate state changes on the connection. This method
    /// correctly handles cases where handling one event can obsolete
    /// previously-queued events, or cause new events to be generated.
    pub fn next_event(&mut self) -> Option<ConnectionEvent> {
        self.events.next_event()
    }

    fn check_loss_detection_timeout(&mut self, now: Instant) {
        qdebug!([self], "check_loss_timeouts");

        if matches!(self.loss_recovery_state.mode(), LossRecoveryMode::None) {
            // LR not the active timer
            return;
        }

        if self.loss_recovery_state.callback_time() > Some(now) {
            // LR timer, but hasn't expired.
            return;
        }

        // Timer expired and LR was active timer.
        match &mut self.loss_recovery_state.mode() {
            LossRecoveryMode::None => unreachable!(),
            LossRecoveryMode::LostPackets => {
                // Time threshold loss detection
                let (pn_space, _) = self
                    .loss_recovery
                    .get_earliest_loss_time()
                    .expect("must be sent packets if in LostPackets mode");
                let packets = self.loss_recovery.detect_lost_packets(pn_space, now);

                qinfo!("lost packets: {}", packets.len());
                for lost in packets {
                    for token in lost.tokens {
                        match token {
                            RecoveryToken::Ack(_) => {} // Do nothing
                            RecoveryToken::Stream(st) => self.send_streams.lost(&st),
                            RecoveryToken::Crypto(ct) => self.crypto.lost(&ct),
                            RecoveryToken::Flow(ft) => self.flow_mgr.borrow_mut().lost(
                                &ft,
                                &mut self.send_streams,
                                &mut self.recv_streams,
                                &mut self.indexes,
                            ),
                        }
                    }
                }
            }
            LossRecoveryMode::PTO => {
                qinfo!(
                    [self],
                    "check_loss_detection_timeout -send_one_or_two_packets"
                );
                self.loss_recovery.increment_pto_count();
                // TODO
                // if (has unacknowledged crypto data):
                //   RetransmitUnackedCryptoData()
                // else if (endpoint is client without 1-RTT keys):
                //   // Client sends an anti-deadlock packet: Initial is padded
                //   // to earn more anti-amplification credit,
                //   // a Handshake packet proves address ownership.
                //   if (has Handshake keys):
                //      SendOneHandshakePacket()
                //    else:
                //      SendOnePaddedInitialPacket()
                // TODO
                // SendOneOrTwoPackets()
                // PTO. Send new data if available, else retransmit old data.
                // If neither is available, send a single PING frame.

                // TODO(agrover): determine if new data is available and if so
                // send 2 packets worth
                // TODO(agrover): else determine if old data is available and if
                // so send 2 packets worth
                // TODO(agrover): else send a single PING frame

                self.tx_mode = TxMode::Pto;
            }
        }
    }
}

impl ::std::fmt::Display for Connection {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{:?} {:p}", self.role, self as *const Connection)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::frame::{CloseError, StreamType};
    use crate::recovery::{INITIAL_CWND_PKTS, MAX_DATAGRAM_SIZE, MIN_CONG_WINDOW};
    use neqo_common::matches;
    use std::mem;
    use test_fixture::{self, assertions, fixture_init, loopback, now};

    // This is fabulous: because test_fixture uses the public API for Connection,
    // it gets a different type to the ones that are referenced via super::*.
    // Thus, this code can't use default_client() and default_server() from
    // test_fixture because they produce different types.
    //
    // These are a direct copy of those functions.
    pub fn default_client() -> Connection {
        fixture_init();
        let mut c = Connection::new_client(
            test_fixture::DEFAULT_SERVER_NAME,
            test_fixture::DEFAULT_ALPN,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(3))),
            loopback(),
            loopback(),
        )
        .expect("create a default client");

        // limit dcid to a constant value to make testing easier
        let mut modded_path = c.path.take().unwrap();
        modded_path.remote_cid.0.truncate(8);
        let modded_dcid = modded_path.remote_cid.0.clone();
        assert_eq!(modded_dcid.len(), 8);
        c.path = Some(modded_path);
        c.crypto.create_initial_state(Role::Client, &modded_dcid);
        c
    }
    pub fn default_server() -> Connection {
        fixture_init();
        Connection::new_server(
            test_fixture::DEFAULT_KEYS,
            test_fixture::DEFAULT_ALPN,
            &test_fixture::anti_replay(),
            Rc::new(RefCell::new(FixedConnectionIdManager::new(5))),
        )
        .expect("create a default server")
    }

    /// If state is AuthenticationNeeded call authenticated(). This function will
    /// consume all outstanding events on the connection.
    pub fn maybe_authenticate(conn: &mut Connection) -> bool {
        let authentication_needed = |e| matches!(e, ConnectionEvent::AuthenticationNeeded);
        if conn.events().any(authentication_needed) {
            conn.authenticated(AuthenticationStatus::Ok, now());
            return true;
        }
        false
    }

    #[test]
    fn bidi_stream_properties() {
        let id1 = StreamIndex::new(4).to_stream_id(StreamType::BiDi, Role::Client);
        assert_eq!(id1.is_bidi(), true);
        assert_eq!(id1.is_uni(), false);
        assert_eq!(id1.is_client_initiated(), true);
        assert_eq!(id1.is_server_initiated(), false);
        assert_eq!(id1.role(), Role::Client);
        assert_eq!(id1.is_self_initiated(Role::Client), true);
        assert_eq!(id1.is_self_initiated(Role::Server), false);
        assert_eq!(id1.is_remote_initiated(Role::Client), false);
        assert_eq!(id1.is_remote_initiated(Role::Server), true);
        assert_eq!(id1.is_send_only(Role::Server), false);
        assert_eq!(id1.is_send_only(Role::Client), false);
        assert_eq!(id1.is_recv_only(Role::Server), false);
        assert_eq!(id1.is_recv_only(Role::Client), false);
        assert_eq!(id1.as_u64(), 16);
    }

    #[test]
    fn uni_stream_properties() {
        let id2 = StreamIndex::new(8).to_stream_id(StreamType::UniDi, Role::Server);
        assert_eq!(id2.is_bidi(), false);
        assert_eq!(id2.is_uni(), true);
        assert_eq!(id2.is_client_initiated(), false);
        assert_eq!(id2.is_server_initiated(), true);
        assert_eq!(id2.role(), Role::Server);
        assert_eq!(id2.is_self_initiated(Role::Client), false);
        assert_eq!(id2.is_self_initiated(Role::Server), true);
        assert_eq!(id2.is_remote_initiated(Role::Client), true);
        assert_eq!(id2.is_remote_initiated(Role::Server), false);
        assert_eq!(id2.is_send_only(Role::Server), true);
        assert_eq!(id2.is_send_only(Role::Client), false);
        assert_eq!(id2.is_recv_only(Role::Server), false);
        assert_eq!(id2.is_recv_only(Role::Client), true);
        assert_eq!(id2.as_u64(), 35);
    }

    #[test]
    fn test_conn_stream_create() {
        let mut client = default_client();
        let out = client.process(None, now());
        let mut server = default_server();
        let out = server.process(out.dgram(), now());

        let out = client.process(out.dgram(), now());
        let _ = server.process(out.dgram(), now());
        assert!(maybe_authenticate(&mut client));
        let out = client.process(None, now());

        // client now in State::Connected
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 6);
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 4);

        let _ = server.process(out.dgram(), now());
        // server now in State::Connected
        assert_eq!(server.stream_create(StreamType::UniDi).unwrap(), 3);
        assert_eq!(server.stream_create(StreamType::UniDi).unwrap(), 7);
        assert_eq!(server.stream_create(StreamType::BiDi).unwrap(), 1);
        assert_eq!(server.stream_create(StreamType::BiDi).unwrap(), 5);
    }

    #[test]
    fn test_conn_handshake() {
        qdebug!("---- client: generate CH");
        let mut client = default_client();
        let out = client.process(None, now());
        assert!(out.as_dgram_ref().is_some());
        assert_eq!(out.as_dgram_ref().unwrap().len(), 1232);
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
        let mut server = default_server();
        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        qdebug!("---- client: cert verification");
        let out = client.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());

        assert!(maybe_authenticate(&mut client));

        qdebug!("---- client: SH..FIN -> FIN");
        let out = client.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        qdebug!("---- server: FIN -> ACKS");
        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        qdebug!("---- client: ACKS -> 0");
        let out = client.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        assert_eq!(*client.state(), State::Connected);
        assert_eq!(*server.state(), State::Connected);
    }

    #[test]
    fn test_conn_handshake_failed_authentication() {
        qdebug!("---- client: generate CH");
        let mut client = default_client();
        let out = client.process(None, now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
        let mut server = default_server();
        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        qdebug!("---- client: cert verification");
        let out = client.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        let authentication_needed = |e| matches!(e, ConnectionEvent::AuthenticationNeeded);
        assert!(client.events().any(authentication_needed));
        qdebug!("---- client: Alert(certificate_revoked)");
        client.authenticated(AuthenticationStatus::CertRevoked, now());

        qdebug!("---- client: -> Alert(certificate_revoked)");
        let out = client.process(None, now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        qdebug!("---- server: Alert(certificate_revoked)");
        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        assert_error(&client, ConnectionError::Transport(Error::CryptoAlert(44)));
        assert_error(&server, ConnectionError::Transport(Error::PeerError(300)));
    }

    #[test]
    #[allow(clippy::cognitive_complexity)]
    // tests stream send/recv after connection is established.
    fn test_conn_stream() {
        let mut client = default_client();
        let mut server = default_server();

        qdebug!("---- client");
        let out = client.process(None, now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        // -->> Initial[0]: CRYPTO[CH]

        qdebug!("---- server");
        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        // <<-- Initial[0]: CRYPTO[SH] ACK[0]
        // <<-- Handshake[0]: CRYPTO[EE, CERT, CV, FIN]

        qdebug!("---- client");
        let out = client.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        // -->> Initial[1]: ACK[0]

        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());

        assert!(maybe_authenticate(&mut client));

        qdebug!("---- client");
        let out = client.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        assert_eq!(*client.state(), State::Connected);
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        // -->> Handshake[0]: CRYPTO[FIN], ACK[0]

        qdebug!("---- server");
        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        assert_eq!(*server.state(), State::Connected);
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        // ACKs
        // -->> nothing

        qdebug!("---- client");
        // Send
        let client_stream_id = client.stream_create(StreamType::UniDi).unwrap();
        client.stream_send(client_stream_id, &[6; 100]).unwrap();
        client.stream_send(client_stream_id, &[7; 40]).unwrap();
        client.stream_send(client_stream_id, &[8; 4000]).unwrap();

        // Send to another stream but some data after fin has been set
        let client_stream_id2 = client.stream_create(StreamType::UniDi).unwrap();
        client.stream_send(client_stream_id2, &[6; 60]).unwrap();
        client.stream_close_send(client_stream_id2).unwrap();
        client.stream_send(client_stream_id2, &[7; 50]).unwrap_err();
        // Sending this much takes a few datagrams.
        let mut datagrams = vec![];
        let mut out = client.process(out.dgram(), now());
        while let Some(d) = out.dgram() {
            datagrams.push(d);
            out = client.process(None, now());
        }
        assert_eq!(datagrams.len(), 4);

        qdebug!("---- server");
        let mut expect_ack = false;
        for d in datagrams {
            let out = server.process(Some(d), now());
            assert_eq!(out.as_dgram_ref().is_some(), expect_ack); // ACK every second.
            qdebug!("Output={:0x?}", out.as_dgram_ref());
            expect_ack = !expect_ack;
        }
        assert_eq!(*server.state(), State::Connected);

        let mut buf = vec![0; 4000];

        let mut stream_ids = server.events().filter_map(|evt| match evt {
            ConnectionEvent::NewStream { stream_id, .. } => Some(stream_id),
            _ => None,
        });
        let stream_id = stream_ids.next().expect("should have a new stream event");
        let (received, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(received, 4000);
        assert_eq!(fin, false);
        let (received, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(received, 140);
        assert_eq!(fin, false);

        let stream_id = stream_ids
            .next()
            .expect("should have a second new stream event");
        let (received, fin) = server.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(received, 60);
        assert_eq!(fin, true);
    }

    /// Drive the handshake between the client and server.
    fn handshake(client: &mut Connection, server: &mut Connection) {
        let mut a = client;
        let mut b = server;
        let mut datagram = None;
        let is_done = |c: &mut Connection| match c.state() {
            // TODO(mt): Finish on Closed and not Closing.
            State::Connected | State::Closing { .. } | State::Closed(..) => true,
            _ => false,
        };
        while !is_done(a) {
            let _ = maybe_authenticate(a);
            let d = a.process(datagram, now());
            datagram = d.dgram();
            mem::swap(&mut a, &mut b);
        }
        a.process(datagram, now());
    }

    fn connect(client: &mut Connection, server: &mut Connection) {
        handshake(client, server);
        assert_eq!(*client.state(), State::Connected);
        assert_eq!(*server.state(), State::Connected);
    }

    fn assert_error(c: &Connection, err: ConnectionError) {
        match c.state() {
            // TODO(mt): Finish on Closed and not Closing.
            State::Closing { error, .. } | State::Closed(error) => {
                assert_eq!(*error, err);
            }
            _ => panic!("bad state {:?}", c.state()),
        }
    }

    #[test]
    fn test_no_alpn() {
        fixture_init();
        let mut client = Connection::new_client(
            "example.com",
            &["bad-alpn"],
            Rc::new(RefCell::new(FixedConnectionIdManager::new(9))),
            loopback(),
            loopback(),
        )
        .unwrap();
        let mut server = default_server();

        handshake(&mut client, &mut server);
        // TODO (mt): errors are immediate, which means that we never send CONNECTION_CLOSE
        // and the client never sees the server's rejection of its handshake.
        //assert_error(&client, ConnectionError::Transport(Error::CryptoAlert(120)));
        assert_error(&server, ConnectionError::Transport(Error::CryptoAlert(120)));
    }

    #[test]
    fn test_dup_server_flight1() {
        qdebug!("---- client: generate CH");
        let mut client = default_client();
        let out = client.process(None, now());
        assert!(out.as_dgram_ref().is_some());
        assert_eq!(out.as_dgram_ref().unwrap().len(), 1232);
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
        let mut server = default_server();
        let out_to_rep = server.process(out.dgram(), now());
        assert!(out_to_rep.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out_to_rep.as_dgram_ref());

        qdebug!("---- client: cert verification");
        let out = client.process(Some(out_to_rep.as_dgram_ref().unwrap().clone()), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());

        assert!(maybe_authenticate(&mut client));

        qdebug!("---- client: SH..FIN -> FIN");
        let out = client.process(None, now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        assert_eq!(2, client.stats().packets_rx);
        assert_eq!(0, client.stats().dups_rx);

        qdebug!("---- Dup, ignored");
        let out = client.process(out_to_rep.dgram(), now());
        assert!(out.as_dgram_ref().is_none());
        qdebug!("Output={:0x?}", out.as_dgram_ref());

        // Four packets total received, two of them are dups
        assert_eq!(4, client.stats().packets_rx);
        assert_eq!(2, client.stats().dups_rx);
    }

    fn exchange_ticket(client: &mut Connection, server: &mut Connection) -> Vec<u8> {
        server.send_ticket(now(), &[]).expect("can send ticket");
        let out = server.process_output(now());
        assert!(out.as_dgram_ref().is_some());
        client.process_input(out.dgram().unwrap(), now());
        assert_eq!(*client.state(), State::Connected);
        client.resumption_token().expect("should have token")
    }

    #[test]
    fn connection_close() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        client.close(now, 42, "");

        let out = client.process(None, now);

        let frames = server.test_process_input(out.dgram().unwrap(), now);
        assert_eq!(frames.len(), 1);
        assert!(matches!(
            frames[0],
            (
                Frame::ConnectionClose {
                    error_code: CloseError::Application(42),
                    ..
                },
                3,
            )
        ));
    }

    #[test]
    fn resume() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let token = exchange_ticket(&mut client, &mut server);
        let mut client = default_client();
        client
            .set_resumption_token(now(), &token[..])
            .expect("should set token");
        let mut server = default_server();
        connect(&mut client, &mut server);
        assert!(client.crypto.tls.info().unwrap().resumed());
        assert!(server.crypto.tls.info().unwrap().resumed());
    }

    #[test]
    fn zero_rtt_negotiate() {
        // Note that the two servers in this test will get different anti-replay filters.
        // That's OK because we aren't testing anti-replay.
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let token = exchange_ticket(&mut client, &mut server);
        let mut client = default_client();
        client
            .set_resumption_token(now(), &token[..])
            .expect("should set token");
        let mut server = default_server();
        connect(&mut client, &mut server);
        assert!(client.crypto.tls.info().unwrap().early_data_accepted());
        assert!(server.crypto.tls.info().unwrap().early_data_accepted());
    }

    #[test]
    fn zero_rtt_send_recv() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let token = exchange_ticket(&mut client, &mut server);
        let mut client = default_client();
        client
            .set_resumption_token(now(), &token[..])
            .expect("should set token");
        let mut server = default_server();

        // Send ClientHello.
        let client_hs = client.process(None, now());
        assert!(client_hs.as_dgram_ref().is_some());

        // Now send a 0-RTT packet.
        let client_stream_id = client.stream_create(StreamType::UniDi).unwrap();
        client.stream_send(client_stream_id, &[1, 2, 3]).unwrap();
        let client_0rtt = client.process(None, now());
        assert!(client_0rtt.as_dgram_ref().is_some());
        // 0-RTT packets on their own shouldn't be padded to 1200.
        assert!(client_0rtt.as_dgram_ref().unwrap().len() < 1200);

        let server_hs = server.process(client_hs.dgram(), now());
        assert!(server_hs.as_dgram_ref().is_some()); // ServerHello, etc...
        let server_process_0rtt = server.process(client_0rtt.dgram(), now());
        assert!(server_process_0rtt.as_dgram_ref().is_none());

        let server_stream_id = server
            .events()
            .find_map(|evt| match evt {
                ConnectionEvent::NewStream { stream_id, .. } => Some(stream_id),
                _ => None,
            })
            .expect("should have received a new stream event");
        assert_eq!(client_stream_id, server_stream_id);
    }

    #[test]
    fn zero_rtt_send_coalesce() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let token = exchange_ticket(&mut client, &mut server);
        let mut client = default_client();
        client
            .set_resumption_token(now(), &token[..])
            .expect("should set token");
        let mut server = default_server();

        // Write 0-RTT before generating any packets.
        // This should result in a datagram that coalesces Initial and 0-RTT.
        let client_stream_id = client.stream_create(StreamType::UniDi).unwrap();
        client.stream_send(client_stream_id, &[1, 2, 3]).unwrap();
        let client_0rtt = client.process(None, now());
        assert!(client_0rtt.as_dgram_ref().is_some());

        assertions::assert_coalesced_0rtt(&client_0rtt.as_dgram_ref().unwrap()[..]);

        let server_hs = server.process(client_0rtt.dgram(), now());
        assert!(server_hs.as_dgram_ref().is_some()); // Should produce ServerHello etc...

        let server_stream_id = server
            .events()
            .find_map(|evt| match evt {
                ConnectionEvent::NewStream { stream_id, .. } => Some(stream_id),
                _ => None,
            })
            .expect("should have received a new stream event");
        assert_eq!(client_stream_id, server_stream_id);
    }

    #[test]
    fn zero_rtt_before_resumption_token() {
        let mut client = default_client();
        assert!(client.stream_create(StreamType::BiDi).is_err());
    }

    #[test]
    fn zero_rtt_send_reject() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let token = exchange_ticket(&mut client, &mut server);
        let mut client = default_client();
        client
            .set_resumption_token(now(), &token[..])
            .expect("should set token");
        // Using a freshly initialized anti-replay context
        // should result in the server rejecting 0-RTT.
        let ar = AntiReplay::new(now(), test_fixture::ANTI_REPLAY_WINDOW, 1, 3)
            .expect("setup anti-replay");
        let mut server = Connection::new_server(
            test_fixture::DEFAULT_KEYS,
            test_fixture::DEFAULT_ALPN,
            &ar,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(10))),
        )
        .unwrap();

        // Send ClientHello.
        let client_hs = client.process(None, now());
        assert!(client_hs.as_dgram_ref().is_some());

        // Write some data on the client.
        let stream_id = client.stream_create(StreamType::UniDi).unwrap();
        let msg = &[1, 2, 3];
        client.stream_send(stream_id, msg).unwrap();
        let client_0rtt = client.process(None, now());
        assert!(client_0rtt.as_dgram_ref().is_some());

        let server_hs = server.process(client_hs.dgram(), now());
        assert!(server_hs.as_dgram_ref().is_some()); // Should produce ServerHello etc...
        let server_ignored = server.process(client_0rtt.dgram(), now());
        assert!(server_ignored.as_dgram_ref().is_none());

        // The server shouldn't receive that 0-RTT data.
        let recvd_stream_evt = |e| matches!(e, ConnectionEvent::NewStream { .. });
        assert!(!server.events().any(recvd_stream_evt));

        // Client should get a rejection.
        let client_fin = client.process(server_hs.dgram(), now());
        let recvd_0rtt_reject = |e| e == ConnectionEvent::ZeroRttRejected;
        assert!(client.events().any(recvd_0rtt_reject));

        // Server consume client_fin
        let server_ack = server.process(client_fin.dgram(), now());
        assert!(server_ack.as_dgram_ref().is_some());
        let client_out = client.process(server_ack.dgram(), now());
        assert!(client_out.as_dgram_ref().is_none());

        // ...and the client stream should be gone.
        let res = client.stream_send(stream_id, msg);
        assert!(res.is_err());
        assert_eq!(res.unwrap_err(), Error::InvalidStreamId);

        // Open a new stream and send data. StreamId should start with 0.
        let stream_id_after_reject = client.stream_create(StreamType::UniDi).unwrap();
        assert_eq!(stream_id, stream_id_after_reject);
        let msg = &[1, 2, 3];
        client.stream_send(stream_id_after_reject, msg).unwrap();
        let client_after_reject = client.process(None, now());
        assert!(client_after_reject.as_dgram_ref().is_some());

        // The server should receive new stream
        let server_out = server.process(client_after_reject.dgram(), now());
        assert!(server_out.as_dgram_ref().is_some()); // an ack
        let recvd_stream_evt = |e| matches!(e, ConnectionEvent::NewStream { .. });
        assert!(server.events().any(recvd_stream_evt));
    }

    #[test]
    // Send fin even if a peer closes a reomte bidi send stream before sending any data.
    fn report_fin_when_stream_closed_wo_data() {
        // Note that the two servers in this test will get different anti-replay filters.
        // That's OK because we aren't testing anti-replay.
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        // create a stream
        let stream_id = client.stream_create(StreamType::BiDi).unwrap();
        client.stream_send(stream_id, &[0x00]).unwrap();
        let out = client.process(None, now());
        server.process(out.dgram(), now());

        assert_eq!(Ok(()), server.stream_close_send(stream_id));
        let out = server.process(None, now());
        client.process(out.dgram(), now());
        let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable {..});
        assert!(client.events().any(stream_readable));
    }

    #[test]
    fn idle_timeout() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        // Still connected after 59 seconds. Idle timer not reset
        client.process(None, now + Duration::from_secs(59));
        assert!(matches!(client.state(), State::Connected));

        client.process_timer(now + Duration::from_secs(60));

        // Not connected after 60 seconds.
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn idle_send_packet1() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"hello").unwrap(), 5);

        let out = client.process(None, now + Duration::from_secs(10));
        let out = server.process(out.dgram(), now + Duration::from_secs(10));

        // Still connected after 69 seconds because idle timer reset by outgoing
        // packet
        client.process(out.dgram(), now + Duration::from_secs(69));
        assert!(matches!(client.state(), State::Connected));

        // Not connected after 70 seconds.
        client.process_timer(now + Duration::from_secs(70));
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn idle_send_packet2() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"hello").unwrap(), 5);

        let _out = client.process(None, now + Duration::from_secs(10));

        assert_eq!(client.stream_send(2, b"there").unwrap(), 5);
        let _out = client.process(None, now + Duration::from_secs(20));

        // Still connected after 69 seconds.
        client.process(None, now + Duration::from_secs(69));
        assert!(matches!(client.state(), State::Connected));

        // Not connected after 70 seconds because timer not reset by second
        // outgoing packet
        client.process_timer(now + Duration::from_secs(70));
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn idle_recv_packet() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);
        assert_eq!(client.stream_send(0, b"hello").unwrap(), 5);

        // Respond with another packet
        let out = client.process(None, now + Duration::from_secs(10));
        server.process_input(out.dgram().unwrap(), now + Duration::from_secs(10));
        assert_eq!(server.stream_send(0, b"world").unwrap(), 5);
        let out = server.process_output(now + Duration::from_secs(10));
        assert_ne!(out.as_dgram_ref(), None);

        // Still connected after 79 seconds because idle timer reset by received
        // packet
        client.process(out.dgram(), now + Duration::from_secs(20));
        assert!(matches!(client.state(), State::Connected));

        // Still connected after 79 seconds.
        client.process_timer(now + Duration::from_secs(79));
        assert!(matches!(client.state(), State::Connected));

        // Not connected after 80 seconds.
        client.process_timer(now + Duration::from_secs(80));
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn max_data() {
        let mut client = default_client();
        let mut server = default_server();

        const SMALL_MAX_DATA: u64 = 16383;

        server
            .set_local_tparam(
                tp_constants::INITIAL_MAX_DATA,
                TransportParameter::Integer(SMALL_MAX_DATA),
            )
            .unwrap();

        connect(&mut client, &mut server);

        let stream_id = client.stream_create(StreamType::UniDi).unwrap();
        assert_eq!(stream_id, 2);
        assert_eq!(
            client.stream_avail_send_space(stream_id).unwrap(),
            SMALL_MAX_DATA
        );
        assert_eq!(
            client
                .stream_send(stream_id, &[b'a'; RX_STREAM_DATA_WINDOW as usize])
                .unwrap(),
            SMALL_MAX_DATA.try_into().unwrap()
        );
        let evts = client.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 2); // SendStreamWritable, StateChange(connected)
        assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 0);
        let ss = client.send_streams.get_mut(stream_id.into()).unwrap();
        ss.mark_as_sent(0, 4096, false);
        ss.mark_as_acked(0, 4096, false);

        // no event because still limited by conn max data
        let evts = client.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 0);

        // increase max data
        client.handle_max_data(100_000);
        assert_eq!(client.stream_avail_send_space(stream_id).unwrap(), 49152);
        let evts = client.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 1);
        assert!(matches!(evts[0], ConnectionEvent::SendStreamWritable{..}));
    }

    // Test that we split crypto data if they cannot fit into one packet.
    // To test this we will use a long server certificate.
    #[test]
    fn test_crypto_frame_split() {
        let mut client = default_client();
        let mut server = Connection::new_server(
            test_fixture::LONG_CERT_KEYS,
            test_fixture::DEFAULT_ALPN,
            &test_fixture::anti_replay(),
            Rc::new(RefCell::new(FixedConnectionIdManager::new(6))),
        )
        .expect("create a server");

        let client1 = client.process(None, now());
        assert!(client1.as_dgram_ref().is_some());

        // The entire server flight doesn't fit in a single packet because the
        // certificate is large, therefore the server will produce 2 packets.
        let server1 = server.process(client1.dgram(), now());
        assert!(server1.as_dgram_ref().is_some());
        let server2 = server.process(None, now());
        assert!(server2.as_dgram_ref().is_some());

        let client2 = client.process(server1.dgram(), now());
        // This is an ack.
        assert!(client2.as_dgram_ref().is_some());
        // The client might have the certificate now, so we can't guarantee that
        // this will work.
        let auth1 = maybe_authenticate(&mut client);
        assert_eq!(*client.state(), State::Handshaking);

        // let server process the ack for the first packet.
        let server3 = server.process(client2.dgram(), now());
        assert!(server3.as_dgram_ref().is_none());

        // Consume the second packet from the server.
        let client3 = client.process(server2.dgram(), now());

        // Check authentication.
        let auth2 = maybe_authenticate(&mut client);
        assert!(auth1 ^ auth2);
        // Now client has all data to finish handshake.
        assert_eq!(*client.state(), State::Connected);

        let client4 = client.process(server3.dgram(), now());
        // One of these will contain data depending on whether Authentication was completed
        // after the first or second server packet.
        assert!(client3.as_dgram_ref().is_some() ^ client4.as_dgram_ref().is_some());

        let _ = server.process(client3.dgram(), now());
        let _ = server.process(client4.dgram(), now());

        assert_eq!(*client.state(), State::Connected);
        assert_eq!(*server.state(), State::Connected);
    }

    #[test]
    fn set_local_tparam() {
        let client = default_client();

        client
            .set_local_tparam(
                tp_constants::INITIAL_MAX_DATA,
                TransportParameter::Integer(55),
            )
            .unwrap()
    }

    #[test]
    // If we send a stop_sending to the peer, we should not accept more data from the peer.
    fn do_not_accept_data_after_stop_sending() {
        // Note that the two servers in this test will get different anti-replay filters.
        // That's OK because we aren't testing anti-replay.
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        // create a stream
        let stream_id = client.stream_create(StreamType::BiDi).unwrap();
        client.stream_send(stream_id, &[0x00]).unwrap();
        let out = client.process(None, now());
        server.process(out.dgram(), now());

        let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable {..});
        assert!(server.events().any(stream_readable));

        // Send one more packet from client. The packet should arrive after the server
        // has already requested stop_sending.
        client.stream_send(stream_id, &[0x00]).unwrap();
        let out_second_data_frame = client.process(None, now());
        // Call stop sending.
        assert_eq!(
            Ok(()),
            server.stream_stop_sending(stream_id, Error::NoError.code())
        );

        // Receive the second data frame. The frame should be ignored and now
        // DataReadable events should be posted.
        let out = server.process(out_second_data_frame.dgram(), now());
        assert!(!server.events().any(stream_readable));

        client.process(out.dgram(), now());
        assert_eq!(
            Err(Error::FinalSizeError),
            client.stream_send(stream_id, &[0x00])
        );
    }

    #[test]
    // Server sends stop_sending, the client simultaneous sends reset.
    fn simultaneous_stop_sending_and_reset() {
        // Note that the two servers in this test will get different anti-replay filters.
        // That's OK because we aren't testing anti-replay.
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        // create a stream
        let stream_id = client.stream_create(StreamType::BiDi).unwrap();
        client.stream_send(stream_id, &[0x00]).unwrap();
        let out = client.process(None, now());
        server.process(out.dgram(), now());

        let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable {..});
        assert!(server.events().any(stream_readable));

        // The client resets the stream. The packet with reset should arrive after the server
        // has already requested stop_sending.
        client
            .stream_reset_send(stream_id, Error::NoError.code())
            .unwrap();
        let out_reset_frame = client.process(None, now());
        // Call stop sending.
        assert_eq!(
            Ok(()),
            server.stream_stop_sending(stream_id, Error::NoError.code())
        );

        // Receive the second data frame. The frame should be ignored and now
        // DataReadable events should be posted.
        let out = server.process(out_reset_frame.dgram(), now());
        assert!(!server.events().any(stream_readable));

        // The client gets the STOP_SENDING frame.
        client.process(out.dgram(), now());
        assert_eq!(
            Err(Error::InvalidStreamId),
            client.stream_send(stream_id, &[0x00])
        );
    }

    #[test]
    fn test_client_fin_reorder() {
        let mut client = default_client();
        let mut server = default_server();

        // Send ClientHello.
        let client_hs = client.process(None, now());
        assert!(client_hs.as_dgram_ref().is_some());

        let server_hs = server.process(client_hs.dgram(), now());
        assert!(server_hs.as_dgram_ref().is_some()); // ServerHello, etc...

        let client_ack = client.process(server_hs.dgram(), now());
        assert!(client_ack.as_dgram_ref().is_some());

        let server_out = server.process(client_ack.dgram(), now());
        assert!(server_out.as_dgram_ref().is_none());

        assert!(maybe_authenticate(&mut client));
        assert_eq!(*client.state(), State::Connected);

        let client_fin = client.process(None, now());
        assert!(client_fin.as_dgram_ref().is_some());

        let client_stream_id = client.stream_create(StreamType::UniDi).unwrap();
        client.stream_send(client_stream_id, &[1, 2, 3]).unwrap();
        let client_stream_data = client.process(None, now());
        assert!(client_stream_data.as_dgram_ref().is_some());

        // Now stream data gets before client_fin
        let server_out = server.process(client_stream_data.dgram(), now());
        assert!(server_out.as_dgram_ref().is_none()); // the packet will be discarded

        assert_eq!(*server.state(), State::Handshaking);
        let server_out = server.process(client_fin.dgram(), now());
        assert!(server_out.as_dgram_ref().is_some());
    }

    #[test]
    fn pto_works_basic() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        // Send data on two streams
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"hello").unwrap(), 5);
        assert_eq!(client.stream_send(2, b" world").unwrap(), 6);

        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 6);
        assert_eq!(client.stream_send(6, b"there!").unwrap(), 6);

        // Send orig pkt
        let _out = client.process(None, now + Duration::from_secs(10));

        // Nothing to do, should return callback
        let out = client.process(None, now + Duration::from_secs(10));
        assert!(matches!(out, Output::Callback(_)));

        // One second later, it should want to send PTO packet
        let out = client.process(None, now + Duration::from_secs(11));

        let frames = server.test_process_input(out.dgram().unwrap(), now + Duration::from_secs(11));

        assert_eq!(frames[0], (Frame::Ping, 0));
        assert_eq!(frames[1], (Frame::Ping, 2));
        assert!(matches!(frames[2], (Frame::Stream { .. }, 3)));
    }

    #[test]
    #[allow(clippy::cognitive_complexity)]
    fn pto_works_ping() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        // Send "zero" pkt
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"zero").unwrap(), 4);
        let pkt0 = client.process(None, now + Duration::from_secs(10));
        assert!(matches!(pkt0, Output::Datagram(_)));

        // Send "one" pkt
        assert_eq!(client.stream_send(2, b"one").unwrap(), 3);
        let pkt1 = client.process(None, now + Duration::from_secs(10));

        // Send "two" pkt
        assert_eq!(client.stream_send(2, b"two").unwrap(), 3);
        let pkt2 = client.process(None, now + Duration::from_secs(10));

        // Send "three" pkt
        assert_eq!(client.stream_send(2, b"three").unwrap(), 5);
        let pkt3 = client.process(None, now + Duration::from_secs(10));

        // Nothing to do, should return callback
        let out = client.process(None, now + Duration::from_secs(10));
        // Check callback delay is what we expect
        assert!(matches!(out, Output::Callback(x) if x == Duration::from_millis(45)));

        // Process these by server, skipping pkt0
        let srv0_pkt1 = server.process(pkt1.dgram(), now + Duration::from_secs(10));
        // ooo, ack client pkt 1
        assert!(matches!(srv0_pkt1, Output::Datagram(_)));

        // process pkt2 (no ack yet)
        let srv2 = server.process(
            pkt2.dgram(),
            now + Duration::from_secs(10) + Duration::from_millis(20),
        );
        assert!(matches!(srv2, Output::Callback(_)));

        // process pkt3 (acked)
        let srv2 = server.process(
            pkt3.dgram(),
            now + Duration::from_secs(10) + Duration::from_millis(20),
        );
        // ack client pkt 2 & 3
        assert!(matches!(srv2, Output::Datagram(_)));

        // client processes ack
        let pkt4 = client.process(
            srv2.dgram(),
            now + Duration::from_secs(10) + Duration::from_millis(40),
        );
        // client resends data from pkt0
        assert!(matches!(pkt4, Output::Datagram(_)));

        // server sees ooo pkt0 and generates ack
        let srv_pkt2 = server.process(
            pkt0.dgram(),
            now + Duration::from_secs(10) + Duration::from_millis(40),
        );
        assert!(matches!(srv_pkt2, Output::Datagram(_)));

        // Orig data is acked
        let pkt5 = client.process(
            srv_pkt2.dgram(),
            now + Duration::from_secs(10) + Duration::from_millis(40),
        );
        assert!(matches!(pkt5, Output::Callback(_)));

        // PTO expires. No unacked data. Only send PING.
        let pkt6 = client.process(
            None,
            now + Duration::from_secs(10) + Duration::from_millis(110),
        );

        let frames = server.test_process_input(
            pkt6.dgram().unwrap(),
            now + Duration::from_secs(10) + Duration::from_millis(110),
        );

        assert_eq!(frames[0], (Frame::Ping, 0));
        assert_eq!(frames[1], (Frame::Ping, 2));
        assert_eq!(frames[2], (Frame::Ping, 3));
    }

    #[test]
    // Absent path PTU discovery, max v6 packet size should be 1232.
    fn verify_pkt_honors_mtu() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        // Try to send a large stream and verify first packet is correctly sized
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, &[0xbb; 2000]).unwrap(), 2000);
        let pkt0 = client.process(None, now);
        assert!(matches!(pkt0, Output::Datagram(_)));
        assert_eq!(pkt0.as_dgram_ref().unwrap().len(), 1232);
    }

    // Handle sending a bunch of bytes from one connection to another, until
    // something stops us from sending.
    fn send_bytes(src: &mut Connection, stream: u64, now: Instant) -> Vec<Datagram> {
        let mut total_dgrams = Vec::new();

        loop {
            let bytes_sent = src.stream_send(stream, &[0x42; 4_096]).unwrap();
            if bytes_sent == 0 {
                break;
            }
        }

        loop {
            let pkt = src.process_output(now);
            match pkt {
                Output::Datagram(dgram) => total_dgrams.push(dgram),
                Output::Callback(_) => break,
                _ => panic!(),
            }
        }

        total_dgrams
    }

    // Receive multiple packets and generate an ack-only packet.
    fn ack_bytes(
        dest: &mut Connection,
        stream: u64,
        in_dgrams: Vec<Datagram>,
        now: Instant,
    ) -> (Datagram, Vec<Frame>) {
        let mut srv_buf = [0; 4_096];
        let mut recvd_frames = Vec::new();

        for dgram in in_dgrams {
            recvd_frames.extend(dest.test_process_input(dgram, now));
        }

        loop {
            let (bytes_read, _fin) = dest.stream_recv(stream, &mut srv_buf).unwrap();
            if bytes_read == 0 {
                break;
            }
        }

        let mut tx_dgrams = Vec::new();
        while let Output::Datagram(dg) = dest.process_output(now) {
            tx_dgrams.push(dg);
        }

        assert_eq!(tx_dgrams.len(), 1);

        (
            tx_dgrams.pop().unwrap(),
            recvd_frames.into_iter().map(|(f, _e)| f).collect(),
        )
    }

    #[test]
    /// Verify initial CWND is honored.
    fn cc_slow_start() {
        let mut client = default_client();
        let mut server = default_server();

        server
            .set_local_tparam(
                tp_constants::INITIAL_MAX_DATA,
                TransportParameter::Integer(65536),
            )
            .unwrap();
        connect(&mut client, &mut server);

        let now = now();

        // Try to send a lot of data
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        let c_tx_dgrams = send_bytes(&mut client, 2, now);

        // Init/Handshake acks have increased cwnd by 630 so we actually can
        // send 11 with the last being shorter
        assert_eq!(
            c_tx_dgrams.iter().map(|d| d.len()).sum::<usize>(),
            (INITIAL_CWND_PKTS * MAX_DATAGRAM_SIZE) + 630
        );
        assert_eq!(c_tx_dgrams.len(), INITIAL_CWND_PKTS + 1);
        let (last, rest) = c_tx_dgrams.split_last().unwrap();
        assert!(rest.iter().all(|d| d.len() == MAX_DATAGRAM_SIZE));
        assert_eq!(last.len(), 630);
        assert_eq!(client.loss_recovery.cwnd_avail(), 0);
    }

    #[test]
    /// Verify that CC moves to cong avoidance when a packet is marked lost.
    fn cc_slow_start_to_cong_avoidance_recovery_period() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let c_tx_dgrams = send_bytes(&mut client, 0, now);

        // Initial/Handshake acks have already increased cwnd so 11 packets are
        // allowed
        assert_eq!(c_tx_dgrams.len(), INITIAL_CWND_PKTS + 1);
        assert_eq!(
            c_tx_dgrams.iter().map(|d| d.len()).sum::<usize>(),
            (INITIAL_CWND_PKTS * MAX_DATAGRAM_SIZE) + 630
        );

        // Server: Receive and generate ack
        let (s_tx_dgram, _recvd_frames) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // Client: Process ack
        let recvd_frames = client.test_process_input(s_tx_dgram, now);

        const INITIAL_CWND_PKTS_U64: u64 = INITIAL_CWND_PKTS as u64;
        // Verify that server-sent frame was what we thought
        assert!(matches!(
            recvd_frames[0],
            (
                Frame::Ack {
                    largest_acknowledged: INITIAL_CWND_PKTS_U64,
                    ..
                },
                3,
            )
        ));

        // Client: send more
        let mut c_tx_dgrams = send_bytes(&mut client, 0, now);

        assert_eq!(c_tx_dgrams.len(), INITIAL_CWND_PKTS * 2 + 1);

        // Server: Receive and generate ack again, but drop first packet
        c_tx_dgrams.remove(0);
        let (s_tx_dgram, _recvd_frames) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // Client: Process ack
        let recvd_frames = client.test_process_input(s_tx_dgram, now);

        // Verify that server-sent frame was what we thought
        assert!(matches!(
            recvd_frames[0],
            (
                Frame::Ack {
                    largest_acknowledged: 31,
                    ..
                },
                3,
            )
        ));

        // If we just triggered cong avoidance, these should be equal
        assert_eq!(client.loss_recovery.cwnd(), client.loss_recovery.ssthresh());
    }

    #[test]
    /// Verify that CC stays in recovery period when packet sent before start of
    /// recovery period is acked.
    fn cc_cong_avoidance_recovery_period_unchanged() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let mut c_tx_dgrams = send_bytes(&mut client, 0, now);

        // Drop 0th packet. When acked, this should put client into CARP.
        c_tx_dgrams.remove(0);
        assert_eq!(c_tx_dgrams.len(), 10);

        let c_tx_dgrams2 = c_tx_dgrams.split_off(5);

        // Server: Receive and generate ack
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        client.test_process_input(s_tx_dgram, now);

        // If we just triggered cong avoidance, these should be equal
        let cwnd1 = client.loss_recovery.cwnd();
        assert_eq!(cwnd1, client.loss_recovery.ssthresh());

        // Generate ACK for more received packets
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams2, now);

        // ACK more packets but they were sent before end of recovery period
        client.test_process_input(s_tx_dgram, now);

        // cwnd should not have changed since ACKed packets were sent before
        // recovery period expired
        let cwnd2 = client.loss_recovery.cwnd();
        assert_eq!(cwnd1, cwnd2);
    }

    #[test]
    /// Verify that CC moves out of recovery period when packet sent after start
    /// of recovery period is acked.
    fn cc_cong_avoidance_recovery_period_to_cong_avoidance() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let mut now = now();

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let mut c_tx_dgrams = send_bytes(&mut client, 0, now);

        // Drop 0th packet. When acked, this should put client into CARP.
        c_tx_dgrams.remove(0);

        // Server: Receive and generate ack
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // Client: Process ack
        client.test_process_input(s_tx_dgram, now);

        // Should be in CARP now.
        let cwnd1 = client.loss_recovery.cwnd();

        now += Duration::from_secs(10); // Time passes. CARP -> CA

        // Client: Send more data
        let mut c_tx_dgrams = send_bytes(&mut client, 0, now);

        // Only sent 2 packets, to generate an ack but also keep cwnd increase
        // small
        c_tx_dgrams.truncate(2);

        // Generate ACK
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        client.test_process_input(s_tx_dgram, now);

        // ACK of pkts sent after start of recovery period should have caused
        // exit from recovery period to just regular congestion avoidance. cwnd
        // should now be a little higher but not as high as acked pkts during
        // slow-start would cause it to be.
        let cwnd2 = client.loss_recovery.cwnd();
        assert!(cwnd2 > cwnd1);
        assert!(cwnd2 < cwnd1 + 500);
    }

    #[test]
    /// Verify transition to persistent congestion state if conditions are met.
    fn cc_slow_start_to_persistent_congestion_no_acks() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let mut now = now();

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 11);

        // Server: Receive and generate ack
        now += Duration::from_millis(100);
        let (_s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // ACK lost.

        // Note: wait some arbitrary time that should be longer than pto
        // timer. This is rather brittle.
        now += Duration::from_secs(1);

        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        now += Duration::from_secs(2);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        now += Duration::from_secs(4);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        // Generate ACK
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // In PC now.
        client.test_process_input(s_tx_dgram, now);

        assert_eq!(client.loss_recovery.cwnd(), MIN_CONG_WINDOW);
    }

    #[test]
    /// Verify transition to persistent congestion state if conditions are met.
    fn cc_slow_start_to_persistent_congestion_some_acks() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let mut now = now();

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 11);

        // Server: Receive and generate ack
        now += Duration::from_millis(100);
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        now += Duration::from_millis(100);
        client.test_process_input(s_tx_dgram, now);

        // send bytes that will be lost
        let _c_tx_dgrams = send_bytes(&mut client, 0, now);
        now += Duration::from_millis(100);
        // Not received.
        // let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        now += Duration::from_secs(1);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        now += Duration::from_secs(2);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        now += Duration::from_secs(4);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        // Generate ACK
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // In PC now.
        client.test_process_input(s_tx_dgram, now);

        assert_eq!(client.loss_recovery.cwnd(), MIN_CONG_WINDOW);
    }

    #[test]
    /// Verify persistent congestion moves to slow start after recovery period
    /// ends.
    fn cc_persistent_congestion_to_slow_start() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let mut now = now();

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 11);

        // Server: Receive and generate ack
        now += Duration::from_millis(100);
        let (_s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // ACK lost.

        // Note: wait some arbitrary time that should be longer than pto
        // timer. This is rather brittle.
        now += Duration::from_secs(1);

        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        now += Duration::from_secs(2);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        now += Duration::from_secs(4);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 1); // One PTO packet

        // Generate ACK
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // In PC now.
        client.test_process_input(s_tx_dgram, now);

        assert_eq!(client.loss_recovery.cwnd(), MIN_CONG_WINDOW);

        // New part of test starts here

        now += Duration::from_millis(100);

        // Send packets from after start of CARP
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 2);

        // Server: Receive and generate ack
        now += Duration::from_millis(100);
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // No longer in CARP. (pkts acked from after start of CARP)
        // Should be in slow start now.
        client.test_process_input(s_tx_dgram, now);

        // ACKing 2 packets should let client send 4.
        let c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 4);
    }
}
