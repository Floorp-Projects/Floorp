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
use std::mem;
use std::net::SocketAddr;
use std::rc::Rc;
use std::time::{Duration, Instant};

use smallvec::SmallVec;

use neqo_common::{
    hex, hex_snip_middle, qdebug, qerror, qinfo, qlog::NeqoQlog, qtrace, qwarn, Datagram, Decoder,
    Encoder, Role,
};
use neqo_crypto::agent::CertificateInfo;
use neqo_crypto::{
    Agent, AntiReplay, AuthenticationStatus, Cipher, Client, HandshakeState, SecretAgentInfo,
    Server, ZeroRttChecker,
};

use crate::cid::{ConnectionId, ConnectionIdDecoder, ConnectionIdManager, ConnectionIdRef};
use crate::crypto::{Crypto, CryptoDxState};
use crate::dump::*;
use crate::events::{ConnectionEvent, ConnectionEvents};
use crate::flow_mgr::FlowMgr;
use crate::frame::{
    AckRange, CloseError, Frame, FrameType, StreamType, FRAME_TYPE_CONNECTION_CLOSE_APPLICATION,
    FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT,
};
use crate::packet::{
    DecryptedPacket, PacketBuilder, PacketNumber, PacketType, PublicPacket, QuicVersion,
};
use crate::path::Path;
use crate::qlog;
use crate::recovery::{LossRecovery, RecoveryToken, SendProfile, GRANULARITY};
use crate::recv_stream::{RecvStream, RecvStreams, RECV_BUFFER_SIZE};
use crate::send_stream::{SendStream, SendStreams};
use crate::stats::{Stats, StatsCell};
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
        timeout: Instant,
    },
    Draining {
        error: ConnectionError,
        timeout: Instant,
    },
    Closed(ConnectionError),
}

impl State {
    #[must_use]
    pub fn connected(&self) -> bool {
        matches!(self, Self::Connected | Self::Confirmed)
    }

    #[must_use]
    pub fn closed(&self) -> bool {
        matches!(self, Self::Closing { .. } | Self::Draining { .. } | Self::Closed(_))
    }
}

