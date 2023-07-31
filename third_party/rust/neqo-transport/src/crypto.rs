// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    cell::RefCell,
    cmp::{max, min},
    collections::HashMap,
    convert::TryFrom,
    mem,
    ops::{Index, IndexMut, Range},
    rc::Rc,
    time::Instant,
};

use neqo_common::{hex, hex_snip_middle, qdebug, qinfo, qtrace, Encoder, Role};

use neqo_crypto::{
    hkdf, hp::HpKey, Aead, Agent, AntiReplay, Cipher, Epoch, Error as CryptoError, HandshakeState,
    PrivateKey, PublicKey, Record, RecordList, ResumptionToken, SymKey, ZeroRttChecker,
    TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384, TLS_CHACHA20_POLY1305_SHA256, TLS_CT_HANDSHAKE,
    TLS_EPOCH_APPLICATION_DATA, TLS_EPOCH_HANDSHAKE, TLS_EPOCH_INITIAL, TLS_EPOCH_ZERO_RTT,
    TLS_VERSION_1_3,
};

use crate::{
    cid::ConnectionIdRef,
    packet::{PacketBuilder, PacketNumber},
    recovery::RecoveryToken,
    recv_stream::RxStreamOrderer,
    send_stream::TxBuffer,
    stats::FrameStats,
    tparams::{TpZeroRttChecker, TransportParameters, TransportParametersHandler},
    tracking::PacketNumberSpace,
    version::Version,
    Error, Res,
};

const MAX_AUTH_TAG: usize = 32;
/// The number of invocations remaining on a write cipher before we try
/// to update keys.  This has to be much smaller than the number returned
/// by `CryptoDxState::limit` or updates will happen too often.  As we don't
/// need to ask permission to update, this can be quite small.
pub(crate) const UPDATE_WRITE_KEYS_AT: PacketNumber = 100;

// This is a testing kludge that allows for overwriting the number of
// invocations of the next cipher to operate.  With this, it is possible
// to test what happens when the number of invocations reaches 0, or
// when it hits `UPDATE_WRITE_KEYS_AT` and an automatic update should occur.
// This is a little crude, but it saves a lot of plumbing.
#[cfg(test)]
thread_local!(pub(crate) static OVERWRITE_INVOCATIONS: RefCell<Option<PacketNumber>> = RefCell::default());

#[derive(Debug)]
pub struct Crypto {
    version: Version,
    protocols: Vec<String>,
    pub(crate) tls: Agent,
    pub(crate) streams: CryptoStreams,
    pub(crate) states: CryptoStates,
}

type TpHandler = Rc<RefCell<TransportParametersHandler>>;

impl Crypto {
    pub fn new(
        version: Version,
        mut agent: Agent,
        protocols: Vec<String>,
        tphandler: TpHandler,
        fuzzing: bool,
    ) -> Res<Self> {
        agent.set_version_range(TLS_VERSION_1_3, TLS_VERSION_1_3)?;
        agent.set_ciphers(&[
            TLS_AES_128_GCM_SHA256,
            TLS_AES_256_GCM_SHA384,
            TLS_CHACHA20_POLY1305_SHA256,
        ])?;
        agent.set_alpn(&protocols)?;
        agent.disable_end_of_early_data()?;
        // Always enable 0-RTT on the client, but the server needs
        // more configuration passed to server_enable_0rtt.
        if let Agent::Client(c) = &mut agent {
            c.enable_0rtt()?;
        }
        let extension = match version {
            Version::Version2 | Version::Version1 => 0x39,
            Version::Draft29 | Version::Draft30 | Version::Draft31 | Version::Draft32 => 0xffa5,
        };
        agent.extension_handler(extension, tphandler)?;
        Ok(Self {
            version,
            protocols,
            tls: agent,
            streams: Default::default(),
            states: CryptoStates {
                fuzzing,
                ..Default::default()
            },
        })
    }

    /// Get the name of the server.  (Only works for the client currently).
    pub fn server_name(&self) -> Option<&str> {
        if let Agent::Client(c) = &self.tls {
            Some(c.server_name())
        } else {
            None
        }
    }

    /// Get the set of enabled protocols.
    pub fn protocols(&self) -> &[String] {
        &self.protocols
    }

    pub fn server_enable_0rtt(
        &mut self,
        tphandler: TpHandler,
        anti_replay: &AntiReplay,
        zero_rtt_checker: impl ZeroRttChecker + 'static,
    ) -> Res<()> {
        if let Agent::Server(s) = &mut self.tls {
            Ok(s.enable_0rtt(
                anti_replay,
                0xffff_ffff,
                TpZeroRttChecker::wrap(tphandler, zero_rtt_checker),
            )?)
        } else {
            panic!("not a server");
        }
    }

    pub fn server_enable_ech(
        &mut self,
        config: u8,
        public_name: &str,
        sk: &PrivateKey,
        pk: &PublicKey,
    ) -> Res<()> {
        if let Agent::Server(s) = &mut self.tls {
            s.enable_ech(config, public_name, sk, pk)?;
            Ok(())
        } else {
            panic!("not a client");
        }
    }

    pub fn client_enable_ech(&mut self, ech_config_list: impl AsRef<[u8]>) -> Res<()> {
        if let Agent::Client(c) = &mut self.tls {
            c.enable_ech(ech_config_list)?;
            Ok(())
        } else {
            panic!("not a client");
        }
    }

    /// Get the active ECH configuration, which is empty if ECH is disabled.
    pub fn ech_config(&self) -> &[u8] {
        self.tls.ech_config()
    }

    pub fn handshake(
        &mut self,
        now: Instant,
        space: PacketNumberSpace,
        data: Option<&[u8]>,
    ) -> Res<&HandshakeState> {
        let input = data.map(|d| {
            qtrace!("Handshake record received {:0x?} ", d);
            let epoch = match space {
                PacketNumberSpace::Initial => TLS_EPOCH_INITIAL,
                PacketNumberSpace::Handshake => TLS_EPOCH_HANDSHAKE,
                // Our epoch progresses forward, but the TLS epoch is fixed to 3.
                PacketNumberSpace::ApplicationData => TLS_EPOCH_APPLICATION_DATA,
            };
            Record {
                ct: TLS_CT_HANDSHAKE,
                epoch,
                data: d.to_vec(),
            }
        });

        match self.tls.handshake_raw(now, input) {
            Ok(output) => {
                self.buffer_records(output)?;
                Ok(self.tls.state())
            }
            Err(CryptoError::EchRetry(v)) => Err(Error::EchRetry(v)),
            Err(e) => {
                qinfo!("Handshake failed {:?}", e);
                Err(match self.tls.alert() {
                    Some(a) => Error::CryptoAlert(*a),
                    _ => Error::CryptoError(e),
                })
            }
        }
    }

