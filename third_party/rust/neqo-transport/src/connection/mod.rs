// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// The class implementing a QUIC connection.

use std::cell::RefCell;
use std::cmp::max;
use std::collections::HashMap;
use std::convert::TryFrom;
use std::fmt::{self, Debug};
use std::mem;
use std::net::SocketAddr;
use std::rc::{Rc, Weak};
use std::time::{Duration, Instant};

use smallvec::SmallVec;

use neqo_common::{
    event::Provider as EventProvider, hex, hex_snip_middle, qdebug, qerror, qinfo, qlog::NeqoQlog,
    qtrace, qwarn, Datagram, Decoder, Encoder, Role,
};
use neqo_crypto::agent::CertificateInfo;
use neqo_crypto::{
    Agent, AntiReplay, AuthenticationStatus, Cipher, Client, HandshakeState, ResumptionToken,
    SecretAgentInfo, Server, ZeroRttChecker,
};

use crate::addr_valid::{AddressValidation, NewTokenState};
use crate::cc::CongestionControlAlgorithm;
use crate::cid::{ConnectionId, ConnectionIdDecoder, ConnectionIdManager, ConnectionIdRef};
use crate::crypto::{Crypto, CryptoDxState, CryptoSpace};
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
use crate::{AppError, ConnectionError, Error, Res};

mod idle;
mod saved;
mod state;

use idle::IdleTimeout;
pub use idle::LOCAL_IDLE_TIMEOUT;
use saved::SavedDatagrams;
pub use state::State;
use state::StateSignaling;

#[derive(Debug, Default)]
struct Packet(Vec<u8>);

pub const LOCAL_STREAM_LIMIT_BIDI: u64 = 16;
pub const LOCAL_STREAM_LIMIT_UNI: u64 = 16;

/// The number of Initial packets that the client will send in response
/// to receiving an undecryptable packet during the early part of the
/// handshake.  This is a hack, but a useful one.
const EXTRA_INITIALS: usize = 4;
const LOCAL_MAX_DATA: u64 = 0x3FFF_FFFF_FFFF_FFFF; // 2^62-1

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

/// Used by `Connection::preprocess` to determine what to do
/// with an packet before attempting to remove protection.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum PreprocessResult {
    /// End processing and return successfully.
    End,
    /// Stop processing this datagram and move on to the next.
    Next,
    /// Continue and process this packet.
    Continue,
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

/// `AddressValidationInfo` holds information relevant to either
/// responding to address validation (`NewToken`, `Retry`) or generating
/// tokens for address validation (`Server`).
enum AddressValidationInfo {
    None,
    // We are a client and have information from `NEW_TOKEN`.
    NewToken(Vec<u8>),
    // We are a client and have received a `Retry` packet.
    Retry {
        token: Vec<u8>,
        retry_source_cid: ConnectionId,
    },
    // We are a server and can generate tokens.
    Server(Weak<RefCell<AddressValidation>>),
}

impl AddressValidationInfo {
    pub fn token(&self) -> &[u8] {
        match self {
            Self::NewToken(token) | Self::Retry { token, .. } => &token,
            _ => &[],
        }
    }