// Implement Ord so that we can enforce monotonic state progression.
impl PartialOrd for State {
    #[allow(clippy::match_same_arms)] // Lint bug: rust-lang/rust-clippy#860
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        if mem::discriminant(self) == mem::discriminant(other) {
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
            (Self::Draining { .. }, _) => Ordering::Less,
            (_, Self::Draining { .. }) => Ordering::Greater,
            (Self::Closed(_), _) => unreachable!(),
        })
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum ZeroRttState {
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

/// Used by inner functions like Connection::output.
enum SendOption {
    /// Yes, please send this datagram.
    Yes(Datagram),
    /// Don't send.  If this was blocked on the pacer (the arg is true).
    No(bool),
}

impl Default for SendOption {
    fn default() -> Self {
        Self::No(false)
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
    retry_source_cid: ConnectionId,
}

impl RetryInfo {
    fn new(retry_source_cid: ConnectionId, token: Vec<u8>) -> Self {
        Self {
            token,
            retry_source_cid,
        }
    }
}

#[derive(Debug, Clone)]
/// There's a little bit of different behavior for resetting idle timeout. See
/// -transport 10.2 ("Idle Timeout").
enum IdleTimeoutState {
    Init,
    New(Instant),
    PacketReceived(Instant),
    AckElicitingPacketSent(Instant),
}

#[derive(Debug, Clone)]
/// There's a little bit of different behavior for resetting idle timeout. See
/// -transport 10.2 ("Idle Timeout").
struct IdleTimeout {
    timeout: Duration,
    state: IdleTimeoutState,
}

impl Default for IdleTimeout {
    fn default() -> Self {
        Self {
            timeout: LOCAL_IDLE_TIMEOUT,
            state: IdleTimeoutState::Init,
        }
    }
}

impl IdleTimeout {
    pub fn set_peer_timeout(&mut self, peer_timeout: Duration) {
        self.timeout = min(self.timeout, peer_timeout);
    }

    pub fn expiry(&mut self, now: Instant, pto: Duration) -> Instant {
        let start = match self.state {
            IdleTimeoutState::Init => {
                self.state = IdleTimeoutState::New(now);
                now
            }
            IdleTimeoutState::New(t)
            | IdleTimeoutState::PacketReceived(t)
            | IdleTimeoutState::AckElicitingPacketSent(t) => t,
        };
        start + max(self.timeout, pto * 3)
    }

    fn on_packet_sent(&mut self, now: Instant) {
        // Only reset idle timeout if we've received a packet since the last
        // time we reset the timeout here.
        match self.state {
            IdleTimeoutState::AckElicitingPacketSent(_) => {}
            IdleTimeoutState::Init
            | IdleTimeoutState::New(_)
            | IdleTimeoutState::PacketReceived(_) => {
                self.state = IdleTimeoutState::AckElicitingPacketSent(now);
            }
        }
    }

    fn on_packet_received(&mut self, now: Instant) {
        self.state = IdleTimeoutState::PacketReceived(now);
    }

    pub fn expired(&mut self, now: Instant, pto: Duration) -> bool {
        now >= self.expiry(now, pto)
    }
}

/// `StateSignaling` manages whether we need to send HANDSHAKE_DONE and CONNECTION_CLOSE.
/// Valid state transitions are:
/// * Idle -> HandshakeDone: at the server when the handshake completes
/// * HandshakeDone -> Idle: when a HANDSHAKE_DONE frame is sent
/// * Idle/HandshakeDone -> Closing/Draining: when closing or draining
/// * Closing/Draining -> CloseSent: after sending CONNECTION_CLOSE
/// * CloseSent -> Closing: any time a new CONNECTION_CLOSE is needed
/// * -> Reset: from any state in case of a stateless reset
#[derive(Debug, Clone, PartialEq)]
enum StateSignaling {
    Idle,
    HandshakeDone,
    /// These states save the frame that needs to be sent.
    Closing(Frame),
    Draining(Frame),
    /// This state saves the frame that might need to be sent again.
    /// If it is `None`, then we are draining and don't send.
    CloseSent(Option<Frame>),
    Reset,
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

    fn make_close_frame(
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) -> Frame {
        let reason_phrase = message.as_ref().as_bytes().to_owned();
        Frame::ConnectionClose {
            error_code: CloseError::from(error),
            frame_type,
            reason_phrase,
        }
    }

    pub fn close(
        &mut self,
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) {
        if *self != Self::Reset {
            *self = Self::Closing(Self::make_close_frame(error, frame_type, message));
        }
    }

    pub fn drain(
        &mut self,
        error: ConnectionError,
        frame_type: FrameType,
        message: impl AsRef<str>,
    ) {
        if *self != Self::Reset {
            *self = Self::Draining(Self::make_close_frame(error, frame_type, message));
        }
    }

    /// If a close is pending, take a frame.
    pub fn close_frame(&mut self) -> Option<Frame> {
        match self {
            Self::Closing(frame) => {
                // When we are closing, we might need to send the close frame again.
                let frame = mem::replace(frame, Frame::Padding);
                *self = Self::CloseSent(Some(frame.clone()));
                Some(frame)
            }
            Self::Draining(frame) => {
                // When we are draining, just send once.
                let frame = mem::replace(frame, Frame::Padding);
                *self = Self::CloseSent(None);
                Some(frame)
            }
            _ => None,
        }
    }

    /// If a close can be sent again, prepare to send it again.
    pub fn send_close(&mut self) {
        if let Self::CloseSent(Some(frame)) = self {
            let frame = mem::replace(frame, Frame::Padding);
            *self = Self::Closing(frame);
        }
    }

    /// We just got a stateless reset.  Terminate.
    pub fn reset(&mut self) {
        *self = Self::Reset;
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

    /// Since we need to communicate this to our peer in tparams, setting this
    /// value is part of constructing the struct.
    local_initial_source_cid: ConnectionId,
    /// Checked against tparam value from peer
    remote_initial_source_cid: Option<ConnectionId>,
    /// Checked against tparam value from peer
    remote_original_destination_cid: Option<ConnectionId>,

    /// We sometimes save a datagram (just one) against the possibility that keys
    /// will later become available.
    /// One example of this is the case where a Handshake packet containing a
    /// certificate is received coalesced with a 1-RTT packet.  The certificate
    /// might need to authenticated by the application before the 1-RTT packet can
    /// be processed.
    /// The boolean indicates whether processing should be deferred: after saving,
    /// we usually immediately try to process this, but that won't work until after
    /// returning to the application.
    saved_datagram: Option<(Datagram, bool)>,

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
    stats: StatsCell,
    qlog: NeqoQlog,

    quic_version: QuicVersion,
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
        quic_version: QuicVersion,
    ) -> Res<Self> {
        let dcid = ConnectionId::generate_initial();
        let mut c = Self::new(
            Role::Client,
            Client::new(server_name)?.into(),
            cid_manager,
            protocols,
            None,
            quic_version,
        )?;
        c.crypto.states.init(quic_version, Role::Client, &dcid);
        c.remote_original_destination_cid = Some(dcid);
        c.initialize_path(local_addr, remote_addr);
        Ok(c)
    }

    /// Create a new QUIC connection with Server role.
    pub fn new_server(
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        cid_manager: CidMgr,
        quic_version: QuicVersion,
    ) -> Res<Self> {
        Self::new(
            Role::Server,
            Server::new(certs)?.into(),
            cid_manager,
            protocols,
            None,
            quic_version,
        )
    }

    pub fn server_enable_0rtt(
        &mut self,
        anti_replay: &AntiReplay,
        zero_rtt_checker: impl ZeroRttChecker + 'static,
    ) -> Res<()> {
        self.crypto
            .server_enable_0rtt(self.tps.clone(), anti_replay, zero_rtt_checker)
    }

    fn set_tp_defaults(tps: &mut TransportParameters) {
        tps.set_integer(
            tparams::INITIAL_MAX_STREAM_DATA_BIDI_LOCAL,
            u64::try_from(RECV_BUFFER_SIZE).unwrap(),
        );
        tps.set_integer(
            tparams::INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
            u64::try_from(RECV_BUFFER_SIZE).unwrap(),
        );
        tps.set_integer(
            tparams::INITIAL_MAX_STREAM_DATA_UNI,
            u64::try_from(RECV_BUFFER_SIZE).unwrap(),
        );
        tps.set_integer(tparams::INITIAL_MAX_STREAMS_BIDI, LOCAL_STREAM_LIMIT_BIDI);
        tps.set_integer(tparams::INITIAL_MAX_STREAMS_UNI, LOCAL_STREAM_LIMIT_UNI);
        tps.set_integer(tparams::INITIAL_MAX_DATA, LOCAL_MAX_DATA);
        tps.set_integer(
            tparams::IDLE_TIMEOUT,
            u64::try_from(LOCAL_IDLE_TIMEOUT.as_millis()).unwrap(),
        );
        tps.set_empty(tparams::DISABLE_MIGRATION);
        tps.set_empty(tparams::GREASE_QUIC_BIT);
    }

    fn new(
        role: Role,
        agent: Agent,
        cid_manager: CidMgr,
        protocols: &[impl AsRef<str>],
        path: Option<Path>,
        quic_version: QuicVersion,
    ) -> Res<Self> {
        let tphandler = Rc::new(RefCell::new(TransportParametersHandler::default()));
        Self::set_tp_defaults(&mut tphandler.borrow_mut().local);
        let local_initial_source_cid = cid_manager.borrow_mut().generate_cid();
        tphandler.borrow_mut().local.set_bytes(
            tparams::INITIAL_SOURCE_CONNECTION_ID,
            local_initial_source_cid.to_vec(),
        );

        let crypto = Crypto::new(agent, protocols, tphandler.clone())?;

        let stats = StatsCell::default();
        let c = Self {
            role,
            state: State::Init,
            cid_manager,
            path,
            valid_cids: Vec::new(),
            tps: tphandler,
            zero_rtt_state: ZeroRttState::Init,
            retry_info: None,
            local_initial_source_cid,
            remote_initial_source_cid: None,
            remote_original_destination_cid: None,
            saved_datagram: None,
            crypto,
            acks: AckTracker::default(),
            idle_timeout: IdleTimeout::default(),
            indexes: StreamIndexes::new(),
            connection_ids: HashMap::new(),
            send_streams: SendStreams::default(),
            recv_streams: RecvStreams::default(),
            flow_mgr: Rc::new(RefCell::new(FlowMgr::default())),
            state_signaling: StateSignaling::Idle,
            loss_recovery: LossRecovery::new(stats.clone()),
            events: ConnectionEvents::default(),
            token: None,
            stats,
            qlog: NeqoQlog::disabled(),
            quic_version,
        };
        c.stats.borrow_mut().init(format!("{}", c));
        Ok(c)
    }

    /// Get the local path.
    pub fn path(&self) -> Option<&Path> {
        self.path.as_ref()
    }

    /// Set or clear the qlog for this connection.
    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.loss_recovery.set_qlog(qlog.clone());
        self.qlog = qlog;
    }

    /// Get the qlog (if any) for this connection.
    pub fn qlog_mut(&mut self) -> &mut NeqoQlog {
        &mut self.qlog
    }

    /// Get the original destination connection id for this connection. This
    /// will always be present for Role::Client but not if Role::Server is in
    /// State::Init.
    pub fn odcid(&self) -> Option<&ConnectionId> {
        self.remote_original_destination_cid.as_ref()
    }

    /// Set a local transport parameter, possibly overriding a default value.
    pub fn set_local_tparam(&self, tp: TransportParameterId, value: TransportParameter) -> Res<()> {
        if *self.state() == State::Init {
            self.tps.borrow_mut().local.set(tp, value);
            Ok(())
        } else {
            qerror!("Current state: {:?}", self.state());
            qerror!("Cannot set local tparam when not in an initial connection state.");
            Err(Error::ConnectionState)
        }
    }

    /// `odcid` is their original choice for our CID, which we get from the Retry token.
    /// `remote_cid` is the value from the Source Connection ID field of
    ///   an incoming packet: what the peer wants us to use now.
    /// `retry_cid` is what we asked them to use when we sent the Retry.
    pub(crate) fn set_retry_cids(
        &mut self,
        odcid: ConnectionId,
        remote_cid: ConnectionId,
        retry_cid: ConnectionId,
    ) {
        debug_assert_eq!(self.role, Role::Server);
        qtrace!(
            [self],
            "Retry CIDs: odcid={} remote={} retry={}",
            odcid,
            remote_cid,
            retry_cid
        );
        // We advertise "our" choices in transport parameters.
        let local_tps = &mut self.tps.borrow_mut().local;
        local_tps.set_bytes(tparams::ORIGINAL_DESTINATION_CONNECTION_ID, odcid.to_vec());
        local_tps.set_bytes(tparams::RETRY_SOURCE_CONNECTION_ID, retry_cid.to_vec());

        // ...and save their choices for later validation.
        self.remote_initial_source_cid = Some(remote_cid);
    }

    fn retry_sent(&self) -> bool {
        self.tps
            .borrow()
            .local
            .get_bytes(tparams::RETRY_SOURCE_CONNECTION_ID)
            .is_some()
    }

    /// Set ALPN preferences. Strings that appear earlier in the list are given
    /// higher preference.
    pub fn set_alpn(&mut self, protocols: &[impl AsRef<str>]) -> Res<()> {
        self.crypto.tls.set_alpn(protocols)?;
        Ok(())
    }

    /// Enable a set of ciphers.
    pub fn set_ciphers(&mut self, ciphers: &[Cipher]) -> Res<()> {
        if self.state != State::Init {
            qerror!([self], "Cannot enable ciphers in state {:?}", self.state);
            return Err(Error::ConnectionState);
        }
        self.crypto.tls.set_ciphers(ciphers)?;
        Ok(())
    }

    /// Access the latest resumption token on the connection.
    pub fn resumption_token(&self) -> Option<Vec<u8>> {
        if self.state < State::Connected {
            return None;
        }
        match self.crypto.tls {
            Agent::Client(ref c) => match c.resumption_token() {
                Some(ref t) => {
                    qtrace!("TLS token {}", hex(&t));
                    let mut enc = Encoder::default();
                    let rtt = self.loss_recovery.rtt();
                    let rtt = u64::try_from(rtt.as_millis()).unwrap_or(0);
                    enc.encode_varint(rtt);
                    enc.encode_vvec_with(|enc_inner| {
                        self.tps
                            .borrow()
                            .remote
                            .as_ref()
                            .expect("should have transport parameters")
                            .encode(enc_inner);
                    });
                    enc.encode(&t[..]);
                    qinfo!("resumption token {}", hex_snip_middle(&enc[..]));
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
        qinfo!([self], "resumption token {}", hex_snip_middle(token));
        let mut dec = Decoder::from(token);

        let smoothed_rtt = match dec.decode_varint() {
            Some(v) => Duration::from_millis(v),
            _ => return Err(Error::InvalidResumptionToken),
        };

        let tp_slice = match dec.decode_vvec() {
            Some(v) => v,
            _ => return Err(Error::InvalidResumptionToken),
        };
        qtrace!([self], "  transport parameters {}", hex(&tp_slice));
        let mut dec_tp = Decoder::from(tp_slice);
        let tp =
            TransportParameters::decode(&mut dec_tp).map_err(|_| Error::InvalidResumptionToken)?;

        let tok = dec.decode_remainder();
        qtrace!([self], "  TLS token {}", hex(&tok));
        match self.crypto.tls {
            Agent::Client(ref mut c) => {
                let res = c.set_resumption_token(&tok);
                if let Err(e) = res {
                    self.absorb_error::<Error>(now, Err(Error::CryptoError(e)));
                    return Ok(());
                }
            }
            Agent::Server(_) => return Err(Error::WrongRole),
        }

        self.tps.borrow_mut().remote_0rtt = Some(tp);

        if smoothed_rtt > GRANULARITY {
            self.loss_recovery.set_initial_rtt(smoothed_rtt);
        }
        self.set_initial_limits();
        // Start up TLS, which has the effect of setting up all the necessary
        // state for 0-RTT.  This only stages the CRYPTO frames.
        let res = self.client_start(now);
        self.absorb_error(now, res);
        Ok(())
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
                self.crypto.buffer_records(records)?;
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
        qinfo!([self], "Authenticated {:?}", status);
        self.crypto.tls.authenticated(status);
        let res = self.handshake(now, PNSpace::Handshake, None);
        self.absorb_error(now, res);
        // Try to handle packets that couldn't be processed before.
        self.process_saved(now, true);
    }

    /// Get the role of the connection.
    pub fn role(&self) -> Role {
        self.role
    }

    /// Get the state of the connection.
    pub fn state(&self) -> &State {
        &self.state
    }

    /// Get the 0-RTT state of the connection.
    pub fn zero_rtt_state(&self) -> &ZeroRttState {
        &self.zero_rtt_state
    }

    /// Get a snapshot of collected statistics.
    pub fn stats(&self) -> Stats {
        self.stats.borrow().clone()
    }

    // This function wraps a call to another function and sets the connection state
    // properly if that call fails.
    fn capture_error<T>(&mut self, now: Instant, frame_type: FrameType, res: Res<T>) -> Res<T> {
        if let Err(v) = &res {
            #[cfg(debug_assertions)]
            let msg = format!("{:?}", v);
            #[cfg(not(debug_assertions))]
            let msg = "";
            let error = ConnectionError::Transport(v.clone());
            match &self.state {
                State::Closing { error: err, .. }
                | State::Draining { error: err, .. }
                | State::Closed(err) => {
                    qwarn!([self], "Closing again after error {:?}", err);
                }
                State::Init => {
                    // We have not even sent anything just close the connection without sending any error.
                    // This may happen when clieeent_start fails.
                    self.set_state(State::Closed(error));
                }
                State::WaitInitial => {
                    // We don't have any state yet, so don't bother with
                    // the closing state, just send one CONNECTION_CLOSE.
                    self.state_signaling.close(error.clone(), frame_type, msg);
                    self.set_state(State::Closed(error));
                }
                _ => {
                    self.state_signaling.close(error.clone(), frame_type, msg);
                    self.set_state(State::Closing {
                        error,
                        timeout: self.get_closing_period_time(now),
                    });
                }
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
        if let State::Closing { error, timeout } | State::Draining { error, timeout } = &self.state
        {
            if *timeout <= now {
                // Close timeout expired, move to Closed
                let st = State::Closed(error.clone());
                self.set_state(st);
                qinfo!("Closing timer expired");
                return;
            }
        }
        if let State::Closed(_) = self.state {
            qdebug!("Timer fired while closed");
            return;
        }

        let pto = self.loss_recovery.pto_raw(PNSpace::ApplicationData);
        if self.idle_timeout.expired(now, pto) {
            qinfo!([self], "idle timeout expired");
            self.set_state(State::Closed(ConnectionError::Transport(
                Error::IdleTimeout,
            )));
            return;
        }

        self.process_saved(now, false);

        let res = self.crypto.states.check_key_update(now);
        self.absorb_error(now, res);

        let lost = self.loss_recovery.timeout(now);
        self.handle_lost_packets(&lost);
        qlog::packets_lost(&mut self.qlog, &lost);
    }

    /// Call in to process activity on the connection. Either new packets have
    /// arrived or a timeout has expired (or both).
    pub fn process_input(&mut self, d: Datagram, now: Instant) {
        let res = self.input(d, now);
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
    fn next_delay(&mut self, now: Instant, paced: bool) -> Duration {
        qtrace!([self], "Get callback delay {:?}", now);

        // Only one timer matters when closing...
        if let State::Closing { timeout, .. } | State::Draining { timeout, .. } = self.state {
            return timeout.duration_since(now);
        }

        let mut delays = SmallVec::<[_; 5]>::new();
        if let Some(ack_time) = self.acks.ack_time(now) {
            qtrace!([self], "Delayed ACK timer {:?}", ack_time);
            delays.push(ack_time);
        }

        let pto = self.loss_recovery.pto_raw(PNSpace::ApplicationData);
        let idle_time = self.idle_timeout.expiry(now, pto);
        qtrace!([self], "Idle timer {:?}", idle_time);
        delays.push(idle_time);

        if let Some(lr_time) = self.loss_recovery.next_timeout() {
            qtrace!([self], "Loss recovery timer {:?}", lr_time);
            delays.push(lr_time);
        }

        if let Some(key_update_time) = self.crypto.states.update_time() {
            qtrace!([self], "Key update timer {:?}", key_update_time);
            delays.push(key_update_time);
        }

        if paced {
            if let Some(pace_time) = self.loss_recovery.next_paced() {
                qtrace!([self], "Pacing timer {:?}", pace_time);
                delays.push(pace_time);
            }
        }

        let earliest = delays.into_iter().min().unwrap();
        // TODO(agrover, mt) - need to analyze and fix #47
        // rather than just clamping to zero here.
        qdebug!(
            [self],
            "delay duration {:?}",
            max(now, earliest).duration_since(now)
        );
        debug_assert!(earliest > now);
        max(now, earliest).duration_since(now)
    }

    /// Get output packets, as a result of receiving packets, or actions taken
    /// by the application.
    /// Returns datagrams to send, and how long to wait before calling again
    /// even if no incoming packets.
    pub fn process_output(&mut self, now: Instant) -> Output {
        qtrace!([self], "process_output {:?} {:?}", self.state, now);

        if self.state == State::Init {
            if self.role == Role::Client {
                let res = self.client_start(now);
                self.absorb_error(now, res);
            }
        } else {
            self.process_timer(now);
        }

        match self.output(now) {
            SendOption::Yes(dgram) => Output::Datagram(dgram),
            SendOption::No(paced) => match self.state {
                State::Init | State::Closed(_) => Output::None,
                State::Closing { timeout, .. } | State::Draining { timeout, .. } => {
                    Output::Callback(timeout.duration_since(now))
                }
                _ => Output::Callback(self.next_delay(now, paced)),
            },
        }
    }

    /// Process input and generate output.
    #[must_use = "Output of the process function must be handled"]
    pub fn process(&mut self, dgram: Option<Datagram>, now: Instant) -> Output {
        if let Some(d) = dgram {
            self.process_input(d, now);
        }
        self.process_output(now)
    }

    fn is_valid_cid(&self, cid: &ConnectionIdRef) -> bool {
        self.valid_cids.iter().any(|c| c == cid) || self.path.iter().any(|p| p.valid_local_cid(cid))
    }

    fn handle_retry(&mut self, packet: PublicPacket) -> Res<()> {
        qinfo!([self], "received Retry");
        if self.retry_info.is_some() {
            self.stats.borrow_mut().pkt_dropped("Extra Retry");
            return Ok(());
        }
        if packet.token().is_empty() {
            self.stats.borrow_mut().pkt_dropped("Retry without a token");
            return Ok(());
        }
        if !packet.is_valid_retry(&self.remote_original_destination_cid.as_ref().unwrap()) {
            self.stats
                .borrow_mut()
                .pkt_dropped("Retry with bad integrity tag");
            return Ok(());
        }
        if let Some(p) = &mut self.path {
            // At this point, we shouldn't have a remote connection ID for the path.
            p.set_remote_cid(packet.scid());
        } else {
            qinfo!([self], "No path, but we received a Retry");
            return Err(Error::InternalError);
        };

        qinfo!(
            [self],
            "Valid Retry received, token={} scid={}",
            hex(packet.token()),
            packet.scid()
        );

        self.retry_info = Some(RetryInfo::new(
            ConnectionId::from(packet.scid()),
            packet.token().to_vec(),
        ));

        let lost_packets = self.loss_recovery.retry();
        self.handle_lost_packets(&lost_packets);

        self.crypto
            .states
            .init(self.quic_version, self.role, packet.scid());
        Ok(())
    }

    fn discard_keys(&mut self, space: PNSpace, now: Instant) {
        if self.crypto.discard(space) {
            qinfo!([self], "Drop packet number space {}", space);
            self.loss_recovery.discard(space, now);
            self.acks.drop_space(space);
        }
    }

    fn token_equal(a: &[u8; 16], b: &[u8; 16]) -> bool {
        // rustc might decide to optimize this and make this non-constant-time
        // with respect to `t`, but it doesn't appear to currently.
        let mut c = 0;
        for (&a, &b) in a.iter().zip(b) {
            c |= a ^ b;
        }
        c == 0
    }

    fn is_stateless_reset(&self, d: &Datagram) -> bool {
        if d.len() < 16 {
            return false;
        }
        let token = <&[u8; 16]>::try_from(&d[d.len() - 16..]).unwrap();
        // TODO(mt) only check the path that matches the datagram.
        self.path
            .as_ref()
            .map(|p| p.reset_token())
            .flatten()
            .map_or(false, |t| Self::token_equal(t, token))
    }

    fn check_stateless_reset<'a, 'b>(
        &'a mut self,
        d: &'b Datagram,
        slice: &'b [u8],
        now: Instant,
    ) -> Res<()> {
        // Only look for a stateless reset if we are dealing with the entire
        // datagram.  No point in running the check multiple times.
        if d.len() == slice.len() && self.is_stateless_reset(d) {
            // Failing to process a packet in a datagram might
            // indicate that there is a stateless reset present.
            qdebug!([self], "Stateless reset: {}", hex(&d[d.len() - 16..]));
            self.state_signaling.reset();
            self.set_state(State::Draining {
                error: ConnectionError::Transport(Error::StatelessReset),
                timeout: self.get_closing_period_time(now),
            });
            Err(Error::StatelessReset)
        } else {
            Ok(())
        }
    }

    /// Process any saved packet.
    /// When `always` is false, the packet is only processed if time has
    /// passed since it was saved.  Otherwise saved packets are saved and then
    /// dropped instantly.
    fn process_saved(&mut self, now: Instant, always: bool) {
        if let Some((_, ref mut defer)) = self.saved_datagram {
            if always || !*defer {
                let d = self.saved_datagram.take().unwrap().0;
                qdebug!([self], "process saved datagram: {:?}", d);
                self.process_input(d, now);
            } else {
                *defer = false;
            }
        }
    }

    /// In case a datagram arrives that we can only partially process, save any
    /// part that we don't have keys for.
    fn maybe_save_datagram<'a, 'b>(
        &'a mut self,
        d: &'b Datagram,
        slice: &'b [u8],
        now: Instant,
    ) -> bool {
        // Only save partial datagrams to avoid looping.
        // It also means that we don't have to worry about it being a stateless reset.
        if slice.len() < d.len() {
            let save = Datagram::new(d.source(), d.destination(), slice);
            qdebug!([self], "saving datagram@{:?} {:?}", now, save);
            self.saved_datagram = Some((save, true));
            true
        } else {
            false
        }
    }

    fn input(&mut self, d: Datagram, now: Instant) -> Res<Vec<(Frame, PNSpace)>> {
        let mut slc = &d[..];
        let mut frames = Vec::new();

        qtrace!([self], "input {}", hex(&**d));

        // Handle each packet in the datagram
        while !slc.is_empty() {
            self.stats.borrow_mut().packets_rx += 1;
            let (packet, remainder) =
                match PublicPacket::decode(slc, self.cid_manager.borrow().as_decoder()) {
                    Ok((packet, remainder)) => (packet, remainder),
                    Err(e) => {
                        qinfo!([self], "Garbage packet: {}", e);
                        qtrace!([self], "Garbage packet contents: {}", hex(slc));
                        self.stats.borrow_mut().pkt_dropped("Garbage packet");
                        break;
                    }
                };
            match (packet.packet_type(), &self.state, &self.role) {
                (PacketType::Initial, State::Init, Role::Server) => {
                    if !packet.is_valid_initial() {
                        self.stats.borrow_mut().pkt_dropped("Invalid Initial");
                        break;
                    }
                    qinfo!(
                        [self],
                        "Received valid Initial packet with scid {:?} dcid {:?}",
                        packet.scid(),
                        packet.dcid()
                    );
                    self.set_state(State::WaitInitial);
                    self.loss_recovery.start_pacer(now);
                    self.crypto
                        .states
                        .init(self.quic_version, self.role, &packet.dcid());

                    // We need to make sure that we set this transport parameter.
                    // This has to happen prior to processing the packet so that
                    // the TLS handshake has all it needs.
                    if !self.retry_sent() {
                        self.tps.borrow_mut().local.set_bytes(
                            tparams::ORIGINAL_DESTINATION_CONNECTION_ID,
                            packet.dcid().to_vec(),
                        )
                    }
                }
                (PacketType::VersionNegotiation, State::WaitInitial, Role::Client) => {
                    match packet.supported_versions() {
                        Ok(versions) => {
                            if versions.is_empty() || versions.contains(&self.quic_version.as_u32())
                            {
                                // Ignore VersionNegotiation packets that contain the current version.
                                self.stats.borrow_mut().dropped_rx += 1;
                                return Ok(frames);
                            }
                            self.set_state(State::Closed(ConnectionError::Transport(
                                Error::VersionNegotiation,
                            )));
                            return Err(Error::VersionNegotiation);
                        }
                        Err(_) => {
                            self.stats.borrow_mut().dropped_rx += 1;
                            return Ok(frames);
                        }
                    }
                }
                (PacketType::Retry, State::WaitInitial, Role::Client) => {
                    self.handle_retry(packet)?;
                    break;
                }
                (PacketType::VersionNegotiation, ..)
                | (PacketType::Retry, ..)
                | (PacketType::OtherVersion, ..) => {
                    self.stats
                        .borrow_mut()
                        .pkt_dropped(format!("{:?}", packet.packet_type()));
                    break;
                }
                _ => {}
            };

            match self.state {
                State::Init => {
                    self.stats
                        .borrow_mut()
                        .pkt_dropped("Received while in Init state");
                    break;
                }
                State::WaitInitial => {}
                State::Handshaking | State::Connected | State::Confirmed => {
                    if !self.is_valid_cid(packet.dcid()) {
                        self.stats
                            .borrow_mut()
                            .pkt_dropped(format!("Ignoring packet with CID {:?}", packet.dcid()));
                        break;
                    }
                    if self.role == Role::Server && packet.packet_type() == PacketType::Handshake {
                        // Server has received a Handshake packet -> discard Initial keys and states
                        self.discard_keys(PNSpace::Initial, now);
                    }
                }
                State::Closing { .. } => {
                    // Don't bother processing the packet. Instead ask to get a
                    // new close frame.
                    self.state_signaling.send_close();
                    break;
                }
                State::Draining { .. } | State::Closed(..) => {
                    // Do nothing.
                    self.stats
                        .borrow_mut()
                        .pkt_dropped(format!("State {:?}", self.state));
                    break;
                }
            }

            qtrace!([self], "Received unverified packet {:?}", packet);

            let pto = self.loss_recovery.pto_raw(PNSpace::ApplicationData);
            match packet.decrypt(&mut self.crypto.states, now + pto) {
                Ok(payload) => {
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
                    qlog::packet_received(&mut self.qlog, &packet, &payload);
                    let res = self.process_packet(&payload, now);
                    if res.is_err() && self.path.is_none() {
                        // We need to make a path for sending an error message.
                        // But this connection is going to be closed.
                        self.remote_initial_source_cid = Some(ConnectionId::from(packet.scid()));
                        self.initialize_path(d.destination(), d.source());
                    }
                    frames.extend(res?);
                    if self.state == State::WaitInitial {
                        self.start_handshake(&packet, &d)?;
                    }
                    self.process_migrations(&d)?;
                }
                Err(e) => {
                    // While connecting we might want to save the remainder of a packet.
                    if matches!(e, Error::KeysNotFound) && self.maybe_save_datagram(&d, slc, now) {
                        break;
                    }
                    // Decryption failure, or not having keys is not fatal.
                    // If the state isn't available, or we can't decrypt the packet, drop
                    // the rest of the datagram on the floor, but don't generate an error.
                    self.check_stateless_reset(&d, slc, now)?;
                    self.stats.borrow_mut().pkt_dropped("Decryption failure");
                    qlog::packet_dropped(&mut self.qlog, &packet);
                }
            }
            slc = remainder;
        }
        self.check_stateless_reset(&d, slc, now)?;
        Ok(frames)
    }

    fn process_packet(
        &mut self,
        packet: &DecryptedPacket,
        now: Instant,
    ) -> Res<Vec<(Frame, PNSpace)>> {
        // TODO(ekr@rtfm.com): Have the server blow away the initial
        // crypto state if this fails? Otherwise, we will get a panic
        // on the assert for doesn't exist.
        // OK, we have a valid packet.

        let space = PNSpace::from(packet.packet_type());
        if self.acks.get_mut(space).unwrap().is_duplicate(packet.pn()) {
            qdebug!([self], "Duplicate packet from {} pn={}", space, packet.pn());
            self.stats.borrow_mut().dups_rx += 1;
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
                qdebug!(
                    [self],
                    "PADDING frame repeated {} times",
                    consecutive_padding
                );
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
        self.acks
            .get_mut(space)
            .unwrap()
            .set_received(now, packet.pn(), ack_eliciting);

        Ok(frames)
    }

    fn initialize_path(&mut self, local_addr: SocketAddr, remote_addr: SocketAddr) {
        debug_assert!(self.path.is_none());
        self.path = Some(Path::new(
            local_addr,
            remote_addr,
            self.local_initial_source_cid.clone(),
            // Ideally we know what the peer wants us to use for the remote CID.
            // But we will use our own guess if necessary.
            self.remote_initial_source_cid
                .as_ref()
                .or_else(|| self.remote_original_destination_cid.as_ref())
                .unwrap()
                .clone(),
        ));
    }

    fn start_handshake(&mut self, packet: &PublicPacket, d: &Datagram) -> Res<()> {
        self.remote_initial_source_cid = Some(ConnectionId::from(packet.scid()));
        if self.role == Role::Server {
            assert_eq!(packet.packet_type(), PacketType::Initial);

            // A server needs to accept the client's selected CID during the handshake.
            self.valid_cids.push(ConnectionId::from(packet.dcid()));
            // Install a path.
            self.initialize_path(d.destination(), d.source());

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

    fn output(&mut self, now: Instant) -> SendOption {
        qtrace!([self], "output {:?}", now);
        if let Some(mut path) = self.path.take() {
            let res = match &self.state {
                State::Init
                | State::WaitInitial
                | State::Handshaking
                | State::Connected
                | State::Confirmed => self.output_path(&mut path, now),
                State::Closing { .. } | State::Draining { .. } | State::Closed(_) => {
                    if let Some(frame) = self.state_signaling.close_frame() {
                        self.output_close(&path, &frame)
                    } else {
                        Ok(SendOption::default())
                    }
                }
            };
            let out = self.absorb_error(now, res).unwrap_or_default();
            self.path = Some(path);
            out
        } else {
            SendOption::default()
        }
    }

    fn build_packet_header(
        path: &Path,
        space: PNSpace,
        encoder: Encoder,
        tx: &CryptoDxState,
        retry_info: &Option<RetryInfo>,
        quic_version: QuicVersion,
        grease_quic_bit: bool,
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

            PacketBuilder::long(
                encoder,
                pt,
                quic_version,
                path.remote_cid(),
                path.local_cid(),
            )
        };
        builder.scramble(grease_quic_bit);
        if pt == PacketType::Initial {
            builder.initial_token(if let Some(info) = retry_info {
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

    fn can_grease_quic_bit(&self) -> bool {
        let tph = self.tps.borrow();
        if let Some(r) = &tph.remote {
            r.get_empty(tparams::GREASE_QUIC_BIT)
        } else if let Some(r) = &tph.remote_0rtt {
            r.get_empty(tparams::GREASE_QUIC_BIT)
        } else {
            false
        }
    }

    fn output_close(&mut self, path: &Path, frame: &Frame) -> Res<SendOption> {
        let mut encoder = Encoder::with_capacity(path.mtu());
        let grease_quic_bit = self.can_grease_quic_bit();
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

            let (_, _, mut builder) = Self::build_packet_header(
                path,
                *space,
                encoder,
                tx,
                &None,
                self.quic_version,
                grease_quic_bit,
            );
            // ConnectionError::Application is only allowed at 1RTT.
            if *space == PNSpace::ApplicationData {
                frame.marshal(&mut builder);
            } else {
                frame.sanitize_close().marshal(&mut builder);
            }

            encoder = builder.build(tx)?;
        }

        Ok(SendOption::Yes(path.datagram(encoder)))
    }

    /// Add frames to the provided builder and
    /// return whether any of them were ACK eliciting.
    fn add_frames(
        &mut self,
        builder: &mut PacketBuilder,
        space: PNSpace,
        limit: usize,
        profile: &SendProfile,
        now: Instant,
    ) -> (Vec<RecoveryToken>, bool) {
        let mut tokens = Vec::new();

        let mut ack_eliciting = if profile.should_probe(space) {
            // Send PING in all spaces that allow a probe.
            // This might get a more expedient ACK.
            builder.encode_varint(Frame::Ping.get_type());
            true
        } else {
            false
        };

        // All useful frames are at least 2 bytes.
        while builder.len() + 2 < limit {
            let remaining = limit - builder.len();
            // Try to get a frame from frame sources
            let mut frame = self.acks.get_frame(now, space);
            // If we are CC limited we can only send acks!
            if !profile.ack_only(space) {
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
    fn output_path(&mut self, path: &mut Path, now: Instant) -> Res<SendOption> {
        let mut initial_sent = None;
        let mut needs_padding = false;
        let grease_quic_bit = self.can_grease_quic_bit();

        // Determine how we are sending packets (PTO, etc..).
        let profile = self.loss_recovery.send_profile(now, path.mtu());
        qdebug!([self], "output_path send_profile {:?}", profile);

        // Frames for different epochs must go in different packets, but then these
        // packets can go in a single datagram
        let mut encoder = Encoder::with_capacity(profile.limit());
        for space in PNSpace::iter() {
            // Ensure we have tx crypto state for this epoch, or skip it.
            let tx = if let Some(tx_state) = self.crypto.states.tx(*space) {
                tx_state
            } else {
                continue;
            };

            let header_start = encoder.len();
            let (pt, pn, mut builder) = Self::build_packet_header(
                path,
                *space,
                encoder,
                tx,
                &self.retry_info,
                self.quic_version,
                grease_quic_bit,
            );
            let payload_start = builder.len();

            // Work out if we have space left.
            let aead_expansion = tx.expansion();
            if builder.len() + aead_expansion > profile.limit() {
                // No space for a packet of this type.
                encoder = builder.abort();
                continue;
            }

            // Add frames to the packet.
            let limit = profile.limit() - aead_expansion;
            let (tokens, ack_eliciting) =
                self.add_frames(&mut builder, *space, limit, &profile, now);
            if builder.is_empty() {
                // Nothing to include in this packet.
                encoder = builder.abort();
                continue;
            }

            dump_packet(self, "TX ->", pt, pn, &builder[payload_start..]);
            qlog::packet_sent(
                &mut self.qlog,
                pt,
                pn,
                builder.len() - header_start + aead_expansion,
                &builder[payload_start..],
            );

            self.stats.borrow_mut().packets_tx += 1;
            encoder = builder.build(self.crypto.states.tx(*space).unwrap())?;
            debug_assert!(encoder.len() <= path.mtu());

            if ack_eliciting {
                self.idle_timeout.on_packet_sent(now);
            }
            let sent = SentPacket::new(
                pt,
                pn,
                now,
                ack_eliciting,
                Rc::new(tokens),
                encoder.len() - header_start,
            );
            if pt == PacketType::Initial && self.role == Role::Client {
                // Packets containing Initial packets might need padding, and we want to
                // track that padding along with the Initial packet.  So defer tracking.
                initial_sent = Some(sent);
                needs_padding = true;
            } else {
                if pt != PacketType::ZeroRtt {
                    needs_padding = false;
                }
                self.loss_recovery.on_packet_sent(sent);
            }

            if *space == PNSpace::Handshake {
                if self.role == Role::Client {
                    // Client can send Handshake packets -> discard Initial keys and states
                    self.discard_keys(PNSpace::Initial, now);
                } else if self.state == State::Confirmed {
                    // We could discard handshake keys in set_state, but wait until after sending an ACK.
                    self.discard_keys(PNSpace::Handshake, now);
                }
            }
        }

        if encoder.is_empty() {
            Ok(SendOption::No(profile.paced()))
        } else {
            // Pad Initial packets sent by the client to mtu bytes.
            let mut packets: Vec<u8> = encoder.into();
            if let Some(mut initial) = initial_sent.take() {
                if needs_padding {
                    qdebug!([self], "pad Initial to path MTU {}", path.mtu());
                    initial.size += path.mtu() - packets.len();
                    packets.resize(path.mtu(), 0);
                }
                self.loss_recovery.on_packet_sent(initial);
            }
            Ok(SendOption::Yes(path.datagram(packets)))
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
        debug_assert_eq!(self.role, Role::Client);
        qlog::client_connection_started(&mut self.qlog, self.path.as_ref().unwrap());
        self.loss_recovery.start_pacer(now);

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
        now + (self.loss_recovery.pto_raw(PNSpace::ApplicationData) * 3)
    }

    /// Close the connection.
    pub fn close(&mut self, now: Instant, app_error: AppError, msg: impl AsRef<str>) {
        let error = ConnectionError::Application(app_error);
        let timeout = self.get_closing_period_time(now);
        self.state_signaling.close(error.clone(), 0, msg);
        self.set_state(State::Closing { error, timeout });
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

        let peer_timeout = remote.get_integer(tparams::IDLE_TIMEOUT);
        if peer_timeout > 0 {
            self.idle_timeout
                .set_peer_timeout(Duration::from_millis(peer_timeout));
        }
    }

    /// Process the final set of transport parameters.
    fn process_tps(&mut self) -> Res<()> {
        self.validate_cids()?;
        if let Some(token) = self
            .tps
            .borrow()
            .remote
            .as_ref()
            .unwrap()
            .get_bytes(tparams::STATELESS_RESET_TOKEN)
        {
            let reset_token = <[u8; 16]>::try_from(token).unwrap().to_owned();
            self.path.as_mut().unwrap().set_reset_token(reset_token);
        }
        self.set_initial_limits();
        Ok(())
    }

    fn validate_cids(&mut self) -> Res<()> {
        match self.quic_version {
            QuicVersion::Draft27 => self.validate_cids_draft_27(),
            _ => self.validate_cids_draft_28_plus(),
        }
    }

    fn validate_cids_draft_27(&mut self) -> Res<()> {
        if let Some(info) = &self.retry_info {
            debug_assert!(!info.token.is_empty());
            let tph = self.tps.borrow();
            let tp = tph
                .remote
                .as_ref()
                .unwrap()
                .get_bytes(tparams::ORIGINAL_DESTINATION_CONNECTION_ID);
            if self
                .remote_original_destination_cid
                .as_ref()
                .map(ConnectionId::as_cid_ref)
                != tp.map(ConnectionIdRef::from)
            {
                return Err(Error::InvalidRetry);
            }
        }
        Ok(())
    }

    fn validate_cids_draft_28_plus(&mut self) -> Res<()> {
        let tph = self.tps.borrow();
        let remote_tps = tph.remote.as_ref().unwrap();

        let tp = remote_tps.get_bytes(tparams::INITIAL_SOURCE_CONNECTION_ID);
        if self
            .remote_initial_source_cid
            .as_ref()
            .map(ConnectionId::as_cid_ref)
            != tp.map(ConnectionIdRef::from)
        {
            qwarn!(
                "{} ISCID test failed: self cid {:?} != tp cid {:?}",
                self.role,
                self.remote_initial_source_cid,
                tp.map(hex),
            );
            return Err(Error::ProtocolViolation);
        }

        if self.role == Role::Client {
            let tp = remote_tps.get_bytes(tparams::ORIGINAL_DESTINATION_CONNECTION_ID);
            if self
                .remote_original_destination_cid
                .as_ref()
                .map(ConnectionId::as_cid_ref)
                != tp.map(ConnectionIdRef::from)
            {
                qwarn!(
                    "{} ODCID test failed: self cid {:?} != tp cid {:?}",
                    self.role,
                    self.remote_original_destination_cid,
                    tp.map(hex),
                );
                return Err(Error::ProtocolViolation);
            }

            let tp = remote_tps.get_bytes(tparams::RETRY_SOURCE_CONNECTION_ID);
            let expected = self
                .retry_info
                .as_ref()
                .map(|ri| ri.retry_source_cid.as_cid_ref());
            if expected != tp.map(ConnectionIdRef::from) {
                qwarn!(
                    "{} RSCID test failed. self cid {:?} != tp cid {:?}",
                    self.role,
                    expected,
                    tp.map(hex),
                );
                return Err(Error::ProtocolViolation);
            }
        }

        Ok(())
    }

    fn handshake(&mut self, now: Instant, space: PNSpace, data: Option<&[u8]>) -> Res<()> {
        qtrace!([self], "Handshake space={} data={:0x?}", space, data);

        let try_update = data.is_some();
        match self.crypto.handshake(now, space, data)? {
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
            // We have transport parameters, it's go time.
            if self.tps.borrow().remote.is_some() {
                self.set_initial_limits();
            }
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
        let space = PNSpace::from(ptype);
        match frame {
            Frame::Padding => {
                // Ignore
            }
            Frame::Ping => {
                // If we get a PING and there are outstanding CRYPTO frames,
                // prepare to resend them.
                self.crypto.resend_unacked(space);
            }
            Frame::Ack {
                largest_acknowledged,
                ack_delay,
                first_ack_range,
                ack_ranges,
            } => {
                self.handle_ack(
                    space,
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
                    let read = self.crypto.streams.read_to_end(space, &mut buf);
                    qdebug!("Read {} bytes", read);
                    self.handshake(now, space, Some(&buf))?;
                } else {
                    // If we get a useless CRYPTO frame send outstanding CRYPTO frames again.
                    self.crypto.resend_unacked(space);
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
            Frame::StreamDataBlocked {
                stream_id,
                stream_data_limit,
            } => {
                // Terminate connection with STREAM_STATE_ERROR if send-only
                // stream (-transport 19.13)
                if stream_id.is_send_only(self.role()) {
                    return Err(Error::StreamStateError);
                }

                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    if let Some(msd) = rs.max_stream_data() {
                        qinfo!(
                            [self],
                            "Got StreamDataBlocked(id {} MSD {}); curr MSD {}",
                            stream_id.as_u64(),
                            stream_data_limit,
                            msd
                        );
                        if stream_data_limit != msd {
                            self.flow_mgr.borrow_mut().max_stream_data(stream_id, msd)
                        }
                    }
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
                let (detail, frame_type) = if let CloseError::Application(_) = error_code {
                    // Use a transport error here because we want to send
                    // NO_ERROR in this case.
                    (
                        Error::PeerApplicationError(error_code.code()),
                        FRAME_TYPE_CONNECTION_CLOSE_APPLICATION,
                    )
                } else {
                    (
                        Error::PeerError(error_code.code()),
                        FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT,
                    )
                };
                let error = ConnectionError::Transport(detail);
                self.state_signaling.drain(error.clone(), frame_type, "");
                self.set_state(State::Draining {
                    error,
                    timeout: self.get_closing_period_time(now),
                });
            }
            Frame::HandshakeDone => {
                if self.role == Role::Server || !self.state.connected() {
                    return Err(Error::ProtocolViolation);
                }
                self.set_state(State::Confirmed);
                self.discard_keys(PNSpace::Handshake, now);
            }
        };

        Ok(())
    }

    /// Given a set of `SentPacket` instances, ensure that the source of the packet
    /// is told that they are lost.  This gives the frame generation code a chance
    /// to retransmit the frame as needed.
    fn handle_lost_packets(&mut self, lost_packets: &[SentPacket]) {
        for lost in lost_packets {
            for token in lost.tokens.as_ref() {
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

    fn decode_ack_delay(&self, v: u64) -> Duration {
        // If we have remote transport parameters, use them.
        // Otherwise, ack delay should be zero (because it's the handshake).
        if let Some(r) = self.tps.borrow().remote.as_ref() {
            let exponent = u32::try_from(r.get_integer(tparams::ACK_DELAY_EXPONENT)).unwrap();
            Duration::from_micros(v.checked_shl(exponent).unwrap_or(u64::MAX))
        } else {
            Duration::new(0, 0)
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
            Frame::decode_ack_frame(largest_acknowledged, first_ack_range, &ack_ranges)?;
        let (acked_packets, lost_packets) = self.loss_recovery.on_ack_received(
            space,
            largest_acknowledged,
            acked_ranges,
            self.decode_ack_delay(ack_delay),
            now,
        );
        for acked in acked_packets {
            for token in acked.tokens.as_ref() {
                match token {
                    RecoveryToken::Ack(at) => self.acks.acked(at),
                    RecoveryToken::Stream(st) => self.send_streams.acked(st),
                    RecoveryToken::Crypto(ct) => self.crypto.acked(ct),
                    RecoveryToken::Flow(ft) => {
                        self.flow_mgr.borrow_mut().acked(ft, &mut self.send_streams)
                    }
                    RecoveryToken::HandshakeDone => (),
                }
            }
        }
        self.handle_lost_packets(&lost_packets);
        qlog::packets_lost(&mut self.qlog, &lost_packets);
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
            debug_assert_eq!(1, self.valid_cids.len());
            self.valid_cids.clear();
            // Generate a qlog event that the server connection started.
            qlog::server_connection_started(&mut self.qlog, self.path.as_ref().unwrap());
        } else {
            self.zero_rtt_state = if self.crypto.tls.info().unwrap().early_data_accepted() {
                ZeroRttState::AcceptedClient
            } else {
                self.client_0rtt_rejected();
                ZeroRttState::Rejected
            };
        }

        // Setting application keys has to occur after 0-RTT rejection.
        let pto = self.loss_recovery.pto_raw(PNSpace::ApplicationData);
        self.crypto.install_application_keys(now + pto)?;
        self.process_tps()?;
        self.set_state(State::Connected);
        self.stats.borrow_mut().resumed = self.crypto.tls.info().unwrap().resumed();
        if self.role == Role::Server {
            self.state_signaling.handshake_done();
            self.set_state(State::Confirmed);
        }
        qinfo!([self], "Connection established");
        qlog::connection_tparams_set(&mut self.qlog, &*self.tps.borrow());
        Ok(())
    }

    fn set_state(&mut self, state: State) {
        if state > self.state {
            qinfo!([self], "State change from {:?} -> {:?}", self.state, state);
            self.state = state.clone();
            if self.state.closed() {
                self.send_streams.clear();
                self.recv_streams.clear();
            }
            self.events.connection_state_change(state);
            qlog::connection_state_updated(&mut self.qlog, &self.state)
        } else if mem::discriminant(&state) != mem::discriminant(&self.state) {
            // Only tolerate a regression in state if the new state is closing
            // and the connection is already closed.
            debug_assert!(matches!(state, State::Closing { .. } | State::Draining { .. }));
            debug_assert!(self.state.closed());
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
                    self.events.new_stream(next_stream_id);

                    self.recv_streams.insert(
                        next_stream_id,
                        RecvStream::new(
                            next_stream_id,
                            recv_initial_max_stream_data,
                            self.flow_mgr.clone(),
                            self.events.clone(),
                        ),
                    );

                    if next_stream_id.is_bidi() {
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
    /// Returns new stream id
    /// # Errors
    /// `ConnectionState` if the connecton stat does not allow to create streams.
    /// `StreamLimitError` if we are limiied by server's stream concurence.
    pub fn stream_create(&mut self, st: StreamType) -> Res<u64> {
        // Can't make streams while closing, otherwise rely on the stream limits.
        match self.state {
            State::Closing { .. } | State::Draining { .. } | State::Closed { .. } => {
                return Err(Error::ConnectionState);
            }
            State::WaitInitial | State::Handshaking => {
                if self.role == Role::Client && self.zero_rtt_state != ZeroRttState::Sending {
                    return Err(Error::ConnectionState);
                }
            }
            // In all other states, trust that the stream limits are correct.
            _ => (),
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
    /// # Errors
    /// `InvalidStreamId` the stream does not exist,
    /// `InvalidInput` if length of `data` is zero,
    /// `FinalSizeError` if the stream has already been closed.
    pub fn stream_send(&mut self, stream_id: u64, data: &[u8]) -> Res<usize> {
        self.send_streams.get_mut(stream_id.into())?.send(data)
    }

    /// Send all data or nothing on a stream. May cause DATA_BLOCKED or
    /// STREAM_DATA_BLOCKED frames to be sent.
    /// Returns true if data was successfully sent, otherwise false.
    /// # Errors
    /// `InvalidStreamId` the stream does not exist,
    /// `InvalidInput` if length of `data` is zero,
    /// `FinalSizeError` if the stream has already been closed.
    pub fn stream_send_atomic(&mut self, stream_id: u64, data: &[u8]) -> Res<bool> {
        let val = self
            .send_streams
            .get_mut(stream_id.into())?
            .send_atomic(data);
        if let Ok(val) = val {
            debug_assert!(
                val == 0 || val == data.len(),
                "Unexpected value {} when trying to send {} bytes atomically",
                val,
                data.len()
            );
        }
        val.map(|v| v == data.len())
    }

    /// Bytes that stream_send() is guaranteed to accept for sending.
    /// i.e. that will not be blocked by flow credits or send buffer max
    /// capacity.
    pub fn stream_avail_send_space(&self, stream_id: u64) -> Res<usize> {
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
    /// # Errors
    /// `InvalidStreamId` if the stream does not exist.
    /// `NoMoreData` if data and fin bit were previously read by the application.
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
    use crate::cc::PACING_BURST_SIZE;
    use crate::cc::{INITIAL_CWND_PKTS, MAX_DATAGRAM_SIZE, MIN_CONG_WINDOW};
    use crate::frame::{CloseError, StreamType};
    use crate::packet::PACKET_BIT_LONG;
    use crate::path::PATH_MTU_V6;
    use crate::recovery::ACK_ONLY_SIZE_LIMIT;
    use crate::recovery::PTO_PACKET_COUNT;
    use crate::send_stream::SEND_BUFFER_SIZE;
    use crate::tracking::{ACK_DELAY, MAX_UNACKED_PKTS};
    use std::convert::TryInto;

    use neqo_crypto::{constants::TLS_CHACHA20_POLY1305_SHA256, AllowZeroRtt};
    use std::mem;
    use test_fixture::{self, assertions, fixture_init, loopback, now};

    const AT_LEAST_PTO: Duration = Duration::from_secs(1);
    const DEFAULT_STREAM_DATA: &[u8] = b"message";

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
            QuicVersion::default(),
        )
        .expect("create a default client")
    }
    pub fn default_server() -> Connection {
        fixture_init();

        let mut c = Connection::new_server(
            test_fixture::DEFAULT_KEYS,
            test_fixture::DEFAULT_ALPN,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(5))),
            QuicVersion::default(),
        )
        .expect("create a default server");
        c.server_enable_0rtt(&test_fixture::anti_replay(), AllowZeroRtt {})
            .expect("enable 0-RTT");
        c
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
    fn handshake_failed_authentication() {
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
        assert!(out.as_dgram_ref().is_some());
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
        for (d_num, d) in datagrams.into_iter().enumerate() {
            let out = server.process(Some(d), now());
            assert_eq!(
                out.as_dgram_ref().is_some(),
                (d_num + 1) % (MAX_UNACKED_PKTS + 1) == 0
            );
            qdebug!("Output={:0x?}", out.as_dgram_ref());
        }
        assert_eq!(*server.state(), State::Confirmed);

        let mut buf = vec![0; 4000];

        let mut stream_ids = server.events().filter_map(|evt| match evt {
            ConnectionEvent::NewStream { stream_id, .. } => Some(stream_id),
            _ => None,
        });
        let stream_id = stream_ids.next().expect("should have a new stream event");
        let (received, fin) = server.stream_recv(stream_id.as_u64(), &mut buf).unwrap();
        assert_eq!(received, 4000);
        assert_eq!(fin, false);
        let (received, fin) = server.stream_recv(stream_id.as_u64(), &mut buf).unwrap();
        assert_eq!(received, 140);
        assert_eq!(fin, false);

        let stream_id = stream_ids
            .next()
            .expect("should have a second new stream event");
        let (received, fin) = server.stream_recv(stream_id.as_u64(), &mut buf).unwrap();
        assert_eq!(received, 60);
        assert_eq!(fin, true);
    }

    /// Drive the handshake between the client and server.
    fn handshake(
        client: &mut Connection,
        server: &mut Connection,
        now: Instant,
        rtt: Duration,
    ) -> Instant {
        let mut a = client;
        let mut b = server;
        let mut now = now;

        let mut input = None;
        let is_done = |c: &mut Connection| match c.state() {
            State::Confirmed | State::Closing { .. } | State::Closed(..) => true,
            _ => false,
        };

        while !is_done(a) {
            let _ = maybe_authenticate(a);
            let had_input = input.is_some();
            let output = a.process(input, now).dgram();
            assert!(had_input || output.is_some());
            input = output;
            qtrace!("t += {:?}", rtt / 2);
            now += rtt / 2;
            mem::swap(&mut a, &mut b);
        }
        let _ = a.process(input, now);
        now
    }

    fn connect_with_rtt(
        client: &mut Connection,
        server: &mut Connection,
        now: Instant,
        rtt: Duration,
    ) -> Instant {
        let now = handshake(client, server, now, rtt);
        assert_eq!(*client.state(), State::Confirmed);
        assert_eq!(*client.state(), State::Confirmed);

        assert_eq!(client.loss_recovery.rtt(), rtt);
        assert_eq!(server.loss_recovery.rtt(), rtt);
        now
    }

    fn connect(client: &mut Connection, server: &mut Connection) {
        connect_with_rtt(client, server, now(), Duration::new(0, 0));
    }

    fn assert_error(c: &Connection, err: ConnectionError) {
        match c.state() {
            State::Closing { error, .. } | State::Draining { error, .. } | State::Closed(error) => {
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
            QuicVersion::default(),
        )
        .unwrap();
        let mut server = default_server();

        handshake(&mut client, &mut server, now(), Duration::new(0, 0));
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

    fn exchange_ticket(client: &mut Connection, server: &mut Connection, now: Instant) -> Vec<u8> {
        server.send_ticket(now, &[]).expect("can send ticket");
        let ticket = server.process_output(now).dgram();
        assert!(ticket.is_some());
        client.process_input(ticket.unwrap(), now);
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

        let token = exchange_ticket(&mut client, &mut server, now());
        let mut client = default_client();
        client
            .set_resumption_token(now(), &token[..])
            .expect("should set token");
        let mut server = default_server();
        connect(&mut client, &mut server);
        assert!(client.tls_info().unwrap().resumed());
        assert!(server.tls_info().unwrap().resumed());
    }

    #[test]
    fn remember_smoothed_rtt() {
        let mut client = default_client();
        let mut server = default_server();

        const RTT1: Duration = Duration::from_millis(130);
        let now = connect_with_rtt(&mut client, &mut server, now(), RTT1);
        assert_eq!(client.loss_recovery.rtt(), RTT1);

        let token = exchange_ticket(&mut client, &mut server, now);
        let mut client = default_client();
        let mut server = default_server();
        client.set_resumption_token(now, &token[..]).unwrap();
        assert_eq!(
            client.loss_recovery.rtt(),
            RTT1,
            "client should remember previous RTT"
        );

        const RTT2: Duration = Duration::from_millis(70);
        connect_with_rtt(&mut client, &mut server, now, RTT2);
        assert_eq!(
            client.loss_recovery.rtt(),
            RTT2,
            "previous RTT should be completely erased"
        );
    }

    #[test]
    fn zero_rtt_negotiate() {
        // Note that the two servers in this test will get different anti-replay filters.
        // That's OK because we aren't testing anti-replay.
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let token = exchange_ticket(&mut client, &mut server, now());
        let mut client = default_client();
        client
            .set_resumption_token(now(), &token[..])
            .expect("should set token");
        let mut server = default_server();
        connect(&mut client, &mut server);
        assert!(client.tls_info().unwrap().early_data_accepted());
        assert!(server.tls_info().unwrap().early_data_accepted());
    }

    #[test]
    fn zero_rtt_send_recv() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let token = exchange_ticket(&mut client, &mut server, now());
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
        assert_eq!(client_stream_id, server_stream_id.as_u64());
    }

    #[test]
    fn zero_rtt_send_coalesce() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let token = exchange_ticket(&mut client, &mut server, now());
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
                ConnectionEvent::NewStream { stream_id } => Some(stream_id),
                _ => None,
            })
            .expect("should have received a new stream event");
        assert_eq!(client_stream_id, server_stream_id.as_u64());
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

        let token = exchange_ticket(&mut client, &mut server, now());
        let mut client = default_client();
        client
            .set_resumption_token(now(), &token[..])
            .expect("should set token");
        let mut server = Connection::new_server(
            test_fixture::DEFAULT_KEYS,
            test_fixture::DEFAULT_ALPN,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(10))),
            QuicVersion::default(),
        )
        .unwrap();
        // Using a freshly initialized anti-replay context
        // should result in the server rejecting 0-RTT.
        let ar = AntiReplay::new(now(), test_fixture::ANTI_REPLAY_WINDOW, 1, 3)
            .expect("setup anti-replay");
        server
            .server_enable_0rtt(&ar, AllowZeroRtt {})
            .expect("enable 0-RTT");

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
        let _ = server.process(out.dgram(), now());

        assert_eq!(Ok(()), server.stream_close_send(stream_id));
        let out = server.process(None, now());
        let _ = client.process(out.dgram(), now());
        let stream_readable = |e| matches!(e, ConnectionEvent::RecvStreamReadable {..});
        assert!(client.events().any(stream_readable));
    }

    /// Connect with an RTT and then force both peers to be idle.
    /// Getting the client and server to reach an idle state is surprisingly hard.
    /// The server sends HANDSHAKE_DONE at the end of the handshake, and the client
    /// doesn't immediately acknowledge it.  Reordering packets does the trick.
    fn connect_rtt_idle(
        client: &mut Connection,
        server: &mut Connection,
        rtt: Duration,
    ) -> Instant {
        let mut now = connect_with_rtt(client, server, now(), rtt);
        let p1 = send_something(server, now);
        let p2 = send_something(server, now);
        now += rtt / 2;
        // Delivering p2 first at the client causes it to want to ACK.
        client.process_input(p2, now);
        // Delivering p1 should not have the client change its mind about the ACK.
        let ack = client.process(Some(p1), now).dgram();
        assert!(ack.is_some());
        assert_eq!(
            server.process(ack, now),
            Output::Callback(LOCAL_IDLE_TIMEOUT)
        );
        assert_eq!(
            client.process_output(now),
            Output::Callback(LOCAL_IDLE_TIMEOUT)
        );
        now
    }

    fn connect_force_idle(client: &mut Connection, server: &mut Connection) {
        connect_rtt_idle(client, server, Duration::new(0, 0));
    }

    #[test]
    fn idle_timeout() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

        // Still connected after 29 seconds. Idle timer not reset
        let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT - Duration::from_secs(1));
        assert!(matches!(client.state(), State::Confirmed));

        let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT);

        // Not connected after LOCAL_IDLE_TIMEOUT seconds.
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn asymmetric_idle_timeout() {
        const LOWER_TIMEOUT_MS: u64 = 1000;
        const LOWER_TIMEOUT: Duration = Duration::from_millis(LOWER_TIMEOUT_MS);
        // Sanity check the constant.
        assert!(LOWER_TIMEOUT < LOCAL_IDLE_TIMEOUT);

        let mut client = default_client();
        let mut server = default_server();

        // Overwrite the default at the server.
        server
            .tps
            .borrow_mut()
            .local
            .set_integer(tparams::IDLE_TIMEOUT, LOWER_TIMEOUT_MS);
        server.idle_timeout.timeout = LOWER_TIMEOUT;

        // Now connect and force idleness manually.
        connect(&mut client, &mut server);
        let p1 = send_something(&mut server, now());
        let p2 = send_something(&mut server, now());
        client.process_input(p2, now());
        let ack = client.process(Some(p1), now()).dgram();
        assert!(ack.is_some());
        // Now the server has its ACK and both should be idle.
        assert_eq!(server.process(ack, now()), Output::Callback(LOWER_TIMEOUT));
        assert_eq!(client.process(None, now()), Output::Callback(LOWER_TIMEOUT));
    }

    #[test]
    fn tiny_idle_timeout() {
        const RTT: Duration = Duration::from_millis(500);
        const LOWER_TIMEOUT_MS: u64 = 100;
        const LOWER_TIMEOUT: Duration = Duration::from_millis(LOWER_TIMEOUT_MS);
        // We won't respect a value that is lower than 3*PTO, sanity check.
        assert!(LOWER_TIMEOUT < 3 * RTT);

        let mut client = default_client();
        let mut server = default_server();

        // Overwrite the default at the server.
        server
            .set_local_tparam(
                tparams::IDLE_TIMEOUT,
                TransportParameter::Integer(LOWER_TIMEOUT_MS),
            )
            .unwrap();
        server.idle_timeout.timeout = LOWER_TIMEOUT;

        // Now connect with an RTT and force idleness manually.
        let mut now = connect_with_rtt(&mut client, &mut server, now(), RTT);
        let p1 = send_something(&mut server, now);
        let p2 = send_something(&mut server, now);
        now += RTT / 2;
        client.process_input(p2, now);
        let ack = client.process(Some(p1), now).dgram();
        assert!(ack.is_some());

        // The client should be idle now, but with a different timer.
        if let Output::Callback(t) = client.process(None, now) {
            assert!(t > LOWER_TIMEOUT);
        } else {
            panic!("Client not idle");
        }

        // The server should go idle after the ACK, but again with a larger timeout.
        now += RTT / 2;
        if let Output::Callback(t) = client.process(ack, now) {
            assert!(t > LOWER_TIMEOUT);
        } else {
            panic!("Client not idle");
        }
    }

    #[test]
    fn idle_send_packet1() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"hello").unwrap(), 5);

        let out = client.process(None, now + Duration::from_secs(10));
        let out = server.process(out.dgram(), now + Duration::from_secs(10));

        // Still connected after 39 seconds because idle timer reset by outgoing
        // packet
        let _ = client.process(
            out.dgram(),
            now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(9),
        );
        assert!(matches!(client.state(), State::Confirmed));

        // Not connected after 40 seconds.
        let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(10));

        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn idle_send_packet2() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, b"hello").unwrap(), 5);

        let _out = client.process(None, now + Duration::from_secs(10));

        assert_eq!(client.stream_send(2, b"there").unwrap(), 5);
        let _out = client.process(None, now + Duration::from_secs(20));

        // Still connected after 39 seconds.
        let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(9));
        assert!(matches!(client.state(), State::Confirmed));

        // Not connected after 40 seconds because timer not reset by second
        // outgoing packet
        let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(10));
        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn idle_recv_packet() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);
        assert_eq!(client.stream_send(0, b"hello").unwrap(), 5);

        // Respond with another packet
        let out = client.process(None, now + Duration::from_secs(10));
        server.process_input(out.dgram().unwrap(), now + Duration::from_secs(10));
        assert_eq!(server.stream_send(0, b"world").unwrap(), 5);
        let out = server.process_output(now + Duration::from_secs(10));
        assert_ne!(out.as_dgram_ref(), None);

        let _ = client.process(out.dgram(), now + Duration::from_secs(20));
        assert!(matches!(client.state(), State::Confirmed));

        // Still connected after 49 seconds because idle timer reset by received
        // packet
        let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(19));
        assert!(matches!(client.state(), State::Confirmed));

        // Not connected after 50 seconds.
        let _ = client.process(None, now + LOCAL_IDLE_TIMEOUT + Duration::from_secs(20));

        assert!(matches!(client.state(), State::Closed(_)));
    }

    #[test]
    fn max_data() {
        let mut client = default_client();
        let mut server = default_server();

        const SMALL_MAX_DATA: usize = 16383;

        server
            .set_local_tparam(
                tparams::INITIAL_MAX_DATA,
                TransportParameter::Integer(SMALL_MAX_DATA.try_into().unwrap()),
            )
            .unwrap();

        connect(&mut client, &mut server);

        let stream_id = client.stream_create(StreamType::UniDi).unwrap();
        assert_eq!(client.events().count(), 2); // SendStreamWritable, StateChange(connected)
        assert_eq!(stream_id, 2);
        assert_eq!(
            client.stream_avail_send_space(stream_id).unwrap(),
            SMALL_MAX_DATA
        );
        assert_eq!(
            client
                .stream_send(stream_id, &[b'a'; RECV_BUFFER_SIZE])
                .unwrap(),
            SMALL_MAX_DATA
        );
        assert_eq!(client.events().count(), 0);

        assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 0);
        client
            .send_streams
            .get_mut(stream_id.into())
            .unwrap()
            .mark_as_sent(0, 4096, false);
        assert_eq!(client.events().count(), 0);
        client
            .send_streams
            .get_mut(stream_id.into())
            .unwrap()
            .mark_as_acked(0, 4096, false);
        assert_eq!(client.events().count(), 0);

        assert_eq!(client.stream_send(stream_id, b"hello").unwrap(), 0);
        // no event because still limited by conn max data
        assert_eq!(client.events().count(), 0);

        // Increase max data. Avail space now limited by stream credit
        client.handle_max_data(100_000_000);
        assert_eq!(
            client.stream_avail_send_space(stream_id).unwrap(),
            SEND_BUFFER_SIZE - SMALL_MAX_DATA
        );

        // Increase max stream data. Avail space now limited by tx buffer
        client
            .send_streams
            .get_mut(stream_id.into())
            .unwrap()
            .set_max_stream_data(100_000_000);
        assert_eq!(
            client.stream_avail_send_space(stream_id).unwrap(),
            SEND_BUFFER_SIZE - SMALL_MAX_DATA + 4096
        );

        let evts = client.events().collect::<Vec<_>>();
        assert_eq!(evts.len(), 1);
        assert!(matches!(evts[0], ConnectionEvent::SendStreamWritable{..}));
    }

    // Test that we split crypto data if they cannot fit into one packet.
    // To test this we will use a long server certificate.
    #[test]
    fn crypto_frame_split() {
        let mut client = default_client();

        let mut server = Connection::new_server(
            test_fixture::LONG_CERT_KEYS,
            test_fixture::DEFAULT_ALPN,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(6))),
            QuicVersion::default(),
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
        let _ = server.process(out.dgram(), now());

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

        let _ = client.process(out.dgram(), now());
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
        let _ = server.process(out.dgram(), now());

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
        let _ = client.process(out.dgram(), now());
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
        assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

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
        now += AT_LEAST_PTO;
        let out = client.process(None, now);

        let frames = server.test_process_input(out.dgram().unwrap(), now);

        assert!(frames.iter().all(|(_, sp)| *sp == PNSpace::ApplicationData));
        assert!(frames.iter().any(|(f, _)| *f == Frame::Ping));
        assert!(frames
            .iter()
            .any(|(f, _)| matches!(f, Frame::Stream { stream_id, .. } if stream_id.as_u64() == 2)));
        assert!(frames
            .iter()
            .any(|(f, _)| matches!(f, Frame::Stream { stream_id, .. } if stream_id.as_u64() == 6)));
    }

    #[test]
    fn pto_works_full_cwnd() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let res = client.process(None, now());
        assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

        // Send lots of data.
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        let (dgrams, now) = fill_cwnd(&mut client, 2, now());
        assert_full_cwnd(&dgrams, POST_HANDSHAKE_CWND);

        // Fill the CWND after waiting for a PTO.
        let (dgrams, now) = fill_cwnd(&mut client, 2, now + AT_LEAST_PTO);
        assert_eq!(dgrams.len(), 2); // Two packets in the PTO.

        // All (2) datagrams contain one PING frame and at least one STREAM frame.
        for d in dgrams {
            assert_eq!(d.len(), PATH_MTU_V6);
            let frames = server.test_process_input(d, now);
            assert_eq!(
                frames
                    .iter()
                    .filter(|i| matches!(i, (Frame::Ping, PNSpace::ApplicationData)))
                    .count(),
                1
            );
            assert!(
                frames
                    .iter()
                    .filter(|i| matches!(i, (Frame::Stream { .. }, PNSpace::ApplicationData)))
                    .count()
                    >= 1
            );
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
        assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

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
        const INITIAL_PTO: Duration = Duration::from_millis(300);
        let mut now = now();

        qdebug!("---- client: generate CH");
        let mut client = default_client();
        let pkt1 = client.process(None, now).dgram();
        assert!(pkt1.is_some());
        assert_eq!(pkt1.clone().unwrap().len(), PATH_MTU_V6);

        let delay = client.process(None, now).callback();
        assert_eq!(delay, INITIAL_PTO);

        // Resend initial after PTO.
        now += delay;
        let pkt2 = client.process(None, now).dgram();
        assert!(pkt2.is_some());
        assert_eq!(pkt2.unwrap().len(), PATH_MTU_V6);

        let pkt3 = client.process(None, now).dgram();
        assert!(pkt3.is_some());
        assert_eq!(pkt3.unwrap().len(), PATH_MTU_V6);

        let delay = client.process(None, now).callback();
        // PTO has doubled.
        assert_eq!(delay, INITIAL_PTO * 2);

        // Server process the first initial pkt.
        let mut server = default_server();
        let out = server.process(pkt1, now).dgram();
        assert!(out.is_some());

        // Client receives ack for the first initial packet as well a Handshake packet.
        // After the handshake packet the initial keys and the crypto stream for the initial
        // packet number space will be discarded.
        // Here only an ack for the Handshake packet will be sent.
        let out = client.process(out, now).dgram();
        assert!(out.is_some());

        // We do not have PTO for the resent initial packet any more, but
        // the Handshake PTO timer should be armed.  As the RTT is apparently
        // the same as the initial PTO value, and there is only one sample,
        // the PTO will be 3x the INITIAL PTO.
        let delay = client.process(None, now).callback();
        assert_eq!(delay, INITIAL_PTO * 3);
    }

    /// A complete handshake that involves a PTO in the Handshake space.
    #[test]
    fn pto_handshake_complete() {
        let mut now = now();
        // start handshake
        let mut client = default_client();
        let mut server = default_server();

        let pkt = client.process(None, now).dgram();
        let cb = client.process(None, now).callback();
        assert_eq!(cb, Duration::from_millis(300));

        now += Duration::from_millis(10);
        let pkt = server.process(pkt, now).dgram();

        now += Duration::from_millis(10);
        let pkt = client.process(pkt, now).dgram();

        let cb = client.process(None, now).callback();
        // The client now has a single RTT estimate (20ms), so
        // the handshake PTO is set based on that.
        assert_eq!(cb, Duration::from_millis(60));

        now += Duration::from_millis(10);
        let pkt = server.process(pkt, now).dgram();
        assert!(pkt.is_none());

        now += Duration::from_millis(10);
        client.authenticated(AuthenticationStatus::Ok, now);

        qdebug!("---- client: SH..FIN -> FIN");
        let pkt1 = client.process(None, now).dgram();
        assert!(pkt1.is_some());

        let cb = client.process(None, now).callback();
        assert_eq!(cb, Duration::from_millis(60));

        // Wait for PTO to expire and resend a handshake packet
        now += Duration::from_millis(60);
        let pkt2 = client.process(None, now).dgram();
        assert!(pkt2.is_some());

        // Get a second PTO packet.
        let pkt3 = client.process(None, now).dgram();
        assert!(pkt3.is_some());

        // PTO has been doubled.
        let cb = client.process(None, now).callback();
        assert_eq!(cb, Duration::from_millis(120));

        now += Duration::from_millis(10);
        // Server receives the first packet.
        // The output will be a Handshake packet with an ack and a app pn space packet with
        // HANDSHAKE_DONE.
        let pkt = server.process(pkt1, now).dgram();
        assert!(pkt.is_some());

        // Check that the PTO packets (pkt2, pkt3) are Handshake packets.
        // The server discarded the Handshake keys already, therefore they are dropped.
        let dropped_before = server.stats().dropped_rx;
        let frames = server.test_process_input(pkt2.unwrap(), now);
        assert_eq!(1, server.stats().dropped_rx - dropped_before);
        assert!(frames.is_empty());

        let dropped_before = server.stats().dropped_rx;
        let frames = server.test_process_input(pkt3.unwrap(), now);
        assert_eq!(1, server.stats().dropped_rx - dropped_before);
        assert!(frames.is_empty());

        now += Duration::from_millis(10);
        // Client receive ack for the first packet
        let cb = client.process(pkt, now).callback();
        // Ack delay timer for the packet carrying HANDSHAKE_DONE.
        assert_eq!(cb, ACK_DELAY);

        // Let the ack timer expire.
        now += cb;
        let out = client.process(None, now).dgram();
        assert!(out.is_some());
        let cb = client.process(None, now).callback();
        // The handshake keys are discarded, but now we're back to the idle timeout.
        // We don't send another PING because the handshake space is done and there
        // is nothing to probe for.
        assert_eq!(cb, LOCAL_IDLE_TIMEOUT - ACK_DELAY);
    }

    /// Test that PTO in the Handshake space contains the right frames.
    #[test]
    fn pto_handshake_frames() {
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

        // Wait for PTO to expire and resend a handshake packet.
        now += Duration::from_millis(60);
        let pkt2 = client.process(None, now).dgram();
        assert!(pkt2.is_some());

        now += Duration::from_millis(10);
        let frames = server.test_process_input(pkt2.unwrap(), now);

        assert_eq!(frames.len(), 2);
        assert_eq!(frames[0], (Frame::Ping, PNSpace::Handshake));
        assert!(matches!(
            frames[1],
            (Frame::Crypto { .. }, PNSpace::Handshake)
        ));
    }

    /// In the case that the Handshake takes too many packets, the server might
    /// be stalled on the anti-amplification limit.  If a Handshake ACK from the
    /// client is lost, the client has to keep the PTO timer armed or the server
    /// might be unable to send anything, causing a deadlock.
    #[test]
    fn handshake_ack_pto() {
        const RTT: Duration = Duration::from_millis(10);
        let mut now = now();
        let mut client = default_client();
        let mut server = default_server();
        // This is a greasing transport parameter, and large enough that the
        // server needs to send two Handshake packets.
        let big = TransportParameter::Bytes(vec![0; PATH_MTU_V6]);
        server.set_local_tparam(0xce16, big).unwrap();

        let c1 = client.process(None, now).dgram();

        now += RTT / 2;
        let s1 = server.process(c1, now).dgram();
        assert!(s1.is_some());
        let s2 = server.process(None, now).dgram();
        assert!(s1.is_some());

        // Now let the client have the Initial, but drop the first coalesced Handshake packet.
        now += RTT / 2;
        let (initial, _) = split_datagram(s1.unwrap());
        client.process_input(initial, now);
        let c2 = client.process(s2, now).dgram();
        assert!(c2.is_some()); // This is an ACK.  Drop it.
        let delay = client.process(None, now).callback();
        assert_eq!(delay, RTT * 3);

        // Wait for the PTO and ensure that the client generates a packet.
        now += delay;
        let c3 = client.process(None, now).dgram();
        assert!(c3.is_some());

        now += RTT / 2;
        let frames = server.test_process_input(c3.unwrap(), now);
        assert_eq!(frames, vec![(Frame::Ping, PNSpace::Handshake)]);

        // Now complete the handshake as cheaply as possible.
        let dgram = server.process(None, now).dgram();
        client.process_input(dgram.unwrap(), now);
        maybe_authenticate(&mut client);
        let dgram = client.process(None, now).dgram();
        assert_eq!(*client.state(), State::Connected);
        let dgram = server.process(dgram, now).dgram();
        assert_eq!(*server.state(), State::Confirmed);
        client.process_input(dgram.unwrap(), now);
        assert_eq!(*client.state(), State::Confirmed);
    }

    #[test]
    // Absent path PTU discovery, max v6 packet size should be PATH_MTU_V6.
    fn verify_pkt_honors_mtu() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let now = now();

        let res = client.process(None, now);
        assert_eq!(res, Output::Callback(LOCAL_IDLE_TIMEOUT));

        // Try to send a large stream and verify first packet is correctly sized
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        assert_eq!(client.stream_send(2, &[0xbb; 2000]).unwrap(), 2000);
        let pkt0 = client.process(None, now);
        assert!(matches!(pkt0, Output::Datagram(_)));
        assert_eq!(pkt0.as_dgram_ref().unwrap().len(), PATH_MTU_V6);
    }

    /// This fills the congestion window from a single source.
    /// As the pacer will interfere with this, this moves time forward
    /// as `Output::Callback` is received.  Because it is hard to tell
    /// from the return value whether a timeout is an ACK delay, PTO, or
    /// pacing, this looks at the congestion window to tell when to stop.
    /// Returns a list of datagrams and the new time.
    fn fill_cwnd(src: &mut Connection, stream: u64, mut now: Instant) -> (Vec<Datagram>, Instant) {
        const BLOCK_SIZE: usize = 4_096;
        let mut total_dgrams = Vec::new();

        qtrace!(
            "fill_cwnd starting cwnd: {}",
            src.loss_recovery.cwnd_avail()
        );

        loop {
            let bytes_sent = src.stream_send(stream, &[0x42; BLOCK_SIZE]).unwrap();
            qtrace!("fill_cwnd wrote {} bytes", bytes_sent);
            if bytes_sent < BLOCK_SIZE {
                break;
            }
        }

        loop {
            let pkt = src.process_output(now);
            qtrace!(
                "fill_cwnd cwnd remaining={}, output: {:?}",
                src.loss_recovery.cwnd_avail(),
                pkt
            );
            match pkt {
                Output::Datagram(dgram) => {
                    total_dgrams.push(dgram);
                }
                Output::Callback(t) => {
                    if src.loss_recovery.cwnd_avail() < ACK_ONLY_SIZE_LIMIT {
                        break;
                    }
                    now += t;
                }
                _ => panic!(),
            }
        }

        (total_dgrams, now)
    }

    // Receive multiple packets and generate an ack-only packet.
    fn ack_bytes<D>(
        dest: &mut Connection,
        stream: u64,
        in_dgrams: D,
        now: Instant,
    ) -> (Vec<Datagram>, Vec<Frame>)
    where
        D: IntoIterator<Item = Datagram>,
        D::IntoIter: ExactSizeIterator,
    {
        let mut srv_buf = [0; 4_096];
        let mut recvd_frames = Vec::new();

        let in_dgrams = in_dgrams.into_iter();
        qdebug!([dest], "ack_bytes {} datagrams", in_dgrams.len());
        for dgram in in_dgrams {
            recvd_frames.extend(dest.test_process_input(dgram, now));
        }

        loop {
            let (bytes_read, _fin) = dest.stream_recv(stream, &mut srv_buf).unwrap();
            qtrace!([dest], "ack_bytes read {} bytes", bytes_read);
            if bytes_read == 0 {
                break;
            }
        }

        let mut tx_dgrams = Vec::new();
        while let Output::Datagram(dg) = dest.process_output(now) {
            tx_dgrams.push(dg);
        }

        assert!((tx_dgrams.len() == 1) || (tx_dgrams.len() == 2));

        (
            tx_dgrams,
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
        (data + ACK_ONLY_SIZE_LIMIT - 1) / PATH_MTU_V6
    }

    /// Determine the size of the last packet.
    /// The minimal size of a packet is `ACK_ONLY_SIZE_LIMIT`.
    fn last_packet(cwnd: usize) -> usize {
        if (cwnd % PATH_MTU_V6) > ACK_ONLY_SIZE_LIMIT {
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
        connect_force_idle(&mut client, &mut server);

        let now = now();

        // Try to send a lot of data
        assert_eq!(client.stream_create(StreamType::UniDi).unwrap(), 2);
        let (c_tx_dgrams, _) = fill_cwnd(&mut client, 2, now);
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);
        assert!(client.loss_recovery.cwnd_avail() < ACK_ONLY_SIZE_LIMIT);
    }

    #[test]
    /// Verify that CC moves to cong avoidance when a packet is marked lost.
    fn cc_slow_start_to_cong_avoidance_recovery_period() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let (c_tx_dgrams, now) = fill_cwnd(&mut client, 0, now());
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);
        // Predict the packet number of the last packet sent.
        // We have already sent one packet in `connect_force_idle` (an ACK),
        // so this will be equal to the number of packets in this flight.
        let flight1_largest = PacketNumber::try_from(c_tx_dgrams.len()).unwrap();

        // Server: Receive and generate ack
        let (s_tx_dgram, _recvd_frames) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // Client: Process ack
        for dgram in s_tx_dgram {
            let recvd_frames = client.test_process_input(dgram, now);

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
        }

        // Client: send more
        let (mut c_tx_dgrams, now) = fill_cwnd(&mut client, 0, now);
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND * 2);
        let flight2_largest = flight1_largest + u64::try_from(c_tx_dgrams.len()).unwrap();

        // Server: Receive and generate ack again, but drop first packet
        c_tx_dgrams.remove(0);
        let (s_tx_dgram, _recvd_frames) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // Client: Process ack
        for dgram in s_tx_dgram {
            let recvd_frames = client.test_process_input(dgram, now);

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
        connect_force_idle(&mut client, &mut server);

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let (mut c_tx_dgrams, now) = fill_cwnd(&mut client, 0, now());
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

        // Drop 0th packet. When acked, this should put client into CARP.
        c_tx_dgrams.remove(0);

        let c_tx_dgrams2 = c_tx_dgrams.split_off(5);

        // Server: Receive and generate ack
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);
        for dgram in s_tx_dgram {
            client.test_process_input(dgram, now);
        }

        // If we just triggered cong avoidance, these should be equal
        let cwnd1 = client.loss_recovery.cwnd();
        assert_eq!(cwnd1, client.loss_recovery.ssthresh());

        // Generate ACK for more received packets
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams2, now);

        // ACK more packets but they were sent before end of recovery period
        for dgram in s_tx_dgram {
            client.test_process_input(dgram, now);
        }

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

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let (mut c_tx_dgrams, mut now) = fill_cwnd(&mut client, 0, now());

        // Drop 0th packet. When acked, this should put client into CARP.
        c_tx_dgrams.remove(0);

        // Server: Receive and generate ack
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // Client: Process ack
        for dgram in s_tx_dgram {
            client.test_process_input(dgram, now);
        }

        // Should be in CARP now.

        now += Duration::from_millis(10); // Time passes. CARP -> CA

        // Now make sure that we increase congestion window according to the
        // accurate byte counting version of congestion avoidance.
        // Check over several increases to be sure.
        let mut expected_cwnd = client.loss_recovery.cwnd();
        for i in 0..5 {
            println!("{}", i);
            // Client: Send more data
            let (mut c_tx_dgrams, next_now) = fill_cwnd(&mut client, 0, now);
            now = next_now;

            let c_tx_size: usize = c_tx_dgrams.iter().map(|d| d.len()).sum();
            println!(
                "client sending {} bytes into cwnd of {}",
                c_tx_size,
                client.loss_recovery.cwnd()
            );
            assert_eq!(c_tx_size, expected_cwnd);

            // Until we process all the packets, the congestion window remains the same.
            // Note that we need the client to process ACK frames in stages, so split the
            // datagrams into two, ensuring that we allow for an ACK for each batch.
            let most = c_tx_dgrams.len() - MAX_UNACKED_PKTS - 1;
            let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams.drain(..most), now);
            for dgram in s_tx_dgram {
                assert_eq!(client.loss_recovery.cwnd(), expected_cwnd);
                client.process_input(dgram, now);
            }
            let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);
            for dgram in s_tx_dgram {
                assert_eq!(client.loss_recovery.cwnd(), expected_cwnd);
                client.process_input(dgram, now);
            }
            expected_cwnd += MAX_DATAGRAM_SIZE;
            assert_eq!(client.loss_recovery.cwnd(), expected_cwnd);
        }
    }

    fn induce_persistent_congestion(
        client: &mut Connection,
        server: &mut Connection,
        mut now: Instant,
    ) -> Instant {
        // Note: wait some arbitrary time that should be longer than pto
        // timer. This is rather brittle.
        now += AT_LEAST_PTO;

        qtrace!([client], "first PTO");
        let (c_tx_dgrams, next_now) = fill_cwnd(client, 0, now);
        now = next_now;
        assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

        qtrace!([client], "second PTO");
        now += AT_LEAST_PTO * 2;
        let (c_tx_dgrams, next_now) = fill_cwnd(client, 0, now);
        now = next_now;
        assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

        qtrace!([client], "third PTO");
        now += AT_LEAST_PTO * 4;
        let (c_tx_dgrams, next_now) = fill_cwnd(client, 0, now);
        now = next_now;
        assert_eq!(c_tx_dgrams.len(), 2); // Two PTO packets

        // Generate ACK
        let (s_tx_dgram, _) = ack_bytes(server, 0, c_tx_dgrams, now);

        // An ACK for the third PTO causes persistent congestion.
        for dgram in s_tx_dgram {
            client.process_input(dgram, now);
        }

        assert_eq!(client.loss_recovery.cwnd(), MIN_CONG_WINDOW);
        now
    }

    #[test]
    /// Verify transition to persistent congestion state if conditions are met.
    fn cc_slow_start_to_persistent_congestion_no_acks() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, 0, now());
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
        connect_force_idle(&mut client, &mut server);

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, 0, now());
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

        // Server: Receive and generate ack
        now += Duration::from_millis(100);
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        now += Duration::from_millis(100);
        for dgram in s_tx_dgram {
            client.process_input(dgram, now);
        }

        // send bytes that will be lost
        let (_c_tx_dgrams, next_now) = fill_cwnd(&mut client, 0, now);
        now = next_now + Duration::from_millis(100);

        induce_persistent_congestion(&mut client, &mut server, now);
    }

    #[test]
    /// Verify persistent congestion moves to slow start after recovery period
    /// ends.
    fn cc_persistent_congestion_to_slow_start() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        // Create stream 0
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets
        let (c_tx_dgrams, mut now) = fill_cwnd(&mut client, 0, now());
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

        // Server: Receive and generate ack
        now += Duration::from_millis(10);
        let (_s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // ACK lost.

        now = induce_persistent_congestion(&mut client, &mut server, now);

        // New part of test starts here

        now += Duration::from_millis(10);

        // Send packets from after start of CARP
        let (c_tx_dgrams, next_now) = fill_cwnd(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 2);

        // Server: Receive and generate ack
        now = next_now + Duration::from_millis(100);
        let (s_tx_dgram, _) = ack_bytes(&mut server, 0, c_tx_dgrams, now);

        // No longer in CARP. (pkts acked from after start of CARP)
        // Should be in slow start now.
        for dgram in s_tx_dgram {
            client.test_process_input(dgram, now);
        }

        // ACKing 2 packets should let client send 4.
        let (c_tx_dgrams, _) = fill_cwnd(&mut client, 0, now);
        assert_eq!(c_tx_dgrams.len(), 4);
    }

    fn check_discarded(peer: &mut Connection, pkt: Datagram, dropped: usize, dups: usize) {
        // Make sure to flush any saved datagrams before doing this.
        let _ = peer.process_output(now());

        let before = peer.stats();
        let out = peer.process(Some(pkt), now());
        assert!(out.as_dgram_ref().is_none());
        let after = peer.stats();
        assert_eq!(dropped, after.dropped_rx - before.dropped_rx);
        assert_eq!(dups, after.dups_rx - before.dups_rx);
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
    #[must_use]
    fn send_something(sender: &mut Connection, now: Instant) -> Datagram {
        let stream_id = sender.stream_create(StreamType::UniDi).unwrap();
        assert!(sender.stream_send(stream_id, DEFAULT_STREAM_DATA).is_ok());
        assert!(sender.stream_close_send(stream_id).is_ok());
        qdebug!([sender], "send_something on {}", stream_id);
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

        assert_eq!(client.get_epochs(), (Some(3), Some(3))); // (write, read)
        assert_eq!(server.get_epochs(), (Some(3), Some(3)));

        // TODO(mt) this needs to wait for handshake confirmation,
        // but for now, we can do this immediately.
        assert!(client.initiate_key_update().is_ok());
        assert!(client.initiate_key_update().is_err());

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
        now += AT_LEAST_PTO;
        let dgram = client.process(None, now).dgram();
        assert!(dgram.is_some()); // Drop this packet.
        assert_eq!(client.get_epochs(), (Some(4), Some(3)));
        let _ = server.process(None, now);
        assert_eq!(server.get_epochs(), (Some(4), Some(4)));

        // Even though the server has updated, it hasn't received an ACK yet.
        assert!(server.initiate_key_update().is_err());

        // Now get an ACK from the server.
        // The previous PTO packet (see above) was dropped, so we should get an ACK here.
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

        now += AT_LEAST_PTO;
        let _ = client.process(None, now);
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
            // rotates the keys.  The client stays in the present.
            let _ = server.process(None, now + AT_LEAST_PTO);
            assert_eq!(server.get_epochs(), (Some(4), Some(4)));
        } else {
            panic!("server should have a timer set");
        }

        // Now update keys on the server again.
        assert!(server.initiate_key_update().is_ok());
        assert_eq!(server.get_epochs(), (Some(5), Some(4)));

        let dgram = send_something(&mut server, now + AT_LEAST_PTO);

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
        connect_force_idle(&mut client, &mut server);

        // Create a stream
        assert_eq!(client.stream_create(StreamType::BiDi).unwrap(), 0);

        // Buffer up lot of data and generate packets, so that cc window is filled.
        let (c_tx_dgrams, now) = fill_cwnd(&mut client, 0, now());
        assert_full_cwnd(&c_tx_dgrams, POST_HANDSHAKE_CWND);

        // The server hasn't received any of these packets yet, the server
        // won't ACK, but if it sends an ack-eliciting packet instead.
        qdebug!([server], "Sending ack-eliciting");
        assert_eq!(server.stream_create(StreamType::BiDi).unwrap(), 1);
        server.stream_send(1, b"dropped").unwrap();
        let dropped_packet = server.process(None, now).dgram();
        assert!(dropped_packet.is_some()); // Now drop this one.

        // Now the server sends a packet that will force an ACK,
        // because the client will detect a gap.
        server.stream_send(1, b"sent").unwrap();
        let ack_eliciting_packet = server.process(None, now).dgram();
        assert!(ack_eliciting_packet.is_some());

        // The client can ack the server packet even if cc windows is full.
        qdebug!([client], "Process ack-eliciting");
        let ack_pkt = client.process(ack_eliciting_packet, now).dgram();
        assert!(ack_pkt.is_some());
        qdebug!([server], "Handle ACK");
        let frames = server.test_process_input(ack_pkt.unwrap(), now);
        assert_eq!(frames.len(), 1);
        assert!(matches!(
            frames[0],
            (Frame::Ack { .. }, PNSpace::ApplicationData)
        ));
    }

    #[test]
    fn after_fin_is_read_conn_events_for_stream_should_be_removed() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let id = server.stream_create(StreamType::BiDi).unwrap();
        server.stream_send(id, &[6; 10]).unwrap();
        server.stream_close_send(id).unwrap();
        let out = server.process(None, now()).dgram();
        assert!(out.is_some());

        let _ = client.process(out, now());

        // read from the stream before checking connection events.
        let mut buf = vec![0; 4000];
        let (_, fin) = client.stream_recv(id, &mut buf).unwrap();
        assert_eq!(fin, true);

        // Make sure we do not have RecvStreamReadable events for the stream when fin has been read.
        let readable_stream_evt =
            |e| matches!(e, ConnectionEvent::RecvStreamReadable { stream_id } if stream_id == id);
        assert!(!client.events().any(readable_stream_evt));
    }

    #[test]
    fn after_stream_stop_sending_is_called_conn_events_for_stream_should_be_removed() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let id = server.stream_create(StreamType::BiDi).unwrap();
        server.stream_send(id, &[6; 10]).unwrap();
        server.stream_close_send(id).unwrap();
        let out = server.process(None, now()).dgram();
        assert!(out.is_some());

        let _ = client.process(out, now());

        // send stop seending.
        client
            .stream_stop_sending(id, Error::NoError.code())
            .unwrap();

        // Make sure we do not have RecvStreamReadable events for the stream after stream_stop_sending
        // has been called.
        let readable_stream_evt =
            |e| matches!(e, ConnectionEvent::RecvStreamReadable { stream_id } if stream_id == id);
        assert!(!client.events().any(readable_stream_evt));
    }

    // During the handshake, an application close should be sanitized.
    #[test]
    fn early_application_close() {
        let mut client = default_client();
        let mut server = default_server();

        // One flight each.
        let dgram = client.process(None, now()).dgram();
        assert!(dgram.is_some());
        let dgram = server.process(dgram, now()).dgram();
        assert!(dgram.is_some());

        server.close(now(), 77, String::from(""));
        assert!(server.state().closed());
        let dgram = server.process(None, now()).dgram();
        assert!(dgram.is_some());

        let frames = client.test_process_input(dgram.unwrap(), now());
        assert!(matches!(
            frames[0],
            (
                Frame::ConnectionClose {
                    error_code: CloseError::Transport(code),
                    ..
                },
                PNSpace::Initial,
            ) if code == Error::ApplicationError.code()
        ));
        assert!(client.state().closed());
    }

    #[test]
    fn bad_tls_version() {
        let mut client = default_client();
        // Do a bad, bad thing.
        client
            .crypto
            .tls
            .set_option(neqo_crypto::Opt::Tls13CompatMode, true)
            .unwrap();
        let mut server = default_server();
        let dgram = client.process(None, now()).dgram();
        assert!(dgram.is_some());
        let dgram = server.process(dgram, now()).dgram();
        assert_eq!(
            *server.state(),
            State::Closed(ConnectionError::Transport(Error::ProtocolViolation))
        );
        assert!(dgram.is_some());
        let frames = client.test_process_input(dgram.unwrap(), now());
        assert!(matches!(
            frames[0],
            (
                Frame::ConnectionClose {
                    error_code: CloseError::Transport(_),
                    ..
                },
                PNSpace::Initial,
            )
        ));
    }

    #[test]
    fn pace() {
        const RTT: Duration = Duration::from_millis(1000);
        const DATA: &[u8] = &[0xcc; 4_096];
        let mut client = default_client();
        let mut server = default_server();
        let mut now = connect_rtt_idle(&mut client, &mut server, RTT);

        // Now fill up the pipe and watch it trickle out.
        let stream = client.stream_create(StreamType::BiDi).unwrap();
        loop {
            let written = client.stream_send(stream, DATA).unwrap();
            if written < DATA.len() {
                break;
            }
        }
        let mut count = 0;
        // We should get a burst at first.
        for _ in 0..PACING_BURST_SIZE {
            let dgram = client.process_output(now).dgram();
            assert!(dgram.is_some());
            count += 1;
        }
        let gap = client.process_output(now).callback();
        assert_ne!(gap, Duration::new(0, 0));
        for _ in PACING_BURST_SIZE..cwnd_packets(POST_HANDSHAKE_CWND) {
            assert_eq!(client.process_output(now).callback(), gap);
            now += gap;
            let dgram = client.process_output(now).dgram();
            assert!(dgram.is_some());
            count += 1;
        }
        assert_eq!(count, cwnd_packets(POST_HANDSHAKE_CWND));
        let fin = client.process_output(now).callback();
        assert_ne!(fin, Duration::new(0, 0));
        assert_ne!(fin, gap);
    }

    #[test]
    fn loss_recovery_crash() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);
        let now = now();

        // The server sends something, but we will drop this.
        let _ = send_something(&mut server, now);

        // Then send something again, but let it through.
        let ack = send_and_receive(&mut server, &mut client, now);
        assert!(ack.is_some());

        // Have the server process the ACK.
        let cb = server.process(ack, now).callback();
        assert!(cb > Duration::from_secs(0));

        // Now we leap into the future.  The server should regard the first
        // packet as lost based on time alone.
        let dgram = server.process(None, now + AT_LEAST_PTO).dgram();
        assert!(dgram.is_some());

        // This crashes.
        let _ = send_something(&mut server, now + AT_LEAST_PTO);
    }

    // If we receive packets after the PTO timer has fired, we won't clear
    // the PTO state, but we might need to acknowledge those packets.
    // This shouldn't happen, but we found that some implementations do this.
    #[test]
    fn ack_after_pto() {
        let mut client = default_client();
        let mut server = default_server();
        connect_force_idle(&mut client, &mut server);

        let mut now = now();

        // The client sends and is forced into a PTO.
        let _ = send_something(&mut client, now);

        // Jump forward to the PTO and drain the PTO packets.
        now += AT_LEAST_PTO;
        for _ in 0..PTO_PACKET_COUNT {
            let dgram = client.process(None, now).dgram();
            assert!(dgram.is_some());
        }
        assert!(client.process(None, now).dgram().is_none());

        // The server now needs to send something that will cause the
        // client to want to acknowledge it.  A little out of order
        // delivery is just the thing.
        // Note: The server can't ACK anything here, but none of what
        // the client has sent so far has been transferred.
        let _ = send_something(&mut server, now);
        let dgram = send_something(&mut server, now);

        // The client is now after a PTO, but if it receives something
        // that demands acknowledgment, it will send just the ACK.
        let ack = client.process(Some(dgram), now).dgram();
        assert!(ack.is_some());

        // Make sure that the packet only contained ACK frames.
        let frames = server.test_process_input(ack.unwrap(), now);
        assert_eq!(frames.len(), 1);
        for (frame, space) in frames {
            assert_eq!(space, PNSpace::ApplicationData);
            assert!(matches!(frame, Frame::Ack { .. }));
        }
    }

    /// Test the interaction between the loss recovery timer
    /// and the closing timer.
    #[test]
    fn closing_timers_interation() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let mut now = now();

        // We're going to induce time-based loss recovery so that timer is set.
        let _p1 = send_something(&mut client, now);
        let p2 = send_something(&mut client, now);
        let ack = server.process(Some(p2), now).dgram();
        assert!(ack.is_some()); // This is an ACK.

        // After processing the ACK, we should be on the loss recovery timer.
        let cb = client.process(ack, now).callback();
        assert_ne!(cb, Duration::from_secs(0));
        now += cb;

        // Rather than let the timer pop, close the connection.
        client.close(now, 0, "");
        let client_close = client.process(None, now).dgram();
        assert!(client_close.is_some());
        // This should now report the end of the closing period, not a
        // zero-duration wait driven by the (now defunct) loss recovery timer.
        let client_close_timer = client.process(None, now).callback();
        assert_ne!(client_close_timer, Duration::from_secs(0));
    }

    #[test]
    fn closing_and_draining() {
        const APP_ERROR: AppError = 7;
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        // Save a packet from the client for later.
        let p1 = send_something(&mut client, now());

        // Close the connection.
        client.close(now(), APP_ERROR, "");
        let client_close = client.process(None, now()).dgram();
        assert!(client_close.is_some());
        let client_close_timer = client.process(None, now()).callback();
        assert_ne!(client_close_timer, Duration::from_secs(0));

        // The client will spit out the same packet in response to anything it receives.
        let p3 = send_something(&mut server, now());
        let client_close2 = client.process(Some(p3), now()).dgram();
        assert_eq!(
            client_close.as_ref().unwrap().len(),
            client_close2.as_ref().unwrap().len()
        );

        // After this time, the client should transition to closed.
        let end = client.process(None, now() + client_close_timer);
        assert_eq!(end, Output::None);
        assert_eq!(
            *client.state(),
            State::Closed(ConnectionError::Application(APP_ERROR))
        );

        // When the server receives the close, it too should generate CONNECTION_CLOSE.
        let server_close = server.process(client_close, now()).dgram();
        assert!(server.state().closed());
        assert!(server_close.is_some());
        // .. but it ignores any further close packets.
        let server_close_timer = server.process(client_close2, now()).callback();
        assert_ne!(server_close_timer, Duration::from_secs(0));
        // Even a legitimate packet without a close in it.
        let server_close_timer2 = server.process(Some(p1), now()).callback();
        assert_eq!(server_close_timer, server_close_timer2);

        let end = server.process(None, now() + server_close_timer);
        assert_eq!(end, Output::None);
        assert_eq!(
            *server.state(),
            State::Closed(ConnectionError::Transport(Error::PeerApplicationError(
                APP_ERROR
            )))
        );
    }

    /// When we declare a packet as lost, we keep it around for a while for another loss period.
    /// Those packets should not affect how we report the loss recovery timer.
    /// As the loss recovery timer based on RTT we use that to drive the state.
    #[test]
    fn lost_but_kept_and_lr_timer() {
        const RTT: Duration = Duration::from_secs(1);
        let mut client = default_client();
        let mut server = default_server();
        let mut now = connect_with_rtt(&mut client, &mut server, now(), RTT);

        // Two packets (p1, p2) are sent at around t=0.  The first is lost.
        let _p1 = send_something(&mut client, now);
        let p2 = send_something(&mut client, now);

        // At t=RTT/2 the server receives the packet and ACKs it.
        now += RTT / 2;
        let ack = server.process(Some(p2), now).dgram();
        assert!(ack.is_some());
        // The client also sends another two packets (p3, p4), again losing the first.
        let _p3 = send_something(&mut client, now);
        let p4 = send_something(&mut client, now);

        // At t=RTT the client receives the ACK and goes into timed loss recovery.
        // The client doesn't call p1 lost at this stage, but it will soon.
        now += RTT / 2;
        let res = client.process(ack, now);
        // The client should be on a loss recovery timer as p1 is missing.
        let lr_timer = res.callback();
        // Loss recovery timer should be RTT/8, but only check for 0 or >=RTT/2.
        assert_ne!(lr_timer, Duration::from_secs(0));
        assert!(lr_timer < (RTT / 2));
        // The server also receives and acknowledges p4, again sending an ACK.
        let ack = server.process(Some(p4), now).dgram();
        assert!(ack.is_some());

        // At t=RTT*3/2 the client should declare p1 to be lost.
        now += RTT / 2;
        // So the client will send the data from p1 again.
        let res = client.process(None, now);
        assert!(res.dgram().is_some());
        // When the client processes the ACK, it should engage the
        // loss recovery timer for p3, not p1 (even though it still tracks p1).
        let res = client.process(ack, now);
        let lr_timer2 = res.callback();
        assert_eq!(lr_timer, lr_timer2);
    }

    /// Split the first packet off a coalesced packet.
    fn split_packet(buf: &[u8]) -> (&[u8], Option<&[u8]>) {
        if buf[0] & 0x80 == 0 {
            // Short header: easy.
            return (buf, None);
        }
        let mut dec = Decoder::from(buf);
        let first = dec.decode_byte().unwrap();
        dec.skip(4); // Version.
        dec.skip_vec(1); // DCID
        dec.skip_vec(1); // SCID
        if first & 0x30 == 0 {
            // Initial
            dec.skip_vvec();
        }
        dec.skip_vvec(); // The rest of the packet.
        let p1 = &buf[..dec.offset()];
        let p2 = if dec.remaining() > 0 {
            Some(dec.decode_remainder())
        } else {
            None
        };
        (p1, p2)
    }

    /// Split the first datagram off a coalesced datagram.
    fn split_datagram(d: Datagram) -> (Datagram, Option<Datagram>) {
        let (a, b) = split_packet(&d[..]);
        (
            Datagram::new(d.source(), d.destination(), a),
            b.map(|b| Datagram::new(d.source(), d.destination(), b)),
        )
    }

    /// We should not be setting the loss recovery timer based on packets
    /// that are sent prior to the largest acknowledged.
    /// Testing this requires that we construct a case where one packet
    /// number space causes the loss recovery timer to be engaged.  At the same time,
    /// there is a packet in another space that hasn't been acknowledged AND
    /// that packet number space has not received acknowledgments for later packets.
    #[test]
    fn loss_time_past_largest_acked() {
        const RTT: Duration = Duration::from_secs(10);
        const INCR: Duration = Duration::from_millis(1);
        let mut client = default_client();
        let mut server = default_server();

        let mut now = now();

        // Start the handshake.
        let c_in = client.process(None, now).dgram();
        now += RTT / 2;
        let s_hs1 = server.process(c_in, now).dgram();

        // Get some spare server handshake packets for the client to ACK.
        // This involves a time machine, so be a little cautious.
        // This test uses an RTT of 10s, but our server starts
        // with a much lower RTT estimate, so the PTO at this point should
        // be much smaller than an RTT and so the server shouldn't see
        // time go backwards.
        let s_pto = server.process(None, now).callback();
        assert_ne!(s_pto, Duration::from_secs(0));
        assert!(s_pto < RTT);
        let s_hs2 = server.process(None, now + s_pto).dgram();
        assert!(s_hs2.is_some());
        let s_hs3 = server.process(None, now + s_pto).dgram();
        assert!(s_hs3.is_some());

        // Get some Handshake packets from the client.
        // We need one to be left unacknowledged before one that is acknowledged.
        // So that the client engages the loss recovery timer.
        // This is complicated by the fact that it is hard to cause the client
        // to generate an ack-eliciting packet.  For that, we use the Finished message.
        // Reordering delivery ensures that the later packet is also acknowledged.
        now += RTT / 2;
        let c_hs1 = client.process(s_hs1, now).dgram();
        assert!(c_hs1.is_some()); // This comes first, so it's useless.
        maybe_authenticate(&mut client);
        let c_hs2 = client.process(None, now).dgram();
        assert!(c_hs2.is_some()); // This one will elicit an ACK.

        // The we need the outstanding packet to be sent after the
        // application data packet, so space these out a tiny bit.
        let _p1 = send_something(&mut client, now + INCR);
        let c_hs3 = client.process(s_hs2, now + (INCR * 2)).dgram();
        assert!(c_hs3.is_some()); // This will be left outstanding.
        let c_hs4 = client.process(s_hs3, now + (INCR * 3)).dgram();
        assert!(c_hs4.is_some()); // This will be acknowledged.

        // Process c_hs2 and c_hs4, but skip c_hs3.
        // Then get an ACK for the client.
        now += RTT / 2;
        // Deliver c_hs4 first, but don't generate a packet.
        server.process_input(c_hs4.unwrap(), now);
        let s_ack = server.process(c_hs2, now).dgram();
        assert!(s_ack.is_some());
        // This includes an ACK, but it also includes HANDSHAKE_DONE,
        // which we need to remove because that will cause the Handshake loss
        // recovery state to be dropped.
        let (s_hs_ack, _s_ap_ack) = split_datagram(s_ack.unwrap());

        // Now the client should start its loss recovery timer based on the ACK.
        now += RTT / 2;
        let c_ack = client.process(Some(s_hs_ack), now).dgram();
        assert!(c_ack.is_none());
        // The client should now have the loss recovery timer active.
        let lr_time = client.process(None, now).callback();
        assert_ne!(lr_time, Duration::from_secs(0));
        assert!(lr_time < (RTT / 2));

        // Skipping forward by the loss recovery timer should cause the client to
        // mark packets as lost and retransmit, after which we should be on the PTO
        // timer.
        now += lr_time;
        let delay = client.process(None, now).callback();
        assert_ne!(delay, Duration::from_secs(0));
        assert!(delay > lr_time);
    }

    #[test]
    fn unknown_version() {
        let mut client = default_client();
        // Start the handshake.
        let _ = client.process(None, now()).dgram();

        let mut unknown_version_packet = vec![0x80, 0x1a, 0x1a, 0x1a, 0x1a];
        unknown_version_packet.resize(1200, 0x0);
        let _ = client.process(
            Some(Datagram::new(
                loopback(),
                loopback(),
                unknown_version_packet,
            )),
            now(),
        );
        assert_eq!(1, client.stats().dropped_rx);
    }

    /// Run a single ChaCha20-Poly1305 test and get a PTO.
    #[test]
    fn chacha20poly1305() {
        let mut server = default_server();
        let mut client = Connection::new_client(
            test_fixture::DEFAULT_SERVER_NAME,
            test_fixture::DEFAULT_ALPN,
            Rc::new(RefCell::new(FixedConnectionIdManager::new(0))),
            loopback(),
            loopback(),
            QuicVersion::default(),
        )
        .expect("create a default client");
        client.set_ciphers(&[TLS_CHACHA20_POLY1305_SHA256]).unwrap();
        connect_force_idle(&mut client, &mut server);
    }

    /// Test that a client can handle a stateless reset correctly.
    #[test]
    fn stateless_reset_client() {
        let mut client = default_client();
        let mut server = default_server();
        server
            .set_local_tparam(
                tparams::STATELESS_RESET_TOKEN,
                TransportParameter::Bytes(vec![77; 16]),
            )
            .unwrap();
        connect_force_idle(&mut client, &mut server);

        client.process_input(Datagram::new(loopback(), loopback(), vec![77; 21]), now());
        assert!(matches!(client.state(), State::Draining { .. }));
    }

    /// Test that a server can send 0.5 RTT application data.
    #[test]
    fn send_05rtt() {
        let mut client = default_client();
        let mut server = default_server();

        let c1 = client.process(None, now()).dgram();
        assert!(c1.is_some());
        let s1 = server.process(c1, now()).dgram();
        assert!(s1.is_some());

        // The server should accept writes at this point.
        let s2 = send_something(&mut server, now());

        // We can't use the standard facility to complete the handshake, so
        // drive it as aggressively as possible.
        let _ = client.process(s1, now()).dgram(); // Just an ACK out of this.
        client.authenticated(AuthenticationStatus::Ok, now());
        assert_eq!(*client.state(), State::Connected);

        // The client should accept data now.
        client.process_input(s2, now());
        let mut buf = vec![0; DEFAULT_STREAM_DATA.len() + 1];
        let stream_id = client
            .events()
            .find_map(|e| {
                if let ConnectionEvent::RecvStreamReadable { stream_id } = e {
                    Some(stream_id)
                } else {
                    None
                }
            })
            .unwrap();
        let (l, ended) = client.stream_recv(stream_id, &mut buf).unwrap();
        assert_eq!(&buf[..l], DEFAULT_STREAM_DATA);
        assert!(ended);
    }

    /// Test that a server that coalesces 0.5 RTT with handshake packets
    /// doesn't cause the client to drop application data.
    #[test]
    fn coalesce_05rtt() {
        const RTT: Duration = Duration::from_millis(100);
        let mut client = default_client();
        let mut server = default_server();
        let mut now = now();

        // The first exchange doesn't offer a chance for the server to send.
        // So drop the server flight and wait for the PTO.
        let c1 = client.process(None, now).dgram();
        assert!(c1.is_some());
        now += RTT / 2;
        let s1 = server.process(c1, now).dgram();
        assert!(s1.is_some());

        // Drop the server flight.  Then send some data.
        let stream_id = server.stream_create(StreamType::UniDi).unwrap();
        assert!(server.stream_send(stream_id, DEFAULT_STREAM_DATA).is_ok());
        assert!(server.stream_close_send(stream_id).is_ok());

        // Now after a PTO the client can send another packet.
        // The server should then send its entire flight again,
        // including the application data, which it sends in a 1-RTT packet.
        now += AT_LEAST_PTO;
        let c2 = client.process(None, now).dgram();
        assert!(c2.is_some());
        now += RTT / 2;
        let s2 = server.process(c2, now).dgram();
        assert!(s2.is_some());

        // The client should process the datagram.  It can't process the 1-RTT
        // packet until authentication completes though.  So it saves it.
        now += RTT / 2;
        assert_eq!(client.stats().dropped_rx, 0);
        let _ = client.process(s2, now).dgram();
        // This packet will contain an ACK, but we can ignore it.
        assert_eq!(client.stats().dropped_rx, 0);
        assert!(client.saved_datagram.is_some());

        // After (successful) authentication, the packet is processed.
        maybe_authenticate(&mut client);
        let c3 = client.process(None, now).dgram();
        assert!(c3.is_some());
        assert_eq!(client.stats().dropped_rx, 0);
        assert!(client.saved_datagram.is_none());

        // Allow the handshake to complete.
        now += RTT / 2;
        let s3 = server.process(c3, now).dgram();
        assert!(s3.is_some());
        assert_eq!(*server.state(), State::Confirmed);
        now += RTT / 2;
        let c5 = client.process(s3, now).dgram();
        assert!(c5.is_none());
        assert_eq!(*client.state(), State::Confirmed);

        assert_eq!(client.stats().dropped_rx, 0);
    }

    #[test]
    fn server_receive_unknown_first_packet() {
        let mut server = default_server();

        let mut unknown_version_packet = vec![0x80, 0x1a, 0x1a, 0x1a, 0x1a];
        unknown_version_packet.resize(1200, 0x0);

        assert_eq!(
            server.process(
                Some(Datagram::new(
                    loopback(),
                    loopback(),
                    unknown_version_packet,
                )),
                now(),
            ),
            Output::None
        );

        assert_eq!(1, server.stats().dropped_rx);
    }

    fn create_vn(initial_pkt: &[u8], versions: &[u32]) -> Vec<u8> {
        let mut dec = Decoder::from(&initial_pkt[5..]); // Skip past version.
        let dcid = dec.decode_vec(1).expect("client DCID");
        let scid = dec.decode_vec(1).expect("client SCID");

        let mut encoder = Encoder::default();
        encoder.encode_byte(PACKET_BIT_LONG);
        encoder.encode(&[0; 4]); // Zero version == VN.
        encoder.encode_vec(1, dcid);
        encoder.encode_vec(1, scid);

        for v in versions {
            encoder.encode_uint(4, *v);
        }
        encoder.into()
    }

    #[test]
    fn version_negotiation_current_version() {
        let mut client = default_client();
        // Start the handshake.
        let initial_pkt = client
            .process(None, now())
            .dgram()
            .expect("a datagram")
            .to_vec();

        let vn = create_vn(
            &initial_pkt,
            &[0x1a1_a1a1a, QuicVersion::default().as_u32()],
        );

        let dgram = Datagram::new(loopback(), loopback(), vn);
        let delay = client.process(Some(dgram), now()).callback();
        assert_eq!(delay, Duration::from_millis(300));
        assert_eq!(*client.state(), State::WaitInitial);
        assert_eq!(1, client.stats().dropped_rx);
    }

    #[test]
    fn version_negotiation_only_reserved() {
        let mut client = default_client();
        // Start the handshake.
        let initial_pkt = client
            .process(None, now())
            .dgram()
            .expect("a datagram")
            .to_vec();

        let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a]);

        let dgram = Datagram::new(loopback(), loopback(), vn);
        assert_eq!(client.process(Some(dgram), now()), Output::None);
        match client.state() {
            State::Closed(err) => {
                assert_eq!(*err, ConnectionError::Transport(Error::VersionNegotiation))
            }
            _ => panic!("Invalid client state"),
        }
    }

    #[test]
    fn version_negotiation_corrupted() {
        let mut client = default_client();
        // Start the handshake.
        let initial_pkt = client
            .process(None, now())
            .dgram()
            .expect("a datagram")
            .to_vec();

        let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a]);

        let dgram = Datagram::new(loopback(), loopback(), &vn[..vn.len() - 1]);
        let delay = client.process(Some(dgram), now()).callback();
        assert_eq!(delay, Duration::from_millis(300));
        assert_eq!(*client.state(), State::WaitInitial);
        assert_eq!(1, client.stats().dropped_rx);
    }

    #[test]
    fn version_negotiation_empty() {
        let mut client = default_client();
        // Start the handshake.
        let initial_pkt = client
            .process(None, now())
            .dgram()
            .expect("a datagram")
            .to_vec();

        let vn = create_vn(&initial_pkt, &[]);

        let dgram = Datagram::new(loopback(), loopback(), vn);
        let delay = client.process(Some(dgram), now()).callback();
        assert_eq!(delay, Duration::from_millis(300));
        assert_eq!(*client.state(), State::WaitInitial);
        assert_eq!(1, client.stats().dropped_rx);
    }

    #[test]
    fn version_negotiation_not_supported() {
        let mut client = default_client();
        // Start the handshake.
        let initial_pkt = client
            .process(None, now())
            .dgram()
            .expect("a datagram")
            .to_vec();

        let vn = create_vn(&initial_pkt, &[0x1a1a_1a1a, 0x2a2a_2a2a, 0xff00_0001]);

        assert_eq!(
            client.process(Some(Datagram::new(loopback(), loopback(), vn)), now(),),
            Output::None
        );
        match client.state() {
            State::Closed(err) => {
                assert_eq!(*err, ConnectionError::Transport(Error::VersionNegotiation))
            }
            _ => panic!("Invalid client state"),
        }
    }

    #[test]
    fn stream_data_blocked_generates_max_stream_data() {
        let mut client = default_client();
        let mut server = default_server();
        connect(&mut client, &mut server);

        let now = now();

        // Try to say we're blocked beyond the initial data window
        server
            .flow_mgr
            .borrow_mut()
            .stream_data_blocked(3.into(), RECV_BUFFER_SIZE as u64 * 4);

        let out = server.process(None, now);
        assert!(out.as_dgram_ref().is_some());

        let frames = client.test_process_input(out.dgram().unwrap(), now);
        assert!(frames
            .iter()
            .any(|(f, _)| matches!(f, Frame::StreamDataBlocked { .. })));

        let out = client.process_output(now);
        assert!(out.as_dgram_ref().is_some());

        let frames = server.test_process_input(out.dgram().unwrap(), now);
        // Client should have sent a MaxStreamData frame with just the initial
        // window value.
        assert!(frames.iter().any(
            |(f, _)| matches!(f, Frame::MaxStreamData { maximum_stream_data, .. }
				   if *maximum_stream_data == RECV_BUFFER_SIZE as u64)
        ));
    }

    #[test]
    fn corrupted_initial() {
        let mut client = default_client();
        let mut server = default_server();
        let d = client.process(None, now()).dgram().unwrap();
        let mut corrupted = Vec::from(&d[..]);
        // Find the last non-zero value and corrupt that.
        let (idx, _) = corrupted
            .iter()
            .enumerate()
            .rev()
            .find(|(_, &v)| v != 0)
            .unwrap();
        corrupted[idx] ^= 0x76;
        let dgram = Datagram::new(d.source(), d.destination(), corrupted);
        server.process_input(dgram, now());
        // The server should have received two packets,
        // the first should be dropped, the second saved.
        assert_eq!(server.stats().packets_rx, 2);
        assert_eq!(server.stats().dropped_rx, 1);
        // assert_eq!(server.stats().saved_datagram, 1);
    }
}
