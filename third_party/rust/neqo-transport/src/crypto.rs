// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::cell::RefCell;
use std::cmp::max;
use std::mem;
use std::ops::{Index, IndexMut, Range};
use std::rc::Rc;
use std::time::Instant;

use neqo_common::{hex, matches, qdebug, qerror, qinfo, qtrace};
use neqo_crypto::aead::Aead;
use neqo_crypto::hp::HpKey;
use neqo_crypto::{
    hkdf, Agent, AntiReplay, Cipher, Epoch, RecordList, SymKey, TLS_AES_128_GCM_SHA256,
    TLS_AES_256_GCM_SHA384, TLS_EPOCH_APPLICATION_DATA, TLS_EPOCH_HANDSHAKE, TLS_EPOCH_INITIAL,
    TLS_EPOCH_ZERO_RTT, TLS_VERSION_1_3,
};

use crate::connection::Role;
use crate::frame::Frame;
use crate::packet::PacketNumber;
use crate::recovery::RecoveryToken;
use crate::recv_stream::RxStreamOrderer;
use crate::send_stream::TxBuffer;
use crate::tparams::{TpZeroRttChecker, TransportParametersHandler};
use crate::tracking::PNSpace;
use crate::{Error, Res};

const MAX_AUTH_TAG: usize = 32;

#[derive(Debug)]
pub struct Crypto {
    pub(crate) tls: Agent,
    pub(crate) streams: CryptoStreams,
    pub(crate) states: CryptoStates,
}

impl Crypto {
    pub fn new(
        mut agent: Agent,
        protocols: &[impl AsRef<str>],
        tphandler: Rc<RefCell<TransportParametersHandler>>,
        anti_replay: Option<&AntiReplay>,
    ) -> Res<Self> {
        agent.set_version_range(TLS_VERSION_1_3, TLS_VERSION_1_3)?;
        agent.enable_ciphers(&[TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384])?;
        agent.set_alpn(protocols)?;
        agent.disable_end_of_early_data();
        match &mut agent {
            Agent::Client(c) => c.enable_0rtt()?,
            Agent::Server(s) => s.enable_0rtt(
                anti_replay.unwrap(),
                0xffff_ffff,
                TpZeroRttChecker::wrap(tphandler.clone()),
            )?,
        }
        agent.extension_handler(0xffa5, tphandler)?;
        Ok(Self {
            tls: agent,
            streams: Default::default(),
            states: Default::default(),
        })
    }

    /// Enable 0-RTT and return `true` if it is enabled successfully.
    pub fn enable_0rtt(&mut self, role: Role) -> Res<bool> {
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
        let secret = secret.ok_or(Error::KeysNotFound)?;
        self.states.set_0rtt_keys(dir, &secret, cipher.unwrap());
        Ok(true)
    }

    pub fn install_keys(&mut self, role: Role) {
        if self.tls.state().is_final() {
            return;
        }
        // These functions only work once, but will usually return KeysNotFound.
        // Anything else is unusual and worth logging.
        if let Err(e) = self.install_handshake_keys() {
            qerror!([self], "Error installing handshake keys: {:?}", e);
        }
        if role == Role::Server {
            if let Err(e) = self.install_application_write_key() {
                qerror!([self], "Error installing application write key: {:?}", e);
            }
        }
    }

    fn install_handshake_keys(&mut self) -> Res<()> {
        qtrace!([self], "Attempt to install handshake keys");
        let write_secret = if let Some(secret) = self.tls.write_secret(TLS_EPOCH_HANDSHAKE) {
            secret
        } else {
            // No keys is fine.
            return Ok(());
        };
        let read_secret = self
            .tls
            .read_secret(TLS_EPOCH_HANDSHAKE)
            .ok_or(Error::KeysNotFound)?;
        let cipher = match self.tls.info() {
            None => self.tls.preinfo()?.cipher_suite(),
            Some(info) => Some(info.cipher_suite()),
        }
        .ok_or(Error::KeysNotFound)?;
        self.states
            .set_handshake_keys(&write_secret, &read_secret, cipher);
        qdebug!([self], "Handshake keys installed");
        Ok(())
    }