    /// Enable 0-RTT and return `true` if it is enabled successfully.
    pub fn enable_0rtt(&mut self, version: Version, role: Role) -> Res<bool> {
        let info = self.tls.preinfo()?;
        // `info.early_data()` returns false for a server,
        // so use `early_data_cipher()` to tell if 0-RTT is enabled.
        let cipher = info.early_data_cipher();
        if cipher.is_none() {
            return Ok(false);
        }
        let (dir, secret) = match role {
            Role::Client => (
                CryptoDxDirection::Write,
                self.tls.write_secret(TLS_EPOCH_ZERO_RTT),
            ),
            Role::Server => (
                CryptoDxDirection::Read,
                self.tls.read_secret(TLS_EPOCH_ZERO_RTT),
            ),
        };
        let secret = secret.ok_or(Error::InternalError(1))?;
        self.states
            .set_0rtt_keys(version, dir, &secret, cipher.unwrap());
        Ok(true)
    }

    /// Lock in a compatible upgrade.
    pub fn confirm_version(&mut self, confirmed: Version) {
        self.states.confirm_version(self.version, confirmed);
        self.version = confirmed;
    }

    /// Returns true if new handshake keys were installed.
    pub fn install_keys(&mut self, role: Role) -> Res<bool> {
        if !self.tls.state().is_final() {
            let installed_hs = self.install_handshake_keys()?;
            if role == Role::Server {
                self.maybe_install_application_write_key(self.version)?;
            }
            Ok(installed_hs)
        } else {
            Ok(false)
        }
    }

    fn install_handshake_keys(&mut self) -> Res<bool> {
        qtrace!([self], "Attempt to install handshake keys");
        let write_secret = if let Some(secret) = self.tls.write_secret(TLS_EPOCH_HANDSHAKE) {
            secret
        } else {
            // No keys is fine.
            return Ok(false);
        };
        let read_secret = self
            .tls
            .read_secret(TLS_EPOCH_HANDSHAKE)
            .ok_or(Error::InternalError(2))?;
        let cipher = match self.tls.info() {
            None => self.tls.preinfo()?.cipher_suite(),
            Some(info) => Some(info.cipher_suite()),
        }
        .ok_or(Error::InternalError(3))?;
        self.states
            .set_handshake_keys(self.version, &write_secret, &read_secret, cipher);
        qdebug!([self], "Handshake keys installed");
        Ok(true)
    }

    fn maybe_install_application_write_key(&mut self, version: Version) -> Res<()> {
        qtrace!([self], "Attempt to install application write key");
        if let Some(secret) = self.tls.write_secret(TLS_EPOCH_APPLICATION_DATA) {
            self.states.set_application_write_key(version, secret)?;
            qdebug!([self], "Application write key installed");
        }
        Ok(())
    }

    pub fn install_application_keys(&mut self, version: Version, expire_0rtt: Instant) -> Res<()> {
        self.maybe_install_application_write_key(version)?;
        // The write key might have been installed earlier, but it should
        // always be installed now.
        debug_assert!(self.states.app_write.is_some());
        let read_secret = self
            .tls
            .read_secret(TLS_EPOCH_APPLICATION_DATA)
            .ok_or(Error::InternalError(4))?;
        self.states
            .set_application_read_key(version, read_secret, expire_0rtt)?;
        qdebug!([self], "application read keys installed");
        Ok(())
    }

    /// Buffer crypto records for sending.
    pub fn buffer_records(&mut self, records: RecordList) -> Res<()> {
        for r in records {
            if r.ct != TLS_CT_HANDSHAKE {
                return Err(Error::ProtocolViolation);
            }
            qtrace!([self], "Adding CRYPTO data {:?}", r);
            self.streams.send(PacketNumberSpace::from(r.epoch), &r.data);
        }
        Ok(())
    }

    pub fn write_frame(
        &mut self,
        space: PacketNumberSpace,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> Res<()> {
        self.streams.write_frame(space, builder, tokens, stats)
    }

    pub fn acked(&mut self, token: &CryptoRecoveryToken) {
        qinfo!(
            "Acked crypto frame space={} offset={} length={}",
            token.space,
            token.offset,
            token.length
        );
        self.streams.acked(token);
    }

    pub fn lost(&mut self, token: &CryptoRecoveryToken) {
        qinfo!(
            "Lost crypto frame space={} offset={} length={}",
            token.space,
            token.offset,
            token.length
        );
        self.streams.lost(token);
    }

    /// Mark any outstanding frames in the indicated space as "lost" so
    /// that they can be sent again.
    pub fn resend_unacked(&mut self, space: PacketNumberSpace) {
        self.streams.resend_unacked(space);
    }

    /// Discard state for a packet number space and return true
    /// if something was discarded.
    pub fn discard(&mut self, space: PacketNumberSpace) -> bool {
        self.streams.discard(space);
        self.states.discard(space)
    }

    pub fn create_resumption_token(
        &mut self,
        new_token: Option<&[u8]>,
        tps: &TransportParameters,
        version: Version,
        rtt: u64,
    ) -> Option<ResumptionToken> {
        if let Agent::Client(ref mut c) = self.tls {
            if let Some(ref t) = c.resumption_token() {
                qtrace!("TLS token {}", hex(t.as_ref()));
                let mut enc = Encoder::default();
                enc.encode_uint(4, version.wire_version());
                enc.encode_varint(rtt);
                enc.encode_vvec_with(|enc_inner| {
                    tps.encode(enc_inner);
                });
                enc.encode_vvec(new_token.unwrap_or(&[]));
                enc.encode(t.as_ref());
                qinfo!("resumption token {}", hex_snip_middle(enc.as_ref()));
                Some(ResumptionToken::new(enc.into(), t.expiration_time()))
            } else {
                None
            }
        } else {
            unreachable!("It is a server.");
        }
    }

    pub fn has_resumption_token(&self) -> bool {
        if let Agent::Client(c) = &self.tls {
            c.has_resumption_token()
        } else {
            unreachable!("It is a server.");
        }
    }
}

impl ::std::fmt::Display for Crypto {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "Crypto")
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CryptoDxDirection {
    Read,
    Write,
}

#[derive(Debug)]
pub struct CryptoDxState {
    /// The QUIC version.
    version: Version,
    /// Whether packets protected with this state will be read or written.
    direction: CryptoDxDirection,
    /// The epoch of this crypto state.  This initially tracks TLS epochs
    /// via DTLS: 0 = initial, 1 = 0-RTT, 2 = handshake, 3 = application.
    /// But we don't need to keep that, and QUIC isn't limited in how
    /// many times keys can be updated, so we don't use `u16` for this.
    epoch: usize,
    aead: Aead,
    hpkey: HpKey,
    /// This tracks the range of packet numbers that have been seen.  This allows
    /// for verifying that packet numbers before a key update are strictly lower
    /// than packet numbers after a key update.
    used_pn: Range<PacketNumber>,
    /// This is the minimum packet number that is allowed.
    min_pn: PacketNumber,
    /// The total number of operations that are remaining before the keys
    /// become exhausted and can't be used any more.
    invocations: PacketNumber,
    fuzzing: bool,
}

impl CryptoDxState {
    #[allow(clippy::reversed_empty_ranges)] // To initialize an empty range.
    pub fn new(
        version: Version,
        direction: CryptoDxDirection,
        epoch: Epoch,
        secret: &SymKey,
        cipher: Cipher,
        fuzzing: bool,
    ) -> Self {
        qinfo!(
            "Making {:?} {} CryptoDxState, v={:?} cipher={}",
            direction,
            epoch,
            version,
            cipher,
        );
        let hplabel = String::from(version.label_prefix()) + "hp";
        Self {
            version,
            direction,
            epoch: usize::from(epoch),
            aead: Aead::new(
                fuzzing,
                TLS_VERSION_1_3,
                cipher,
                secret,
                version.label_prefix(),
            )
            .unwrap(),
            hpkey: HpKey::extract(TLS_VERSION_1_3, cipher, secret, &hplabel).unwrap(),
            used_pn: 0..0,
            min_pn: 0,
            invocations: Self::limit(direction, cipher),
            fuzzing,
        }
    }

