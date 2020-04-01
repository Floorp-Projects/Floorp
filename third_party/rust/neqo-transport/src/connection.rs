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
use std::fmt::{self, Debug};
use std::net::SocketAddr;
use std::rc::Rc;
use std::time::{Duration, Instant};

use smallvec::SmallVec;

use neqo_common::{hex, matches, qdebug, qerror, qinfo, qtrace, qwarn, Datagram, Decoder, Encoder};
use neqo_crypto::agent::CertificateInfo;
use neqo_crypto::{
    Agent, AntiReplay, AuthenticationStatus, Client, HandshakeState, Record, SecretAgentInfo,
    Server,
};

use crate::cid::{ConnectionId, ConnectionIdDecoder, ConnectionIdManager, ConnectionIdRef};
use crate::crypto::{Crypto, CryptoDxState};
use crate::dump::*;
use crate::events::{ConnectionEvent, ConnectionEvents};
use crate::flow_mgr::FlowMgr;
use crate::frame::{AckRange, Frame, FrameType, StreamType};
use crate::packet::{DecryptedPacket, PacketBuilder, PacketNumber, PacketType, PublicPacket};
use crate::path::Path;
use crate::recovery::{LossRecovery, RecoveryToken};
use crate::recv_stream::{RecvStream, RecvStreams, RX_STREAM_DATA_WINDOW};
use crate::send_stream::{SendStream, SendStreams};
use crate::stats::Stats;
use crate::stream_id::{StreamId, StreamIndex, StreamIndexes};
use crate::tparams::{
    self, TransportParameter, TransportParameterId, TransportParameters, TransportParametersHandler,
};
use crate::tracking::{AckTracker, PNSpace, SentPacket};
use crate::{AppError, ConnectionError, Error, Res, LOCAL_IDLE_TIMEOUT};

#[derive(Debug, Default)]
struct Packet(Vec<u8>);

pub const LOCAL_STREAM_LIMIT_BIDI: u64 = 16;
pub const LOCAL_STREAM_LIMIT_UNI: u64 = 16;

const LOCAL_MAX_DATA: u64 = 0x3FFF_FFFF_FFFF_FFFF; // 2^62-1

const MIN_CC_WINDOW: usize = 0x200; // let's not send packets smaller than 512

#[derive(Debug, PartialEq, Copy, Clone)]
/// Client or Server.
pub enum Role {
    Client,
    Server,
}

impl Role {
    pub fn remote(self) -> Self {
        match self {
            Self::Client => Self::Server,
            Self::Server => Self::Client,
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
    Confirmed,
    Closing {
        error: ConnectionError,
        frame_type: FrameType,
        msg: String,
        timeout: Instant,
    },
    Closed(ConnectionError),
}

impl State {
    #[must_use]
    pub fn connected(&self) -> bool {
        matches!(self, Self::Connected | Self::Confirmed)
    }
}

// Implement Ord so that we can enforce monotonic state progression.
impl PartialOrd for State {
    #[allow(clippy::match_same_arms)] // Lint bug: rust-lang/rust-clippy#860
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        if std::mem::discriminant(self) == std::mem::discriminant(other) {
            return Some(Ordering::Equal);
        }
        Some(match (self, other) {
            (Self::Init, _) => Ordering::Less,
            (_, Self::Init) => Ordering::Greater,
            (Self::WaitInitial, _) => Ordering::Less,
            (_, Self::WaitInitial) => Ordering::Greater,
            (Self::Handshaking, _) => Ordering::Less,
            (_, Self::Handshaking) => Ordering::Greater,
            (Self::Connected, _) => Ordering::Less,
            (_, Self::Connected) => Ordering::Greater,
            (Self::Confirmed, _) => Ordering::Less,
            (_, Self::Confirmed) => Ordering::Greater,
            (Self::Closing { .. }, _) => Ordering::Less,
            (_, Self::Closing { .. }) => Ordering::Greater,
            (Self::Closed(_), _) => unreachable!(),
        })
    }
}

#[derive(Debug)]
enum ZeroRttState {
    Init,
    Sending,
    AcceptedClient,
    AcceptedServer,
    Rejected,
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
    #[must_use]
    pub fn dgram(self) -> Option<Datagram> {
        match self {
            Self::Datagram(dg) => Some(dg),
            _ => None,
        }
    }

    /// Get a reference to the Datagram, if any.
    pub fn as_dgram_ref(&self) -> Option<&Datagram> {
        match self {
            Self::Datagram(dg) => Some(dg),
            _ => None,
        }
    }

    /// Ask how long the caller should wait before calling back.
    #[must_use]
    pub fn callback(&self) -> Duration {
        match self {
            Self::Callback(t) => *t,
            _ => Duration::new(0, 0),
        }
    }
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
    fn decode_cid<'a>(&self, dec: &mut Decoder<'a>) -> Option<ConnectionIdRef<'a>> {
        dec.decode(self.len).map(ConnectionIdRef::from)
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

impl RetryInfo {
    fn new(odcid: ConnectionId) -> Self {
        Self {
            token: Vec::new(),
            odcid,
        }
    }
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
        Self::Init
    }
}

impl IdleTimeout {
    pub fn as_instant(&self) -> Option<Instant> {
        match self {
            Self::Init => None,
            Self::PacketReceived(t) | Self::AckElicitingPacketSent(t) => Some(*t),
        }
    }

    fn on_packet_sent(&mut self, now: Instant) {
        // Only reset idle timeout if we've received a packet since the last
        // time we reset the timeout here.
        match self {
            Self::AckElicitingPacketSent(_) => {}
            Self::Init | Self::PacketReceived(_) => {
                *self = Self::AckElicitingPacketSent(now + LOCAL_IDLE_TIMEOUT);
            }
        }
    }

    fn on_packet_received(&mut self, now: Instant) {
        *self = Self::PacketReceived(now + LOCAL_IDLE_TIMEOUT);
    }

    pub fn expired(&self, now: Instant) -> bool {
        if let Some(timeout) = self.as_instant() {
            now >= timeout
        } else {
            false
        }
    }
}

/// StateManagement manages whether we need to send HANDSHAKE_DONE and CONNECTION_CLOSE.
/// Valid state transitions are:
/// * Idle -> HandshakeDone: at the server when the handshake completes
/// * HandshakeDone -> Idle: when a HANDSHAKE_DONE frame is sent
/// * Idle/HandshakeDone -> ConnectionClose: when closing
/// * ConnectionClose -> CloseSent: after sending CONNECTION_CLOSE
/// * CloseSent -> ConnectionClose: any time a new CONNECTION_CLOSE is needed
#[derive(Debug, Clone, PartialEq)]
enum StateSignaling {
    Idle,
    HandshakeDone,
    ConnectionClose,
    CloseSent,
}

impl StateSignaling {
    pub fn handshake_done(&mut self) {
        if *self != Self::Idle {
            debug_assert!(false, "StateSignaling must be in Idle state.");
            return;
        }
        *self = Self::HandshakeDone
    }

    pub fn send_done(&mut self) -> Option<(Frame, Option<RecoveryToken>)> {
        if *self == Self::HandshakeDone {
            *self = Self::Idle;
            Some((Frame::HandshakeDone, Some(RecoveryToken::HandshakeDone)))
        } else {
            None
        }
    }

    pub fn closing(&self) -> bool {
        *self == Self::ConnectionClose
    }

    pub fn close(&mut self) {
        *self = Self::ConnectionClose
    }

