// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// The class implementing a QUIC connection.

use std::cell::RefCell;
use std::cmp::{max, min};
use std::convert::TryFrom;
use std::fmt::{self, Debug};
use std::mem;
use std::net::{IpAddr, SocketAddr};
use std::ops::RangeInclusive;
use std::rc::{Rc, Weak};
use std::time::{Duration, Instant};

use smallvec::SmallVec;

use neqo_common::{
    event::Provider as EventProvider, hex, hex_snip_middle, hrtime, qdebug, qerror, qinfo,
    qlog::NeqoQlog, qtrace, qwarn, Datagram, Decoder, Encoder, Role,
};
use neqo_crypto::{
    agent::CertificateInfo, random, Agent, AntiReplay, AuthenticationStatus, Cipher, Client,
    HandshakeState, PrivateKey, PublicKey, ResumptionToken, SecretAgentInfo, SecretAgentPreInfo,
    Server, ZeroRttChecker,
};

use crate::addr_valid::{AddressValidation, NewTokenState};
use crate::cid::{
    ConnectionId, ConnectionIdEntry, ConnectionIdGenerator, ConnectionIdManager, ConnectionIdRef,
    ConnectionIdStore, LOCAL_ACTIVE_CID_LIMIT,
};

use crate::crypto::{Crypto, CryptoDxState, CryptoSpace};
use crate::dump::*;
use crate::events::{ConnectionEvent, ConnectionEvents, OutgoingDatagramOutcome};
use crate::frame::{
    CloseError, Frame, FrameType, FRAME_TYPE_CONNECTION_CLOSE_APPLICATION,
    FRAME_TYPE_CONNECTION_CLOSE_TRANSPORT,
};
use crate::packet::{DecryptedPacket, PacketBuilder, PacketNumber, PacketType, PublicPacket};
use crate::path::{Path, PathRef, Paths};
use crate::quic_datagrams::{DatagramTracking, QuicDatagrams};
use crate::recovery::{LossRecovery, RecoveryToken, SendProfile};
use crate::rtt::GRANULARITY;
pub use crate::send_stream::{RetransmissionPriority, TransmissionPriority};
use crate::stats::{Stats, StatsCell};
use crate::stream_id::StreamType;
use crate::streams::Streams;
use crate::tparams::{
    self, TransportParameter, TransportParameterId, TransportParameters, TransportParametersHandler,
};
use crate::tracking::{AckTracker, PacketNumberSpace, SentPacket};
use crate::version::{Version, WireVersion};
use crate::{qlog, AppError, ConnectionError, Error, Res, StreamId};

mod idle;
pub mod params;
mod saved;
mod state;
#[cfg(test)]
pub mod test_internal;

use idle::IdleTimeout;
use params::PreferredAddressConfig;
pub use params::{ConnectionParameters, ACK_RATIO_SCALE};
use saved::SavedDatagrams;
use state::StateSignaling;
pub use state::{ClosingFrame, State};

#[derive(Debug, Default)]
struct Packet(Vec<u8>);

/// The number of Initial packets that the client will send in response
/// to receiving an undecryptable packet during the early part of the
/// handshake.  This is a hack, but a useful one.
const EXTRA_INITIALS: usize = 4;

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum ZeroRttState {
    Init,
    Sending,
    AcceptedClient,
    AcceptedServer,
    Rejected,
}

#[derive(Clone, Debug, PartialEq, Eq)]
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
            Self::NewToken(token) | Self::Retry { token, .. } => token,
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
    version: Version,
    state: State,
    tps: Rc<RefCell<TransportParametersHandler>>,
    /// What we are doing with 0-RTT.
    zero_rtt_state: ZeroRttState,
    /// All of the network paths that we are aware of.
    paths: Paths,
    /// This object will generate connection IDs for the connection.
    cid_manager: ConnectionIdManager,
    address_validation: AddressValidationInfo,
    /// The connection IDs that were provided by the peer.
    connection_ids: ConnectionIdStore<[u8; 16]>,

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
    /// Some packets were received, but not tracked.
    received_untracked: bool,

    /// This is responsible for the QuicDatagrams' handling:
    /// https://datatracker.ietf.org/doc/html/draft-ietf-quic-datagram
    quic_datagrams: QuicDatagrams,

    pub(crate) crypto: Crypto,
    pub(crate) acks: AckTracker,
    idle_timeout: IdleTimeout,
    streams: Streams,
    state_signaling: StateSignaling,
    loss_recovery: LossRecovery,
    events: ConnectionEvents,
    new_token: NewTokenState,
    stats: StatsCell,
    qlog: NeqoQlog,
    /// A session ticket was received without NEW_TOKEN,
    /// this is when that turns into an event without NEW_TOKEN.
    release_resumption_token_timer: Option<Instant>,
    conn_params: ConnectionParameters,
    hrtime: hrtime::Handle,

    /// For testing purposes it is sometimes necessary to inject frames that wouldn't
    /// otherwise be sent, just to see how a connection handles them.  Inserting them
    /// into packets proper mean that the frames follow the entire processing path.
    #[cfg(test)]
    pub test_frame_writer: Option<Box<dyn test_internal::FrameWriter>>,
}

impl Debug for Connection {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "{:?} Connection: {:?} {:?}",
            self.role,
            self.state,
            self.paths.primary_fallible()
        )
    }
}

impl Connection {
    /// A long default for timer resolution, so that we don't tax the
    /// system too hard when we don't need to.
    const LOOSE_TIMER_RESOLUTION: Duration = Duration::from_millis(50);

    /// Create a new QUIC connection with Client role.
    pub fn new_client(
        server_name: impl Into<String>,
        protocols: &[impl AsRef<str>],
        cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
        local_addr: SocketAddr,
        remote_addr: SocketAddr,
        conn_params: ConnectionParameters,
        now: Instant,
    ) -> Res<Self> {
        let dcid = ConnectionId::generate_initial();
        let mut c = Self::new(
            Role::Client,
            Agent::from(Client::new(server_name.into())?),
            cid_generator,
            protocols,
            conn_params,
        )?;
        c.crypto.states.init(
            c.conn_params.get_versions().compatible(),
            Role::Client,
            &dcid,
        );
        c.original_destination_cid = Some(dcid);
        let path = Path::temporary(
            local_addr,
            remote_addr,
            c.conn_params.get_cc_algorithm(),
            NeqoQlog::default(),
            now,
        );
        c.setup_handshake_path(&Rc::new(RefCell::new(path)), now);
        Ok(c)
    }

    /// Create a new QUIC connection with Server role.
    pub fn new_server(
        certs: &[impl AsRef<str>],
        protocols: &[impl AsRef<str>],
        cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
        conn_params: ConnectionParameters,
    ) -> Res<Self> {
        Self::new(
            Role::Server,
            Agent::from(Server::new(certs)?),
            cid_generator,
            protocols,
            conn_params,
        )
    }

    fn new<P: AsRef<str>>(
        role: Role,
        agent: Agent,
        cid_generator: Rc<RefCell<dyn ConnectionIdGenerator>>,
        protocols: &[P],
        conn_params: ConnectionParameters,
    ) -> Res<Self> {
        // Setup the local connection ID.
        let local_initial_source_cid = cid_generator
            .borrow_mut()
            .generate_cid()
            .ok_or(Error::ConnectionIdsExhausted)?;
        let mut cid_manager =
            ConnectionIdManager::new(cid_generator, local_initial_source_cid.clone());
        let mut tps = conn_params.create_transport_parameter(role, &mut cid_manager)?;
        tps.local.set_bytes(
            tparams::INITIAL_SOURCE_CONNECTION_ID,
            local_initial_source_cid.to_vec(),
        );

        let tphandler = Rc::new(RefCell::new(tps));
        let crypto = Crypto::new(
            conn_params.get_versions().initial(),
            agent,
            protocols.iter().map(P::as_ref).map(String::from).collect(),
            Rc::clone(&tphandler),
        )?;

        let stats = StatsCell::default();
        let events = ConnectionEvents::default();
        let quic_datagrams = QuicDatagrams::new(
            conn_params.get_datagram_size(),
            conn_params.get_outgoing_datagram_queue(),
            conn_params.get_incoming_datagram_queue(),
            events.clone(),
        );

        let c = Self {
            role,
            version: conn_params.get_versions().initial(),
            state: State::Init,
            paths: Paths::default(),
            cid_manager,
            tps: tphandler.clone(),
            zero_rtt_state: ZeroRttState::Init,
            address_validation: AddressValidationInfo::None,
            local_initial_source_cid,
            remote_initial_source_cid: None,
            original_destination_cid: None,
            saved_datagrams: SavedDatagrams::default(),
            received_untracked: false,
            crypto,
            acks: AckTracker::default(),
            idle_timeout: IdleTimeout::new(conn_params.get_idle_timeout()),
            streams: Streams::new(tphandler, role, events.clone()),
            connection_ids: ConnectionIdStore::default(),
            state_signaling: StateSignaling::Idle,
            loss_recovery: LossRecovery::new(stats.clone(), conn_params.get_fast_pto()),
            events,
            new_token: NewTokenState::new(role),
            stats,
            qlog: NeqoQlog::disabled(),
            release_resumption_token_timer: None,
            conn_params,
            hrtime: hrtime::Time::get(Self::LOOSE_TIMER_RESOLUTION),
            quic_datagrams,
            #[cfg(test)]
            test_frame_writer: None,
        };
        c.stats.borrow_mut().init(format!("{}", c));
        Ok(c)
    }

    pub fn server_enable_0rtt(
        &mut self,
        anti_replay: &AntiReplay,
        zero_rtt_checker: impl ZeroRttChecker + 'static,
    ) -> Res<()> {
        self.crypto
            .server_enable_0rtt(self.tps.clone(), anti_replay, zero_rtt_checker)
    }

    pub fn server_enable_ech(
        &mut self,
        config: u8,
        public_name: &str,
        sk: &PrivateKey,
        pk: &PublicKey,
    ) -> Res<()> {
        self.crypto.server_enable_ech(config, public_name, sk, pk)
    }