    pub fn new_initial(
        version: Version,
        direction: CryptoDxDirection,
        label: &str,
        dcid: &[u8],
        fuzzing: bool,
    ) -> Self {
        qtrace!("new_initial {:?} {}", version, ConnectionIdRef::from(dcid));
        let salt = version.initial_salt();
        let cipher = TLS_AES_128_GCM_SHA256;
        let initial_secret = hkdf::extract(
            TLS_VERSION_1_3,
            cipher,
            Some(hkdf::import_key(TLS_VERSION_1_3, salt).as_ref().unwrap()),
            hkdf::import_key(TLS_VERSION_1_3, dcid).as_ref().unwrap(),
        )
        .unwrap();

        let secret =
            hkdf::expand_label(TLS_VERSION_1_3, cipher, &initial_secret, &[], label).unwrap();

        Self::new(
            version,
            direction,
            TLS_EPOCH_INITIAL,
            &secret,
            cipher,
            fuzzing,
        )
    }

    /// Determine the confidentiality and integrity limits for the cipher.
    fn limit(direction: CryptoDxDirection, cipher: Cipher) -> PacketNumber {
        match direction {
            // This uses the smaller limits for 2^16 byte packets
            // as we don't control incoming packet size.
            CryptoDxDirection::Read => match cipher {
                TLS_AES_128_GCM_SHA256 => 1 << 52,
                TLS_AES_256_GCM_SHA384 => PacketNumber::MAX,
                TLS_CHACHA20_POLY1305_SHA256 => 1 << 36,
                _ => unreachable!(),
            },
            // This uses the larger limits for 2^11 byte packets.
            CryptoDxDirection::Write => match cipher {
                TLS_AES_128_GCM_SHA256 | TLS_AES_256_GCM_SHA384 => 1 << 28,
                TLS_CHACHA20_POLY1305_SHA256 => PacketNumber::MAX,
                _ => unreachable!(),
            },
        }
    }

    fn invoked(&mut self) -> Res<()> {
        #[cfg(test)]
        OVERWRITE_INVOCATIONS.with(|v| {
            if let Some(i) = v.borrow_mut().take() {
                neqo_common::qwarn!("Setting {:?} invocations to {}", self.direction, i);
                self.invocations = i;
            }
        });
        self.invocations = self
            .invocations
            .checked_sub(1)
            .ok_or(Error::KeysExhausted)?;
        Ok(())
    }

    /// Determine whether we should initiate a key update.
    pub fn should_update(&self) -> bool {
        // There is no point in updating read keys as the limit is global.
        debug_assert_eq!(self.direction, CryptoDxDirection::Write);
        self.invocations <= UPDATE_WRITE_KEYS_AT
    }

    pub fn next(&self, next_secret: &SymKey, cipher: Cipher) -> Self {
        let pn = self.next_pn();
        // We count invocations of each write key just for that key, but all
        // attempts to invocations to read count toward a single limit.
        // This doesn't count use of Handshake keys.
        let invocations = if self.direction == CryptoDxDirection::Read {
            self.invocations
        } else {
            Self::limit(CryptoDxDirection::Write, cipher)
        };
        Self {
            version: self.version,
            direction: self.direction,
            epoch: self.epoch + 1,
            aead: Aead::new(
                self.fuzzing,
                TLS_VERSION_1_3,
                cipher,
                next_secret,
                self.version.label_prefix(),
            )
            .unwrap(),
            hpkey: self.hpkey.clone(),
            used_pn: pn..pn,
            min_pn: pn,
            invocations,
            fuzzing: self.fuzzing,
        }
    }

    #[must_use]
    pub fn version(&self) -> Version {
        self.version
    }

    #[must_use]
    pub fn key_phase(&self) -> bool {
        // Epoch 3 => 0, 4 => 1, 5 => 0, 6 => 1, ...
        self.epoch & 1 != 1
    }

    /// This is a continuation of a previous, so adjust the range accordingly.
    /// Fail if the two ranges overlap.  Do nothing if the directions don't match.
    pub fn continuation(&mut self, prev: &Self) -> Res<()> {
        debug_assert_eq!(self.direction, prev.direction);
        let next = prev.next_pn();
        self.min_pn = next;
        if self.used_pn.is_empty() {
            self.used_pn = next..next;
            Ok(())
        } else if prev.used_pn.end > self.used_pn.start {
            qdebug!(
                [self],
                "Found packet with too new packet number {} > {}, compared to {}",
                self.used_pn.start,
                prev.used_pn.end,
                prev,
            );
            Err(Error::PacketNumberOverlap)
        } else {
            self.used_pn.start = next;
            Ok(())
        }
    }