    pub fn close_sent(&mut self) {
        debug_assert!(self.closing());
        *self = Self::CloseSent
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
    state_signaling: StateSignaling,
    loss_recovery: LossRecovery,
    events: ConnectionEvents,
    token: Option<Vec<u8>>,
    stats: Stats,
}

impl Debug for Connection {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{:?} Connection: {:?} {:?}",
            self.role, self.state, self.path
        )
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
        let scid = cid_manager.borrow_mut().generate_cid();
        let mut c = Self::new(
            Role::Client,
            Client::new(server_name)?.into(),
            cid_manager,
            None,
            protocols,
            Some(Path::new(local_addr, remote_addr, scid, dcid.clone())),
        );
        c.crypto.states.init(Role::Client, &dcid);
        c.retry_info = Some(RetryInfo::new(dcid));
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
            tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL,
            RX_STREAM_DATA_WINDOW,
        );
        tps.set_integer(
            tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
            RX_STREAM_DATA_WINDOW,
        );
        tps.set_integer(tparams::INITIAL_MAX_STREAM_DATA_UNI, RX_STREAM_DATA_WINDOW);
        tps.set_integer(tparams::INITIAL_MAX_STREAMS_BIDI, LOCAL_STREAM_LIMIT_BIDI);
        tps.set_integer(tparams::INITIAL_MAX_STREAMS_UNI, LOCAL_STREAM_LIMIT_UNI);
        tps.set_integer(tparams::INITIAL_MAX_DATA, LOCAL_MAX_DATA);
        tps.set_integer(
            tparams::IDLE_TIMEOUT,
            u64::try_from(LOCAL_IDLE_TIMEOUT.as_millis()).unwrap(),
        );
        tps.set_empty(tparams::DISABLE_MIGRATION);
    }

    fn new(
        role: Role,
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
            role,
            state: match role {
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
            state_signaling: StateSignaling::Idle,
            loss_recovery: LossRecovery::new(),
            events: ConnectionEvents::default(),
            token: None,
            stats: Stats::default(),
        }
    }

    /// Set a local transport parameter, possibly overriding a default value.
    pub fn set_local_tparam(&self, tp: TransportParameterId, value: TransportParameter) -> Res<()> {
        if matches!(
            (self.role(), self.state()),
            (Role::Client, State::Init) | (Role::Server, State::WaitInitial)
        ) {
            self.tps.borrow_mut().local.set(tp, value);
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
            .set_bytes(tparams::ORIGINAL_CONNECTION_ID, odcid.to_vec());
    }

    /// Set ALPN preferences. Strings that appear earlier in the list are given
    /// higher preference.
    pub fn set_alpn(&mut self, protocols: &[impl AsRef<str>]) -> Res<()> {
        self.crypto.tls.set_alpn(protocols)?;
        Ok(())
    }

    /// Access the latest resumption token on the connection.
    pub fn resumption_token(&self) -> Option<Vec<u8>> {
        if !self.state.connected() {
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
        let res = self.handshake(now, PNSpace::Handshake, None);
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
            return;
        }

        let res = self.crypto.states.check_key_update(now);
        self.absorb_error(now, res);

        if let Some(packets) = self.loss_recovery.check_loss_detection_timeout(now) {
            self.handle_lost_packets(&packets);
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
    pub fn test_process_input(&mut self, dgram: Datagram, now: Instant) -> Vec<(Frame, PNSpace)> {
        let res = self.input(dgram, now);
        let frames = self.absorb_error(now, res).unwrap_or_default();
        self.cleanup_streams();
        frames
    }

    /// Get the time that we next need to be called back, relative to `now`.
    fn next_delay(&mut self, now: Instant) -> Duration {
        qtrace!([self], "Get callback delay {:?}", now);
        let mut delays = SmallVec::<[_; 4]>::new();

        if let Some(lr_time) = self.loss_recovery.calculate_timer() {
            qtrace!([self], "Loss recovery timer {:?}", lr_time);
            delays.push(lr_time);
        }

        if let Some(ack_time) = self.acks.ack_time() {
            qtrace!([self], "Delayed ACK timer {:?}", ack_time);
            delays.push(ack_time);
        }

        if let Some(idle_time) = self.idle_timeout.as_instant() {
            qtrace!([self], "Idle timer {:?}", idle_time);
            delays.push(idle_time);
        }

        if let Some(key_update_time) = self.crypto.states.update_time() {
            qtrace!([self], "Key update timer {:?}", key_update_time);
            delays.push(key_update_time);
        }

        // Should always at least have idle timeout, once connected
        assert!(!delays.is_empty());
        let earliest = delays.into_iter().min().unwrap();

        // TODO(agrover, mt) - need to analyze and fix #47
        // rather than just clamping to zero here.
        qdebug!(
            [self],
            "delay duration {:?}",
            max(now, earliest).duration_since(now)
        );
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

    fn is_valid_cid(&self, cid: &ConnectionIdRef) -> bool {
        self.valid_cids.iter().any(|c| c == cid) || self.path.iter().any(|p| p.valid_local_cid(cid))
    }

    fn handle_retry(&mut self, packet: PublicPacket) -> Res<()> {
        qdebug!([self], "received Retry");
        debug_assert!(self.retry_info.is_some());
        if !self.retry_info.as_ref().unwrap().token.is_empty() {
            qinfo!([self], "Dropping extra Retry");
            self.stats.dropped_rx += 1;
            return Ok(());
        }
        if packet.token().is_empty() {
            qinfo!([self], "Dropping Retry without a token");
            self.stats.dropped_rx += 1;
            return Ok(());
        }
        if !packet.is_valid_retry(&self.retry_info.as_ref().unwrap().odcid) {
            qinfo!([self], "Dropping Retry with bad integrity tag");
            self.stats.dropped_rx += 1;
            return Ok(());
        }
        if let Some(p) = &mut self.path {
            // At this point, we shouldn't have a remote connection ID for the path.
            p.set_remote_cid(packet.scid());
        } else {
            qinfo!([self], "No path, but we received a Retry");
            return Err(Error::InternalError);
        };
        self.retry_info.as_mut().unwrap().token = packet.token().to_vec();
        qinfo!(
            [self],
            "Valid Retry received, token={}",
            hex(packet.token())
        );
        let lost_packets = self.loss_recovery.retry();
        self.handle_lost_packets(&lost_packets);

        // Switching crypto state here might not happen eventually.
        // https://github.com/quicwg/base-drafts/issues/2823
        self.crypto.states.init(self.role, packet.scid());
        Ok(())
    }

    fn discard_keys(&mut self, space: PNSpace) {
        if self.crypto.discard(space) {
            self.loss_recovery.discard(space);
        }
    }

    fn input(&mut self, d: Datagram, now: Instant) -> Res<Vec<(Frame, PNSpace)>> {
        let mut slc = &d[..];
        let mut frames = Vec::new();

        qtrace!([self], "input {}", hex(&**d));

        // Handle each packet in the datagram
        while !slc.is_empty() {
            let (packet, remainder) =
                match PublicPacket::decode(slc, self.cid_manager.borrow().as_decoder()) {
                    Ok((packet, remainder)) => (packet, remainder),
                    Err(e) => {
                        qinfo!([self], "Garbage packet: {} {}", e, hex(slc));
                        self.stats.dropped_rx += 1;
                        return Ok(frames);
                    }
                }; // TODO(mt) use in place of res, and allow errors
            self.stats.packets_rx += 1;
            match (packet.packet_type(), &self.state, &self.role) {
                (PacketType::VersionNegotiation, State::WaitInitial, Role::Client) => {
                    self.set_state(State::Closed(ConnectionError::Transport(
                        Error::VersionNegotiation,
                    )));
                    return Err(Error::VersionNegotiation);
                }
                (PacketType::Retry, State::WaitInitial, Role::Client) => {
                    self.handle_retry(packet)?;
                    return Ok(frames);
                }
                (PacketType::VersionNegotiation, ..) | (PacketType::Retry, ..) => {
                    qwarn!("dropping {:?}", packet.packet_type());
                    self.stats.dropped_rx += 1;
                    return Ok(frames);
                }
                _ => {}
            };

            match self.state {
                State::Init => {
                    qinfo!([self], "Received message while in Init state");
                    self.stats.dropped_rx += 1;
                    return Ok(frames);
                }
                State::WaitInitial => {
                    qinfo!([self], "Received packet in WaitInitial");
                    if self.role == Role::Server {
                        if !packet.is_valid_initial() {
                            self.stats.dropped_rx += 1;
                            return Ok(frames);
                        }
                        self.crypto.states.init(self.role, &packet.dcid());
                    }
                }
                State::Handshaking | State::Connected | State::Confirmed => {
                    if !self.is_valid_cid(packet.dcid()) {
                        qinfo!([self], "Ignoring packet with CID {:?}", packet.dcid());
                        self.stats.dropped_rx += 1;
                        return Ok(frames);
                    }
                    if self.role == Role::Server && packet.packet_type() == PacketType::Handshake {
                        // Server has received a Handshake packet -> discard Initial keys and states
                        self.discard_keys(PNSpace::Initial);
                    }
                }
                State::Closing { .. } => {
                    // Don't bother processing the packet. Instead ask to get a
                    // new close frame.
                    self.state_signaling.close();
                    return Ok(frames);
                }
                State::Closed(..) => {
                    // Do nothing.
                    self.stats.dropped_rx += 1;
                    return Ok(frames);
                }
            }

            qtrace!([self], "Received unverified packet {:?}", packet);

            let pto = self.loss_recovery.pto();
            let payload = packet.decrypt(&mut self.crypto.states, now + pto);
            slc = remainder;
            if let Ok(payload) = payload {
                // TODO(ekr@rtfm.com): Have the server blow away the initial
                // crypto state if this fails? Otherwise, we will get a panic
                // on the assert for doesn't exist.
                // OK, we have a valid packet.
                self.idle_timeout.on_packet_received(now);
                dump_packet(
                    self,
                    "-> RX",
                    payload.packet_type(),
                    payload.pn(),
                    &payload[..],
                );
                frames.extend(self.process_packet(&payload, now)?);
                if matches!(self.state, State::WaitInitial) {
                    self.start_handshake(&packet, &d)?;
                }
                self.process_migrations(&d)?;
            } else {
                // Decryption failure, or not having keys is not fatal.
                // If the state isn't available, or we can't decrypt the packet, drop
                // the rest of the datagram on the floor, but don't generate an error.
                self.stats.dropped_rx += 1;
            }
        }
        Ok(frames)
    }

    /// Ok(true) if the packet is a duplicate
    fn process_packet(
        &mut self,
        packet: &DecryptedPacket,
        now: Instant,
    ) -> Res<Vec<(Frame, PNSpace)>> {
        // TODO(ekr@rtfm.com): Have the server blow away the initial
        // crypto state if this fails? Otherwise, we will get a panic
        // on the assert for doesn't exist.
        // OK, we have a valid packet.

        // TODO(ekr@rtfm.com): Filter for valid for this epoch.

        let space = PNSpace::from(packet.packet_type());
        if self.acks[space].is_duplicate(packet.pn()) {
            qdebug!([self], "Duplicate packet from {} pn={}", space, packet.pn());
            self.stats.dups_rx += 1;
            return Ok(vec![]);
        }

        let mut ack_eliciting = false;
        let mut d = Decoder::from(&packet[..]);
        let mut consecutive_padding = 0;
        #[allow(unused_mut)]
        let mut frames = Vec::new();
        while d.remaining() > 0 {
            let mut f = Frame::decode(&mut d)?;

            // Skip padding
            while f == Frame::Padding && d.remaining() > 0 {
                consecutive_padding += 1;
                f = Frame::decode(&mut d)?;
            }
            if consecutive_padding > 0 {
                qdebug!("PADDING frame repeated {} times", consecutive_padding);
                consecutive_padding = 0;
            }

            if cfg!(test) {
                frames.push((f.clone(), space));
            }
            ack_eliciting |= f.ack_eliciting();
            let t = f.get_type();
            let res = self.input_frame(packet.packet_type(), f, now);
            self.capture_error(now, t, res)?;
        }
        self.acks[space].set_received(now, packet.pn(), ack_eliciting);

        Ok(frames)
    }

    fn start_handshake(&mut self, packet: &PublicPacket, d: &Datagram) -> Res<()> {
        if self.role == Role::Server {
            assert_eq!(packet.packet_type(), PacketType::Initial);
            // A server needs to accept the client's selected CID during the handshake.
            self.valid_cids.push(ConnectionId::from(packet.dcid()));
            // Install a path.
            assert!(self.path.is_none());
            let mut p = Path::from_datagram(&d, ConnectionId::from(packet.scid()));
            p.add_local_cid(self.cid_manager.borrow_mut().generate_cid());
            self.path = Some(p);

            self.zero_rtt_state = match self.crypto.enable_0rtt(self.role) {
                Ok(true) => {
                    qdebug!([self], "Accepted 0-RTT");
                    ZeroRttState::AcceptedServer
                }
                _ => ZeroRttState::Rejected,
            };
        } else {
            qdebug!([self], "Changing to use Server CID={}", packet.scid());
            let p = self
                .path
                .iter_mut()
                .find(|p| p.received_on(&d))
                .expect("should have a path for sending Initial");
            p.set_remote_cid(packet.scid());
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
        if let Some(mut path) = self.path.take() {
            let res = match &self.state {
                State::Init
                | State::WaitInitial
                | State::Handshaking
                | State::Connected
                | State::Confirmed => self.output_path(&mut path, now),
                State::Closing {
                    error,
                    frame_type,
                    msg,
                    ..
                } => {
                    let err = error.clone();
                    let frame_type = *frame_type;
                    let msg = msg.clone();
                    self.output_close(&path, err, frame_type, msg)
                }
                State::Closed(_) => Ok(None),
            };
            let out = self.absorb_error(now, res).unwrap_or(None);
            self.path = Some(path);
            out
        } else {
            None
        }
    }

    fn build_packet_header(
        path: &Path,
        space: PNSpace,
        encoder: Encoder,
        tx: &CryptoDxState,
        retry_info: &Option<RetryInfo>,
    ) -> (PacketType, PacketNumber, PacketBuilder) {
        let pt = match space {
            PNSpace::Initial => PacketType::Initial,
            PNSpace::Handshake => PacketType::Handshake,
            PNSpace::ApplicationData => {
                if tx.is_0rtt() {
                    PacketType::ZeroRtt
                } else {
                    PacketType::Short
                }
            }
        };
        let mut builder = if pt == PacketType::Short {
            qdebug!("Building Short dcid {}", path.remote_cid());
            PacketBuilder::short(encoder, tx.key_phase(), path.remote_cid())
        } else {
            qdebug!(
                "Building {:?} dcid {} scid {}",
                pt,
                path.remote_cid(),
                path.local_cid(),
            );

            PacketBuilder::long(encoder, pt, path.remote_cid(), path.local_cid())
        };
        if pt == PacketType::Initial {
            builder.initial_token(if let Some(info) = retry_info {
                qtrace!("Initial token {}", hex(&info.token));
                &info.token
            } else {
                &[]
            });
        }
        // TODO(mt) work out packet number length based on `4*path CWND/path MTU`.
        let pn = tx.next_pn();
        builder.pn(pn, 3);
        (pt, pn, builder)
    }

    fn output_close(
        &mut self,
        path: &Path,
        error: ConnectionError,
        frame_type: FrameType,
        msg: String,
    ) -> Res<Option<Datagram>> {
        if !self.state_signaling.closing() {
            return Ok(None);
        }
        let mut close_sent = false;
        let mut encoder = Encoder::with_capacity(path.mtu());
        for space in PNSpace::iter() {
            let tx = if let Some(tx_state) = self.crypto.states.tx(*space) {
                tx_state
            } else {
                continue;
            };

            // ConnectionClose frame not allowed for 0RTT.
            if tx.is_0rtt() {
                continue;
            }

            // ConnectionError::Application only allowed at 1RTT.
            if *space != PNSpace::ApplicationData
                && matches!(error, ConnectionError::Application(_))
            {
                continue;
            }
            let (_, _, mut builder) = Self::build_packet_header(path, *space, encoder, tx, &None);
            let frame = Frame::ConnectionClose {
                error_code: error.clone().into(),
                frame_type,
                reason_phrase: Vec::from(msg.clone()),
            };
            frame.marshal(&mut builder);
            encoder = builder.build(tx)?;
            close_sent = true;
        }

        if close_sent {
            self.state_signaling.close_sent();
        }
        Ok(Some(path.datagram(encoder)))
    }

    /// Add frames to the provided builder and
    /// return whether any of them were ACK eliciting.
    #[allow(clippy::useless_let_if_seq)]
    fn add_frames(
        &mut self,
        builder: &mut PacketBuilder,
        space: PNSpace,
        limit: usize,
        cc_limited: bool,
        now: Instant,
    ) -> (Vec<RecoveryToken>, bool) {
        let mut tokens = Vec::new();
        let mut ack_eliciting = false;
        // All useful frames are at least 2 bytes.
        while builder.len() + 2 < limit {
            let remaining = limit - builder.len();
            // Try to get a frame from frame sources
            let mut frame = self.acks.get_frame(now, space);
            // If we are cc limited we can only send acks!
            if !cc_limited {
                if frame.is_none() && space == PNSpace::ApplicationData && self.role == Role::Server
                {
                    frame = self.state_signaling.send_done();
                }
                if frame.is_none() {
                    frame = self.crypto.streams.get_frame(space, remaining)
                }
                if frame.is_none() {
                    frame = self.flow_mgr.borrow_mut().get_frame(space, remaining);
                }
                if frame.is_none() {
                    frame = self.send_streams.get_frame(space, remaining);
                }
            }

            if let Some((frame, token)) = frame {
                ack_eliciting |= frame.ack_eliciting();
                debug_assert_ne!(frame, Frame::Padding);
                frame.marshal(builder);
                if let Some(t) = token {
                    tokens.push(t);
                }
            } else {
                return (tokens, ack_eliciting);
            }
        }
        (tokens, ack_eliciting)
    }

    /// Build a datagram, possibly from multiple packets (for different PN
    /// spaces) and each containing 1+ frames.
    fn output_path(&mut self, path: &mut Path, now: Instant) -> Res<Option<Datagram>> {
        let mut initial_sent = None;
        let mut needs_padding = false;

        // Check whether we are sending packets in PTO mode.
        let (pto, cong_avail, min_pn_space, cc_limited) =
            if let Some((min_pto_pn_space, can_send)) = self.loss_recovery.check_pto() {
                if !can_send {
                    return Ok(None);
                }
                (true, path.mtu(), min_pto_pn_space, false)
            } else if self.loss_recovery.cwnd_avail() < MIN_CC_WINDOW {
                // If avail == 0 we do not have available congestion window, we may send only
                // non-congestion controlled frames
                (false, path.mtu(), PNSpace::Initial, true)
            } else {
                (
                    false,
                    self.loss_recovery.cwnd_avail(),
                    PNSpace::Initial,
                    false,
                )
            };

        // Frames for different epochs must go in different packets, but then these
        // packets can go in a single datagram
        let mut encoder = Encoder::with_capacity(path.mtu());
        for space in PNSpace::iter() {
            // Ensure we have tx crypto state for this epoch, or skip it.
            let tx = if let Some(tx_state) = self.crypto.states.tx(*space) {
                tx_state
            } else {
                continue;
            };

            let header_start = encoder.len();
            let (pt, pn, mut builder) =
                Self::build_packet_header(path, *space, encoder, tx, &self.retry_info);
            let payload_start = builder.len();

            // Work out how much space we have in the congestion window.
            let limit = min(path.mtu(), cong_avail);
            if builder.len() + tx.expansion() > limit {
                // No space for a packet of this type in the congestion window.
                encoder = builder.abort();
                continue;
            }
            let limit = limit - tx.expansion();

            debug_assert!(!(pto && cc_limited));

            // Add frames to the packet.
            let (tokens, ack_eliciting) = if *space >= min_pn_space {
                let r = self.add_frames(&mut builder, *space, limit, cc_limited, now);
                if builder.is_empty() {
                    if pto {
                        // Add a PING if there is a PTO and nothing to send.
                        builder.encode_varint(Frame::Ping.get_type());
                        (Vec::new(), true)
                    } else {
                        // Nothing to include in this packet.
                        encoder = builder.abort();
                        continue;
                    }
                } else {
                    r
                }
            } else {
                // A higher packet number space has a PTO; only add a PING.
                builder.encode_varint(Frame::Ping.get_type());
                (Vec::new(), true)
            };

            dump_packet(self, "TX ->", pt, pn, &builder[payload_start..]);

            qdebug!("Need to send a packet: {:?}", pt);

            self.stats.packets_tx += 1;
            encoder = builder.build(self.crypto.states.tx(*space).unwrap())?;
            debug_assert!(encoder.len() <= path.mtu());

            if !pto && ack_eliciting {
                self.idle_timeout.on_packet_sent(now);
            }

            // Normal packets are in flight if they include PADDING frames,
            // but we don't send those.
            let in_flight = !pto && ack_eliciting;

            let sent = SentPacket::new(
                now,
                ack_eliciting,
                tokens,
                encoder.len() - header_start,
                in_flight,
            );
            if pt == PacketType::Initial && self.role == Role::Client {
                // Packets containing Initial packets might need padding, and we want to
                // track that padding along with the Initial packet.  So defer tracking.
                initial_sent = Some((pn, sent));
                needs_padding = true;
            } else {
                if pt != PacketType::ZeroRtt {
                    needs_padding = false;
                }
                self.loss_recovery.on_packet_sent(*space, pn, sent);
            }

            if *space == PNSpace::Handshake {
                if self.role == Role::Client {
                    // Client can send Handshake packets -> discard Initial keys and states
                    self.discard_keys(PNSpace::Initial);
                } else if self.state == State::Confirmed {
                    // We could discard handshake keys in set_state, but wait until after sending an ACK.
                    self.discard_keys(PNSpace::Handshake);
                }
            }
        }

        if encoder.is_empty() {
            assert!(!pto);
            Ok(None)
        } else {
            // Pad Initial packets sent by the client to mtu bytes.
            let mut packets: Vec<u8> = encoder.into();
            if let Some((initial_pn, mut initial)) = initial_sent.take() {
                if needs_padding {
                    qdebug!([self], "pad Initial to path MTU {}", path.mtu());
                    initial.size += path.mtu() - packets.len();
                    packets.resize(path.mtu(), 0);
                }
                self.loss_recovery
                    .on_packet_sent(PNSpace::Initial, initial_pn, initial);
            }
            Ok(Some(path.datagram(packets)))
        }
    }

    pub fn initiate_key_update(&mut self) -> Res<()> {
        if self.state == State::Confirmed {
            let la = self
                .loss_recovery
                .largest_acknowledged_pn(PNSpace::ApplicationData);
            qinfo!([self], "Initiating key update");
            self.crypto.states.initiate_key_update(la)
        } else {
            Err(Error::NotConnected)
        }
    }

    #[cfg(test)]
    pub fn get_epochs(&self) -> (Option<usize>, Option<usize>) {
        self.crypto.states.get_epochs()
    }

    fn client_start(&mut self, now: Instant) -> Res<()> {
        qinfo!([self], "client_start");
        self.handshake(now, PNSpace::Initial, None)?;
        self.set_state(State::WaitInitial);
        self.zero_rtt_state = if self.crypto.enable_0rtt(self.role)? {
            qdebug!([self], "Enabled 0-RTT");
            ZeroRttState::Sending
        } else {
            ZeroRttState::Init
        };
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
            StreamIndex::new(remote.get_integer(tparams::INITIAL_MAX_STREAMS_BIDI));
        self.indexes.remote_max_stream_uni =
            StreamIndex::new(remote.get_integer(tparams::INITIAL_MAX_STREAMS_UNI));
        self.flow_mgr
            .borrow_mut()
            .conn_increase_max_credit(remote.get_integer(tparams::INITIAL_MAX_DATA));
    }

    fn validate_odcid(&mut self) -> Res<()> {
        // Here we drop our Retry state then validate it.
        if let Some(info) = self.retry_info.take() {
            if info.token.is_empty() {
                Ok(())
            } else {
                let tph = self.tps.borrow();
                let tp = tph.remote().get_bytes(tparams::ORIGINAL_CONNECTION_ID);
                if let Some(odcid_tp) = tp {
                    if odcid_tp[..] == info.odcid[..] {
                        Ok(())
                    } else {
                        Err(Error::InvalidRetry)
                    }
                } else {
                    Err(Error::InvalidRetry)
                }
            }
        } else {
            debug_assert_eq!(self.role, Role::Server);
            Ok(())
        }
    }

    fn handshake(&mut self, now: Instant, space: PNSpace, data: Option<&[u8]>) -> Res<()> {
        qtrace!("Handshake space={} data={:0x?}", space, data);

        let rec = data.map(|d| {
            qtrace!([self], "Handshake received {:0x?} ", d);
            Record {
                ct: 22, // TODO(ekr@rtfm.com): Symbolic constants for CT. This is handshake.
                epoch: space.into(),
                data: d.to_vec(),
            }
        });
        let try_update = rec.is_some();

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

        match self.crypto.tls.state() {
            HandshakeState::Authenticated(_) | HandshakeState::InProgress => (),
            HandshakeState::AuthenticationPending => self.events.authentication_needed(),
            HandshakeState::Complete(_) => {
                if !self.state.connected() {
                    self.set_connected(now)?;
                }
            }
            _ => {
                unreachable!("Crypto state should not be new or failed after successful handshake")
            }
        }
        // There is a chance that this could be called less often, but getting the
        // conditions right is a little tricky, so call it on every  CRYPTO frame.
        if try_update {
            self.crypto.install_keys(self.role);
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

    fn input_frame(&mut self, ptype: PacketType, frame: Frame, now: Instant) -> Res<()> {
        if !frame.is_allowed(ptype) {
            qerror!("frame not allowed: {:?} {:?}", frame, ptype);
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
                    PNSpace::from(ptype),
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
                let space = PNSpace::from(ptype);
                qtrace!(
                    [self],
                    "Crypto frame on space={} offset={}, data={:0x?}",
                    space,
                    offset,
                    &data
                );
                self.crypto.streams.inbound_frame(space, offset, data)?;
                if self.crypto.streams.data_ready(space) {
                    let mut buf = Vec::new();
                    let read = self.crypto.streams.read_to_end(space, &mut buf)?;
                    qdebug!("Read {} bytes", read);
                    self.handshake(now, space, Some(&buf))?;
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
                qerror!(
                    [self],
                    "ConnectionClose received. Error code: {:?} frame type {:x} reason {}",
                    error_code,
                    frame_type,
                    reason_phrase
                );
                self.set_state(State::Closed(error_code.into()));
            }
            Frame::HandshakeDone => {
                if self.role == Role::Server || !self.state.connected() {
                    return Err(Error::ProtocolViolation);
                }
                self.set_state(State::Confirmed);
                self.discard_keys(PNSpace::Handshake);
            }
        };

        Ok(())
    }

    /// Given a set of `SentPacket` instances, ensure that the source of the packet
    /// is told that they are lost.  This gives the frame generation code a chance
    /// to retransmit the frame as needed.
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
                    RecoveryToken::HandshakeDone => self.state_signaling.handshake_done(),
                }
            }
        }
    }

    fn handle_ack(
        &mut self,
        space: PNSpace,
        largest_acknowledged: u64,
        ack_delay: u64,
        first_ack_range: u64,
        ack_ranges: Vec<AckRange>,
        now: Instant,
    ) -> Res<()> {
        qinfo!(
            [self],
            "Rx ACK space={}, largest_acked={}, first_ack_range={}, ranges={:?}",
            space,
            largest_acknowledged,
            first_ack_range,
            ack_ranges
        );

        let acked_ranges =
            Frame::decode_ack_frame(largest_acknowledged, first_ack_range, ack_ranges)?;
        let (acked_packets, lost_packets) = self.loss_recovery.on_ack_received(
            space,
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
                    RecoveryToken::HandshakeDone => (),
                }
            }
        }
        self.handle_lost_packets(&lost_packets);
        Ok(())
    }

    /// When the server rejects 0-RTT we need to drop a bunch of stuff.
    fn client_0rtt_rejected(&mut self) {
        if !matches!(self.zero_rtt_state, ZeroRttState::Sending) {
            return;
        }
        qdebug!([self], "0-RTT rejected");

        // Tell 0-RTT packets that they were "lost".
        let dropped = self.loss_recovery.drop_0rtt();
        self.handle_lost_packets(&dropped);

        self.send_streams.clear();
        self.recv_streams.clear();
        self.indexes = StreamIndexes::new();
        self.crypto.states.discard_0rtt_keys();
        self.events.client_0rtt_rejected();
    }

    fn set_connected(&mut self, now: Instant) -> Res<()> {
        qinfo!([self], "TLS connection complete");
        if self.crypto.tls.info().map(SecretAgentInfo::alpn).is_none() {
            qwarn!([self], "No ALPN. Closing connection.");
            // 120 = no_application_protocol
            return Err(Error::CryptoAlert(120));
        }
        if self.role == Role::Server {
            // Remove the randomized client CID from the list of acceptable CIDs.
            assert_eq!(1, self.valid_cids.len());
            self.valid_cids.clear();
        } else {
            self.zero_rtt_state = if self.crypto.tls.info().unwrap().early_data_accepted() {
                ZeroRttState::AcceptedClient
            } else {
                self.client_0rtt_rejected();
                ZeroRttState::Rejected
            };
        }

        // Setting application keys has to occur after 0-RTT rejection.
        let pto = self.loss_recovery.pto();
        self.crypto.install_application_keys(now + pto)?;
        self.validate_odcid()?;
        self.set_initial_limits();
        self.set_state(State::Connected);
        if self.role == Role::Server {
            self.state_signaling.handshake_done();
            self.set_state(State::Confirmed);
        }
        qinfo!([self], "Connection established");
        Ok(())
    }

    fn set_state(&mut self, state: State) {
        if state > self.state {
            qinfo!([self], "State change from {:?} -> {:?}", self.state, state);
            self.state = state.clone();
            match &self.state {
                State::Closing { .. } => {
                    self.send_streams.clear();
                    self.recv_streams.clear();
                    self.state_signaling.close();
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
        if !self.state.connected()
            && !matches!(
                (&self.state, &self.zero_rtt_state),
                (State::Handshaking, ZeroRttState::AcceptedServer)
            )
        {
            return Err(Error::ConnectionState);
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
                        .get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE)
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
                        .get_integer(tparams::INITIAL_MAX_STREAM_DATA_UNI)
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
                            .get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL);
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
                if !matches!(self.zero_rtt_state, ZeroRttState::Sending) {
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
                    .get_integer(tparams::INITIAL_MAX_STREAM_DATA_UNI);

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
                    .get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE);

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
                    .get_integer(tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL);

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
}

impl ::std::fmt::Display for Connection {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{:?} {:p}", self.role, self as *const Self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::cc::{INITIAL_CWND_PKTS, MIN_CONG_WINDOW};
    use crate::frame::{CloseError, StreamType};
    use crate::path::PATH_MTU_V6;

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
        Connection::new_client(
            test_fixture::DEFAULT_SERVER_NAME,
            test_fixture::DEFAULT_ALPN,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(3))),
            loopback(),
            loopback(),
        )
        .expect("create a default client")
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
        assert_eq!(out.as_dgram_ref().unwrap().len(), PATH_MTU_V6);
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
        assert_eq!(*client.state(), State::Connected);

        qdebug!("---- server: FIN -> ACKS");
        let out = server.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_some());
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        assert_eq!(*server.state(), State::Confirmed);

        qdebug!("---- client: ACKS -> 0");
        let out = client.process(out.dgram(), now());
        assert!(out.as_dgram_ref().is_none());
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        assert_eq!(*client.state(), State::Confirmed);
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
        assert_eq!(*server.state(), State::Confirmed);
        qdebug!("Output={:0x?}", out.as_dgram_ref());
        // ACK and HANDSHAKE_DONE
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
        assert_eq!(*client.state(), State::Confirmed);

        qdebug!("---- server");
        let mut expect_ack = false;
        for d in datagrams {
            let out = server.process(Some(d), now());
            assert_eq!(out.as_dgram_ref().is_some(), expect_ack); // ACK every second.
            qdebug!("Output={:0x?}", out.as_dgram_ref());
            expect_ack = !expect_ack;
        }
        assert_eq!(*server.state(), State::Confirmed);

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
            State::Confirmed | State::Closing { .. } | State::Closed(..) => true,
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
        assert_eq!(*client.state(), State::Confirmed);
        assert_eq!(*server.state(), State::Confirmed);
    }

    fn assert_error(c: &Connection, err: ConnectionError) {
        match c.state() {
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
        assert_eq!(out.as_dgram_ref().unwrap().len(), PATH_MTU_V6);
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

        // Four packets total received, 1 of them is a dup and one has been dropped because Initial keys
        // are dropped.
        assert_eq!(4, client.stats().packets_rx);
        assert_eq!(1, client.stats().dups_rx);
        assert_eq!(1, client.stats().dropped_rx);
    }

    fn exchange_ticket(client: &mut Connection, server: &mut Connection) -> Vec<u8> {
        server.send_ticket(now(), &[]).expect("can send ticket");
        let ticket = server.process_output(now()).dgram();
        assert!(ticket.is_some());
        client.process_input(ticket.unwrap(), now());
        assert_eq!(*client.state(), State::Confirmed);
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
                PNSpace::ApplicationData,
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
        assert!(server_out.as_dgram_ref().is_none()); // suppress the ack
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

    /// Getting the client and server to reach an idle state is surprisingly hard.
    /// The server sends HANDSHAKE_DONE at the end of the handshake, and the client
    /// doesn't immediately acknowledge it.

    /// Force the client to send an ACK by having the server send two packets out
    /// of order.
    fn connect_force_idle(client: &mut Connection, server: &mut Connection) {
        connect(client, server);
        let p1 = send_something(server, now());
        let p2 = send_something(server, now());
        client.process_input(p2, now());
        // Now the client really wants to send an ACK, but hold it back.
        let ack = client.process(Some(p1), now()).dgram();
        assert!(ack.is_some());
        // Now the server has its ACK and both should be idle.
        assert_eq!(
            server.process(ack, now()),
            Output::Callback(LOCAL_IDLE_TIMEOUT)
        );
        assert_eq!(
            client.process_output(now()),
            Output::Callback(LOCAL_IDLE_TIMEOUT)
        );
    }

    #[test]
    fn idle_timeout() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        // Still connected after 59 seconds. Idle timer not reset
        client.process(None, now + Duration::from_secs(59));
        assert!(matches!(client.state(), State::Confirmed));

        client.process_timer(now + Duration::from_secs(60));

        // Not connected after 60 seconds.
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn idle_send_packet1() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

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
        assert!(matches!(client.state(), State::Confirmed));

        // Not connected after 70 seconds.
        client.process_timer(now + Duration::from_secs(70));
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn idle_send_packet2() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

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
        assert!(matches!(client.state(), State::Confirmed));

        // Not connected after 70 seconds because timer not reset by second
        // outgoing packet
        client.process_timer(now + Duration::from_secs(70));
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn idle_recv_packet() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

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
        assert!(matches!(client.state(), State::Confirmed));

        // Still connected after 79 seconds.
        client.process_timer(now + Duration::from_secs(79));
        assert!(matches!(client.state(), State::Confirmed));

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
                tparams::INITIAL_MAX_DATA,
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
            usize::try_from(SMALL_MAX_DATA).unwrap()
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
        assert_eq!(*server.state(), State::Confirmed);
    }

    #[test]
    fn set_local_tparam() {
        let client = default_client();

        client
            .set_local_tparam(tparams::INITIAL_MAX_DATA, TransportParameter::Integer(55))
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
        connect_force_idle(&mut client, &mut server);

        let mut now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        // Send data on two streams
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"hello").unwrap(), 5);
        assert_eq!(client.stream_send(2, b" world").unwrap(), 6);

        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 6);
        assert_eq!(client.stream_send(6, b"there!").unwrap(), 6);

        // Send a packet after some time.
        now += Duration::from_secs(10);
        let out = client.process(None, now);
        assert!(out.dgram().is_some());

        // Nothing to do, should return callback
        let out = client.process(None, now);
        assert!(matches!(out, Output::Callback(_)));

        // One second later, it should want to send PTO packet
        now += Duration::from_secs(1);
        let out = client.process(None, now);

        let frames = server.test_process_input(out.dgram().unwrap(), now);

        assert!(matches!(
            frames[0],
            (Frame::Stream { .. }, PNSpace::ApplicationData)
        ));
        assert!(matches!(
            frames[1],
            (Frame::Stream { .. }, PNSpace::ApplicationData)
        ));
    }

    #[test]
    fn pto_works_full_cwnd() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let mut now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        // Send lots of data.
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        let _dgrams = send_bytes(&mut client, 2, now);
        // TODO assert_full_cwnd()

        // Wait for the PTO.
        now += Duration::from_secs(1);
        client.process_timer(now); // TODO merge process_timer into process_output.
        let dgrams = send_bytes(&mut client, 2, now);
        assert_eq!(dgrams.len(), 2); // Two packets in the PTO.

        // All (2) datagrams contain STREAM frames.
        for d in dgrams {
            assert_eq!(d.len(), PATH_MTU_V6);
            let frames = server.test_process_input(d, now);
            assert!(matches!(
                frames[0],
                (Frame::Stream { .. }, PNSpace::ApplicationData)
            ));
        }
    }

    #[test]
    #[allow(clippy::cognitive_complexity)]
    fn pto_works_ping() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

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

        assert_eq!(frames[0], (Frame::Ping, PNSpace::ApplicationData));
    }

    #[test]
    fn pto_initial() {
        let mut now = now();

        qdebug!("---- client: generate CH");
        let mut client = default_client();
        let pkt1 = client.process(None, now).dgram();
        assert!(pkt1.is_some());
        assert_eq!(pkt1.clone().unwrap().len(), PATH_MTU_V6);

        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_millis(120)));

        // Resend initial after PTO.
        now += Duration::from_millis(120);
        let pkt2 = client.process(None, now).dgram();
        assert!(pkt2.is_some());
        assert_eq!(pkt2.unwrap().len(), PATH_MTU_V6);

        let pkt3 = client.process(None, now).dgram();
        assert!(pkt3.is_some());
        assert_eq!(pkt3.unwrap().len(), PATH_MTU_V6);

        let out = client.process(None, now);
        // PTO has doubled.
        assert_eq!(out, Output::Callback(Duration::from_millis(240)));

        // Server process the first initial pkt.
        let mut server = default_server();
        let out = server.process(pkt1, now).dgram();
        assert!(out.is_some());

        // Client receives ack for the first initial packet as well a Handshake packet.
        // After the handshake packet the initial keys and the crypto stream for the initial
        // packet number space will be discarded.
        // Here only an ack for the Handshake packet will be sent.
        now += Duration::from_millis(10);
        let out = client.process(out, now).dgram();
        assert!(out.is_some());

        // We do not have PTO for the resent initial packet any more, because keys are discarded.
        // The timeout will be an idle time out of 60s
        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_secs(60)));
    }

    #[test]
    fn pto_handshake() {
        let mut now = now();
        // start handshake
        let mut client = default_client();
        let mut server = default_server();

        let pkt = client.process(None, now).dgram();
        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_millis(120)));

        now += Duration::from_millis(10);
        let pkt = server.process(pkt, now).dgram();

        now += Duration::from_millis(10);
        let pkt = client.process(pkt, now).dgram();

        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_secs(60)));

        now += Duration::from_millis(10);
        let pkt = server.process(pkt, now).dgram();
        assert!(pkt.is_none());

        now += Duration::from_millis(10);
        client.authenticated(AuthenticationStatus::Ok, now);

        qdebug!("---- client: SH..FIN -> FIN");
        let pkt1 = client.process(None, now).dgram();
        assert!(pkt1.is_some());

        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_millis(60)));

        // Wait for PTO to expire and resend a handshake packet
        now += Duration::from_millis(60);
        let pkt2 = client.process(None, now).dgram();
        assert!(pkt2.is_some());

        // Get a second PTO packet.
        let pkt3 = client.process(None, now).dgram();
        assert!(pkt3.is_some());

        // PTO has been doubled.
        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_millis(120)));

        now += Duration::from_millis(10);
        // Server receives the first packet.
        // The output will be a Handshake packet with an ack and a app pn space packet with
        // HANDSHAKE_DONE.
        let pkt = server.process(pkt1, now).dgram();
        assert!(pkt.is_some());

        // Check that the PTO packets (pkt2, pkt3) have a Handshake and an app pn space packet.
        // The server has discarded the Handshake keys already, therefore the handshake packet
        // will be dropped.
        let dropped_before = server.stats().dropped_rx;
        let frames = server.test_process_input(pkt2.unwrap(), now);
        assert_eq!(1, server.stats().dropped_rx - dropped_before);
        assert!(matches!(frames[0], (Frame::Ping, PNSpace::ApplicationData)));

        let dropped_before = server.stats().dropped_rx;
        let frames = server.test_process_input(pkt3.unwrap(), now);
        assert_eq!(1, server.stats().dropped_rx - dropped_before);
        assert!(matches!(frames[0], (Frame::Ping, PNSpace::ApplicationData)));

        now += Duration::from_millis(10);
        // Client receive ack for the first packet
        let out = client.process(pkt, now);
        // Ack delay timer for the packet carrying HANDSHAKE_DONE.
        assert_eq!(out, Output::Callback(Duration::from_millis(20)));

        // Let the ack timer expire.
        now += Duration::from_millis(20);
        let out = client.process(None, now).dgram();
        assert!(out.is_some());
        let out = client.process(None, now);
        // The handshake keys are discarded
        // Return PTO timer for an app pn space packet (when the Handshake PTO timer has expired,
        // a PING in the app pn space has been send as well).
        // pto=142.5ms, the PTO packet was sent 40ms ago. The timer will be 102.5ms.
        assert_eq!(out, Output::Callback(Duration::from_micros(102_500)));

        // Let PTO expire. We will send a PING only in the APP pn space, the client has discarded
        // Handshshake keys.
        now += Duration::from_micros(102_500);
        let out = client.process(None, now).dgram();
        assert!(out.is_some());

        now += Duration::from_millis(10);
        let frames = server.test_process_input(out.unwrap(), now);

        assert_eq!(frames[0], (Frame::Ping, PNSpace::ApplicationData));
    }

    #[test]
    fn test_pto_handshake_and_app_data() {
        let mut now = now();
        qdebug!("---- client: generate CH");
        let mut client = default_client();
        let pkt = client.process(None, now);

        now += Duration::from_millis(10);
        qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
        let mut server = default_server();
        let pkt = server.process(pkt.dgram(), now);

        now += Duration::from_millis(10);
        qdebug!("---- client: cert verification");
        let pkt = client.process(pkt.dgram(), now);

        now += Duration::from_millis(10);
        let _pkt = server.process(pkt.dgram(), now);

        now += Duration::from_millis(10);
        client.authenticated(AuthenticationStatus::Ok, now);

        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"zero").unwrap(), 4);
        qdebug!("---- client: SH..FIN -> FIN and 1RTT packet");
        let pkt1 = client.process(None, now).dgram();
        assert!(pkt1.is_some());

        // Get PTO timer.
        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_millis(60)));

        // Wait for PTO o expire and resend a handshake and 1rtt packet
        now += Duration::from_millis(60);
        let pkt2 = client.process(None, now).dgram();
        assert!(pkt2.is_some());

        now += Duration::from_millis(10);
        let frames = server.test_process_input(pkt2.unwrap(), now);

        assert!(matches!(
            frames[0],
            (Frame::Crypto { .. }, PNSpace::Handshake)
        ));
        assert!(matches!(
            frames[1],
            (Frame::Stream { .. }, PNSpace::ApplicationData)
        ));
    }

    #[test]
    fn test_pto_count_increase_across_spaces() {
        let mut now = now();
        qdebug!("---- client: generate CH");
        let mut client = default_client();
        let pkt = client.process(None, now).dgram();

        now += Duration::from_millis(10);
        qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
        let mut server = default_server();
        let pkt = server.process(pkt, now).dgram();

        now += Duration::from_millis(10);
        qdebug!("---- client: cert verification");
        let pkt = client.process(pkt, now).dgram();

        now += Duration::from_millis(10);
        let _pkt = server.process(pkt, now);

        now += Duration::from_millis(10);
        client.authenticated(AuthenticationStatus::Ok, now);

        qdebug!("---- client: SH..FIN -> FIN");
        let pkt1 = client.process(None, now).dgram();
        assert!(pkt1.is_some());
        // Get PTO timer.
        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_millis(60)));

        now += Duration::from_millis(10);
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"zero").unwrap(), 4);
        qdebug!("---- client: 1RTT packet");
        let pkt2 = client.process(None, now).dgram();
        assert!(pkt2.is_some());

        // Get PTO timer. It is the timer for pkt1(handshake pn space).
        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_millis(50)));

        // Wait for PTO to expire and resend a handshake and 1rtt packet
        now += Duration::from_millis(50);
        let pkt3 = client.process(None, now).dgram();
        assert!(pkt3.is_some());
        let pkt4 = client.process(None, now).dgram();
        assert!(pkt4.is_some());

        // Get PTO timer. It is the timer for pkt2(app pn space). PTO has been doubled.
        // pkt2 has been sent 50ms ago (50 + 120 = 170 == 2*85)
        let out = client.process(None, now);
        assert_eq!(out, Output::Callback(Duration::from_millis(120)));

        // Wait for PTO to expire and resend a handshake and 1rtt packet
        now += Duration::from_millis(120);
        let pkt5 = client.process(None, now).dgram();
        assert!(pkt5.is_some());

        now += Duration::from_millis(10);
        let frames = server.test_process_input(pkt3.unwrap(), now);

        assert!(matches!(
            frames[0],
            (Frame::Crypto { .. }, PNSpace::Handshake)
        ));

        now += Duration::from_millis(10);
        let frames = server.test_process_input(pkt5.unwrap(), now);
        assert!(matches!(
            frames[1],
            (Frame::Stream { .. }, PNSpace::ApplicationData)
        ));
    }

    #[test]
    // Absent path PTU discovery, max v6 packet size should be PATH_MTU_V6.
    fn verify_pkt_honors_mtu() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(Duration::from_secs(60)));

        // Try to send a large stream and verify first packet is correctly sized
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, &[0xbb; 2000]).unwrap(), 2000);
        let pkt0 = client.process(None, now);
        assert!(matches!(pkt0, Output::Datagram(_)));
        assert_eq!(pkt0.as_dgram_ref().unwrap().len(), PATH_MTU_V6);
    }

    // Handle sending a bunch of bytes from one connection to another, until
    // something stops us from sending.
    fn send_bytes(src: &mut Connection, stream: u64, now: Instant) -> Vec<Datagram> {
        let mut total_dgrams = Vec::new();

        loop {
            let bytes_sent = src.stream_send(stream, &[0x42; 4_096]).unwrap();
            qtrace!("send_bytes wrote {} bytes", bytes_sent);
            if bytes_sent == 0 {
                break;
            }
        }

        loop {
            let pkt = src.process_output(now);
            qtrace!("send_bytes output: {:?}", pkt);
            match pkt {
                Output::Datagram(dgram) => {
                    total_dgrams.push(dgram);
                }
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

    /// This magic number is the size of the client's CWND after the handshake completes.
    /// This includes the initial congestion window, as increased as a result
    /// receiving acknowledgments for Initial and Handshake packets, which is
    /// at least one full packet (the first Initial) and a little extra.
    ///
    /// As we change how we build packets, or even as NSS changes,
    /// this number might be different.  The tests that depend on this
    /// value could fail as a result of variations, so it's OK to just
    /// change this value, but it is good to first understand where the
    /// change came from.
    const POST_HANDSHAKE_CWND: usize = PATH_MTU_V6 * (INITIAL_CWND_PKTS + 1) + 75;

    /// Determine the number of packets required to fill the CWND.
    const fn cwnd_packets(data: usize) -> usize {
        (data + MIN_CC_WINDOW - 1) / PATH_MTU_V6
    }

    /// Determin the size of the last packet.
    /// The minimal size of a packet is MIN_CC_WINDOW.
    fn last_packet(cwnd: usize) -> usize {
        if (cwnd % PATH_MTU_V6) > MIN_CC_WINDOW {
            cwnd % PATH_MTU_V6
        } else {
            PATH_MTU_V6
        }
    }

    /// Assert that the set of packets fill the CWND.
    fn assert_full_cwnd(packets: &[Datagram], cwnd: usize) {
        assert_eq!(packets.len(), cwnd_packets(cwnd));
        let (last, rest) = packets.split_last().unwrap();
        assert!(rest.iter().all(|d| d.len() == PATH_MTU_V6));
        assert_eq!(last.len(), last_packet(cwnd));
    }

    #[test]
    /// Verify initial CWND is honored.
    fn cc_slow_start() {
        let mut client = default_client();
        let mut server = default_server();

        server
            .set_local_tparam(
                tparams::INITIAL_MAX_DATA,
                TransportParameter::Integer(65536),
            )
            .unwrap();
        connect(&mut client, &mut server);

        let now = now();

        // Try to send a lot of data
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        let c_tx_dgrams = send_bytes(&mut client, 2, now);
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);
        assert!(client.loss_recovery.cwnd_avail() < MIN_CC_WINDOW);
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
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);
        let flight1_largest: PacketNumber = u64::try_from(c_tx_dgrams.len()).unwrap() - 1;

        // Server: Receive and generate ack
        let (s_tx_dgram, _recvd_frames) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // Client: Process ack
        let recvd_frames = client.test_process_input(s_tx_dgram, now);

        // Verify that server-sent frame was what we thought.
        if let (
            Frame::Ack {
                largest_acknowledged,
                ..
            },
            PNSpace::ApplicationData,
        ) = recvd_frames[0]
        {
            assert_eq!(largest_acknowledged, flight1_largest);
        } else {
            panic!("Expected an application ACK");
        }

        // Client: send more
        let mut c_tx_dgrams = send_bytes(&mut client, 0, now);
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND * 2);
        let flight2_largest = flight1_largest + u64::try_from(c_tx_dgrams.len()).unwrap();

        // Server: Receive and generate ack again, but drop first packet
        c_tx_dgrams.remove(0);
        let (s_tx_dgram, _recvd_frames) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // Client: Process ack
        let recvd_frames = client.test_process_input(s_tx_dgram, now);

        // Verify that server-sent frame was what we thought.
        if let (
            Frame::Ack {
                largest_acknowledged,
                ..
            },
            PNSpace::ApplicationData,
        ) = recvd_frames[0]
        {
            assert_eq!(largest_acknowledged, flight2_largest);
        } else {
            panic!("Expected an application ACK");
        }

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
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

        // Drop 0th packet. When acked, this should put client into CARP.
        c_tx_dgrams.remove(0);

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

    fn induce_persistent_congestion(
        client: &mut Connection,
        server: &mut Connection,
        mut now: Instant,
    ) -> Instant {
        // Note: wait some arbitrary time that should be longer than pto
        // timer. This is rather brittle.
        now += Duration::from_secs(1);

        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

        now += Duration::from_secs(2);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

        now += Duration::from_secs(4);
        client.process_timer(now); // Should enter PTO mode
        let c_tx_dgrams = send_bytes(client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

        // Generate ACK
        let (s_tx_dgram, _) = ack_bytes(server, 0, c_tx_dgrams, now);

        // In PC now.
        client.test_process_input(s_tx_dgram, now);

        assert_eq!(client.loss_recovery.cwnd(), MIN_CONG_WINDOW);
        now
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
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

        // Server: Receive and generate ack
        now += Duration::from_millis(100);
        let (_s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // ACK lost.

        induce_persistent_congestion(&mut client, &mut server, now);
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
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

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

        induce_persistent_congestion(&mut client, &mut server, now);
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
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

        // Server: Receive and generate ack
        now += Duration::from_millis(100);
        let (_s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // ACK lost.

        now = induce_persistent_congestion(&mut client, &mut server, now);

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

    fn check_discarded(peer: &mut Connection, pkt: Datagram, dropped: usize, dups: usize) {
        let dropped_before = peer.stats.dropped_rx;
        let dups_before = peer.stats.dups_rx;
        let out = peer.process(Some(pkt), now());
        assert!(out.as_dgram_ref().is_none());
        assert_eq!(dropped, peer.stats.dropped_rx - dropped_before);
        assert_eq!(dups, peer.stats.dups_rx - dups_before);
    }

    #[test]
    fn discarded_initial_keys() {
        qdebug!("---- client: generate CH");
        let mut client = default_client();
        let init_pkt_c = client.process(None, now()).dgram();
        assert!(init_pkt_c.is_some());
        assert_eq!(init_pkt_c.as_ref().unwrap().len(), PATH_MTU_V6);

        qdebug!("---- server: CH -> SH, EE, CERT, CV, FIN");
        let mut server = default_server();
        let init_pkt_s = server.process(init_pkt_c.clone(), now()).dgram();
        assert!(init_pkt_s.is_some());

        qdebug!("---- client: cert verification");
        let out = client.process(init_pkt_s.clone(), now()).dgram();
        assert!(out.is_some());

        // The client has received handshake packet. It will remove the Initial keys.
        // We will check this by processing init_pkt_s a second time.
        // The initial packet should be dropped. The packet contains a Handshake packet as well, which
        // will be marked as dup.
        check_discarded(&mut client, init_pkt_s.unwrap(), 1, 1);

        assert!(maybe_authenticate(&mut client));

        // The server has not removed the Initial keys yet, because it has not yet received a Handshake
        // packet from the client.
        // We will check this by processing init_pkt_c a second time.
        // The dropped packet is padding. The Initial packet has been mark dup.
        check_discarded(&mut server, init_pkt_c.clone().unwrap(), 1, 1);

        qdebug!("---- client: SH..FIN -> FIN");
        let out = client.process(None, now()).dgram();
        assert!(out.is_some());

        // The server will process the first Handshake packet.
        // After this the Initial keys will be dropped.
        let out = server.process(out, now()).dgram();
        assert!(out.is_some());

        // Check that the Initial keys are dropped at the server
        // We will check this by processing init_pkt_c a third time.
        // The Initial packet has been dropped and padding that follows it.
        // There is no dups, everything has been dropped.
        check_discarded(&mut server, init_pkt_c.unwrap(), 1, 0);
    }

    /// Send something on a stream from `sender` to `receiver`.
    /// Return the resulting datagram.
    fn send_something(sender: &mut Connection, now: Instant) -> Datagram {
        let stream_id = sender.stream_create(StreamType::UniDi).unwrap();
        assert!(sender.stream_send(stream_id, b"data").is_ok());
        assert!(sender.stream_close_send(stream_id).is_ok());
        let dgram = sender.process(None, now).dgram();
        dgram.expect("should have something to send")
    }

    /// Send something on a stream from `sender` to `receiver`.
    /// Return any ACK that might result.
    fn send_and_receive(
        sender: &mut Connection,
        receiver: &mut Connection,
        now: Instant,
    ) -> Option<Datagram> {
        let dgram = send_something(sender, now);
        receiver.process(Some(dgram), now).dgram()
    }

    #[test]
    fn key_update_client() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);
        let mut now = now();

        // Both client and server should be idle now.
        assert_eq!(
            Output::Callback(LOCAL_IDLE_TIMEOUT),
            client.process(None, now)
        );
        assert_eq!(
            Output::Callback(LOCAL_IDLE_TIMEOUT),
            server.process(None, now)
        );
        assert_eq!(client.get_epochs(), (Some(3), Some(3))); // (write, read)
        assert_eq!(server.get_epochs(), (Some(3), Some(3)));

        // TODO(mt) this needs to wait for handshake confirmation,
        // but for now, we can do this immediately.
        let res = client.initiate_key_update();
        assert!(res.is_ok());
        let res = client.initiate_key_update();
        assert!(res.is_err());

        // Initiating an update should only increase the write epoch.
        assert_eq!(
            Output::Callback(LOCAL_IDLE_TIMEOUT),
            client.process(None, now)
        );
        assert_eq!(client.get_epochs(), (Some(4), Some(3)));

        // Send something to propagate the update.
        assert!(send_and_receive(&mut client, &mut server, now).is_none());

        // The server should now be waiting to discharge read keys.
        assert_eq!(server.get_epochs(), (Some(4), Some(3)));
        let res = server.process(None, now);
        if let Output::Callback(t) = res {
            assert!(t < LOCAL_IDLE_TIMEOUT);
        } else {
            panic!("server should now be waiting to clear keys");
        }

        // Without having had time to purge old keys, more updates are blocked.
        // The spec would permits it at this point, but we are more conservative.
        assert!(client.initiate_key_update().is_err());
        // The server can't update until it receives an ACK for a packet.
        assert!(server.initiate_key_update().is_err());

        // Waiting now for at least a PTO should cause the server to drop old keys.
        // But at this point the client hasn't received a key update from the server.
        // It will be stuck with old keys.
        now += Duration::from_secs(1);
        client.process_timer(now);
        assert_eq!(client.get_epochs(), (Some(4), Some(3)));
        server.process_timer(now);
        assert_eq!(server.get_epochs(), (Some(4), Some(4)));

        // Even though the server has updated, it hasn't received an ACK yet.
        assert!(server.initiate_key_update().is_err());

        // Now get an ACK from the server.
        let dgram = send_and_receive(&mut client, &mut server, now);
        assert!(dgram.is_some());
        let res = client.process(dgram, now);
        // This is the first packet that the client has received from the server
        // with new keys, so its read timer just started.
        if let Output::Callback(t) = res {
            assert!(t < LOCAL_IDLE_TIMEOUT);
        } else {
            panic!("client should now be waiting to clear keys");
        }

        assert!(client.initiate_key_update().is_err());
        assert_eq!(client.get_epochs(), (Some(4), Some(3)));
        // The server can't update until it gets something from the client.
        assert!(server.initiate_key_update().is_err());

        now += Duration::from_secs(1);
        client.process_timer(now);
        assert_eq!(client.get_epochs(), (Some(4), Some(4)));
    }

    #[test]
    fn key_update_consecutive() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);
        let now = now();

        assert!(server.initiate_key_update().is_ok());
        assert_eq!(server.get_epochs(), (Some(4), Some(3)));

        // Server sends something.
        // Send twice and drop the first to induce an ACK from the client.
        let _ = send_something(&mut server, now); // Drop this.

        // Another packet from the server will cause the client to ACK and update keys.
        let dgram = send_and_receive(&mut server, &mut client, now);
        assert!(dgram.is_some());
        assert_eq!(client.get_epochs(), (Some(4), Some(3)));

        // Have the server process the ACK.
        if let Output::Callback(_) = server.process(dgram, now) {
            assert_eq!(server.get_epochs(), (Some(4), Some(3)));
            // Now move the server temporarily into the future so that it
            // rotates the keys.  Don't do this at home folks.
            server.process_timer(now + Duration::from_secs(1));
            assert_eq!(server.get_epochs(), (Some(4), Some(4)));
        } else {
            panic!("server should have a timer set");
        }

        // Now update keys on the server again.
        assert!(server.initiate_key_update().is_ok());
        assert_eq!(server.get_epochs(), (Some(5), Some(4)));

        let dgram = send_something(&mut server, now);

        // However, as the server didn't wait long enough to update again, the
        // client hasn't rotated its keys, so the packet gets dropped.
        check_discarded(&mut client, dgram, 1, 0);
    }

    // Key updates can't be initiated too early.
    #[test]
    fn key_update_before_confirmed() {
        let mut client = default_client();
        assert!(client.initiate_key_update().is_err());
        let mut server = default_server();
        assert!(server.initiate_key_update().is_err());

        // Client Initial
        let dgram = client.process(None, now()).dgram();
        assert!(dgram.is_some());
        assert!(client.initiate_key_update().is_err());

        // Server Initial + Handshake
        let dgram = server.process(dgram, now()).dgram();
        assert!(dgram.is_some());
        assert!(server.initiate_key_update().is_err());

        // Client Handshake
        client.process_input(dgram.unwrap(), now());
        assert!(client.initiate_key_update().is_err());

        assert!(maybe_authenticate(&mut client));
        assert!(client.initiate_key_update().is_err());

        let dgram = client.process(None, now()).dgram();
        assert!(dgram.is_some());
        assert!(client.initiate_key_update().is_err());

        // Server HANDSHAKE_DONE
        let dgram = server.process(dgram, now()).dgram();
        assert!(dgram.is_some());
        assert!(server.initiate_key_update().is_ok());

        // Client receives HANDSHAKE_DONE
        let dgram = client.process(dgram, now()).dgram();
        assert!(dgram.is_none());
        assert!(client.initiate_key_update().is_ok());
    }

    #[test]
    fn ack_are_not_cc() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        // Create a stream
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets, so thatt cc window is filled.
        let c_tx_dgrams = send_bytes(&mut client, 0, now());
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

        // Server send a ack-eliciting packet
        assert_eq!(server.stream_create(StreamType::BiDi).unwrap(), 1);
        server.stream_send(1, &[6; 100]).unwrap();
        let ack_eliciting_pkt = server.process(None, now()).dgram();
        assert!(ack_eliciting_pkt.is_some());

        // Make sure client can ack the server packet even if cc windows is full.
        let ack_pkt = client.process(ack_eliciting_pkt, now()).dgram();
        assert!(ack_pkt.is_some());
        let frames = server.test_process_input(ack_pkt.unwrap(), now());
        assert_eq!(frames.len(), 1);
        assert!(matches!(
            frames[0],
            (Frame::Ack { .. }, PNSpace::ApplicationData)
        ));
    }
}