    /// Get the active ECH configuration, which is empty if ECH is disabled.
    pub fn ech_config(&self) -> &[u8] {
        self.crypto.ech_config()
    }

    pub fn client_enable_ech(&mut self, ech_config_list: impl AsRef<[u8]>) -> Res<()> {
        self.crypto.client_enable_ech(ech_config_list)
    }

    /// Set or clear the qlog for this connection.
    pub fn set_qlog(&mut self, qlog: NeqoQlog) {
        self.loss_recovery.set_qlog(qlog.clone());
        self.paths.set_qlog(qlog.clone());
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
    /// This only sets transport parameters without dealing with other aspects of
    /// setting the value.
    /// # Panics
    /// This panics if the transport parameter is known to this crate.
    pub fn set_local_tparam(&self, tp: TransportParameterId, value: TransportParameter) -> Res<()> {
        #[cfg(not(test))]
        {
            assert!(!tparams::INTERNAL_TRANSPORT_PARAMETERS.contains(&tp));
        }
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
        let rtt = self.paths.primary().borrow().rtt().estimate();
        self.crypto
            .create_resumption_token(
                self.new_token.take_token(),
                self.tps
                    .borrow()
                    .remote
                    .as_ref()
                    .expect("should have transport parameters"),
                self.version,
                u64::try_from(rtt.as_millis()).unwrap_or(0),
            )
            .unwrap()
    }

    /// Get the simplest PTO calculation for all those cases where we need
    /// a value of this approximate order.  Don't use this for loss recovery,
    /// only use it where a more precise value is not important.
    fn pto(&self) -> Duration {
        self.paths
            .primary()
            .borrow()
            .rtt()
            .pto(PacketNumberSpace::ApplicationData)
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
                self.release_resumption_token_timer = Some(now + 3 * self.pto());
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
                self.release_resumption_token_timer = Some(now + 3 * self.pto());
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

        let version =
            Version::try_from(dec.decode_uint(4).ok_or(Error::InvalidResumptionToken)? as u32)?;
        qtrace!([self], "  version {:?}", version);
        if !self.conn_params.get_versions().all().contains(&version) {
            return Err(Error::DisabledVersion);
        }

        let rtt = Duration::from_millis(dec.decode_varint().ok_or(Error::InvalidResumptionToken)?);
        qtrace!([self], "  RTT {:?}", rtt);

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

        self.version = version;
        self.conn_params.get_versions_mut().set_initial(version);
        self.tps.borrow_mut().set_version(version);
        self.tps.borrow_mut().remote_0rtt = Some(tp);
        if !init_token.is_empty() {
            self.address_validation = AddressValidationInfo::NewToken(init_token.to_vec());
        }
        self.paths.primary().borrow_mut().rtt_mut().set_initial(rtt);
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
            enc.encode_vvec_with(|enc_inner| {
                tps.borrow().local.encode(enc_inner);
            });
            enc.encode(extra);
            let records = s.send_ticket(now, enc.as_ref())?;
            qinfo!([self], "send session ticket {}", hex(&enc));
            self.crypto.buffer_records(records)?;
        } else {
            unreachable!();
        }

        // If we are able, also send a NEW_TOKEN frame.
        // This should be recording all remote addresses that are valid,
        // but there are just 0 or 1 in the current implementation.
        if let Some(path) = self.paths.primary_fallible() {
            if let Some(token) = self
                .address_validation
                .generate_new_token(path.borrow().remote_address(), now)
            {
                self.new_token.send_new_token(token);
            }
            Ok(())
        } else {
            Err(Error::NotConnected)
        }
    }

    pub fn tls_info(&self) -> Option<&SecretAgentInfo> {
        self.crypto.tls.info()
    }

    pub fn tls_preinfo(&self) -> Res<SecretAgentPreInfo> {
        Ok(self.crypto.tls.preinfo()?)
    }

    /// Get the peer's certificate chain and other info.
    pub fn peer_certificate(&self) -> Option<CertificateInfo> {
        self.crypto.tls.peer_certificate()
    }

    /// Call by application when the peer cert has been verified.
    ///
    /// This panics if there is no active peer.  It's OK to call this
    /// when authentication isn't needed, that will likely only cause
    /// the connection to fail.  However, if no packets have been
    /// exchanged, it's not OK.
    pub fn authenticated(&mut self, status: AuthenticationStatus, now: Instant) {
        qinfo!([self], "Authenticated {:?}", status);
        self.crypto.tls.authenticated(status);
        let res = self.handshake(now, self.version, PacketNumberSpace::Handshake, None);
        self.absorb_error(now, res);
        self.process_saved(now);
    }

    /// Get the role of the connection.
    pub fn role(&self) -> Role {
        self.role
    }

    /// Get the state of the connection.
    pub fn state(&self) -> &State {
        &self.state
    }

    /// The QUIC version in use.
    pub fn version(&self) -> Version {
        self.version
    }

    /// Get the 0-RTT state of the connection.
    pub fn zero_rtt_state(&self) -> ZeroRttState {
        self.zero_rtt_state
    }

    /// Get a snapshot of collected statistics.
    pub fn stats(&self) -> Stats {
        let mut v = self.stats.borrow().clone();
        if let Some(p) = self.paths.primary_fallible() {
            let p = p.borrow();
            v.rtt = p.rtt().estimate();
            v.rttvar = p.rtt().rttvar();
        }
        v
    }