    /// Mark a packet number as used.  If this is too low, reject it.
    /// Note that this won't catch a value that is too high if packets protected with
    /// old keys are received after a key update.  That needs to be caught elsewhere.
    pub fn used(&mut self, pn: PacketNumber) -> Res<()> {
        if pn < self.min_pn {
            qdebug!(
                [self],
                "Found packet with too old packet number: {} < {}",
                pn,
                self.min_pn
            );
            return Err(Error::PacketNumberOverlap);
        }
        if self.used_pn.start == self.used_pn.end {
            self.used_pn.start = pn;
        }
        self.used_pn.end = max(pn + 1, self.used_pn.end);
        Ok(())
    }

    #[must_use]
    pub fn needs_update(&self) -> bool {
        // Only initiate a key update if we have processed exactly one packet
        // and we are in an epoch greater than 3.
        self.used_pn.start + 1 == self.used_pn.end
            && self.epoch > usize::from(TLS_EPOCH_APPLICATION_DATA)
    }

    #[must_use]
    pub fn can_update(&self, largest_acknowledged: Option<PacketNumber>) -> bool {
        if let Some(la) = largest_acknowledged {
            self.used_pn.contains(&la)
        } else {
            // If we haven't received any acknowledgments, it's OK to update
            // the first application data epoch.
            self.epoch == usize::from(TLS_EPOCH_APPLICATION_DATA)
        }
    }

    pub fn compute_mask(&self, sample: &[u8]) -> Res<Vec<u8>> {
        let mask = self.hpkey.mask(sample)?;
        qtrace!([self], "HP sample={} mask={}", hex(sample), hex(&mask));
        Ok(mask)
    }

    #[must_use]
    pub fn next_pn(&self) -> PacketNumber {
        self.used_pn.end
    }

    pub fn encrypt(&mut self, pn: PacketNumber, hdr: &[u8], body: &[u8]) -> Res<Vec<u8>> {
        debug_assert_eq!(self.direction, CryptoDxDirection::Write);
        qtrace!(
            [self],
            "encrypt pn={} hdr={} body={}",
            pn,
            hex(hdr),
            hex(body)
        );
        // The numbers in `Self::limit` assume a maximum packet size of 2^11.
        if body.len() > 2048 {
            debug_assert!(false);
            return Err(Error::InternalError(12));
        }
        self.invoked()?;

        let size = body.len() + MAX_AUTH_TAG;
        let mut out = vec![0; size];
        let res = self.aead.encrypt(pn, hdr, body, &mut out)?;

        qtrace!([self], "encrypt ct={}", hex(res));
        debug_assert_eq!(pn, self.next_pn());
        self.used(pn)?;
        Ok(res.to_vec())
    }

    #[must_use]
    pub fn expansion(&self) -> usize {
        self.aead.expansion()
    }

    pub fn decrypt(&mut self, pn: PacketNumber, hdr: &[u8], body: &[u8]) -> Res<Vec<u8>> {
        debug_assert_eq!(self.direction, CryptoDxDirection::Read);
        qtrace!(
            [self],
            "decrypt pn={} hdr={} body={}",
            pn,
            hex(hdr),
            hex(body)
        );
        self.invoked()?;
        let mut out = vec![0; body.len()];
        let res = self.aead.decrypt(pn, hdr, body, &mut out)?;
        self.used(pn)?;
        Ok(res.to_vec())
    }

    #[cfg(all(test, not(feature = "fuzzing")))]
    pub(crate) fn test_default() -> Self {
        // This matches the value in packet.rs
        const CLIENT_CID: &[u8] = &[0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08];
        Self::new_initial(
            Version::default(),
            CryptoDxDirection::Write,
            "server in",
            CLIENT_CID,
            false,
        )
    }

    /// Get the amount of extra padding packets protected with this profile need.
    /// This is the difference between the size of the header protection sample
    /// and the AEAD expansion.
    pub fn extra_padding(&self) -> usize {
        self.hpkey
            .sample_size()
            .saturating_sub(self.aead.expansion())
    }
}

impl std::fmt::Display for CryptoDxState {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "epoch {} {:?}", self.epoch, self.direction)
    }
}

#[derive(Debug)]
pub struct CryptoState {
    tx: CryptoDxState,
    rx: CryptoDxState,
}

impl Index<CryptoDxDirection> for CryptoState {
    type Output = CryptoDxState;

    fn index(&self, dir: CryptoDxDirection) -> &Self::Output {
        match dir {
            CryptoDxDirection::Read => &self.rx,
            CryptoDxDirection::Write => &self.tx,
        }
    }
}

impl IndexMut<CryptoDxDirection> for CryptoState {
    fn index_mut(&mut self, dir: CryptoDxDirection) -> &mut Self::Output {
        match dir {
            CryptoDxDirection::Read => &mut self.rx,
            CryptoDxDirection::Write => &mut self.tx,
        }
    }
}

/// `CryptoDxAppData` wraps the state necessary for one direction of application data keys.
/// This includes the secret needed to generate the next set of keys.
#[derive(Debug)]
pub(crate) struct CryptoDxAppData {
    dx: CryptoDxState,
    cipher: Cipher,
    // Not the secret used to create `self.dx`, but the one needed for the next iteration.
    next_secret: SymKey,
    fuzzing: bool,
}

impl CryptoDxAppData {
    pub fn new(
        version: Version,
        dir: CryptoDxDirection,
        secret: SymKey,
        cipher: Cipher,
        fuzzing: bool,
    ) -> Res<Self> {
        Ok(Self {
            dx: CryptoDxState::new(
                version,
                dir,
                TLS_EPOCH_APPLICATION_DATA,
                &secret,
                cipher,
                fuzzing,
            ),
            cipher,
            next_secret: Self::update_secret(cipher, &secret)?,
            fuzzing,
        })
    }

    fn update_secret(cipher: Cipher, secret: &SymKey) -> Res<SymKey> {
        let next = hkdf::expand_label(TLS_VERSION_1_3, cipher, secret, &[], "quic ku")?;
        Ok(next)
    }

    pub fn next(&self) -> Res<Self> {
        if self.dx.epoch == usize::max_value() {
            // Guard against too many key updates.
            return Err(Error::KeysExhausted);
        }
        let next_secret = Self::update_secret(self.cipher, &self.next_secret)?;
        Ok(Self {
            dx: self.dx.next(&self.next_secret, self.cipher),
            cipher: self.cipher,
            next_secret,
            fuzzing: self.fuzzing,
        })
    }