    fn install_application_write_key(&mut self) -> Res<()> {
        qtrace!([self], "Attempt to install application write key");
        if let Some(secret) = self.tls.write_secret(TLS_EPOCH_APPLICATION_DATA) {
            self.states.set_application_write_key(secret)?;
            qdebug!([self], "Application write key installed");
        }
        Ok(())
    }

    pub fn install_application_keys(&mut self, expire_0rtt: Instant) -> Res<()> {
        if let Err(e) = self.install_application_write_key() {
            if e != Error::KeysNotFound {
                return Err(e);
            }
        }
        let read_secret = self
            .tls
            .read_secret(TLS_EPOCH_APPLICATION_DATA)
            .ok_or(Error::KeysNotFound)?;
        self.states
            .set_application_read_key(read_secret, expire_0rtt)?;
        qdebug!([self], "application read keys installed");
        Ok(())
    }

    /// Buffer crypto records for sending.
    pub fn buffer_records(&mut self, records: RecordList) {
        for r in records {
            assert_eq!(r.ct, 22);
            qtrace!([self], "Adding CRYPTO data {:?}", r);
            self.streams.send(PNSpace::from(r.epoch), &r.data);
        }
    }

    pub fn acked(&mut self, token: CryptoRecoveryToken) {
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

    /// Discard state for a packet number space and return true
    /// if something was discarded.
    pub fn discard(&mut self, space: PNSpace) -> bool {
        self.streams.discard(space);
        self.states.discard(space)
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
    /// This is the minimum allowed.
    min_pn: PacketNumber,
}

impl CryptoDxState {
    pub fn new(
        direction: CryptoDxDirection,
        epoch: Epoch,
        secret: &SymKey,
        cipher: Cipher,
    ) -> Self {
        qinfo!(
            "Making {:?} {} CryptoDxState, cipher={}",
            direction,
            epoch,
            cipher
        );
        Self {
            direction,
            epoch: usize::from(epoch),
            aead: Aead::new(TLS_VERSION_1_3, cipher, secret, "quic ").unwrap(),
            hpkey: HpKey::extract(TLS_VERSION_1_3, cipher, secret, "quic hp").unwrap(),
            used_pn: 0..0,
            min_pn: 0,
        }
    }

    pub fn new_initial(direction: CryptoDxDirection, label: &str, dcid: &[u8]) -> Self {
        const INITIAL_SALT: &[u8] = &[
            0xc3, 0xee, 0xf7, 0x12, 0xc7, 0x2e, 0xbb, 0x5a, 0x11, 0xa7, 0xd2, 0x43, 0x2b, 0xb4,
            0x63, 0x65, 0xbe, 0xf9, 0xf5, 0x02,
        ];
        let cipher = TLS_AES_128_GCM_SHA256;
        let initial_secret = hkdf::extract(
            TLS_VERSION_1_3,
            cipher,
            Some(
                hkdf::import_key(TLS_VERSION_1_3, cipher, INITIAL_SALT)
                    .as_ref()
                    .unwrap(),
            ),
            hkdf::import_key(TLS_VERSION_1_3, cipher, dcid)
                .as_ref()
                .unwrap(),
        )
        .unwrap();

        let secret =
            hkdf::expand_label(TLS_VERSION_1_3, cipher, &initial_secret, &[], label).unwrap();

        Self::new(direction, TLS_EPOCH_INITIAL, &secret, cipher)
    }

    pub fn next(&self, next_secret: &SymKey, cipher: Cipher) -> Self {
        let pn = self.next_pn();
        Self {
            direction: self.direction,
            epoch: self.epoch + 1,
            aead: Aead::new(TLS_VERSION_1_3, cipher, next_secret, "quic ").unwrap(),
            hpkey: self.hpkey.clone(),
            used_pn: pn..pn,
            min_pn: pn,
        }
    }

    #[must_use]
    pub fn is_0rtt(&self) -> bool {
        self.epoch == usize::from(TLS_EPOCH_ZERO_RTT)
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
        // TODO(mt) use Range::is_empty() when available
        if self.used_pn.start == self.used_pn.end {
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
        let mut out = vec![0; body.len()];
        let res = self.aead.decrypt(pn, hdr, body, &mut out)?;
        self.used(pn)?;
        Ok(res.to_vec())
    }

    #[cfg(test)]
    pub(crate) fn test_default() -> Self {
        // This matches the value in packet.rs
        const CLIENT_CID: &[u8] = &[0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08];
        Self::new_initial(CryptoDxDirection::Write, "server in", CLIENT_CID)
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
}

impl CryptoDxAppData {
    pub fn new(dir: CryptoDxDirection, secret: SymKey, cipher: Cipher) -> Res<Self> {
        Ok(Self {
            dx: CryptoDxState::new(dir, TLS_EPOCH_APPLICATION_DATA, &secret, cipher),
            cipher,
            next_secret: Self::update_secret(cipher, &secret)?,
        })
    }

    fn update_secret(cipher: Cipher, secret: &SymKey) -> Res<SymKey> {
        let next = hkdf::expand_label(TLS_VERSION_1_3, cipher, secret, &[], "quic ku")?;
        Ok(next)
    }

    pub fn next(&self) -> Res<Self> {
        if self.dx.epoch == usize::max_value() {
            // Guard against too many key updates.
            return Err(Error::KeysNotFound);
        }
        let next_secret = Self::update_secret(self.cipher, &self.next_secret)?;
        Ok(Self {
            dx: self.dx.next(&next_secret, self.cipher),
            cipher: self.cipher,
            next_secret,
        })
    }
}

#[derive(Debug, Default)]
pub struct CryptoStates {
    initial: Option<CryptoState>,
    handshake: Option<CryptoState>,
    zero_rtt: Option<CryptoDxState>, // One direction only!
    cipher: Cipher,
    app_write: Option<CryptoDxAppData>,
    app_read: Option<CryptoDxAppData>,
    app_read_next: Option<CryptoDxAppData>,
    // If this is set, then we have noticed a genuine update.
    // Once this time passes, we should switch in new keys.
    read_update_time: Option<Instant>,
}

impl CryptoStates {
    fn select_or_0rtt<'a>(
        app: Option<&'a mut CryptoDxAppData>,
        zero_rtt: Option<&'a mut CryptoDxState>,
        dir: CryptoDxDirection,
    ) -> Option<&'a mut CryptoDxState> {
        app.map(|a| &mut a.dx)
            .or_else(|| zero_rtt.filter(|z| z.direction == dir))
    }

    pub fn tx<'a>(&'a mut self, space: PNSpace) -> Option<&'a mut CryptoDxState> {
        let tx = |x: &'a mut Option<CryptoState>| x.as_mut().map(|dx| &mut dx.tx);
        match space {
            PNSpace::Initial => tx(&mut self.initial),
            PNSpace::Handshake => tx(&mut self.handshake),
            PNSpace::ApplicationData => Self::select_or_0rtt(
                self.app_write.as_mut(),
                self.zero_rtt.as_mut(),
                CryptoDxDirection::Write,
            ),
        }
    }