    // This function wraps a call to another function and sets the connection state
    // properly if that call fails.
    fn capture_error<T>(
        &mut self,
        path: Option<PathRef>,
        now: Instant,
        frame_type: FrameType,
        res: Res<T>,
    ) -> Res<T> {
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
                    if let Some(path) = path.or_else(|| self.paths.primary_fallible()) {
                        self.state_signaling
                            .close(path, error.clone(), frame_type, msg);
                    }
                    self.set_state(State::Closed(error));
                }
                _ => {
                    if let Some(path) = path.or_else(|| self.paths.primary_fallible()) {
                        self.state_signaling
                            .close(path, error.clone(), frame_type, msg);
                        if matches!(v, Error::KeysExhausted) {
                            self.set_state(State::Closed(error));
                        } else {
                            self.set_state(State::Closing {
                                error,
                                timeout: self.get_closing_period_time(now),
                            });
                        }
                    } else {
                        self.set_state(State::Closed(error));
                    }
                }
            }
        }
        res
    }

    /// For use with process_input(). Errors there can be ignored, but this
    /// needs to ensure that the state is updated.
    fn absorb_error<T>(&mut self, now: Instant, res: Res<T>) -> Option<T> {
        self.capture_error(None, now, 0, res).ok()
    }

    fn process_timer(&mut self, now: Instant) {
        match &self.state {
            // Only the client runs timers while waiting for Initial packets.
            State::WaitInitial => debug_assert_eq!(self.role, Role::Client),
            // If Closing or Draining, check if it is time to move to Closed.
            State::Closing { error, timeout } | State::Draining { error, timeout } => {
                if *timeout <= now {
                    let st = State::Closed(error.clone());
                    self.set_state(st);
                    qinfo!("Closing timer expired");
                    return;
                }
            }
            State::Closed(_) => {
                qdebug!("Timer fired while closed");
                return;
            }
            _ => (),
        }

        let pto = self.pto();
        if self.idle_timeout.expired(now, pto) {
            qinfo!([self], "idle timeout expired");
            self.set_state(State::Closed(ConnectionError::Transport(
                Error::IdleTimeout,
            )));
            return;
        }

        self.streams.cleanup_closed_streams();

        let res = self.crypto.states.check_key_update(now);
        self.absorb_error(now, res);

        let lost = self.loss_recovery.timeout(&self.paths.primary(), now);
        self.handle_lost_packets(&lost);
        qlog::packets_lost(&mut self.qlog, &lost);

        if self.release_resumption_token_timer.is_some() {
            self.create_resumption_token(now);
        }

        if !self.paths.process_timeout(now, pto) {
            qinfo!([self], "last available path failed");
            self.absorb_error::<Error>(now, Err(Error::NoAvailablePath));
        }
    }

    /// Process new input datagrams on the connection.
    pub fn process_input(&mut self, d: Datagram, now: Instant) {
        self.input(d, now, now);
        self.process_saved(now);
        self.streams.cleanup_closed_streams();
    }

    /// Get the time that we next need to be called back, relative to `now`.
    fn next_delay(&mut self, now: Instant, paced: bool) -> Duration {
        qtrace!([self], "Get callback delay {:?}", now);

        // Only one timer matters when closing...
        if let State::Closing { timeout, .. } | State::Draining { timeout, .. } = self.state {
            self.hrtime.update(Self::LOOSE_TIMER_RESOLUTION);
            return timeout.duration_since(now);
        }

        let mut delays = SmallVec::<[_; 6]>::new();
        if let Some(ack_time) = self.acks.ack_time(now) {
            qtrace!([self], "Delayed ACK timer {:?}", ack_time);
            delays.push(ack_time);
        }

        if let Some(p) = self.paths.primary_fallible() {
            let path = p.borrow();
            let rtt = path.rtt();
            let pto = rtt.pto(PacketNumberSpace::ApplicationData);

            let keep_alive = self.streams.need_keep_alive();
            let idle_time = self.idle_timeout.expiry(now, pto, keep_alive);
            qtrace!([self], "Idle/keepalive timer {:?}", idle_time);
            delays.push(idle_time);

            if let Some(lr_time) = self.loss_recovery.next_timeout(rtt) {
                qtrace!([self], "Loss recovery timer {:?}", lr_time);
                delays.push(lr_time);
            }

            if paced {
                if let Some(pace_time) = path.sender().next_paced(rtt.estimate()) {
                    qtrace!([self], "Pacing timer {:?}", pace_time);
                    delays.push(pace_time);
                }
            }

            if let Some(path_time) = self.paths.next_timeout(pto) {
                qtrace!([self], "Path probe timer {:?}", path_time);
                delays.push(path_time);
            }
        }

        if let Some(key_update_time) = self.crypto.states.update_time() {
            qtrace!([self], "Key update timer {:?}", key_update_time);
            delays.push(key_update_time);
        }

        // `release_resumption_token_timer` is not considered here, because
        // it is not important enough to force the application to set a
        // timeout for it  It is expected that other activities will
        // drive it.

        let earliest = delays.into_iter().min().unwrap();
        // TODO(agrover, mt) - need to analyze and fix #47
        // rather than just clamping to zero here.
        debug_assert!(earliest > now);
        let delay = earliest.saturating_duration_since(now);
        qdebug!([self], "delay duration {:?}", delay);
        self.hrtime.update(delay / 4);
        delay
    }

    /// Get output packets, as a result of receiving packets, or actions taken
    /// by the application.
    /// Returns datagrams to send, and how long to wait before calling again
    /// even if no incoming packets.
    #[must_use = "Output of the process_output function must be handled"]
    pub fn process_output(&mut self, now: Instant) -> Output {
        qtrace!([self], "process_output {:?} {:?}", self.state, now);

        match (&self.state, self.role) {
            (State::Init, Role::Client) => {
                let res = self.client_start(now);
                self.absorb_error(now, res);
            }
            (State::Init, Role::Server) | (State::WaitInitial, Role::Server) => {
                return Output::None;
            }
            _ => {
                self.process_timer(now);
            }
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
            self.input(d, now, now);
            self.process_saved(now);
        }
        self.process_output(now)
    }

    fn handle_retry(&mut self, packet: &PublicPacket, now: Instant) {
        qinfo!([self], "received Retry");
        if matches!(self.address_validation, AddressValidationInfo::Retry { .. }) {
            self.stats.borrow_mut().pkt_dropped("Extra Retry");
            return;
        }
        if packet.token().is_empty() {
            self.stats.borrow_mut().pkt_dropped("Retry without a token");
            return;
        }
        if !packet.is_valid_retry(self.original_destination_cid.as_ref().unwrap()) {
            self.stats
                .borrow_mut()
                .pkt_dropped("Retry with bad integrity tag");
            return;
        }
        // At this point, we should only have the connection ID that we generated.
        // Update to the one that the server prefers.
        let path = self.paths.primary();
        path.borrow_mut().set_remote_cid(packet.scid());

        let retry_scid = ConnectionId::from(packet.scid());
        qinfo!(
            [self],
            "Valid Retry received, token={} scid={}",
            hex(packet.token()),
            retry_scid
        );

        let lost_packets = self.loss_recovery.retry(&path, now);
        self.handle_lost_packets(&lost_packets);

        self.crypto.states.init(
            self.conn_params.get_versions().compatible(),
            self.role,
            &retry_scid,
        );
        self.address_validation = AddressValidationInfo::Retry {
            token: packet.token().to_vec(),
            retry_source_cid: retry_scid,
        };
    }

    fn discard_keys(&mut self, space: PacketNumberSpace, now: Instant) {
        if self.crypto.discard(space) {
            qinfo!([self], "Drop packet number space {}", space);
            let primary = self.paths.primary();
            self.loss_recovery.discard(&primary, space, now);
            self.acks.drop_space(space);
        }
    }

    fn is_stateless_reset(&self, path: &PathRef, d: &Datagram) -> bool {
        // If the datagram is too small, don't try.
        // If the connection is connected, then the reset token will be invalid.
        if d.len() < 16 || !self.state.connected() {
            return false;
        }
        let token = <&[u8; 16]>::try_from(&d[d.len() - 16..]).unwrap();
        path.borrow().is_stateless_reset(token)
    }

    fn check_stateless_reset(
        &mut self,
        path: &PathRef,
        d: &Datagram,
        first: bool,
        now: Instant,
    ) -> Res<()> {
        if first && self.is_stateless_reset(path, d) {
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

    /// Process any saved datagrams that might be available for processing.
    fn process_saved(&mut self, now: Instant) {
        while let Some(cspace) = self.saved_datagrams.available() {
            qdebug!([self], "process saved for space {:?}", cspace);
            debug_assert!(self.crypto.states.rx_hp(self.version, cspace).is_some());
            for saved in self.saved_datagrams.take_saved() {
                qtrace!([self], "input saved @{:?}: {:?}", saved.t, saved.d);
                self.input(saved.d, saved.t, now);
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

    /// Perform version negotiation.
    fn version_negotiation(&mut self, supported: &[WireVersion], now: Instant) -> Res<()> {
        debug_assert_eq!(self.role, Role::Client);

        if let Some(version) = self.conn_params.get_versions().preferred(supported) {
            assert_ne!(self.version, version);

            qinfo!([self], "Version negotiation: trying {:?}", version);
            let local_addr = self.paths.primary().borrow().local_address();
            let remote_addr = self.paths.primary().borrow().remote_address();
            let conn_params = self
                .conn_params
                .clone()
                .versions(version, self.conn_params.get_versions().all().to_vec());
            let mut c = Self::new_client(
                self.crypto.server_name().unwrap(),
                self.crypto.protocols(),
                self.cid_manager.generator(),
                local_addr,
                remote_addr,
                conn_params,
                now,
            )?;
            c.conn_params
                .get_versions_mut()
                .set_initial(self.conn_params.get_versions().initial());
            mem::swap(self, &mut c);
            Ok(())
        } else {
            qinfo!([self], "Version negotiation: failed with {:?}", supported);
            // This error goes straight to closed.
            self.set_state(State::Closed(ConnectionError::Transport(
                Error::VersionNegotiation,
            )));
            Err(Error::VersionNegotiation)
        }
    }

    /// Perform any processing that we might have to do on packets prior to
    /// attempting to remove protection.
    fn preprocess_packet(
        &mut self,
        packet: &PublicPacket,
        path: &PathRef,
        dcid: Option<&ConnectionId>,
        now: Instant,
    ) -> Res<PreprocessResult> {
        if dcid.map_or(false, |d| d != packet.dcid()) {
            self.stats
                .borrow_mut()
                .pkt_dropped("Coalesced packet has different DCID");
            return Ok(PreprocessResult::Next);
        }

        if (packet.packet_type() == PacketType::Initial
            || packet.packet_type() == PacketType::Handshake)
            && self.role == Role::Client
            && !path.borrow().is_primary()
        {
            // If we have received a packet from a different address than we have sent to
            // we should ignore the packet. In such a case a path will be a newly created
            // temporary path, not the primary path.
            return Ok(PreprocessResult::Next);
        }

        match (packet.packet_type(), &self.state, &self.role) {
            (PacketType::Initial, State::Init, Role::Server) => {
                let version = *packet.version().as_ref().unwrap();
                if !packet.is_valid_initial()
                    || !self.conn_params.get_versions().all().contains(&version)
                {
                    self.stats.borrow_mut().pkt_dropped("Invalid Initial");
                    return Ok(PreprocessResult::Next);
                }
                qinfo!(
                    [self],
                    "Received valid Initial packet with scid {:?} dcid {:?}",
                    packet.scid(),
                    packet.dcid()
                );
                // Record the client's selected CID so that it can be accepted until
                // the client starts using a real connection ID.
                let dcid = ConnectionId::from(packet.dcid());
                self.crypto.states.init_server(version, &dcid);
                self.original_destination_cid = Some(dcid);
                self.set_state(State::WaitInitial);

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
                            || versions.contains(&self.version().wire_version())
                            || versions.contains(&0)
                            || packet.scid() != self.odcid().unwrap()
                            || matches!(
                                self.address_validation,
                                AddressValidationInfo::Retry { .. }
                            )
                        {
                            // Ignore VersionNegotiation packets that contain the current version.
                            // Or don't have the right connection ID.
                            // Or are received after a Retry.
                            self.stats.borrow_mut().pkt_dropped("Invalid VN");
                            return Ok(PreprocessResult::End);
                        }

                        self.version_negotiation(&versions, now)?;
                        return Ok(PreprocessResult::End);
                    }
                    Err(_) => {
                        self.stats.borrow_mut().pkt_dropped("VN with no versions");
                        return Ok(PreprocessResult::End);
                    }
                }
            }
            (PacketType::Retry, State::WaitInitial, Role::Client) => {
                self.handle_retry(packet, now);
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
                    && self.cid_manager.is_valid(packet.dcid())
                    && self.stats.borrow().saved_datagrams <= EXTRA_INITIALS
                {
                    self.crypto.resend_unacked(PacketNumberSpace::Initial);
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
            State::WaitVersion | State::Handshaking | State::Connected | State::Confirmed => {
                if !self.cid_manager.is_valid(packet.dcid()) {
                    self.stats
                        .borrow_mut()
                        .pkt_dropped(format!("Invalid DCID {:?}", packet.dcid()));
                    PreprocessResult::Next
                } else {
                    if self.role == Role::Server && packet.packet_type() == PacketType::Handshake {
                        // Server has received a Handshake packet -> discard Initial keys and states
                        self.discard_keys(PacketNumberSpace::Initial, now);
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

    /// After a Initial, Handshake, ZeroRtt, or Short packet is successfully processed.
    fn postprocess_packet(
        &mut self,
        path: &PathRef,
        d: &Datagram,
        packet: &PublicPacket,
        migrate: bool,
        now: Instant,
    ) {
        if self.state == State::WaitInitial {
            self.start_handshake(path, packet, now);
        }

        if self.state.connected() {
            self.handle_migration(path, d, migrate, now);
        } else if self.role != Role::Client
            && (packet.packet_type() == PacketType::Handshake
                || (packet.dcid().len() >= 8 && packet.dcid() == &self.local_initial_source_cid))
        {
            // We only allow one path during setup, so apply handshake
            // path validation to this path.
            path.borrow_mut().set_valid(now);
        }
    }

    /// Take a datagram as input.  This reports an error if the packet was bad.
    /// This takes two times: when the datagram was received, and the current time.
    fn input(&mut self, d: Datagram, received: Instant, now: Instant) {
        // First determine the path.
        let path = self.paths.find_path_with_rebinding(
            d.destination(),
            d.source(),
            self.conn_params.get_cc_algorithm(),
            now,
        );
        path.borrow_mut().add_received(d.len());
        let res = self.input_path(&path, d, received);
        self.capture_error(Some(path), now, 0, res).ok();
    }

    fn input_path(&mut self, path: &PathRef, d: Datagram, now: Instant) -> Res<()> {
        let mut slc = &d[..];
        let mut dcid = None;

        qtrace!([self], "{} input {}", path.borrow(), hex(&**d));
        let pto = path.borrow().rtt().pto(PacketNumberSpace::ApplicationData);

        // Handle each packet in the datagram.
        while !slc.is_empty() {
            self.stats.borrow_mut().packets_rx += 1;
            let (packet, remainder) =
                match PublicPacket::decode(slc, self.cid_manager.decoder().as_ref()) {
                    Ok((packet, remainder)) => (packet, remainder),
                    Err(e) => {
                        qinfo!([self], "Garbage packet: {}", e);
                        qtrace!([self], "Garbage packet contents: {}", hex(slc));
                        self.stats.borrow_mut().pkt_dropped("Garbage packet");
                        break;
                    }
                };
            match self.preprocess_packet(&packet, path, dcid.as_ref(), now)? {
                PreprocessResult::Continue => (),
                PreprocessResult::Next => break,
                PreprocessResult::End => return Ok(()),
            }

            qtrace!([self], "Received unverified packet {:?}", packet);

            match packet.decrypt(&mut self.crypto.states, now + pto) {
                Ok(payload) => {
                    // OK, we have a valid packet.
                    self.idle_timeout.on_packet_received(now);
                    dump_packet(
                        self,
                        path,
                        "-> RX",
                        payload.packet_type(),
                        payload.pn(),
                        &payload[..],
                    );

                    qlog::packet_received(&mut self.qlog, &packet, &payload);
                    let space = PacketNumberSpace::from(payload.packet_type());
                    if self.acks.get_mut(space).unwrap().is_duplicate(payload.pn()) {
                        qdebug!([self], "Duplicate packet {}-{}", space, payload.pn());
                        self.stats.borrow_mut().dups_rx += 1;
                    } else {
                        match self.process_packet(path, &payload, now) {
                            Ok(migrate) => self.postprocess_packet(path, &d, &packet, migrate, now),
                            Err(e) => {
                                self.ensure_error_path(path, &packet, now);
                                return Err(e);
                            }
                        }
                    }
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
                        Error::KeysDiscarded(cspace) => {
                            // This was a valid-appearing Initial packet: maybe probe with
                            // a Handshake packet to keep the handshake moving.
                            self.received_untracked |=
                                self.role == Role::Client && cspace == CryptoSpace::Initial;
                        }
                        _ => (),
                    }
                    // Decryption failure, or not having keys is not fatal.
                    // If the state isn't available, or we can't decrypt the packet, drop
                    // the rest of the datagram on the floor, but don't generate an error.
                    self.check_stateless_reset(path, &d, dcid.is_none(), now)?;
                    self.stats.borrow_mut().pkt_dropped("Decryption failure");
                    qlog::packet_dropped(&mut self.qlog, &packet);
                }
            }
            slc = remainder;
            dcid = Some(ConnectionId::from(packet.dcid()));
        }
        self.check_stateless_reset(path, &d, dcid.is_none(), now)?;
        Ok(())
    }

    /// Process a packet.  Returns true if the packet might initiate migration.
    fn process_packet(
        &mut self,
        path: &PathRef,
        packet: &DecryptedPacket,
        now: Instant,
    ) -> Res<bool> {
        // TODO(ekr@rtfm.com): Have the server blow away the initial
        // crypto state if this fails? Otherwise, we will get a panic
        // on the assert for doesn't exist.
        // OK, we have a valid packet.

        let mut ack_eliciting = false;
        let mut probing = true;
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
            probing &= f.path_probing();
            let t = f.get_type();
            if let Err(e) = self.input_frame(path, packet.version(), packet.packet_type(), f, now) {
                self.capture_error(Some(Rc::clone(path)), now, t, Err(e))?;
            }
        }

        let largest_received = if let Some(space) = self
            .acks
            .get_mut(PacketNumberSpace::from(packet.packet_type()))
        {
            space.set_received(now, packet.pn(), ack_eliciting)
        } else {
            qdebug!(
                [self],
                "processed a {:?} packet without tracking it",
                packet.packet_type(),
            );
            // This was a valid packet that caused the same packet number to be
            // discarded.  This happens when the client discards the Initial packet
            // number space after receiving the ServerHello.  Remember this so
            // that we guarantee that we send a Handshake packet.
            self.received_untracked = true;
            // We don't migrate during the handshake, so return false.
            false
        };

        Ok(largest_received && !probing)
    }

    /// During connection setup, the first path needs to be setup.
    /// This uses the connection IDs that were provided during the handshake
    /// to setup that path.
    #[allow(clippy::or_fun_call)] // Remove when MSRV >= 1.59
    fn setup_handshake_path(&mut self, path: &PathRef, now: Instant) {
        self.paths.make_permanent(
            path,
            Some(self.local_initial_source_cid.clone()),
            // Ideally we know what the peer wants us to use for the remote CID.
            // But we will use our own guess if necessary.
            ConnectionIdEntry::initial_remote(
                self.remote_initial_source_cid
                    .as_ref()
                    .or(self.original_destination_cid.as_ref())
                    .unwrap()
                    .clone(),
            ),
        );
        path.borrow_mut().set_valid(now);
    }

    /// If the path isn't permanent, assign it a connection ID to make it so.
    fn ensure_permanent(&mut self, path: &PathRef) -> Res<()> {
        if self.paths.is_temporary(path) {
            // If there isn't a connection ID to use for this path, the packet
            // will be processed, but it won't be attributed to a path.  That means
            // no path probes or PATH_RESPONSE.  But it's not fatal.
            if let Some(cid) = self.connection_ids.next() {
                self.paths.make_permanent(path, None, cid);
                Ok(())
            } else if self.paths.primary().borrow().remote_cid().is_empty() {
                self.paths
                    .make_permanent(path, None, ConnectionIdEntry::empty_remote());
                Ok(())
            } else {
                qtrace!([self], "Unable to make path permanent: {}", path.borrow());
                Err(Error::InvalidMigration)
            }
        } else {
            Ok(())
        }
    }

    /// After an error, a permanent path is needed to send the CONNECTION_CLOSE.
    /// This attempts to ensure that this exists.  As the connection is now
    /// temporary, there is no reason to do anything special here.
    fn ensure_error_path(&mut self, path: &PathRef, packet: &PublicPacket, now: Instant) {
        path.borrow_mut().set_valid(now);
        if self.paths.is_temporary(path) {
            // First try to fill in handshake details.
            if packet.packet_type() == PacketType::Initial {
                self.remote_initial_source_cid = Some(ConnectionId::from(packet.scid()));
                self.setup_handshake_path(path, now);
            } else {
                // Otherwise try to get a usable connection ID.
                mem::drop(self.ensure_permanent(path));
            }
        }
    }

    fn start_handshake(&mut self, path: &PathRef, packet: &PublicPacket, now: Instant) {
        qtrace!([self], "starting handshake");
        debug_assert_eq!(packet.packet_type(), PacketType::Initial);
        self.remote_initial_source_cid = Some(ConnectionId::from(packet.scid()));

        let got_version = if self.role == Role::Server {
            self.cid_manager
                .add_odcid(self.original_destination_cid.as_ref().unwrap().clone());
            // Make a path on which to run the handshake.
            self.setup_handshake_path(path, now);

            self.zero_rtt_state = match self.crypto.enable_0rtt(self.version, self.role) {
                Ok(true) => {
                    qdebug!([self], "Accepted 0-RTT");
                    ZeroRttState::AcceptedServer
                }
                _ => ZeroRttState::Rejected,
            };

            // The server knows the final version if it has remote transport parameters.
            self.tps.borrow().remote.is_some()
        } else {
            qdebug!([self], "Changing to use Server CID={}", packet.scid());
            debug_assert!(path.borrow().is_primary());
            path.borrow_mut().set_remote_cid(packet.scid());

            // The client knows the final version if it processed a CRYPTO frame.
            self.stats.borrow().frame_rx.crypto > 0
        };
        if got_version {
            self.set_state(State::Handshaking);
        } else {
            self.set_state(State::WaitVersion);
        }
    }

    /// Migrate to the provided path.
    /// Either local or remote address (but not both) may be provided as `None` to have
    /// the address from the current primary path used.
    /// If `force` is true, then migration is immediate.
    /// Otherwise, migration occurs after the path is probed successfully.
    /// Either way, the path is probed and will be abandoned if the probe fails.
    ///
    /// # Errors
    /// Fails if this is not a client, not confirmed, or there are not enough connection
    /// IDs available to use.
    pub fn migrate(
        &mut self,
        local: Option<SocketAddr>,
        remote: Option<SocketAddr>,
        force: bool,
        now: Instant,
    ) -> Res<()> {
        if self.role != Role::Client {
            return Err(Error::InvalidMigration);
        }
        if !matches!(self.state(), State::Confirmed) {
            return Err(Error::InvalidMigration);
        }

        // Fill in the blanks, using the current primary path.
        if local.is_none() && remote.is_none() {
            // Pointless migration is pointless.
            return Err(Error::InvalidMigration);
        }
        let local = local.unwrap_or_else(|| self.paths.primary().borrow().local_address());
        let remote = remote.unwrap_or_else(|| self.paths.primary().borrow().remote_address());

        if mem::discriminant(&local.ip()) != mem::discriminant(&remote.ip()) {
            // Can't mix address families.
            return Err(Error::InvalidMigration);
        }
        if local.port() == 0 || remote.ip().is_unspecified() || remote.port() == 0 {
            // All but the local address need to be specified.
            return Err(Error::InvalidMigration);
        }
        if (local.ip().is_loopback() ^ remote.ip().is_loopback()) && !local.ip().is_unspecified() {
            // Block attempts to migrate to a path with loopback on only one end, unless the local
            // address is unspecified.
            return Err(Error::InvalidMigration);
        }

        let path = self
            .paths
            .find_path(local, remote, self.conn_params.get_cc_algorithm(), now);
        self.ensure_permanent(&path)?;
        qinfo!(
            [self],
            "Migrate to {} probe {}",
            path.borrow(),
            if force { "now" } else { "after" }
        );
        if self.paths.migrate(&path, force, now) {
            self.loss_recovery.migrate();
        }
        Ok(())
    }

    fn migrate_to_preferred_address(&mut self, now: Instant) -> Res<()> {
        let spa = if matches!(
            self.conn_params.get_preferred_address(),
            PreferredAddressConfig::Disabled
        ) {
            None
        } else {
            self.tps.borrow_mut().remote().get_preferred_address()
        };
        if let Some((addr, cid)) = spa {
            // The connection ID isn't special, so just save it.
            self.connection_ids.add_remote(cid)?;

            // The preferred address doesn't dictate what the local address is, so this
            // has to use the existing address.  So only pay attention to a preferred
            // address from the same family as is currently in use. More thought will
            // be needed to work out how to get addresses from a different family.
            let prev = self.paths.primary().borrow().remote_address();
            let remote = match prev.ip() {
                IpAddr::V4(_) => addr.ipv4(),
                IpAddr::V6(_) => addr.ipv6(),
            };

            if let Some(remote) = remote {
                // Ignore preferred address that move to loopback from non-loopback.
                // `migrate` doesn't enforce this rule.
                if !prev.ip().is_loopback() && remote.ip().is_loopback() {
                    qwarn!([self], "Ignoring a move to a loopback address: {}", remote);
                    return Ok(());
                }

                if self.migrate(None, Some(remote), false, now).is_err() {
                    qwarn!([self], "Ignoring bad preferred address: {}", remote);
                }
            } else {
                qwarn!([self], "Unable to migrate to a different address family");
            }
        }
        Ok(())
    }

    fn handle_migration(&mut self, path: &PathRef, d: &Datagram, migrate: bool, now: Instant) {
        if !migrate {
            return;
        }
        if self.role == Role::Client {
            return;
        }

        if self.ensure_permanent(path).is_ok() {
            self.paths.handle_migration(path, d.source(), now);
        } else {
            qinfo!(
                [self],
                "{} Peer migrated, but no connection ID available",
                path.borrow()
            );
        }
    }

    fn output(&mut self, now: Instant) -> SendOption {
        qtrace!([self], "output {:?}", now);
        let res = match &self.state {
            State::Init
            | State::WaitInitial
            | State::WaitVersion
            | State::Handshaking
            | State::Connected
            | State::Confirmed => {
                if let Some(path) = self.paths.select_path() {
                    let res = self.output_path(&path, now);
                    self.capture_error(Some(path), now, 0, res)
                } else {
                    Ok(SendOption::default())
                }
            }
            State::Closing { .. } | State::Draining { .. } | State::Closed(_) => {
                if let Some(details) = self.state_signaling.close_frame() {
                    let path = Rc::clone(details.path());
                    let res = self.output_close(details);
                    self.capture_error(Some(path), now, 0, res)
                } else {
                    Ok(SendOption::default())
                }
            }
        };
        res.unwrap_or_default()
    }

    fn build_packet_header(
        path: &Path,
        cspace: CryptoSpace,
        encoder: Encoder,
        tx: &CryptoDxState,
        address_validation: &AddressValidationInfo,
        version: Version,
        grease_quic_bit: bool,
    ) -> (PacketType, PacketBuilder) {
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

            PacketBuilder::long(encoder, pt, version, path.remote_cid(), path.local_cid())
        };
        if builder.remaining() > 0 {
            builder.scramble(grease_quic_bit);
            if pt == PacketType::Initial {
                builder.initial_token(address_validation.token());
            }
        }

        (pt, builder)
    }

    #[must_use]
    fn add_packet_number(
        builder: &mut PacketBuilder,
        tx: &CryptoDxState,
        largest_acknowledged: Option<PacketNumber>,
    ) -> PacketNumber {
        // Get the packet number and work out how long it is.
        let pn = tx.next_pn();
        let unacked_range = if let Some(la) = largest_acknowledged {
            // Double the range from this to the last acknowledged in this space.
            (pn - la) << 1
        } else {
            pn + 1
        };
        // Count how many bytes in this range are non-zero.
        let pn_len = mem::size_of::<PacketNumber>()
            - usize::try_from(unacked_range.leading_zeros() / 8).unwrap();
        // pn_len can't be zero (unacked_range is > 0)
        // TODO(mt) also use `4*path CWND/path MTU` to set a minimum length.
        builder.pn(pn, pn_len);
        pn
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

    fn output_close(&mut self, close: ClosingFrame) -> Res<SendOption> {
        let mut encoder = Encoder::with_capacity(256);
        let grease_quic_bit = self.can_grease_quic_bit();
        let version = self.version();
        for space in PacketNumberSpace::iter() {
            let (cspace, tx) =
                if let Some(crypto) = self.crypto.states.select_tx_mut(self.version, *space) {
                    crypto
                } else {
                    continue;
                };

            let path = close.path().borrow();
            let (_, mut builder) = Self::build_packet_header(
                &path,
                cspace,
                encoder,
                tx,
                &AddressValidationInfo::None,
                version,
                grease_quic_bit,
            );
            let _ = Self::add_packet_number(
                &mut builder,
                tx,
                self.loss_recovery.largest_acknowledged_pn(*space),
            );
            // The builder will set the limit to 0 if there isn't enough space for the header.
            if builder.is_full() {
                encoder = builder.abort();
                break;
            }
            builder.set_limit(min(path.amplification_limit(), path.mtu()) - tx.expansion());
            debug_assert!(builder.limit() <= 2048);

            // ConnectionError::Application is only allowed at 1RTT.
            let sanitized = if *space == PacketNumberSpace::ApplicationData {
                None
            } else {
                close.sanitize()
            };
            sanitized
                .as_ref()
                .unwrap_or(&close)
                .write_frame(&mut builder);
            if builder.len() > builder.limit() {
                return Err(Error::InternalError(10));
            }
            encoder = builder.build(tx)?;
        }

        Ok(SendOption::Yes(close.path().borrow().datagram(encoder)))
    }

    /// Write the frames that are exchanged in the application data space.
    /// The order of calls here determines the relative priority of frames.
    fn write_appdata_frames(
        &mut self,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
    ) -> Res<()> {
        if self.role == Role::Server {
            if let Some(t) = self.state_signaling.write_done(builder)? {
                tokens.push(t);
                self.stats.borrow_mut().frame_tx.handshake_done += 1;
            }
        }

        // Check if there is a Datagram to be written
        self.quic_datagrams
            .write_frames(builder, tokens, &mut self.stats.borrow_mut());

        let stats = &mut self.stats.borrow_mut().frame_tx;

        self.streams
            .write_frames(TransmissionPriority::Critical, builder, tokens, stats);
        if builder.is_full() {
            return Ok(());
        }

        self.streams
            .write_frames(TransmissionPriority::Important, builder, tokens, stats);
        if builder.is_full() {
            return Ok(());
        }

        // NEW_CONNECTION_ID, RETIRE_CONNECTION_ID, and ACK_FREQUENCY.
        self.cid_manager.write_frames(builder, tokens, stats)?;
        if builder.is_full() {
            return Ok(());
        }
        self.paths.write_frames(builder, tokens, stats)?;
        if builder.is_full() {
            return Ok(());
        }

        self.streams
            .write_frames(TransmissionPriority::High, builder, tokens, stats);
        if builder.is_full() {
            return Ok(());
        }

        self.streams
            .write_frames(TransmissionPriority::Normal, builder, tokens, stats);
        if builder.is_full() {
            return Ok(());
        }

        // CRYPTO here only includes NewSessionTicket, plus NEW_TOKEN.
        // Both of these are only used for resumption and so can be relatively low priority.
        self.crypto
            .write_frame(PacketNumberSpace::ApplicationData, builder, tokens, stats)?;
        if builder.is_full() {
            return Ok(());
        }
        self.new_token.write_frames(builder, tokens, stats)?;
        if builder.is_full() {
            return Ok(());
        }

        self.streams
            .write_frames(TransmissionPriority::Low, builder, tokens, stats);

        #[cfg(test)]
        {
            if let Some(w) = &mut self.test_frame_writer {
                w.write_frames(builder);
            }
        }

        Ok(())
    }

    // Maybe send a probe.  Return true if the packet was ack-eliciting.
    fn maybe_probe(
        &mut self,
        path: &PathRef,
        force_probe: bool,
        builder: &mut PacketBuilder,
        ack_end: usize,
        tokens: &mut Vec<RecoveryToken>,
        now: Instant,
    ) -> bool {
        let untracked = self.received_untracked && !self.state.connected();
        self.received_untracked = false;

        // Anything written after an ACK already elicits acknowledgment.
        // If we need to probe and nothing has been written, send a PING.
        if builder.len() > ack_end {
            return true;
        }

        let probe = if untracked && builder.packet_empty() || force_probe {
            // If we received an untracked packet and we aren't probing already
            // or the PTO timer fired: probe.
            true
        } else {
            let pto = path.borrow().rtt().pto(PacketNumberSpace::ApplicationData);
            if !builder.packet_empty() {
                // The packet only contains an ACK.  Check whether we want to
                // force an ACK with a PING so we can stop tracking packets.
                self.loss_recovery.should_probe(pto, now)
            } else if self.streams.need_keep_alive() {
                // We need to keep the connection alive, including sending
                // a PING again.
                self.idle_timeout.send_keep_alive(now, pto, tokens)
            } else {
                false
            }
        };
        if probe {
            // Nothing ack-eliciting and we need to probe; send PING.
            debug_assert_ne!(builder.remaining(), 0);
            builder.encode_varint(crate::frame::FRAME_TYPE_PING);
            let stats = &mut self.stats.borrow_mut().frame_tx;
            stats.ping += 1;
            stats.all += 1;
        }
        probe
    }

    /// Write frames to the provided builder.  Returns a list of tokens used for
    /// tracking loss or acknowledgment, whether any frame was ACK eliciting, and
    /// whether the packet was padded.
    fn write_frames(
        &mut self,
        path: &PathRef,
        space: PacketNumberSpace,
        profile: &SendProfile,
        builder: &mut PacketBuilder,
        now: Instant,
    ) -> Res<(Vec<RecoveryToken>, bool, bool)> {
        let mut tokens = Vec::new();
        let primary = path.borrow().is_primary();
        let mut ack_eliciting = false;

        if primary {
            let stats = &mut self.stats.borrow_mut().frame_tx;
            self.acks
                .write_frame(space, now, builder, &mut tokens, stats)?;
        }
        let ack_end = builder.len();

        // Avoid sending probes until the handshake completes,
        // but send them even when we don't have space.
        let full_mtu = profile.limit() == path.borrow().mtu();
        if space == PacketNumberSpace::ApplicationData && self.state.connected() {
            // Probes should only be padded if the full MTU is available.
            // The probing code needs to know so it can track that.
            if path.borrow_mut().write_frames(
                builder,
                &mut self.stats.borrow_mut().frame_tx,
                full_mtu,
                now,
            )? {
                builder.enable_padding(true);
            }
        }

        if profile.ack_only(space) {
            // If we are CC limited we can only send acks!
            return Ok((tokens, false, false));
        }

        if primary {
            if space == PacketNumberSpace::ApplicationData {
                self.write_appdata_frames(builder, &mut tokens)?;
            } else {
                let stats = &mut self.stats.borrow_mut().frame_tx;
                self.crypto
                    .write_frame(space, builder, &mut tokens, stats)?;
            }
        }

        // Maybe send a probe now, either to probe for losses or to keep the connection live.
        let force_probe = profile.should_probe(space);
        ack_eliciting |= self.maybe_probe(path, force_probe, builder, ack_end, &mut tokens, now);
        // If this is not the primary path, this should be ack-eliciting.
        debug_assert!(primary || ack_eliciting);

        // Add padding.  Only pad 1-RTT packets so that we don't prevent coalescing.
        // And avoid padding packets that otherwise only contain ACK because adding PADDING
        // causes those packets to consume congestion window, which is not tracked (yet).
        // And avoid padding if we don't have a full MTU available.
        let stats = &mut self.stats.borrow_mut().frame_tx;
        let padded = if ack_eliciting && full_mtu && builder.pad() {
            stats.padding += 1;
            stats.all += 1;
            true
        } else {
            false
        };

        stats.all += tokens.len();
        Ok((tokens, ack_eliciting, padded))
    }

    /// Build a datagram, possibly from multiple packets (for different PN
    /// spaces) and each containing 1+ frames.
    fn output_path(&mut self, path: &PathRef, now: Instant) -> Res<SendOption> {
        let mut initial_sent = None;
        let mut needs_padding = false;
        let grease_quic_bit = self.can_grease_quic_bit();
        let version = self.version();

        // Determine how we are sending packets (PTO, etc..).
        let mtu = path.borrow().mtu();
        let profile = self.loss_recovery.send_profile(&*path.borrow(), now);
        qdebug!([self], "output_path send_profile {:?}", profile);

        // Frames for different epochs must go in different packets, but then these
        // packets can go in a single datagram
        let mut encoder = Encoder::with_capacity(profile.limit());
        for space in PacketNumberSpace::iter() {
            // Ensure we have tx crypto state for this epoch, or skip it.
            let (cspace, tx) =
                if let Some(crypto) = self.crypto.states.select_tx_mut(self.version, *space) {
                    crypto
                } else {
                    continue;
                };

            let header_start = encoder.len();
            let (pt, mut builder) = Self::build_packet_header(
                &path.borrow(),
                cspace,
                encoder,
                tx,
                &self.address_validation,
                version,
                grease_quic_bit,
            );
            let pn = Self::add_packet_number(
                &mut builder,
                tx,
                self.loss_recovery.largest_acknowledged_pn(*space),
            );
            // The builder will set the limit to 0 if there isn't enough space for the header.
            if builder.is_full() {
                encoder = builder.abort();
                break;
            }

            // Configure the limits and padding for this packet.
            let aead_expansion = tx.expansion();
            builder.set_limit(profile.limit() - aead_expansion);
            builder.enable_padding(needs_padding);
            debug_assert!(builder.limit() <= 2048);
            if builder.is_full() {
                encoder = builder.abort();
                break;
            }

            // Add frames to the packet.
            let payload_start = builder.len();
            let (tokens, ack_eliciting, padded) =
                self.write_frames(path, *space, &profile, &mut builder, now)?;
            if builder.packet_empty() {
                // Nothing to include in this packet.
                encoder = builder.abort();
                continue;
            }

            dump_packet(
                self,
                path,
                "TX ->",
                pt,
                pn,
                &builder.as_ref()[payload_start..],
            );
            qlog::packet_sent(
                &mut self.qlog,
                pt,
                pn,
                builder.len() - header_start + aead_expansion,
                &builder.as_ref()[payload_start..],
            );

            self.stats.borrow_mut().packets_tx += 1;
            let tx = self.crypto.states.tx_mut(self.version, cspace).unwrap();
            encoder = builder.build(tx)?;
            debug_assert!(encoder.len() <= mtu);
            self.crypto.states.auto_update()?;

            if ack_eliciting {
                self.idle_timeout.on_packet_sent(now);
            }
            let sent = SentPacket::new(
                pt,
                pn,
                now,
                ack_eliciting,
                tokens,
                encoder.len() - header_start,
            );
            if padded {
                needs_padding = false;
                self.loss_recovery.on_packet_sent(path, sent);
            } else if pt == PacketType::Initial && (self.role == Role::Client || ack_eliciting) {
                // Packets containing Initial packets might need padding, and we want to
                // track that padding along with the Initial packet.  So defer tracking.
                initial_sent = Some(sent);
                needs_padding = true;
            } else {
                if pt == PacketType::Handshake && self.role == Role::Client {
                    needs_padding = false;
                }
                self.loss_recovery.on_packet_sent(path, sent);
            }

            if *space == PacketNumberSpace::Handshake
                && self.role == Role::Server
                && self.state == State::Confirmed
            {
                // We could discard handshake keys in set_state,
                // but wait until after sending an ACK.
                self.discard_keys(PacketNumberSpace::Handshake, now);
            }
        }

        if encoder.is_empty() {
            Ok(SendOption::No(profile.paced()))
        } else {
            // Perform additional padding for Initial packets as necessary.
            let mut packets: Vec<u8> = encoder.into();
            if let Some(mut initial) = initial_sent.take() {
                if needs_padding {
                    qdebug!([self], "pad Initial to path MTU {}", mtu);
                    initial.size += mtu - packets.len();
                    packets.resize(mtu, 0);
                }
                self.loss_recovery.on_packet_sent(path, initial);
            }
            path.borrow_mut().add_sent(packets.len());
            Ok(SendOption::Yes(path.borrow().datagram(packets)))
        }
    }

    pub fn initiate_key_update(&mut self) -> Res<()> {
        if self.state == State::Confirmed {
            let la = self
                .loss_recovery
                .largest_acknowledged_pn(PacketNumberSpace::ApplicationData);
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
        qlog::client_connection_started(&mut self.qlog, &self.paths.primary());

        self.handshake(now, self.version, PacketNumberSpace::Initial, None)?;
        self.set_state(State::WaitInitial);
        self.zero_rtt_state = if self.crypto.enable_0rtt(self.version, self.role)? {
            qdebug!([self], "Enabled 0-RTT");
            ZeroRttState::Sending
        } else {
            ZeroRttState::Init
        };
        Ok(())
    }

    fn get_closing_period_time(&self, now: Instant) -> Instant {
        // Spec says close time should be at least PTO times 3.
        now + (self.pto() * 3)
    }

    /// Close the connection.
    pub fn close(&mut self, now: Instant, app_error: AppError, msg: impl AsRef<str>) {
        let error = ConnectionError::Application(app_error);
        let timeout = self.get_closing_period_time(now);
        if let Some(path) = self.paths.primary_fallible() {
            self.state_signaling.close(path, error.clone(), 0, msg);
            self.set_state(State::Closing { error, timeout });
        } else {
            self.set_state(State::Closed(error));
        }
    }

    fn set_initial_limits(&mut self) {
        self.streams.set_initial_limits();
        let peer_timeout = self
            .tps
            .borrow()
            .remote()
            .get_integer(tparams::IDLE_TIMEOUT);
        if peer_timeout > 0 {
            self.idle_timeout
                .set_peer_timeout(Duration::from_millis(peer_timeout));
        }

        self.quic_datagrams.set_remote_datagram_size(
            self.tps
                .borrow()
                .remote()
                .get_integer(tparams::MAX_DATAGRAM_FRAME_SIZE),
        );
    }

    pub fn is_stream_id_allowed(&self, stream_id: StreamId) -> bool {
        self.streams.is_stream_id_allowed(stream_id)
    }

    /// Process the final set of transport parameters.
    fn process_tps(&mut self) -> Res<()> {
        self.validate_cids()?;
        self.validate_versions()?;
        {
            let tps = self.tps.borrow();
            let remote = tps.remote.as_ref().unwrap();

            // If the peer provided a preferred address, then we have to be a client
            // and they have to be using a non-empty connection ID.
            if remote.get_preferred_address().is_some()
                && (self.role == Role::Server
                    || self.remote_initial_source_cid.as_ref().unwrap().is_empty())
            {
                return Err(Error::TransportParameterError);
            }

            let reset_token = if let Some(token) = remote.get_bytes(tparams::STATELESS_RESET_TOKEN)
            {
                <[u8; 16]>::try_from(token).unwrap()
            } else {
                // The other side didn't provide a stateless reset token.
                // That's OK, they can try guessing this.
                <[u8; 16]>::try_from(&random(16)[..]).unwrap()
            };
            self.paths
                .primary()
                .borrow_mut()
                .set_reset_token(reset_token);

            let max_ad = Duration::from_millis(remote.get_integer(tparams::MAX_ACK_DELAY));
            let min_ad = if remote.has_value(tparams::MIN_ACK_DELAY) {
                let min_ad = Duration::from_micros(remote.get_integer(tparams::MIN_ACK_DELAY));
                if min_ad > max_ad {
                    return Err(Error::TransportParameterError);
                }
                Some(min_ad)
            } else {
                None
            };
            self.paths.primary().borrow_mut().set_ack_delay(
                max_ad,
                min_ad,
                self.conn_params.get_ack_ratio(),
            );

            let max_active_cids = remote.get_integer(tparams::ACTIVE_CONNECTION_ID_LIMIT);
            self.cid_manager.set_limit(max_active_cids);
        }
        self.set_initial_limits();
        qlog::connection_tparams_set(&mut self.qlog, &*self.tps.borrow());
        Ok(())
    }

    fn validate_cids(&mut self) -> Res<()> {
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

    /// Validate the `version_negotiation` transport parameter from the peer.
    fn validate_versions(&mut self) -> Res<()> {
        let tph = self.tps.borrow();
        let remote_tps = tph.remote.as_ref().unwrap();
        // `current` and `other` are the value from the peer's transport parameters.
        // We're checking that these match our expectations.
        if let Some((current, other)) = remote_tps.get_versions() {
            qtrace!(
                [self],
                "validate_versions: current={:x} chosen={:x} other={:x?}",
                self.version.wire_version(),
                current,
                other,
            );
            if self.role == Role::Server {
                // 1. A server acts on transport parameters, with validation
                // of `current` happening in the transport parameter handler.
                // All we need to do is confirm that the transport parameter
                // was provided.
                Ok(())
            } else if self.version().wire_version() != current {
                qinfo!([self], "validate_versions: current version mismatch");
                Err(Error::VersionNegotiation)
            } else if self
                .conn_params
                .get_versions()
                .initial()
                .is_compatible(self.version)
            {
                // 2. The current version is compatible with what we attempted.
                // That's a compatible upgrade and that's OK.
                Ok(())
            } else {
                // 3. The initial version we attempted isn't compatible.  Check that
                // the one we would have chosen is compatible with this one.
                let mut all_versions = other.to_owned();
                all_versions.push(current);
                if self
                    .conn_params
                    .get_versions()
                    .preferred(&all_versions)
                    .ok_or(Error::VersionNegotiation)?
                    .is_compatible(self.version)
                {
                    Ok(())
                } else {
                    qinfo!([self], "validate_versions: failed");
                    Err(Error::VersionNegotiation)
                }
            }
        } else if self.version != Version::Version1 && !self.version.is_draft() {
            qinfo!([self], "validate_versions: missing extension");
            Err(Error::VersionNegotiation)
        } else {
            Ok(())
        }
    }

    fn confirm_version(&mut self, v: Version) {
        if self.version != v {
            qinfo!([self], "Compatible upgrade {:?} ==> {:?}", self.version, v);
        }
        self.crypto.confirm_version(v);
        self.version = v;
    }

    fn compatible_upgrade(&mut self, packet_version: Version) {
        if !matches!(self.state, State::WaitInitial | State::WaitVersion) {
            return;
        }

        if self.role == Role::Client {
            self.confirm_version(packet_version);
        } else if self.tps.borrow().remote.is_some() {
            let version = self.tps.borrow().version();
            let dcid = self.original_destination_cid.as_ref().unwrap();
            self.crypto.states.init_server(version, dcid);
            self.confirm_version(version);
        }
    }

    fn handshake(
        &mut self,
        now: Instant,
        packet_version: Version,
        space: PacketNumberSpace,
        data: Option<&[u8]>,
    ) -> Res<()> {
        qtrace!([self], "Handshake space={} data={:0x?}", space, data);

        let try_update = data.is_some();
        match self.crypto.handshake(now, space, data)? {
            HandshakeState::Authenticated(_) | HandshakeState::InProgress => (),
            HandshakeState::AuthenticationPending => self.events.authentication_needed(),
            HandshakeState::EchFallbackAuthenticationPending(public_name) => self
                .events
                .ech_fallback_authentication_needed(public_name.clone()),
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
        // conditions right is a little tricky, so call whenever CRYPTO data is used.
        if try_update {
            self.compatible_upgrade(packet_version);
            // We have transport parameters, it's go time.
            if self.tps.borrow().remote.is_some() {
                self.set_initial_limits();
            }
            if self.crypto.install_keys(self.role)? {
                if self.role == Role::Client {
                    // We won't acknowledge Initial packets as a result of this, but the
                    // server can rely on implicit acknowledgment.
                    self.discard_keys(PacketNumberSpace::Initial, now);
                }
                self.saved_datagrams.make_available(CryptoSpace::Handshake);
            }
        }

        Ok(())
    }

    fn input_frame(
        &mut self,
        path: &PathRef,
        packet_version: Version,
        packet_type: PacketType,
        frame: Frame,
        now: Instant,
    ) -> Res<()> {
        if !frame.is_allowed(packet_type) {
            qinfo!("frame not allowed: {:?} {:?}", frame, packet_type);
            return Err(Error::ProtocolViolation);
        }
        self.stats.borrow_mut().frame_rx.all += 1;
        let space = PacketNumberSpace::from(packet_type);
        if frame.is_stream() {
            return self
                .streams
                .input_frame(frame, &mut self.stats.borrow_mut().frame_rx);
        }
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
                if space == PacketNumberSpace::ApplicationData {
                    // Send an ACK immediately if we might not otherwise do so.
                    self.acks.immediate_ack(now);
                }
            }
            Frame::Ack {
                largest_acknowledged,
                ack_delay,
                first_ack_range,
                ack_ranges,
            } => {
                let ranges =
                    Frame::decode_ack_frame(largest_acknowledged, first_ack_range, &ack_ranges)?;
                self.handle_ack(space, largest_acknowledged, ranges, ack_delay, now);
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
                    self.handshake(now, packet_version, space, Some(&buf))?;
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
            Frame::NewConnectionId {
                sequence_number,
                connection_id,
                stateless_reset_token,
                retire_prior,
            } => {
                self.stats.borrow_mut().frame_rx.new_connection_id += 1;
                self.connection_ids.add_remote(ConnectionIdEntry::new(
                    sequence_number,
                    ConnectionId::from(connection_id),
                    stateless_reset_token.to_owned(),
                ))?;
                self.paths
                    .retire_cids(retire_prior, &mut self.connection_ids);
                if self.connection_ids.len() >= LOCAL_ACTIVE_CID_LIMIT {
                    qinfo!([self], "received too many connection IDs");
                    return Err(Error::ConnectionIdLimitExceeded);
                }
            }
            Frame::RetireConnectionId { sequence_number } => {
                self.stats.borrow_mut().frame_rx.retire_connection_id += 1;
                self.cid_manager.retire(sequence_number);
            }
            Frame::PathChallenge { data } => {
                self.stats.borrow_mut().frame_rx.path_challenge += 1;
                // If we were challenged, try to make the path permanent.
                // Report an error if we don't have enough connection IDs.
                self.ensure_permanent(path)?;
                path.borrow_mut().challenged(data);
            }
            Frame::PathResponse { data } => {
                self.stats.borrow_mut().frame_rx.path_response += 1;
                if self.paths.path_response(data, now) {
                    // This PATH_RESPONSE enabled migration; tell loss recovery.
                    self.loss_recovery.migrate();
                }
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
                self.state_signaling
                    .drain(Rc::clone(path), error.clone(), frame_type, "");
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
                self.discard_keys(PacketNumberSpace::Handshake, now);
                self.migrate_to_preferred_address(now)?;
            }
            Frame::AckFrequency {
                seqno,
                tolerance,
                delay,
                ignore_order,
            } => {
                self.stats.borrow_mut().frame_rx.ack_frequency += 1;
                let delay = Duration::from_micros(delay);
                if delay < GRANULARITY {
                    return Err(Error::ProtocolViolation);
                }
                self.acks
                    .ack_freq(seqno, tolerance - 1, delay, ignore_order);
            }
            Frame::Datagram { data, .. } => {
                self.stats.borrow_mut().frame_rx.datagram += 1;
                self.quic_datagrams
                    .handle_datagram(data, &mut self.stats.borrow_mut())?;
            }
            _ => unreachable!("All other frames are for streams"),
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
                    RecoveryToken::Crypto(ct) => self.crypto.lost(ct),
                    RecoveryToken::HandshakeDone => self.state_signaling.handshake_done(),
                    RecoveryToken::NewToken(seqno) => self.new_token.lost(*seqno),
                    RecoveryToken::NewConnectionId(ncid) => self.cid_manager.lost(ncid),
                    RecoveryToken::RetireConnectionId(seqno) => self.paths.lost_retire_cid(*seqno),
                    RecoveryToken::AckFrequency(rate) => self.paths.lost_ack_frequency(rate),
                    RecoveryToken::KeepAlive => self.idle_timeout.lost_keep_alive(),
                    RecoveryToken::Stream(stream_token) => self.streams.lost(stream_token),
                    RecoveryToken::Datagram(dgram_tracker) => {
                        self.events
                            .datagram_outcome(dgram_tracker, OutgoingDatagramOutcome::Lost);
                        self.stats.borrow_mut().datagram_tx.lost += 1;
                    }
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

    fn handle_ack<R>(
        &mut self,
        space: PacketNumberSpace,
        largest_acknowledged: u64,
        ack_ranges: R,
        ack_delay: u64,
        now: Instant,
    ) where
        R: IntoIterator<Item = RangeInclusive<u64>> + Debug,
        R::IntoIter: ExactSizeIterator,
    {
        qinfo!([self], "Rx ACK space={}, ranges={:?}", space, ack_ranges);

        let (acked_packets, lost_packets) = self.loss_recovery.on_ack_received(
            &self.paths.primary(),
            space,
            largest_acknowledged,
            ack_ranges,
            self.decode_ack_delay(ack_delay),
            now,
        );
        for acked in acked_packets {
            for token in &acked.tokens {
                match token {
                    RecoveryToken::Stream(stream_token) => self.streams.acked(stream_token),
                    RecoveryToken::Ack(at) => self.acks.acked(at),
                    RecoveryToken::Crypto(ct) => self.crypto.acked(ct),
                    RecoveryToken::NewToken(seqno) => self.new_token.acked(*seqno),
                    RecoveryToken::NewConnectionId(entry) => self.cid_manager.acked(entry),
                    RecoveryToken::RetireConnectionId(seqno) => self.paths.acked_retire_cid(*seqno),
                    RecoveryToken::AckFrequency(rate) => self.paths.acked_ack_frequency(rate),
                    RecoveryToken::KeepAlive => self.idle_timeout.ack_keep_alive(),
                    RecoveryToken::Datagram(dgram_tracker) => self
                        .events
                        .datagram_outcome(dgram_tracker, OutgoingDatagramOutcome::Acked),
                    // We only worry when these are lost
                    RecoveryToken::HandshakeDone => (),
                }
            }
        }
        self.handle_lost_packets(&lost_packets);
        qlog::packets_lost(&mut self.qlog, &lost_packets);
        let stats = &mut self.stats.borrow_mut().frame_rx;
        stats.ack += 1;
        stats.largest_acknowledged = max(stats.largest_acknowledged, largest_acknowledged);
    }

    /// When the server rejects 0-RTT we need to drop a bunch of stuff.
    fn client_0rtt_rejected(&mut self, now: Instant) {
        if !matches!(self.zero_rtt_state, ZeroRttState::Sending) {
            return;
        }
        qdebug!([self], "0-RTT rejected");

        // Tell 0-RTT packets that they were "lost".
        let dropped = self.loss_recovery.drop_0rtt(&self.paths.primary(), now);
        self.handle_lost_packets(&dropped);

        self.streams.zero_rtt_rejected();

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
            self.cid_manager.remove_odcid();
            // Mark the path as validated, if it isn't already.
            let path = self.paths.primary();
            path.borrow_mut().set_valid(now);
            // Generate a qlog event that the server connection started.
            qlog::server_connection_started(&mut self.qlog, &path);
        } else {
            self.zero_rtt_state = if self.crypto.tls.info().unwrap().early_data_accepted() {
                ZeroRttState::AcceptedClient
            } else {
                self.client_0rtt_rejected(now);
                ZeroRttState::Rejected
            };
        }

        // Setting application keys has to occur after 0-RTT rejection.
        let pto = self.pto();
        self.crypto
            .install_application_keys(self.version, now + pto)?;
        self.process_tps()?;
        self.set_state(State::Connected);
        self.create_resumption_token(now);
        self.saved_datagrams
            .make_available(CryptoSpace::ApplicationData);
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
                self.streams.clear_streams();
            }
            self.events.connection_state_change(state);
            qlog::connection_state_updated(&mut self.qlog, &self.state)
        } else if mem::discriminant(&state) != mem::discriminant(&self.state) {
            // Only tolerate a regression in state if the new state is closing
            // and the connection is already closed.
            debug_assert!(matches!(
                state,
                State::Closing { .. } | State::Draining { .. }
            ));
            debug_assert!(self.state.closed());
        }
    }

    /// Create a stream.
    /// Returns new stream id
    /// # Errors
    /// `ConnectionState` if the connecton stat does not allow to create streams.
    /// `StreamLimitError` if we are limiied by server's stream concurence.
    pub fn stream_create(&mut self, st: StreamType) -> Res<StreamId> {
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

        self.streams.stream_create(st)
    }

    /// Set the priority of a stream.
    /// # Errors
    /// `InvalidStreamId` the stream does not exist.
    pub fn stream_priority(
        &mut self,
        stream_id: StreamId,
        transmission: TransmissionPriority,
        retransmission: RetransmissionPriority,
    ) -> Res<()> {
        self.streams
            .get_send_stream_mut(stream_id)?
            .set_priority(transmission, retransmission);
        Ok(())
    }

    /// Send data on a stream.
    /// Returns how many bytes were successfully sent. Could be less
    /// than total, based on receiver credit space available, etc.
    /// # Errors
    /// `InvalidStreamId` the stream does not exist,
    /// `InvalidInput` if length of `data` is zero,
    /// `FinalSizeError` if the stream has already been closed.
    pub fn stream_send(&mut self, stream_id: StreamId, data: &[u8]) -> Res<usize> {
        self.streams.get_send_stream_mut(stream_id)?.send(data)
    }

    /// Send all data or nothing on a stream. May cause DATA_BLOCKED or
    /// STREAM_DATA_BLOCKED frames to be sent.
    /// Returns true if data was successfully sent, otherwise false.
    /// # Errors
    /// `InvalidStreamId` the stream does not exist,
    /// `InvalidInput` if length of `data` is zero,
    /// `FinalSizeError` if the stream has already been closed.
    pub fn stream_send_atomic(&mut self, stream_id: StreamId, data: &[u8]) -> Res<bool> {
        let val = self
            .streams
            .get_send_stream_mut(stream_id)?
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
    pub fn stream_avail_send_space(&self, stream_id: StreamId) -> Res<usize> {
        Ok(self.streams.get_send_stream(stream_id)?.avail())
    }

    /// Close the stream. Enqueued data will be sent.
    pub fn stream_close_send(&mut self, stream_id: StreamId) -> Res<()> {
        self.streams.get_send_stream_mut(stream_id)?.close();
        Ok(())
    }

    /// Abandon transmission of in-flight and future stream data.
    pub fn stream_reset_send(&mut self, stream_id: StreamId, err: AppError) -> Res<()> {
        self.streams.get_send_stream_mut(stream_id)?.reset(err);
        Ok(())
    }

    /// Read buffered data from stream. bool says whether read bytes includes
    /// the final data on stream.
    /// # Errors
    /// `InvalidStreamId` if the stream does not exist.
    /// `NoMoreData` if data and fin bit were previously read by the application.
    pub fn stream_recv(&mut self, stream_id: StreamId, data: &mut [u8]) -> Res<(usize, bool)> {
        let stream = self.streams.get_recv_stream_mut(stream_id)?;

        let rb = stream.read(data)?;
        Ok((rb.0 as usize, rb.1))
    }

    /// Application is no longer interested in this stream.
    pub fn stream_stop_sending(&mut self, stream_id: StreamId, err: AppError) -> Res<()> {
        let stream = self.streams.get_recv_stream_mut(stream_id)?;

        stream.stop_sending(err);
        Ok(())
    }

    /// Increases `max_stream_data` for a `stream_id`.
    /// # Errors
    /// Returns `InvalidStreamId` if a stream does not exist or the receiving
    /// side is closed.
    pub fn set_stream_max_data(&mut self, stream_id: StreamId, max_data: u64) -> Res<()> {
        let stream = self.streams.get_recv_stream_mut(stream_id)?;

        stream.set_stream_max_data(max_data);
        Ok(())
    }

    /// Mark a receive stream as being important enough to keep the connection alive
    /// (if `keep` is `true`) or no longer important (if `keep` is `false`).  If any
    /// stream is marked this way, PING frames will be used to keep the connection
    /// alive, even when there is no activity.
    /// # Errors
    /// Returns `InvalidStreamId` if a stream does not exist or the receiving
    /// side is closed.
    pub fn stream_keep_alive(&mut self, stream_id: StreamId, keep: bool) -> Res<()> {
        self.streams.keep_alive(stream_id, keep)
    }

    pub fn remote_datagram_size(&self) -> u64 {
        self.quic_datagrams.remote_datagram_size()
    }

    /// Returns the current max size of a datagram that can fit into a packet.
    /// The value will change over time depending on the encoded size of the
    /// packet number, ack frames, etc.
    /// # Error
    /// The function returns `NotAvailable` if datagrams are not enabled.
    pub fn max_datagram_size(&self) -> Res<u64> {
        let max_dgram_size = self.quic_datagrams.remote_datagram_size();
        if max_dgram_size == 0 {
            return Err(Error::NotAvailable);
        }
        let version = self.version();
        let (cspace, tx) = if let Some(crypto) = self
            .crypto
            .states
            .select_tx(self.version, PacketNumberSpace::ApplicationData)
        {
            crypto
        } else {
            return Err(Error::NotAvailable);
        };
        let path = self.paths.primary_fallible().ok_or(Error::NotAvailable)?;
        let mtu = path.borrow().mtu();
        let encoder = Encoder::with_capacity(mtu);

        let (_, mut builder) = Self::build_packet_header(
            &path.borrow(),
            cspace,
            encoder,
            tx,
            &self.address_validation,
            version,
            false,
        );
        let _ = Self::add_packet_number(
            &mut builder,
            tx,
            self.loss_recovery
                .largest_acknowledged_pn(PacketNumberSpace::ApplicationData),
        );

        let data_len_possible =
            u64::try_from(mtu.saturating_sub(tx.expansion() + builder.len() + 1)).unwrap();
        Ok(min(data_len_possible, max_dgram_size))
    }

    /// Queue a datagram for sending.
    /// # Error
    /// The function returns `TooMuchData` if the supply buffer is bigger than
    /// the allowed remote datagram size. The funcion does not check if the
    /// datagram can fit into a packet (i.e. MTU limit). This is checked during
    /// creation of an actual packet and the datagram will be dropped if it does
    /// not fit into the packet. The app is encourage to use `max_datagram_size`
    /// to check the estimated max datagram size and to use smaller datagrams.
    /// `max_datagram_size` is just a current estimate and will change over
    /// time depending on the encoded size of the packet number, ack frames, etc.

    pub fn send_datagram(&mut self, buf: &[u8], id: impl Into<DatagramTracking>) -> Res<()> {
        self.quic_datagrams
            .add_datagram(buf, id.into(), &mut self.stats.borrow_mut())
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