    pub fn epoch(&self) -> usize {
        self.dx.epoch
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub enum CryptoSpace {
    Initial,
    ZeroRtt,
    Handshake,
    ApplicationData,
}

/// All of the keying material needed for a connection.
///
/// Note that the methods on this struct take a version but those are only ever
/// used for Initial keys; a version has been selected at the time we need to
/// get other keys, so those have fixed versions.
#[derive(Debug, Default)]
pub struct CryptoStates {
    initials: HashMap<Version, CryptoState>,
    handshake: Option<CryptoState>,
    zero_rtt: Option<CryptoDxState>, // One direction only!
    cipher: Cipher,
    app_write: Option<CryptoDxAppData>,
    app_read: Option<CryptoDxAppData>,
    app_read_next: Option<CryptoDxAppData>,
    // If this is set, then we have noticed a genuine update.
    // Once this time passes, we should switch in new keys.
    read_update_time: Option<Instant>,
    fuzzing: bool,
}

impl CryptoStates {
    /// Select a `CryptoDxState` and `CryptoSpace` for the given `PacketNumberSpace`.
    /// This selects 0-RTT keys for `PacketNumberSpace::ApplicationData` if 1-RTT keys are
    /// not yet available.
    pub fn select_tx_mut(
        &mut self,
        version: Version,
        space: PacketNumberSpace,
    ) -> Option<(CryptoSpace, &mut CryptoDxState)> {
        match space {
            PacketNumberSpace::Initial => self
                .tx_mut(version, CryptoSpace::Initial)
                .map(|dx| (CryptoSpace::Initial, dx)),
            PacketNumberSpace::Handshake => self
                .tx_mut(version, CryptoSpace::Handshake)
                .map(|dx| (CryptoSpace::Handshake, dx)),
            PacketNumberSpace::ApplicationData => {
                if let Some(app) = self.app_write.as_mut() {
                    Some((CryptoSpace::ApplicationData, &mut app.dx))
                } else {
                    self.zero_rtt.as_mut().map(|dx| (CryptoSpace::ZeroRtt, dx))
                }
            }
        }
    }

    pub fn tx_mut<'a>(
        &'a mut self,
        version: Version,
        cspace: CryptoSpace,
    ) -> Option<&'a mut CryptoDxState> {
        let tx = |k: Option<&'a mut CryptoState>| k.map(|dx| &mut dx.tx);
        match cspace {
            CryptoSpace::Initial => tx(self.initials.get_mut(&version)),
            CryptoSpace::ZeroRtt => self
                .zero_rtt
                .as_mut()
                .filter(|z| z.direction == CryptoDxDirection::Write),
            CryptoSpace::Handshake => tx(self.handshake.as_mut()),
            CryptoSpace::ApplicationData => self.app_write.as_mut().map(|app| &mut app.dx),
        }
    }