    pub fn generate_new_token(
        &mut self,
        peer_address: SocketAddr,
        now: Instant,
    ) -> Option<Vec<u8>> {
        match self {
            Self::Server(ref w) => {
                if let Some(validation) = w.upgrade() {
                    validation
                        .borrow()
                        .generate_new_token(peer_address, now)
                        .ok()
                } else {
                    None
                }
            }
            Self::None => None,
            _ => unreachable!("called a server function on a client"),
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
    address_validation: AddressValidationInfo,

    /// The source connection ID that this endpoint uses for the handshake.
    /// Since we need to communicate this to our peer in tparams, setting this
    /// value is part of constructing the struct.
    local_initial_source_cid: ConnectionId,
    /// The source connection ID from the first packet from the other end.
    /// This is checked against the peer's transport parameters.
    remote_initial_source_cid: Option<ConnectionId>,
    /// The destination connection ID from the first packet from the client.
    /// This is checked by the client against the server's transport parameters.
    original_destination_cid: Option<ConnectionId>,

    /// We sometimes save a datagram against the possibility that keys will later
    /// become available.  This avoids reporting packets as dropped during the handshake
    /// when they are either just reordered or we haven't been able to install keys yet.
    /// In particular, this occurs when asynchronous certificate validation happens.
    saved_datagrams: SavedDatagrams,

    pub(crate) crypto: Crypto,
    pub(crate) acks: AckTracker,
    idle_timeout: IdleTimeout,
    pub(crate) indexes: StreamIndexes,
    connection_ids: HashMap<u64, (ConnectionId, [u8; 16])>, // (sequence number, (connection id, reset token))
    pub(crate) send_streams: SendStreams,
    pub(crate) recv_streams: RecvStreams,
    pub(crate) flow_mgr: Rc<RefCell<FlowMgr>>,
    state_signaling: StateSignaling,
    loss_recovery: LossRecovery,
    events: ConnectionEvents,
    new_token: NewTokenState,
    stats: StatsCell,
    qlog: NeqoQlog,
    /// A session ticket was received without NEW_TOKEN,
    /// this is when that turns into an event without NEW_TOKEN.
    release_resumption_token_timer: Option<Instant>,
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
        cc_algorithm: &CongestionControlAlgorithm,
        quic_version: QuicVersion,
    ) -> Res<Self> {
        let dcid = ConnectionId::generate_initial();
        let mut c = Self::new(
            Role::Client,
            Client::new(server_name)?.into(),
            cid_manager,
            protocols,
            None,
            cc_algorithm,
            quic_version,
        )?;
        c.crypto.states.init(quic_version, Role::Client, &dcid);
        c.original_destination_cid = Some(dcid);
        c.initialize_path(local_addr, remote_addr);
        Ok(c)
    }

    /// Create a new QUIC connection with Server role.
    pub fn new_server(
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        cid_manager: CidMgr,
        cc_algorithm: &CongestionControlAlgorithm,
        quic_version: QuicVersion,
    ) -> Res<Self> {
        Self::new(
            Role::Server,
            Server::new(certs)?.into(),
            cid_manager,
            protocols,
            None,
            cc_algorithm,
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
        cc_algorithm: &CongestionControlAlgorithm,
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
            address_validation: AddressValidationInfo::None,
            local_initial_source_cid,
            remote_initial_source_cid: None,
            original_destination_cid: None,
            saved_datagrams: SavedDatagrams::default(),
            crypto,
            acks: AckTracker::default(),
            idle_timeout: IdleTimeout::default(),
            indexes: StreamIndexes::new(),
            connection_ids: HashMap::new(),
            send_streams: SendStreams::default(),
            recv_streams: RecvStreams::default(),
            flow_mgr: Rc::new(RefCell::new(FlowMgr::default())),
            state_signaling: StateSignaling::Idle,
            loss_recovery: LossRecovery::new(cc_algorithm, stats.clone()),
            events: ConnectionEvents::default(),
            new_token: NewTokenState::new(role),
            stats,
            qlog: NeqoQlog::disabled(),
            release_resumption_token_timer: None,
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
        self.original_destination_cid.as_ref()
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

    fn make_resumption_token(&mut self) -> ResumptionToken {
        debug_assert_eq!(self.role, Role::Client);
        debug_assert!(self.crypto.has_resumption_token());
        self.crypto
            .create_resumption_token(
                self.new_token.take_token(),
                self.tps
                    .borrow()
                    .remote
                    .as_ref()
                    .expect("should have transport parameters"),
                u64::try_from(self.loss_recovery.rtt().as_millis()).unwrap_or(0),
            )
            .unwrap()
    }

    fn create_resumption_token(&mut self, now: Instant) {
        if self.role == Role::Server || self.state < State::Connected {
            return;
        }

        qtrace!(
            [self],
            "Maybe create resumption token: {} {}",
            self.crypto.has_resumption_token(),
            self.new_token.has_token()
        );

        while self.crypto.has_resumption_token() && self.new_token.has_token() {
            let token = self.make_resumption_token();
            self.events.client_resumption_token(token);
        }

        // If we have a resumption ticket check or set a timer.
        if self.crypto.has_resumption_token() {
            let arm = if let Some(expiration_time) = self.release_resumption_token_timer {
                if expiration_time <= now {
                    let token = self.make_resumption_token();
                    self.events.client_resumption_token(token);
                    self.release_resumption_token_timer = None;

                    // This means that we release one session ticket every 3 PTOs
                    // if no NEW_TOKEN frame is received.
                    self.crypto.has_resumption_token()
                } else {
                    false
                }
            } else {
                true
            };

            if arm {
                self.release_resumption_token_timer =
                    Some(now + 3 * self.loss_recovery.pto_raw(PNSpace::ApplicationData));
            }
        }
    }

    /// Get a resumption token.  The correct way to obtain a resumption token is
    /// waiting for the `ConnectionEvent::ResumptionToken` event.  However, some
    /// servers don't send `NEW_TOKEN` frames and so that event might be slow in
    /// arriving.  This is especially a problem for short-lived connections, where
    /// the connection is closed before any events are released.  This retrieves
    /// the token, without waiting for the `NEW_TOKEN` frame to arrive.
    ///
    /// # Panics
    /// If this is called on a server.
    pub fn take_resumption_token(&mut self, now: Instant) -> Option<ResumptionToken> {
        assert_eq!(self.role, Role::Client);

        if self.crypto.has_resumption_token() {
            let token = self.make_resumption_token();
            if self.crypto.has_resumption_token() {
                self.release_resumption_token_timer =
                    Some(now + 3 * self.loss_recovery.pto_raw(PNSpace::ApplicationData));
            }
            Some(token)
        } else {
            None
        }
    }

    /// Enable resumption, using a token previously provided.
    /// This can only be called once and only on the client.
    /// After calling the function, it should be possible to attempt 0-RTT
    /// if the token supports that.
    pub fn enable_resumption(&mut self, now: Instant, token: impl AsRef<[u8]>) -> Res<()> {
        if self.state != State::Init {
            qerror!([self], "set token in state {:?}", self.state);
            return Err(Error::ConnectionState);
        }
        if self.role == Role::Server {
            return Err(Error::ConnectionState);
        }

        qinfo!(
            [self],
            "resumption token {}",
            hex_snip_middle(token.as_ref())
        );
        let mut dec = Decoder::from(token.as_ref());

        let smoothed_rtt =
            Duration::from_millis(dec.decode_varint().ok_or(Error::InvalidResumptionToken)?);
        qtrace!([self], "  RTT {:?}", smoothed_rtt);

        let tp_slice = dec.decode_vvec().ok_or(Error::InvalidResumptionToken)?;
        qtrace!([self], "  transport parameters {}", hex(&tp_slice));
        let mut dec_tp = Decoder::from(tp_slice);
        let tp =
            TransportParameters::decode(&mut dec_tp).map_err(|_| Error::InvalidResumptionToken)?;

        let init_token = dec.decode_vvec().ok_or(Error::InvalidResumptionToken)?;
        qtrace!([self], "  Initial token {}", hex(&init_token));

        let tok = dec.decode_remainder();
        qtrace!([self], "  TLS token {}", hex(&tok));
        match self.crypto.tls {
            Agent::Client(ref mut c) => {
                let res = c.enable_resumption(&tok);
                if let Err(e) = res {
                    self.absorb_error::<Error>(now, Err(Error::from(e)));
                    return Ok(());
                }
            }
            Agent::Server(_) => return Err(Error::WrongRole),
        }

        self.tps.borrow_mut().remote_0rtt = Some(tp);
        if !init_token.is_empty() {
            self.address_validation = AddressValidationInfo::NewToken(init_token.to_vec());
        }
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

    pub(crate) fn set_validation(&mut self, validation: Rc<RefCell<AddressValidation>>) {
        qtrace!([self], "Enabling NEW_TOKEN");
        assert_eq!(self.role, Role::Server);
        self.address_validation = AddressValidationInfo::Server(Rc::downgrade(&validation));
    }

    /// Send a TLS session ticket AND a NEW_TOKEN frame (if possible).
    pub fn send_ticket(&mut self, now: Instant, extra: &[u8]) -> Res<()> {
        if self.role == Role::Client {
            return Err(Error::WrongRole);
        }

        let tps = &self.tps;
        if let Agent::Server(ref mut s) = self.crypto.tls {
            let mut enc = Encoder::default();
            enc.encode_vvec_with(|mut enc_inner| {
                tps.borrow().local.encode(&mut enc_inner);
            });
            enc.encode(extra);
            let records = s.send_ticket(now, &enc)?;
            qinfo!([self], "send session ticket {}", hex(&enc));
            self.crypto.buffer_records(records)?;
        } else {
            unreachable!();
        }

        // If we are able, also send a NEW_TOKEN frame.
        // This should be recording all remote addresses that are valid,
        // but there are just 0 or 1 in the current implementation.
        if let Some(p) = self.path.as_ref() {
            if let Some(token) = self
                .address_validation
                .generate_new_token(p.remote_address(), now)
            {
                self.new_token.send_new_token(token);
            }
        }

        Ok(())
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
                    // This may happen when client_start fails.
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
                    if matches!(v, Error::KeysExhausted) {
                        self.set_state(State::Closed(error));
                    } else {
                        self.set_state(State::Closing {
                            error,
                            timeout: self.get_closing_period_time(now),
                        });
                    }
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

    fn process_timer(&mut self, now: Instant) {
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

        self.cleanup_streams();

        let res = self.crypto.states.check_key_update(now);
        self.absorb_error(now, res);

        let lost = self.loss_recovery.timeout(now);
        self.handle_lost_packets(&lost);
        qlog::packets_lost(&mut self.qlog, &lost);

        if self.release_resumption_token_timer.is_some() {
            self.create_resumption_token(now);
        }
    }

    /// Call in to process activity on the connection. Either new packets have
    /// arrived or a timeout has expired (or both).
    pub fn process_input(&mut self, d: Datagram, now: Instant) {
        let res = self.input(d, now);
        self.absorb_error(now, res);
        self.cleanup_streams();
    }

    /// Get the time that we next need to be called back, relative to `now`.
    fn next_delay(&mut self, now: Instant, paced: bool) -> Duration {
        qtrace!([self], "Get callback delay {:?}", now);

        // Only one timer matters when closing...
        if let State::Closing { timeout, .. } | State::Draining { timeout, .. } = self.state {
            return timeout.duration_since(now);
        }

        let mut delays = SmallVec::<[_; 6]>::new();
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

        // `release_resumption_token_timer` is not considered here, because
        // it is not important enough to force the application to set a
        // timeout for it  It is expected thatt other activities will
        // drive it.

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
    #[must_use = "Output of the process_output function must be handled"]
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
            let res = self.input(d, now);
            self.absorb_error(now, res);
        }
        self.process_output(now)
    }

    fn is_valid_cid(&self, cid: &ConnectionIdRef) -> bool {
        self.valid_cids.iter().any(|c| c == cid) || self.path.iter().any(|p| p.valid_local_cid(cid))
    }

    fn handle_retry(&mut self, packet: &PublicPacket) -> Res<()> {
        qinfo!([self], "received Retry");
        if matches!(self.address_validation, AddressValidationInfo::Retry { .. }) {
            self.stats.borrow_mut().pkt_dropped("Extra Retry");
            return Ok(());
        }
        if packet.token().is_empty() {
            self.stats.borrow_mut().pkt_dropped("Retry without a token");
            return Ok(());
        }
        if !packet.is_valid_retry(&self.original_destination_cid.as_ref().unwrap()) {
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

        let retry_scid = ConnectionId::from(packet.scid());
        qinfo!(
            [self],
            "Valid Retry received, token={} scid={}",
            hex(packet.token()),
            retry_scid
        );

        let lost_packets = self.loss_recovery.retry();
        self.handle_lost_packets(&lost_packets);

        self.crypto
            .states
            .init(self.quic_version, self.role, &retry_scid);
        self.address_validation = AddressValidationInfo::Retry {
            token: packet.token().to_vec(),
            retry_source_cid: retry_scid,
        };
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
        first: bool,
        now: Instant,
    ) -> Res<()> {
        if first && self.is_stateless_reset(d) {
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

    /// Process any saved packets if the appropriate keys are available.
    fn process_saved(&mut self, cspace: CryptoSpace) {
        if self.crypto.states.rx_hp(cspace).is_some() {
            for saved in self.saved_datagrams.take_saved(cspace) {
                qtrace!([self], "process saved @{:?}: {:?}", saved.t, saved.d);
                self.process_input(saved.d, saved.t);
            }
        }
    }

    /// In case a datagram arrives that we can only partially process, save any
    /// part that we don't have keys for.
    fn save_datagram(&mut self, cspace: CryptoSpace, d: Datagram, remaining: usize, now: Instant) {
        let d = if remaining < d.len() {
            Datagram::new(d.source(), d.destination(), &d[d.len() - remaining..])
        } else {
            d
        };
        self.saved_datagrams.save(cspace, d, now);
        self.stats.borrow_mut().saved_datagrams += 1;
    }

    /// Perform any processing that we might have to do on packets prior to
    /// attempting to remove protection.
    fn preprocess(
        &mut self,
        packet: &PublicPacket,
        dcid: Option<&ConnectionId>,
        now: Instant,
    ) -> Res<PreprocessResult> {
        if dcid.map_or(false, |d| d != packet.dcid()) {
            self.stats
                .borrow_mut()
                .pkt_dropped("Coalesced packet has different DCID");
            return Ok(PreprocessResult::Next);
        }

        match (packet.packet_type(), &self.state, &self.role) {
            (PacketType::Initial, State::Init, Role::Server) => {
                if !packet.is_valid_initial() {
                    self.stats.borrow_mut().pkt_dropped("Invalid Initial");
                    return Ok(PreprocessResult::Next);
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
                        if versions.is_empty()
                            || versions.contains(&self.quic_version.as_u32())
                            || packet.dcid() != self.odcid().unwrap()
                            || matches!(self.address_validation, AddressValidationInfo::Retry { .. })
                        {
                            // Ignore VersionNegotiation packets that contain the current version.
                            // Or don't have the right connection ID.
                            // Or are received after a Retry.
                            self.stats.borrow_mut().pkt_dropped("Invalid VN");
                            return Ok(PreprocessResult::End);
                        }

                        self.set_state(State::Closed(ConnectionError::Transport(
                            Error::VersionNegotiation,
                        )));
                        return Err(Error::VersionNegotiation);
                    }
                    Err(_) => {
                        self.stats.borrow_mut().pkt_dropped("Invalid VN");
                        return Ok(PreprocessResult::End);
                    }
                }
            }
            (PacketType::Retry, State::WaitInitial, Role::Client) => {
                self.handle_retry(packet)?;
                return Ok(PreprocessResult::Next);
            }
            (PacketType::Handshake, State::WaitInitial, Role::Client)
            | (PacketType::Short, State::WaitInitial, Role::Client) => {
                // This packet can't be processed now, but it could be a sign
                // that Initial packets were lost.
                // Resend Initial CRYPTO frames immediately a few times just
                // in case.  As we don't have an RTT estimate yet, this helps
                // when there is a short RTT and losses.
                if dcid.is_none()
                    && self.is_valid_cid(packet.dcid())
                    && self.stats.borrow().saved_datagrams <= EXTRA_INITIALS
                {
                    self.crypto.resend_unacked(PNSpace::Initial);
                }
            }
            (PacketType::VersionNegotiation, ..)
            | (PacketType::Retry, ..)
            | (PacketType::OtherVersion, ..) => {
                self.stats
                    .borrow_mut()
                    .pkt_dropped(format!("{:?}", packet.packet_type()));
                return Ok(PreprocessResult::Next);
            }
            _ => {}
        }

        let res = match self.state {
            State::Init => {
                self.stats
                    .borrow_mut()
                    .pkt_dropped("Received while in Init state");
                PreprocessResult::Next
            }
            State::WaitInitial => PreprocessResult::Continue,
            State::Handshaking | State::Connected | State::Confirmed => {
                if !self.is_valid_cid(packet.dcid()) {
                    self.stats
                        .borrow_mut()
                        .pkt_dropped(format!("Invalid DCID {:?}", packet.dcid()));
                    PreprocessResult::Next
                } else {
                    if self.role == Role::Server && packet.packet_type() == PacketType::Handshake {
                        // Server has received a Handshake packet -> discard Initial keys and states
                        self.discard_keys(PNSpace::Initial, now);
                    }
                    PreprocessResult::Continue
                }
            }
            State::Closing { .. } => {
                // Don't bother processing the packet. Instead ask to get a
                // new close frame.
                self.state_signaling.send_close();
                PreprocessResult::Next
            }
            State::Draining { .. } | State::Closed(..) => {
                // Do nothing.
                self.stats
                    .borrow_mut()
                    .pkt_dropped(format!("State {:?}", self.state));
                PreprocessResult::Next
            }
        };
        Ok(res)
    }

    /// Take a datagram as input.  This reports an error if the packet was bad.
    fn input(&mut self, d: Datagram, now: Instant) -> Res<()> {
        let mut slc = &d[..];
        let mut dcid = None;

        qtrace!([self], "input {}", hex(&**d));

        // Handle each packet in the datagram.
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
            match self.preprocess(&packet, dcid.as_ref(), now)? {
                PreprocessResult::Continue => (),
                PreprocessResult::Next => break,
                PreprocessResult::End => return Ok(()),
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
                    res?;
                    if self.state == State::WaitInitial {
                        self.start_handshake(&packet, &d)?;
                    }
                    self.process_migrations(&d)?;
                }
                Err(e) => {
                    match e {
                        Error::KeysPending(cspace) => {
                            // This packet can't be decrypted because we don't have the keys yet.
                            // Don't check this packet for a stateless reset, just return.
                            let remaining = slc.len();
                            self.save_datagram(cspace, d, remaining, now);
                            return Ok(());
                        }
                        Error::KeysExhausted => {
                            // Exhausting read keys is fatal.
                            return Err(e);
                        }
                        _ => (),
                    }
                    // Decryption failure, or not having keys is not fatal.
                    // If the state isn't available, or we can't decrypt the packet, drop
                    // the rest of the datagram on the floor, but don't generate an error.
                    self.check_stateless_reset(&d, dcid.is_none(), now)?;
                    self.stats.borrow_mut().pkt_dropped("Decryption failure");
                    qlog::packet_dropped(&mut self.qlog, &packet);
                }
            }
            slc = remainder;
            dcid = Some(ConnectionId::from(packet.dcid()));
        }
        self.check_stateless_reset(&d, dcid.is_none(), now)?;
        Ok(())
    }

    fn process_packet(&mut self, packet: &DecryptedPacket, now: Instant) -> Res<()> {
        // TODO(ekr@rtfm.com): Have the server blow away the initial
        // crypto state if this fails? Otherwise, we will get a panic
        // on the assert for doesn't exist.
        // OK, we have a valid packet.

        let space = PNSpace::from(packet.packet_type());
        if self.acks.get_mut(space).unwrap().is_duplicate(packet.pn()) {
            qdebug!([self], "Duplicate packet from {} pn={}", space, packet.pn());
            self.stats.borrow_mut().dups_rx += 1;
            return Ok(());
        }

        let mut ack_eliciting = false;
        let mut d = Decoder::from(&packet[..]);
        let mut consecutive_padding = 0;
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

            ack_eliciting |= f.ack_eliciting();
            let t = f.get_type();
            let res = self.input_frame(packet.packet_type(), f, now);
            self.capture_error(now, t, res)?;
        }
        self.acks
            .get_mut(space)
            .unwrap()
            .set_received(now, packet.pn(), ack_eliciting);

        Ok(())
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
                .or_else(|| self.original_destination_cid.as_ref())
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
            self.original_destination_cid = Some(ConnectionId::from(packet.dcid()));
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
        cspace: CryptoSpace,
        encoder: Encoder,
        tx: &CryptoDxState,
        address_validation: &AddressValidationInfo,
        quic_version: QuicVersion,
        grease_quic_bit: bool,
    ) -> (PacketType, PacketNumber, PacketBuilder) {
        let pt = PacketType::from(cspace);
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
            builder.initial_token(address_validation.token());
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
            let (cspace, tx) = if let Some(crypto) = self.crypto.states.select_tx(*space) {
                crypto
            } else {
                continue;
            };

            let (_, _, mut builder) = Self::build_packet_header(
                path,
                cspace,
                encoder,
                tx,
                &AddressValidationInfo::None,
                self.quic_version,
                grease_quic_bit,
            );

            // ConnectionError::Application is only allowed at 1RTT.
            let sanitized = if *space == PNSpace::ApplicationData {
                &frame
            } else {
                frame.sanitize_close()
            };
            if let Frame::ConnectionClose {
                error_code,
                frame_type,
                reason_phrase,
            } = sanitized
            {
                builder.encode_varint(sanitized.get_type());
                builder.encode_varint(error_code.code());
                if let CloseError::Transport(_) = error_code {
                    builder.encode_varint(*frame_type);
                }
                builder.encode_vvec(reason_phrase);
            } else {
                unreachable!();
            }

            encoder = builder.build(tx)?;
        }

        Ok(SendOption::Yes(path.datagram(encoder)))
    }

    /// Write frames to the provided builder.  Returns a list of tokens used for
    /// tracking loss or acknowledgment and whether any frame was ACK eliciting.
    fn write_frames(
        &mut self,
        space: PNSpace,
        profile: &SendProfile,
        builder: &mut PacketBuilder,
        now: Instant,
    ) -> (Vec<RecoveryToken>, bool) {
        let mut tokens = Vec::new();
        let stats = &mut self.stats.borrow_mut().frame_tx;

        let ack_token = self.acks.write_frame(space, now, builder, stats);

        if profile.ack_only(space) {
            // If we are CC limited we can only send acks!
            if let Some(t) = ack_token {
                tokens.push(t);
            }
            return (tokens, false);
        }

        if space == PNSpace::ApplicationData && self.role == Role::Server {
            if let Some(t) = self.state_signaling.write_done(builder) {
                tokens.push(t);
                stats.handshake_done += 1;
            }
        }

        if let Some(t) = self.crypto.streams.write_frame(space, builder) {
            tokens.push(t);
            stats.crypto += 1;
        }

        if space == PNSpace::ApplicationData {
            self.flow_mgr
                .borrow_mut()
                .write_frames(builder, &mut tokens, stats);

            self.send_streams.write_frames(builder, &mut tokens, stats);
            self.new_token.write_frames(builder, &mut tokens, stats);
        }

        // Anything - other than ACK - that registered a token wants an acknowledgment.
        let ack_eliciting = !tokens.is_empty()
            || if profile.should_probe(space) {
                // Nothing ack-eliciting and we need to probe; send PING.
                debug_assert_ne!(builder.remaining(), 0);
                builder.encode_varint(crate::frame::FRAME_TYPE_PING);
                stats.ping += 1;
                stats.all += 1;
                true
            } else {
                false
            };

        if let Some(t) = ack_token {
            tokens.push(t);
        }
        stats.all += tokens.len();
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
            let (cspace, tx) = if let Some(crypto) = self.crypto.states.select_tx(*space) {
                crypto
            } else {
                continue;
            };

            let header_start = encoder.len();
            let (pt, pn, mut builder) = Self::build_packet_header(
                path,
                cspace,
                encoder,
                tx,
                &self.address_validation,
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
            builder.set_limit(limit);
            let (tokens, ack_eliciting) = self.write_frames(*space, &profile, &mut builder, now);
            if builder.packet_empty() {
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
            encoder = builder.build(self.crypto.states.tx(cspace).unwrap())?;
            debug_assert!(encoder.len() <= path.mtu());
            self.crypto.states.auto_update()?;

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
            if pt == PacketType::Initial && (self.role == Role::Client || ack_eliciting) {
                // Packets containing Initial packets might need padding, and we want to
                // track that padding along with the Initial packet.  So defer tracking.
                initial_sent = Some(sent);
                needs_padding = true;
            } else {
                if pt != PacketType::ZeroRtt && self.role == Role::Client {
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
            Err(Error::KeyUpdateBlocked)
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
        {
            let tps = self.tps.borrow();
            if let Some(token) = tps
                .remote
                .as_ref()
                .unwrap()
                .get_bytes(tparams::STATELESS_RESET_TOKEN)
            {
                let reset_token = <[u8; 16]>::try_from(token).unwrap().to_owned();
                self.path.as_mut().unwrap().set_reset_token(reset_token);
            }
            let mad = Duration::from_millis(
                tps.remote
                    .as_ref()
                    .unwrap()
                    .get_integer(tparams::MAX_ACK_DELAY),
            );
            self.loss_recovery.set_peer_max_ack_delay(mad);
        }
        self.set_initial_limits();
        qlog::connection_tparams_set(&mut self.qlog, &*self.tps.borrow());
        Ok(())
    }

    fn validate_cids(&mut self) -> Res<()> {
        match self.quic_version {
            QuicVersion::Draft27 => self.validate_cids_draft_27(),
            _ => self.validate_cids_draft_28_plus(),
        }
    }

    fn validate_cids_draft_27(&mut self) -> Res<()> {
        if let AddressValidationInfo::Retry { token, .. } = &self.address_validation {
            debug_assert!(!token.is_empty());
            let tph = self.tps.borrow();
            let tp = tph
                .remote
                .as_ref()
                .unwrap()
                .get_bytes(tparams::ORIGINAL_DESTINATION_CONNECTION_ID);
            if self
                .original_destination_cid
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
                [self],
                "ISCID test failed: self cid {:?} != tp cid {:?}",
                self.remote_initial_source_cid,
                tp.map(hex),
            );
            return Err(Error::ProtocolViolation);
        }

        if self.role == Role::Client {
            let tp = remote_tps.get_bytes(tparams::ORIGINAL_DESTINATION_CONNECTION_ID);
            if self
                .original_destination_cid
                .as_ref()
                .map(ConnectionId::as_cid_ref)
                != tp.map(ConnectionIdRef::from)
            {
                qwarn!(
                    [self],
                    "ODCID test failed: self cid {:?} != tp cid {:?}",
                    self.original_destination_cid,
                    tp.map(hex),
                );
                return Err(Error::ProtocolViolation);
            }

            let tp = remote_tps.get_bytes(tparams::RETRY_SOURCE_CONNECTION_ID);
            let expected = if let AddressValidationInfo::Retry {
                retry_source_cid, ..
            } = &self.address_validation
            {
                Some(retry_source_cid.as_cid_ref())
            } else {
                None
            };
            if expected != tp.map(ConnectionIdRef::from) {
                qwarn!(
                    [self],
                    "RSCID test failed. self cid {:?} != tp cid {:?}",
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
            if self.crypto.install_keys(self.role)? {
                self.process_saved(CryptoSpace::Handshake);
            }
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
        self.stats.borrow_mut().frame_rx.all += 1;
        let space = PNSpace::from(ptype);
        match frame {
            Frame::Padding => {
                // Note: This counts contiguous padding as a single frame.
                self.stats.borrow_mut().frame_rx.padding += 1;
            }
            Frame::Ping => {
                // If we get a PING and there are outstanding CRYPTO frames,
                // prepare to resend them.
                self.stats.borrow_mut().frame_rx.ping += 1;
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
                self.stats.borrow_mut().frame_rx.reset_stream += 1;
                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    rs.reset(application_error_code);
                }
            }
            Frame::StopSending {
                stream_id,
                application_error_code,
            } => {
                self.stats.borrow_mut().frame_rx.stop_sending += 1;
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
                self.stats.borrow_mut().frame_rx.crypto += 1;
                self.crypto.streams.inbound_frame(space, offset, data);
                if self.crypto.streams.data_ready(space) {
                    let mut buf = Vec::new();
                    let read = self.crypto.streams.read_to_end(space, &mut buf);
                    qdebug!("Read {} bytes", read);
                    self.handshake(now, space, Some(&buf))?;
                    self.create_resumption_token(now);
                } else {
                    // If we get a useless CRYPTO frame send outstanding CRYPTO frames again.
                    self.crypto.resend_unacked(space);
                }
            }
            Frame::NewToken { token } => {
                self.stats.borrow_mut().frame_rx.new_token += 1;
                self.new_token.save_token(token.to_vec());
                self.create_resumption_token(now);
            }
            Frame::Stream {
                fin,
                stream_id,
                offset,
                data,
                ..
            } => {
                self.stats.borrow_mut().frame_rx.stream += 1;
                if let (_, Some(rs)) = self.obtain_stream(stream_id)? {
                    rs.inbound_stream_frame(fin, offset, data)?;
                }
            }
            Frame::MaxData { maximum_data } => {
                self.stats.borrow_mut().frame_rx.max_data += 1;
                self.handle_max_data(maximum_data);
            }
            Frame::MaxStreamData {
                stream_id,
                maximum_stream_data,
            } => {
                self.stats.borrow_mut().frame_rx.max_stream_data += 1;
                if let (Some(ss), _) = self.obtain_stream(stream_id)? {
                    ss.set_max_stream_data(maximum_stream_data);
                }
            }
            Frame::MaxStreams {
                stream_type,
                maximum_streams,
            } => {
                self.stats.borrow_mut().frame_rx.max_streams += 1;
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
                self.stats.borrow_mut().frame_rx.data_blocked += 1;
                // But if it does, open it up all the way
                self.flow_mgr.borrow_mut().max_data(LOCAL_MAX_DATA);
            }
            Frame::StreamDataBlocked {
                stream_id,
                stream_data_limit,
            } => {
                self.stats.borrow_mut().frame_rx.stream_data_blocked += 1;
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
                self.stats.borrow_mut().frame_rx.streams_blocked += 1;
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
                self.stats.borrow_mut().frame_rx.new_connection_id += 1;
                let cid = ConnectionId::from(connection_id);
                let srt = stateless_reset_token.to_owned();
                self.connection_ids.insert(sequence_number, (cid, srt));
            }
            Frame::RetireConnectionId { sequence_number } => {
                self.stats.borrow_mut().frame_rx.retire_connection_id += 1;
                self.connection_ids.remove(&sequence_number);
            }
            Frame::PathChallenge { data } => {
                self.stats.borrow_mut().frame_rx.path_challenge += 1;
                self.flow_mgr.borrow_mut().path_response(data);
            }
            Frame::PathResponse { .. } => {
                // Should never see this, we don't support migration atm and
                // do not send path challenges
                qwarn!([self], "Received Path Response");
                self.stats.borrow_mut().frame_rx.path_response += 1;
            }
            Frame::ConnectionClose {
                error_code,
                frame_type,
                reason_phrase,
            } => {
                self.stats.borrow_mut().frame_rx.connection_close += 1;
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
                self.stats.borrow_mut().frame_rx.handshake_done += 1;
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
                    RecoveryToken::NewToken(seqno) => self.new_token.lost(*seqno),
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
                    RecoveryToken::NewToken(seqno) => self.new_token.acked(*seqno),
                }
            }
        }
        self.handle_lost_packets(&lost_packets);
        qlog::packets_lost(&mut self.qlog, &lost_packets);
        let stats = &mut self.stats.borrow_mut().frame_rx;
        stats.ack += 1;
        stats.largest_acknowledged = max(stats.largest_acknowledged, largest_acknowledged);
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
        self.create_resumption_token(now);
        self.process_saved(CryptoSpace::ApplicationData);
        self.stats.borrow_mut().resumed = self.crypto.tls.info().unwrap().resumed();
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
        self.send_streams.clear_terminal();
        let recv_to_remove = self
            .recv_streams
            .iter()
            .filter_map(|(id, stream)| {
                // Remove all streams for which the receiving is done (or aborted).
                // But only if they are unidirectional, or we have finished sending.
                if stream.is_terminal() && (id.is_uni() || !self.send_streams.exists(*id)) {
                    Some(*id)
                } else {
                    None
                }
            })
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

    #[cfg(test)]
    pub fn get_pto(&self) -> Duration {
        self.loss_recovery.pto_raw(PNSpace::ApplicationData)
    }
}

impl EventProvider for Connection {
    type Event = ConnectionEvent;

    /// Return true if there are outstanding events.
    fn has_events(&self) -> bool {
        self.events.has_events()
    }

    /// Get events that indicate state changes on the connection. This method
    /// correctly handles cases where handling one event can obsolete
    /// previously-queued events, or cause new events to be generated.
    fn next_event(&mut self) -> Option<Self::Event> {
        self.events.next_event()
    }
}

impl ::std::fmt::Display for Connection {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "{:?} ", self.role)?;
        if let Some(cid) = self.odcid() {
            std::fmt::Display::fmt(&cid, f)
        } else {
            write!(f, "...")
        }
    }
}

#[cfg(test)]
mod tests;