    pub fn rx_hp(&mut self, space: PNSpace) -> Option<&mut CryptoDxState> {
        match space {
            PNSpace::ApplicationData => Self::select_or_0rtt(
                self.app_read.as_mut(),
                self.zero_rtt.as_mut(),
                CryptoDxDirection::Read,
            ),
            _ => self.rx(space, false),
        }
    }

    pub fn rx<'a>(&'a mut self, space: PNSpace, key_phase: bool) -> Option<&'a mut CryptoDxState> {
        let rx = |x: &'a mut Option<CryptoState>| x.as_mut().map(|dx| &mut dx.rx);
        match space {
            PNSpace::Initial => rx(&mut self.initial),
            PNSpace::Handshake => rx(&mut self.handshake),
            PNSpace::ApplicationData => {
                let app = if let Some(arn) = &self.app_read_next {
                    if arn.dx.key_phase() == key_phase {
                        self.app_read_next.as_mut()
                    } else {
                        self.app_read.as_mut()
                    }
                } else {
                    self.app_read.as_mut()
                };
                Self::select_or_0rtt(app, self.zero_rtt.as_mut(), CryptoDxDirection::Read)
            }
        }
    }

    /// Create the initial crypto state.
    pub fn init(&mut self, role: Role, dcid: &[u8]) {
        const CLIENT_INITIAL_LABEL: &str = "client in";
        const SERVER_INITIAL_LABEL: &str = "server in";

        qinfo!(
            [self],
            "Creating initial cipher state role={:?} dcid={}",
            role,
            hex(dcid)
        );

        let (write_label, read_label) = match role {
            Role::Client => (CLIENT_INITIAL_LABEL, SERVER_INITIAL_LABEL),
            Role::Server => (SERVER_INITIAL_LABEL, CLIENT_INITIAL_LABEL),
        };

        let mut initial = CryptoState {
            tx: CryptoDxState::new_initial(CryptoDxDirection::Write, write_label, dcid),
            rx: CryptoDxState::new_initial(CryptoDxDirection::Read, read_label, dcid),
        };
        if let Some(prev) = &self.initial {
            qinfo!(
                [self],
                "Continue packet numbers for initial after retry (write is {:?})",
                prev.rx.used_pn,
            );
            initial.tx.continuation(&prev.tx).unwrap();
        }
        self.initial = Some(initial);
    }

    pub fn set_0rtt_keys(&mut self, dir: CryptoDxDirection, secret: &SymKey, cipher: Cipher) {
        self.zero_rtt = Some(CryptoDxState::new(dir, TLS_EPOCH_ZERO_RTT, secret, cipher));
    }

    /// Discard keys and return true if that happened.
    pub fn discard(&mut self, space: PNSpace) -> bool {
        match space {
            PNSpace::Initial => self.initial.take().is_some(),
            PNSpace::Handshake => self.handshake.take().is_some(),
            PNSpace::ApplicationData => panic!("Can't drop application data keys"),
        }
    }

    pub fn discard_0rtt_keys(&mut self) {
        assert!(
            self.app_read.is_none(),
            "Can't discard 0-RTT after setting application keys"
        );
        self.zero_rtt = None;
    }

    pub fn set_handshake_keys(
        &mut self,
        write_secret: &SymKey,
        read_secret: &SymKey,
        cipher: Cipher,
    ) {
        self.cipher = cipher;
        self.handshake = Some(CryptoState {
            tx: CryptoDxState::new(
                CryptoDxDirection::Write,
                TLS_EPOCH_HANDSHAKE,
                write_secret,
                cipher,
            ),
            rx: CryptoDxState::new(
                CryptoDxDirection::Read,
                TLS_EPOCH_HANDSHAKE,
                read_secret,
                cipher,
            ),
        });
    }

    pub fn set_application_write_key(&mut self, secret: SymKey) -> Res<()> {
        debug_assert!(self.app_write.is_none());
        debug_assert_ne!(self.cipher, 0);
        let mut app = CryptoDxAppData::new(CryptoDxDirection::Write, secret, self.cipher)?;
        if let Some(z) = &self.zero_rtt {
            if z.direction == CryptoDxDirection::Write {
                app.dx.continuation(z)?;
            }
        }
        self.zero_rtt = None;
        self.app_write = Some(app);
        Ok(())
    }

    pub fn set_application_read_key(&mut self, secret: SymKey, expire_0rtt: Instant) -> Res<()> {
        debug_assert!(self.app_write.is_some(), "should have write keys installed");
        debug_assert!(self.app_read.is_none());
        let mut app = CryptoDxAppData::new(CryptoDxDirection::Read, secret, self.cipher)?;
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
        let write = &self.app_write.as_ref().unwrap().dx;
        let read = &self.app_read.as_ref().unwrap().dx;
        if write.epoch == read.epoch {
            qdebug!([self], "Updating write keys to epoch={}", write.epoch + 1);
            self.app_write = Some(self.app_write.as_ref().unwrap().next()?);
            Ok(true)
        } else {
            Ok(false)
        }
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
        // If we received a key update, then we assume that the peer has
        // acknowledged a packet we sent in this epoch. It's OK to do that
        // because they aren't allowed to update without first having received
        // something from us. If the ACK isn't in the packet that triggered this
        // key update, it must be in some other packet they have sent.
        let _ = self.maybe_update_write()?;

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
    #[cfg(test)]
    pub(crate) fn test_default() -> Self {
        let read = || {
            let mut dx = CryptoDxState::test_default();
            dx.direction = CryptoDxDirection::Read;
            dx
        };
        let app_read = || CryptoDxAppData {
            dx: read(),
            cipher: TLS_AES_128_GCM_SHA256,
            next_secret: hkdf::import_key(TLS_VERSION_1_3, TLS_AES_128_GCM_SHA256, &[0xaa; 32])
                .unwrap(),
        };
        Self {
            initial: Some(CryptoState {
                tx: CryptoDxState::test_default(),
                rx: read(),
            }),
            handshake: None,
            zero_rtt: None,
            cipher: TLS_AES_128_GCM_SHA256,
            app_write: None,
            app_read: Some(app_read()),
            app_read_next: Some(app_read()),
            read_update_time: None,
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
    pub fn discard(&mut self, space: PNSpace) {
        match space {
            PNSpace::Initial => {
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
            PNSpace::Handshake => {
                if let Self::Handshake { application, .. } = self {
                    *self = Self::ApplicationData {
                        application: mem::take(application),
                    };
                } else if matches!(self, Self::Initial {..}) {
                    panic!("Discarding handshake before initial discarded");
                }
            }
            PNSpace::ApplicationData => panic!("Discarding application data crypto streams"),
        }
    }

    pub fn send(&mut self, space: PNSpace, data: &[u8]) {
        self.get_mut(space).unwrap().tx.send(data);
    }

    pub fn inbound_frame(&mut self, space: PNSpace, offset: u64, data: Vec<u8>) -> Res<()> {
        self.get_mut(space).unwrap().rx.inbound_frame(offset, data)
    }

    pub fn data_ready(&self, space: PNSpace) -> bool {
        self.get(space).map_or(false, |cs| cs.rx.data_ready())
    }

    pub fn read_to_end(&mut self, space: PNSpace, buf: &mut Vec<u8>) -> Res<u64> {
        self.get_mut(space).unwrap().rx.read_to_end(buf)
    }

    pub fn acked(&mut self, token: CryptoRecoveryToken) {
        self.get_mut(token.space)
            .unwrap()
            .tx
            .mark_as_acked(token.offset, token.length)
    }

    pub fn lost(&mut self, token: &CryptoRecoveryToken) {
        // See BZ 1624800, ignore lost packets in spaces we've dropped keys
        if let Some(cs) = self.get_mut(token.space) {
            cs.tx.mark_as_lost(token.offset, token.length)
        }
    }

    fn get(&self, space: PNSpace) -> Option<&CryptoStream> {
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
            PNSpace::Initial => initial,
            PNSpace::Handshake => hs,
            PNSpace::ApplicationData => app,
        }
    }

    fn get_mut(&mut self, space: PNSpace) -> Option<&mut CryptoStream> {
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
            PNSpace::Initial => initial,
            PNSpace::Handshake => hs,
            PNSpace::ApplicationData => app,
        }
    }

    pub fn get_frame(
        &mut self,
        space: PNSpace,
        remaining: usize,
    ) -> Option<(Frame, Option<RecoveryToken>)> {
        let cs = self.get_mut(space).unwrap();
        if let Some((offset, data)) = cs.tx.next_bytes() {
            let (frame, length) = Frame::new_crypto(offset, data, remaining);
            cs.tx.mark_as_sent(offset, length);

            qdebug!(
                "Emitting crypto frame space={}, offset={}, len={}",
                space,
                offset,
                length
            );
            Some((
                frame,
                Some(RecoveryToken::Crypto(CryptoRecoveryToken {
                    space,
                    offset,
                    length,
                })),
            ))
        } else {
            None
        }
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
    space: PNSpace,
    offset: u64,
    length: usize,
}