    pub fn tx<'a>(&'a self, version: Version, cspace: CryptoSpace) -> Option<&'a CryptoDxState> {
        let tx = |k: Option<&'a CryptoState>| k.map(|dx| &dx.tx);
        match cspace {
            CryptoSpace::Initial => tx(self.initials.get(&version)),
            CryptoSpace::ZeroRtt => self
                .zero_rtt
                .as_ref()
                .filter(|z| z.direction == CryptoDxDirection::Write),
            CryptoSpace::Handshake => tx(self.handshake.as_ref()),
            CryptoSpace::ApplicationData => self.app_write.as_ref().map(|app| &app.dx),
        }
    }

    pub fn select_tx(
        &self,
        version: Version,
        space: PacketNumberSpace,
    ) -> Option<(CryptoSpace, &CryptoDxState)> {
        match space {
            PacketNumberSpace::Initial => self
                .tx(version, CryptoSpace::Initial)
                .map(|dx| (CryptoSpace::Initial, dx)),
            PacketNumberSpace::Handshake => self
                .tx(version, CryptoSpace::Handshake)
                .map(|dx| (CryptoSpace::Handshake, dx)),
            PacketNumberSpace::ApplicationData => {
                if let Some(app) = self.app_write.as_ref() {
                    Some((CryptoSpace::ApplicationData, &app.dx))
                } else {
                    self.zero_rtt.as_ref().map(|dx| (CryptoSpace::ZeroRtt, dx))
                }
            }
        }
    }

    pub fn rx_hp(&mut self, version: Version, cspace: CryptoSpace) -> Option<&mut CryptoDxState> {
        if let CryptoSpace::ApplicationData = cspace {
            self.app_read.as_mut().map(|ar| &mut ar.dx)
        } else {
            self.rx(version, cspace, false)
        }
    }

    pub fn rx<'a>(
        &'a mut self,
        version: Version,
        cspace: CryptoSpace,
        key_phase: bool,
    ) -> Option<&'a mut CryptoDxState> {
        let rx = |x: Option<&'a mut CryptoState>| x.map(|dx| &mut dx.rx);
        match cspace {
            CryptoSpace::Initial => rx(self.initials.get_mut(&version)),
            CryptoSpace::ZeroRtt => self
                .zero_rtt
                .as_mut()
                .filter(|z| z.direction == CryptoDxDirection::Read),
            CryptoSpace::Handshake => rx(self.handshake.as_mut()),
            CryptoSpace::ApplicationData => {
                let f = |a: Option<&'a mut CryptoDxAppData>| {
                    a.filter(|ar| ar.dx.key_phase() == key_phase)
                };
                // XOR to reduce the leakage about which key is chosen.
                f(self.app_read.as_mut())
                    .xor(f(self.app_read_next.as_mut()))
                    .map(|ar| &mut ar.dx)
            }
        }
    }

    /// Whether keys for processing packets in the indicated space are pending.
    /// This allows the caller to determine whether to save a packet for later
    /// when keys are not available.
    /// NOTE: 0-RTT keys are not considered here.  The expectation is that a
    /// server will have to save 0-RTT packets in a different place.  Though it
    /// is possible to attribute 0-RTT packets to an existing connection if there
    /// is a multi-packet Initial, that is an unusual circumstance, so we
    /// don't do caching for that in those places that call this function.
    pub fn rx_pending(&self, space: CryptoSpace) -> bool {
        match space {
            CryptoSpace::Initial | CryptoSpace::ZeroRtt => false,
            CryptoSpace::Handshake => self.handshake.is_none() && !self.initials.is_empty(),
            CryptoSpace::ApplicationData => self.app_read.is_none(),
        }
    }

    /// Create the initial crypto state.
    /// Note that the version here can change and that's OK.
    pub fn init<'v, V>(&mut self, versions: V, role: Role, dcid: &[u8])
    where
        V: IntoIterator<Item = &'v Version>,
    {
        const CLIENT_INITIAL_LABEL: &str = "client in";
        const SERVER_INITIAL_LABEL: &str = "server in";

        let (write, read) = match role {
            Role::Client => (CLIENT_INITIAL_LABEL, SERVER_INITIAL_LABEL),
            Role::Server => (SERVER_INITIAL_LABEL, CLIENT_INITIAL_LABEL),
        };

        for v in versions {
            qinfo!(
                [self],
                "Creating initial cipher state v={:?}, role={:?} dcid={}",
                v,
                role,
                hex(dcid)
            );

            let mut initial = CryptoState {
                tx: CryptoDxState::new_initial(
                    *v,
                    CryptoDxDirection::Write,
                    write,
                    dcid,
                    self.fuzzing,
                ),
                rx: CryptoDxState::new_initial(
                    *v,
                    CryptoDxDirection::Read,
                    read,
                    dcid,
                    self.fuzzing,
                ),
            };
            if let Some(prev) = self.initials.get(v) {
                qinfo!(
                    [self],
                    "Continue packet numbers for initial after retry (write is {:?})",
                    prev.rx.used_pn,
                );
                initial.tx.continuation(&prev.tx).unwrap();
            }
            self.initials.insert(*v, initial);
        }
    }

    /// At a server, we can be more targeted in initializing.
    /// Initialize on demand: either to decrypt Initial packets that we receive
    /// or after a version has been selected.
    /// This is maybe slightly inefficient in the first case, because we might
    /// not need the send keys if the packet is subsequently discarded, but
    /// the overall effort is small enough to write off.
    pub fn init_server(&mut self, version: Version, dcid: &[u8]) {
        if !self.initials.contains_key(&version) {
            self.init(&[version], Role::Server, dcid);
        }
    }

    pub fn confirm_version(&mut self, orig: Version, confirmed: Version) {
        if orig != confirmed {
            // This part where the old data is removed and then re-added is to
            // appease the borrow checker.
            // Note that on the server, we might not have initials for |orig| if it
            // was configured for |orig| and only |confirmed| Initial packets arrived.
            if let Some(prev) = self.initials.remove(&orig) {
                let next = self.initials.get_mut(&confirmed).unwrap();
                next.tx.continuation(&prev.tx).unwrap();
                self.initials.insert(orig, prev);
            }
        }
    }

    pub fn set_0rtt_keys(
        &mut self,
        version: Version,
        dir: CryptoDxDirection,
        secret: &SymKey,
        cipher: Cipher,
    ) {
        qtrace!([self], "install 0-RTT keys");
        self.zero_rtt = Some(CryptoDxState::new(
            version,
            dir,
            TLS_EPOCH_ZERO_RTT,
            secret,
            cipher,
            self.fuzzing,
        ));
    }

    /// Discard keys and return true if that happened.
    pub fn discard(&mut self, space: PacketNumberSpace) -> bool {
        match space {
            PacketNumberSpace::Initial => {
                let empty = self.initials.is_empty();
                self.initials.clear();
                !empty
            }
            PacketNumberSpace::Handshake => self.handshake.take().is_some(),
            PacketNumberSpace::ApplicationData => panic!("Can't drop application data keys"),
        }
    }

    pub fn discard_0rtt_keys(&mut self) {
        qtrace!([self], "discard 0-RTT keys");
        assert!(
            self.app_read.is_none(),
            "Can't discard 0-RTT after setting application keys"
        );
        self.zero_rtt = None;
    }

    pub fn set_handshake_keys(
        &mut self,
        version: Version,
        write_secret: &SymKey,
        read_secret: &SymKey,
        cipher: Cipher,
    ) {
        self.cipher = cipher;
        self.handshake = Some(CryptoState {
            tx: CryptoDxState::new(
                version,
                CryptoDxDirection::Write,
                TLS_EPOCH_HANDSHAKE,
                write_secret,
                cipher,
                self.fuzzing,
            ),
            rx: CryptoDxState::new(
                version,
                CryptoDxDirection::Read,
                TLS_EPOCH_HANDSHAKE,
                read_secret,
                cipher,
                self.fuzzing,
            ),
        });
    }

    pub fn set_application_write_key(&mut self, version: Version, secret: SymKey) -> Res<()> {
        debug_assert!(self.app_write.is_none());
        debug_assert_ne!(self.cipher, 0);
        let mut app = CryptoDxAppData::new(
            version,
            CryptoDxDirection::Write,
            secret,
            self.cipher,
            self.fuzzing,
        )?;
        if let Some(z) = &self.zero_rtt {
            if z.direction == CryptoDxDirection::Write {
                app.dx.continuation(z)?;
            }
        }
        self.zero_rtt = None;
        self.app_write = Some(app);
        Ok(())
    }

    pub fn set_application_read_key(
        &mut self,
        version: Version,
        secret: SymKey,
        expire_0rtt: Instant,
    ) -> Res<()> {
        debug_assert!(self.app_write.is_some(), "should have write keys installed");
        debug_assert!(self.app_read.is_none());
        let mut app = CryptoDxAppData::new(
            version,
            CryptoDxDirection::Read,
            secret,
            self.cipher,
            self.fuzzing,
        )?;
        if let Some(z) = &self.zero_rtt {
            if z.direction == CryptoDxDirection::Read {
                app.dx.continuation(z)?;
            }
            self.read_update_time = Some(expire_0rtt);
        }
        self.app_read_next = Some(app.next()?);
        self.app_read = Some(app);
        Ok(())
    }

    /// Update the write keys.
    pub fn initiate_key_update(&mut self, largest_acknowledged: Option<PacketNumber>) -> Res<()> {
        // Only update if we are able to. We can only do this if we have
        // received an acknowledgement for a packet in the current phase.
        // Also, skip this if we are waiting for read keys on the existing
        // key update to be rolled over.
        let write = &self.app_write.as_ref().unwrap().dx;
        if write.can_update(largest_acknowledged) && self.read_update_time.is_none() {
            // This call additionally checks that we don't advance to the next
            // epoch while a key update is in progress.
            if self.maybe_update_write()? {
                Ok(())
            } else {
                qdebug!([self], "Write keys already updated");
                Err(Error::KeyUpdateBlocked)
            }
        } else {
            qdebug!([self], "Waiting for ACK or blocked on read key timer");
            Err(Error::KeyUpdateBlocked)
        }
    }

    /// Try to update, and return true if it happened.
    fn maybe_update_write(&mut self) -> Res<bool> {
        // Update write keys.  But only do so if the write keys are not already
        // ahead of the read keys.  If we initiated the key update, the write keys
        // will already be ahead.
        debug_assert!(self.read_update_time.is_none());
        let write = &self.app_write.as_ref().unwrap();
        let read = &self.app_read.as_ref().unwrap();
        if write.epoch() == read.epoch() {
            qdebug!([self], "Update write keys to epoch={}", write.epoch() + 1);
            self.app_write = Some(write.next()?);
            Ok(true)
        } else {
            Ok(false)
        }
    }

    /// Check whether write keys are close to running out of invocations.
    /// If that is close, update them if possible.  Failing to update at
    /// this stage is cause for a fatal error.
    pub fn auto_update(&mut self) -> Res<()> {
        if let Some(app_write) = self.app_write.as_ref() {
            if app_write.dx.should_update() {
                qinfo!([self], "Initiating automatic key update");
                if !self.maybe_update_write()? {
                    return Err(Error::KeysExhausted);
                }
            }
        }
        Ok(())
    }

    fn has_0rtt_read(&self) -> bool {
        self.zero_rtt
            .as_ref()
            .filter(|z| z.direction == CryptoDxDirection::Read)
            .is_some()
    }

    /// Prepare to update read keys.  This doesn't happen immediately as
    /// we want to ensure that we can continue to receive any delayed
    /// packets that use the old keys.  So we just set a timer.
    pub fn key_update_received(&mut self, expiration: Instant) -> Res<()> {
        qtrace!([self], "Key update received");
        // If we received a key update, then we assume that the peer has
        // acknowledged a packet we sent in this epoch. It's OK to do that
        // because they aren't allowed to update without first having received
        // something from us. If the ACK isn't in the packet that triggered this
        // key update, it must be in some other packet they have sent.
        _ = self.maybe_update_write()?;

        // We shouldn't have 0-RTT keys at this point, but if we do, dump them.
        debug_assert_eq!(self.read_update_time.is_some(), self.has_0rtt_read());
        if self.has_0rtt_read() {
            self.zero_rtt = None;
        }
        self.read_update_time = Some(expiration);
        Ok(())
    }

    #[must_use]
    pub fn update_time(&self) -> Option<Instant> {
        self.read_update_time
    }

    /// Check if time has passed for updating key update parameters.
    /// If it has, then swap keys over and allow more key updates to be initiated.
    /// This is also used to discard 0-RTT read keys at the server in the same way.
    pub fn check_key_update(&mut self, now: Instant) -> Res<()> {
        if let Some(expiry) = self.read_update_time {
            // If enough time has passed, then install new keys and clear the timer.
            if now >= expiry {
                if self.has_0rtt_read() {
                    qtrace!([self], "Discarding 0-RTT keys");
                    self.zero_rtt = None;
                } else {
                    qtrace!([self], "Rotating read keys");
                    mem::swap(&mut self.app_read, &mut self.app_read_next);
                    self.app_read_next = Some(self.app_read.as_ref().unwrap().next()?);
                }
                self.read_update_time = None;
            }
        }
        Ok(())
    }

    /// Get the current/highest epoch.  This returns (write, read) epochs.
    #[cfg(test)]
    pub fn get_epochs(&self) -> (Option<usize>, Option<usize>) {
        let to_epoch = |app: &Option<CryptoDxAppData>| app.as_ref().map(|a| a.dx.epoch);
        (to_epoch(&self.app_write), to_epoch(&self.app_read))
    }

    /// While we are awaiting the completion of a key update, we might receive
    /// valid packets that are protected with old keys. We need to ensure that
    /// these don't carry packet numbers higher than those in packets protected
    /// with the newer keys.  To ensure that, this is called after every decryption.
    pub fn check_pn_overlap(&mut self) -> Res<()> {
        // We only need to do the check while we are waiting for read keys to be updated.
        if self.read_update_time.is_some() {
            qtrace!([self], "Checking for PN overlap");
            let next_dx = &mut self.app_read_next.as_mut().unwrap().dx;
            next_dx.continuation(&self.app_read.as_ref().unwrap().dx)?;
        }
        Ok(())
    }

    /// Make some state for removing protection in tests.
    #[cfg(not(feature = "fuzzing"))]
    #[cfg(test)]
    pub(crate) fn test_default() -> Self {
        let read = |epoch| {
            let mut dx = CryptoDxState::test_default();
            dx.direction = CryptoDxDirection::Read;
            dx.epoch = epoch;
            dx
        };
        let app_read = |epoch| CryptoDxAppData {
            dx: read(epoch),
            cipher: TLS_AES_128_GCM_SHA256,
            next_secret: hkdf::import_key(TLS_VERSION_1_3, &[0xaa; 32]).unwrap(),
            fuzzing: false,
        };
        let mut initials = HashMap::new();
        initials.insert(
            Version::Version1,
            CryptoState {
                tx: CryptoDxState::test_default(),
                rx: read(0),
            },
        );
        Self {
            initials,
            handshake: None,
            zero_rtt: None,
            cipher: TLS_AES_128_GCM_SHA256,
            // This isn't used, but the epoch is read to check for a key update.
            app_write: Some(app_read(3)),
            app_read: Some(app_read(3)),
            app_read_next: Some(app_read(4)),
            read_update_time: None,
            fuzzing: false,
        }
    }

    #[cfg(all(not(feature = "fuzzing"), test))]
    pub(crate) fn test_chacha() -> Self {
        const SECRET: &[u8] = &[
            0x9a, 0xc3, 0x12, 0xa7, 0xf8, 0x77, 0x46, 0x8e, 0xbe, 0x69, 0x42, 0x27, 0x48, 0xad,
            0x00, 0xa1, 0x54, 0x43, 0xf1, 0x82, 0x03, 0xa0, 0x7d, 0x60, 0x60, 0xf6, 0x88, 0xf3,
            0x0f, 0x21, 0x63, 0x2b,
        ];
        let secret = hkdf::import_key(TLS_VERSION_1_3, SECRET).unwrap();
        let app_read = |epoch| CryptoDxAppData {
            dx: CryptoDxState {
                version: Version::Version1,
                direction: CryptoDxDirection::Read,
                epoch,
                aead: Aead::new(
                    false,
                    TLS_VERSION_1_3,
                    TLS_CHACHA20_POLY1305_SHA256,
                    &secret,
                    "quic ", // This is a v1 test so hard-code the label.
                )
                .unwrap(),
                hpkey: HpKey::extract(
                    TLS_VERSION_1_3,
                    TLS_CHACHA20_POLY1305_SHA256,
                    &secret,
                    "quic hp",
                )
                .unwrap(),
                used_pn: 0..645_971_972,
                min_pn: 0,
                invocations: 10,
                fuzzing: false,
            },
            cipher: TLS_CHACHA20_POLY1305_SHA256,
            next_secret: secret.clone(),
            fuzzing: false,
        };
        Self {
            initials: HashMap::new(),
            handshake: None,
            zero_rtt: None,
            cipher: TLS_CHACHA20_POLY1305_SHA256,
            app_write: Some(app_read(3)),
            app_read: Some(app_read(3)),
            app_read_next: Some(app_read(4)),
            read_update_time: None,
            fuzzing: false,
        }
    }
}

impl std::fmt::Display for CryptoStates {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        write!(f, "CryptoStates")
    }
}

#[derive(Debug, Default)]
pub struct CryptoStream {
    tx: TxBuffer,
    rx: RxStreamOrderer,
}

#[derive(Debug)]
#[allow(dead_code)] // Suppress false positive: https://github.com/rust-lang/rust/issues/68408
pub enum CryptoStreams {
    Initial {
        initial: CryptoStream,
        handshake: CryptoStream,
        application: CryptoStream,
    },
    Handshake {
        handshake: CryptoStream,
        application: CryptoStream,
    },
    ApplicationData {
        application: CryptoStream,
    },
}

impl CryptoStreams {
    pub fn discard(&mut self, space: PacketNumberSpace) {
        match space {
            PacketNumberSpace::Initial => {
                if let Self::Initial {
                    handshake,
                    application,
                    ..
                } = self
                {
                    *self = Self::Handshake {
                        handshake: mem::take(handshake),
                        application: mem::take(application),
                    };
                }
            }
            PacketNumberSpace::Handshake => {
                if let Self::Handshake { application, .. } = self {
                    *self = Self::ApplicationData {
                        application: mem::take(application),
                    };
                } else if matches!(self, Self::Initial { .. }) {
                    panic!("Discarding handshake before initial discarded");
                }
            }
            PacketNumberSpace::ApplicationData => {
                panic!("Discarding application data crypto streams")
            }
        }
    }

    pub fn send(&mut self, space: PacketNumberSpace, data: &[u8]) {
        self.get_mut(space).unwrap().tx.send(data);
    }

    pub fn inbound_frame(&mut self, space: PacketNumberSpace, offset: u64, data: &[u8]) {
        self.get_mut(space).unwrap().rx.inbound_frame(offset, data);
    }

    pub fn data_ready(&self, space: PacketNumberSpace) -> bool {
        self.get(space).map_or(false, |cs| cs.rx.data_ready())
    }

    pub fn read_to_end(&mut self, space: PacketNumberSpace, buf: &mut Vec<u8>) -> usize {
        self.get_mut(space).unwrap().rx.read_to_end(buf)
    }

    pub fn acked(&mut self, token: &CryptoRecoveryToken) {
        self.get_mut(token.space)
            .unwrap()
            .tx
            .mark_as_acked(token.offset, token.length);
    }

    pub fn lost(&mut self, token: &CryptoRecoveryToken) {
        // See BZ 1624800, ignore lost packets in spaces we've dropped keys
        if let Some(cs) = self.get_mut(token.space) {
            cs.tx.mark_as_lost(token.offset, token.length);
        }
    }

    /// Resend any Initial or Handshake CRYPTO frames that might be outstanding.
    /// This can help speed up handshake times.
    pub fn resend_unacked(&mut self, space: PacketNumberSpace) {
        if space != PacketNumberSpace::ApplicationData {
            if let Some(cs) = self.get_mut(space) {
                cs.tx.unmark_sent();
            }
        }
    }

    fn get(&self, space: PacketNumberSpace) -> Option<&CryptoStream> {
        let (initial, hs, app) = match self {
            Self::Initial {
                initial,
                handshake,
                application,
            } => (Some(initial), Some(handshake), Some(application)),
            Self::Handshake {
                handshake,
                application,
            } => (None, Some(handshake), Some(application)),
            Self::ApplicationData { application } => (None, None, Some(application)),
        };
        match space {
            PacketNumberSpace::Initial => initial,
            PacketNumberSpace::Handshake => hs,
            PacketNumberSpace::ApplicationData => app,
        }
    }

    fn get_mut(&mut self, space: PacketNumberSpace) -> Option<&mut CryptoStream> {
        let (initial, hs, app) = match self {
            Self::Initial {
                initial,
                handshake,
                application,
            } => (Some(initial), Some(handshake), Some(application)),
            Self::Handshake {
                handshake,
                application,
            } => (None, Some(handshake), Some(application)),
            Self::ApplicationData { application } => (None, None, Some(application)),
        };
        match space {
            PacketNumberSpace::Initial => initial,
            PacketNumberSpace::Handshake => hs,
            PacketNumberSpace::ApplicationData => app,
        }
    }

    pub fn write_frame(
        &mut self,
        space: PacketNumberSpace,
        builder: &mut PacketBuilder,
        tokens: &mut Vec<RecoveryToken>,
        stats: &mut FrameStats,
    ) -> Res<()> {
        let cs = self.get_mut(space).unwrap();
        if let Some((offset, data)) = cs.tx.next_bytes() {
            let mut header_len = 1 + Encoder::varint_len(offset) + 1;

            // Don't bother if there isn't room for the header and some data.
            if builder.remaining() < header_len + 1 {
                return Ok(());
            }
            // Calculate length of data based on the minimum of:
            // - available data
            // - remaining space, less the header, which counts only one byte
            //   for the length at first to avoid underestimating length
            let length = min(data.len(), builder.remaining() - header_len);
            header_len += Encoder::varint_len(u64::try_from(length).unwrap()) - 1;
            let length = min(data.len(), builder.remaining() - header_len);

            builder.encode_varint(crate::frame::FRAME_TYPE_CRYPTO);
            builder.encode_varint(offset);
            builder.encode_vvec(&data[..length]);
            if builder.len() > builder.limit() {
                return Err(Error::InternalError(15));
            }

            cs.tx.mark_as_sent(offset, length);

            qdebug!("CRYPTO for {} offset={}, len={}", space, offset, length);
            tokens.push(RecoveryToken::Crypto(CryptoRecoveryToken {
                space,
                offset,
                length,
            }));
            stats.crypto += 1;
        }
        Ok(())
    }
}

impl Default for CryptoStreams {
    fn default() -> Self {
        Self::Initial {
            initial: CryptoStream::default(),
            handshake: CryptoStream::default(),
            application: CryptoStream::default(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct CryptoRecoveryToken {
    space: PacketNumberSpace,
    offset: u64,
    length: usize,
}
