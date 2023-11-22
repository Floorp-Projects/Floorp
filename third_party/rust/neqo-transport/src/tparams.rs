// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Transport parameters. See -transport section 7.3.

use crate::{
    cid::{ConnectionId, ConnectionIdEntry, CONNECTION_ID_SEQNO_PREFERRED, MAX_CONNECTION_ID_LEN},
    version::{Version, VersionConfig, WireVersion},
    Error, Res,
};

use neqo_common::{hex, qdebug, qinfo, qtrace, Decoder, Encoder, Role};
use neqo_crypto::{
    constants::{TLS_HS_CLIENT_HELLO, TLS_HS_ENCRYPTED_EXTENSIONS},
    ext::{ExtensionHandler, ExtensionHandlerResult, ExtensionWriterResult},
    random, HandshakeMessage, ZeroRttCheckResult, ZeroRttChecker,
};

use std::{
    cell::RefCell,
    collections::HashMap,
    convert::TryFrom,
    net::{Ipv4Addr, Ipv6Addr, SocketAddrV4, SocketAddrV6},
    rc::Rc,
};

pub type TransportParameterId = u64;
macro_rules! tpids {
        { $($n:ident = $v:expr),+ $(,)? } => {
            $(pub const $n: TransportParameterId = $v as TransportParameterId;)+

            /// A complete list of internal transport parameters.
            #[cfg(not(test))]
            pub(crate) const INTERNAL_TRANSPORT_PARAMETERS: &[TransportParameterId] = &[ $($n),+ ];
        };
    }
tpids! {
    ORIGINAL_DESTINATION_CONNECTION_ID = 0x00,
    IDLE_TIMEOUT = 0x01,
    STATELESS_RESET_TOKEN = 0x02,
    MAX_UDP_PAYLOAD_SIZE = 0x03,
    INITIAL_MAX_DATA = 0x04,
    INITIAL_MAX_STREAM_DATA_BIDI_LOCAL = 0x05,
    INITIAL_MAX_STREAM_DATA_BIDI_REMOTE = 0x06,
    INITIAL_MAX_STREAM_DATA_UNI = 0x07,
    INITIAL_MAX_STREAMS_BIDI = 0x08,
    INITIAL_MAX_STREAMS_UNI = 0x09,
    ACK_DELAY_EXPONENT = 0x0a,
    MAX_ACK_DELAY = 0x0b,
    DISABLE_MIGRATION = 0x0c,
    PREFERRED_ADDRESS = 0x0d,
    ACTIVE_CONNECTION_ID_LIMIT = 0x0e,
    INITIAL_SOURCE_CONNECTION_ID = 0x0f,
    RETRY_SOURCE_CONNECTION_ID = 0x10,
    GREASE_QUIC_BIT = 0x2ab2,
    MIN_ACK_DELAY = 0xff02_de1a,
    MAX_DATAGRAM_FRAME_SIZE = 0x0020,
    VERSION_NEGOTIATION = 0xff73db,
}

#[derive(Clone, Debug)]
pub struct PreferredAddress {
    v4: Option<SocketAddrV4>,
    v6: Option<SocketAddrV6>,
}

impl PreferredAddress {
    /// Make a new preferred address configuration.
    ///
    /// # Panics
    /// If neither address is provided, or if either address is of the wrong type.
    #[must_use]
    pub fn new(v4: Option<SocketAddrV4>, v6: Option<SocketAddrV6>) -> Self {
        assert!(v4.is_some() || v6.is_some());
        if let Some(a) = v4 {
            assert!(!a.ip().is_unspecified());
            assert_ne!(a.port(), 0);
        }
        if let Some(a) = v6 {
            assert!(!a.ip().is_unspecified());
            assert_ne!(a.port(), 0);
        }
        Self { v4, v6 }
    }

    /// A generic version of `new()` for testing.
    #[must_use]
    #[cfg(test)]
    pub fn new_any(v4: Option<std::net::SocketAddr>, v6: Option<std::net::SocketAddr>) -> Self {
        use std::net::SocketAddr;

        let v4 = v4.map(|v4| {
            let SocketAddr::V4(v4) = v4 else {
                panic!("not v4");
            };
            v4
        });
        let v6 = v6.map(|v6| {
            let SocketAddr::V6(v6) = v6 else {
                panic!("not v6");
            };
            v6
        });
        Self::new(v4, v6)
    }

    #[must_use]
    pub fn ipv4(&self) -> Option<SocketAddrV4> {
        self.v4
    }
    #[must_use]
    pub fn ipv6(&self) -> Option<SocketAddrV6> {
        self.v6
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum TransportParameter {
    Bytes(Vec<u8>),
    Integer(u64),
    Empty,
    PreferredAddress {
        v4: Option<SocketAddrV4>,
        v6: Option<SocketAddrV6>,
        cid: ConnectionId,
        srt: [u8; 16],
    },
    Versions {
        current: WireVersion,
        other: Vec<WireVersion>,
    },
}

impl TransportParameter {
    fn encode(&self, enc: &mut Encoder, tp: TransportParameterId) {
        qdebug!("TP encoded; type 0x{:02x} val {:?}", tp, self);
        enc.encode_varint(tp);
        match self {
            Self::Bytes(a) => {
                enc.encode_vvec(a);
            }
            Self::Integer(a) => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_varint(*a);
                });
            }
            Self::Empty => {
                enc.encode_varint(0_u64);
            }
            Self::PreferredAddress { v4, v6, cid, srt } => {
                enc.encode_vvec_with(|enc_inner| {
                    if let Some(v4) = v4 {
                        enc_inner.encode(&v4.ip().octets()[..]);
                        enc_inner.encode_uint(2, v4.port());
                    } else {
                        enc_inner.encode(&[0; 6]);
                    }
                    if let Some(v6) = v6 {
                        enc_inner.encode(&v6.ip().octets()[..]);
                        enc_inner.encode_uint(2, v6.port());
                    } else {
                        enc_inner.encode(&[0; 18]);
                    }
                    enc_inner.encode_vec(1, &cid[..]);
                    enc_inner.encode(&srt[..]);
                });
            }
            Self::Versions { current, other } => {
                enc.encode_vvec_with(|enc_inner| {
                    enc_inner.encode_uint(4, *current);
                    for v in other {
                        enc_inner.encode_uint(4, *v);
                    }
                });
            }
        };
    }

    fn decode_preferred_address(d: &mut Decoder) -> Res<Self> {
        // IPv4 address (maybe)
        let v4ip =
            Ipv4Addr::from(<[u8; 4]>::try_from(d.decode(4).ok_or(Error::NoMoreData)?).unwrap());
        let v4port = u16::try_from(d.decode_uint(2).ok_or(Error::NoMoreData)?).unwrap();
        // Can't have non-zero IP and zero port, or vice versa.
        if v4ip.is_unspecified() ^ (v4port == 0) {
            return Err(Error::TransportParameterError);
        }
        let v4 = if v4port == 0 {
            None
        } else {
            Some(SocketAddrV4::new(v4ip, v4port))
        };

        // IPv6 address (mostly the same as v4)
        let v6ip =
            Ipv6Addr::from(<[u8; 16]>::try_from(d.decode(16).ok_or(Error::NoMoreData)?).unwrap());
        let v6port = u16::try_from(d.decode_uint(2).ok_or(Error::NoMoreData)?).unwrap();
        if v6ip.is_unspecified() ^ (v6port == 0) {
            return Err(Error::TransportParameterError);
        }
        let v6 = if v6port == 0 {
            None
        } else {
            Some(SocketAddrV6::new(v6ip, v6port, 0, 0))
        };
        // Need either v4 or v6 to be present.
        if v4.is_none() && v6.is_none() {
            return Err(Error::TransportParameterError);
        }

        // Connection ID (non-zero length)
        let cid = ConnectionId::from(d.decode_vec(1).ok_or(Error::NoMoreData)?);
        if cid.len() == 0 || cid.len() > MAX_CONNECTION_ID_LEN {
            return Err(Error::TransportParameterError);
        }

        // Stateless reset token
        let srtbuf = d.decode(16).ok_or(Error::NoMoreData)?;
        let srt = <[u8; 16]>::try_from(srtbuf).unwrap();

        Ok(Self::PreferredAddress { v4, v6, cid, srt })
    }

    fn decode_versions(dec: &mut Decoder) -> Res<Self> {
        fn dv(dec: &mut Decoder) -> Res<WireVersion> {
            let v = dec.decode_uint(4).ok_or(Error::NoMoreData)?;
            if v == 0 {
                Err(Error::TransportParameterError)
            } else {
                Ok(v as WireVersion)
            }
        }

        let current = dv(dec)?;
        // This rounding down is OK because `decode` checks for left over data.
        let count = dec.remaining() / 4;
        let mut other = Vec::with_capacity(count);
        for _ in 0..count {
            other.push(dv(dec)?);
        }
        Ok(Self::Versions { current, other })
    }

    fn decode(dec: &mut Decoder) -> Res<Option<(TransportParameterId, Self)>> {
        let tp = dec.decode_varint().ok_or(Error::NoMoreData)?;
        let content = dec.decode_vvec().ok_or(Error::NoMoreData)?;
        qtrace!("TP {:x} length {:x}", tp, content.len());
        let mut d = Decoder::from(content);
        let value = match tp {
            ORIGINAL_DESTINATION_CONNECTION_ID
            | INITIAL_SOURCE_CONNECTION_ID
            | RETRY_SOURCE_CONNECTION_ID => Self::Bytes(d.decode_remainder().to_vec()),
            STATELESS_RESET_TOKEN => {
                if d.remaining() != 16 {
                    return Err(Error::TransportParameterError);
                }
                Self::Bytes(d.decode_remainder().to_vec())
            }
            IDLE_TIMEOUT
            | INITIAL_MAX_DATA
            | INITIAL_MAX_STREAM_DATA_BIDI_LOCAL
            | INITIAL_MAX_STREAM_DATA_BIDI_REMOTE
            | INITIAL_MAX_STREAM_DATA_UNI
            | MAX_ACK_DELAY
            | MAX_DATAGRAM_FRAME_SIZE => match d.decode_varint() {
                Some(v) => Self::Integer(v),
                None => return Err(Error::TransportParameterError),
            },

            INITIAL_MAX_STREAMS_BIDI | INITIAL_MAX_STREAMS_UNI => match d.decode_varint() {
                Some(v) if v <= (1 << 60) => Self::Integer(v),
                _ => return Err(Error::StreamLimitError),
            },

            MAX_UDP_PAYLOAD_SIZE => match d.decode_varint() {
                Some(v) if v >= 1200 => Self::Integer(v),
                _ => return Err(Error::TransportParameterError),
            },

            ACK_DELAY_EXPONENT => match d.decode_varint() {
                Some(v) if v <= 20 => Self::Integer(v),
                _ => return Err(Error::TransportParameterError),
            },
            ACTIVE_CONNECTION_ID_LIMIT => match d.decode_varint() {
                Some(v) if v >= 2 => Self::Integer(v),
                _ => return Err(Error::TransportParameterError),
            },

            DISABLE_MIGRATION | GREASE_QUIC_BIT => Self::Empty,

            PREFERRED_ADDRESS => Self::decode_preferred_address(&mut d)?,

            MIN_ACK_DELAY => match d.decode_varint() {
                Some(v) if v < (1 << 24) => Self::Integer(v),
                _ => return Err(Error::TransportParameterError),
            },

            VERSION_NEGOTIATION => Self::decode_versions(&mut d)?,

            // Skip.
            _ => return Ok(None),
        };
        if d.remaining() > 0 {
            return Err(Error::TooMuchData);
        }
        qdebug!("TP decoded; type 0x{:02x} val {:?}", tp, value);
        Ok(Some((tp, value)))
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct TransportParameters {
    params: HashMap<TransportParameterId, TransportParameter>,
}

impl TransportParameters {
    /// Set a value.
    pub fn set(&mut self, k: TransportParameterId, v: TransportParameter) {
        self.params.insert(k, v);
    }

    /// Clear a key.
    pub fn remove(&mut self, k: TransportParameterId) {
        self.params.remove(&k);
    }

    /// Decode is a static function that parses transport parameters
    /// using the provided decoder.
    pub(crate) fn decode(d: &mut Decoder) -> Res<Self> {
        let mut tps = Self::default();
        qtrace!("Parsed fixed TP header");

        while d.remaining() > 0 {
            match TransportParameter::decode(d) {
                Ok(Some((tipe, tp))) => {
                    tps.set(tipe, tp);
                }
                Ok(None) => {}
                Err(e) => return Err(e),
            }
        }
        Ok(tps)
    }

    pub(crate) fn encode(&self, enc: &mut Encoder) {
        for (tipe, tp) in &self.params {
            tp.encode(enc, *tipe);
        }
    }

    // Get an integer type or a default.
    pub fn get_integer(&self, tp: TransportParameterId) -> u64 {
        let default = match tp {
            IDLE_TIMEOUT
            | INITIAL_MAX_DATA
            | INITIAL_MAX_STREAM_DATA_BIDI_LOCAL
            | INITIAL_MAX_STREAM_DATA_BIDI_REMOTE
            | INITIAL_MAX_STREAM_DATA_UNI
            | INITIAL_MAX_STREAMS_BIDI
            | INITIAL_MAX_STREAMS_UNI
            | MIN_ACK_DELAY
            | MAX_DATAGRAM_FRAME_SIZE => 0,
            MAX_UDP_PAYLOAD_SIZE => 65527,
            ACK_DELAY_EXPONENT => 3,
            MAX_ACK_DELAY => 25,
            ACTIVE_CONNECTION_ID_LIMIT => 2,
            _ => panic!("Transport parameter not known or not an Integer"),
        };
        match self.params.get(&tp) {
            None => default,
            Some(TransportParameter::Integer(x)) => *x,
            _ => panic!("Internal error"),
        }
    }

    // Set an integer type or a default.
    pub fn set_integer(&mut self, tp: TransportParameterId, value: u64) {
        match tp {
            IDLE_TIMEOUT
            | INITIAL_MAX_DATA
            | INITIAL_MAX_STREAM_DATA_BIDI_LOCAL
            | INITIAL_MAX_STREAM_DATA_BIDI_REMOTE
            | INITIAL_MAX_STREAM_DATA_UNI
            | INITIAL_MAX_STREAMS_BIDI
            | INITIAL_MAX_STREAMS_UNI
            | MAX_UDP_PAYLOAD_SIZE
            | ACK_DELAY_EXPONENT
            | MAX_ACK_DELAY
            | ACTIVE_CONNECTION_ID_LIMIT
            | MIN_ACK_DELAY
            | MAX_DATAGRAM_FRAME_SIZE => {
                self.set(tp, TransportParameter::Integer(value));
            }
            _ => panic!("Transport parameter not known"),
        }
    }

    pub fn get_bytes(&self, tp: TransportParameterId) -> Option<&[u8]> {
        match tp {
            ORIGINAL_DESTINATION_CONNECTION_ID
            | INITIAL_SOURCE_CONNECTION_ID
            | RETRY_SOURCE_CONNECTION_ID
            | STATELESS_RESET_TOKEN => {}
            _ => panic!("Transport parameter not known or not type bytes"),
        }

        match self.params.get(&tp) {
            None => None,
            Some(TransportParameter::Bytes(x)) => Some(x),
            _ => panic!("Internal error"),
        }
    }

    pub fn set_bytes(&mut self, tp: TransportParameterId, value: Vec<u8>) {
        match tp {
            ORIGINAL_DESTINATION_CONNECTION_ID
            | INITIAL_SOURCE_CONNECTION_ID
            | RETRY_SOURCE_CONNECTION_ID
            | STATELESS_RESET_TOKEN => {
                self.set(tp, TransportParameter::Bytes(value));
            }
            _ => panic!("Transport parameter not known or not type bytes"),
        }
    }

    pub fn set_empty(&mut self, tp: TransportParameterId) {
        match tp {
            DISABLE_MIGRATION | GREASE_QUIC_BIT => {
                self.set(tp, TransportParameter::Empty);
            }
            _ => panic!("Transport parameter not known or not type empty"),
        }
    }

    /// Set version information.
    pub fn set_versions(&mut self, role: Role, versions: &VersionConfig) {
        let rbuf = random(4);
        let mut other = Vec::with_capacity(versions.all().len() + 1);
        let mut dec = Decoder::new(&rbuf);
        let grease = (dec.decode_uint(4).unwrap() as u32) & 0xf0f0_f0f0 | 0x0a0a0a0a;
        other.push(grease);
        for &v in versions.all() {
            if role == Role::Client && !versions.initial().is_compatible(v) {
                continue;
            }
            other.push(v.wire_version());
        }
        let current = versions.initial().wire_version();
        self.set(
            VERSION_NEGOTIATION,
            TransportParameter::Versions { current, other },
        );
    }

    fn compatible_upgrade(&mut self, v: Version) {
        if let Some(TransportParameter::Versions {
            ref mut current, ..
        }) = self.params.get_mut(&VERSION_NEGOTIATION)
        {
            *current = v.wire_version();
        } else {
            unreachable!("Compatible upgrade without transport parameters set!");
        }
    }

    pub fn get_empty(&self, tipe: TransportParameterId) -> bool {
        match self.params.get(&tipe) {
            None => false,
            Some(TransportParameter::Empty) => true,
            _ => panic!("Internal error"),
        }
    }

    /// Return true if the remembered transport parameters are OK for 0-RTT.
    /// Generally this means that any value that is currently in effect is greater than
    /// or equal to the promised value.
    pub(crate) fn ok_for_0rtt(&self, remembered: &Self) -> bool {
        for (k, v_rem) in &remembered.params {
            // Skip checks for these, which don't affect 0-RTT.
            if matches!(
                *k,
                ORIGINAL_DESTINATION_CONNECTION_ID
                    | INITIAL_SOURCE_CONNECTION_ID
                    | RETRY_SOURCE_CONNECTION_ID
                    | STATELESS_RESET_TOKEN
                    | IDLE_TIMEOUT
                    | ACK_DELAY_EXPONENT
                    | MAX_ACK_DELAY
                    | ACTIVE_CONNECTION_ID_LIMIT
                    | PREFERRED_ADDRESS
            ) {
                continue;
            }
            let ok = if let Some(v_self) = self.params.get(k) {
                match (v_self, v_rem) {
                    (TransportParameter::Integer(i_self), TransportParameter::Integer(i_rem)) => {
                        if *k == MIN_ACK_DELAY {
                            // MIN_ACK_DELAY is backwards:
                            // it can only be reduced safely.
                            *i_self <= *i_rem
                        } else {
                            *i_self >= *i_rem
                        }
                    }
                    (TransportParameter::Empty, TransportParameter::Empty) => true,
                    (
                        TransportParameter::Versions {
                            current: v_self, ..
                        },
                        TransportParameter::Versions { current: v_rem, .. },
                    ) => v_self == v_rem,
                    _ => false,
                }
            } else {
                false
            };
            if !ok {
                return false;
            }
        }
        true
    }

    /// Get the preferred address in a usable form.
    #[must_use]
    pub fn get_preferred_address(&self) -> Option<(PreferredAddress, ConnectionIdEntry<[u8; 16]>)> {
        if let Some(TransportParameter::PreferredAddress { v4, v6, cid, srt }) =
            self.params.get(&PREFERRED_ADDRESS)
        {
            Some((
                PreferredAddress::new(*v4, *v6),
                ConnectionIdEntry::new(CONNECTION_ID_SEQNO_PREFERRED, cid.clone(), *srt),
            ))
        } else {
            None
        }
    }

    /// Get the version negotiation values for validation.
    #[must_use]
    pub fn get_versions(&self) -> Option<(WireVersion, &[WireVersion])> {
        if let Some(TransportParameter::Versions { current, other }) =
            self.params.get(&VERSION_NEGOTIATION)
        {
            Some((*current, other))
        } else {
            None
        }
    }

    #[must_use]
    pub fn has_value(&self, tp: TransportParameterId) -> bool {
        self.params.contains_key(&tp)
    }
}

#[derive(Debug)]
pub struct TransportParametersHandler {
    role: Role,
    versions: VersionConfig,
    pub(crate) local: TransportParameters,
    pub(crate) remote: Option<TransportParameters>,
    pub(crate) remote_0rtt: Option<TransportParameters>,
}

impl TransportParametersHandler {
    pub fn new(role: Role, versions: VersionConfig) -> Self {
        let mut local = TransportParameters::default();
        local.set_versions(role, &versions);
        Self {
            role,
            versions,
            local,
            remote: None,
            remote_0rtt: None,
        }
    }

    /// When resuming, the version is set based on the ticket.
    /// That needs to be done to override the default choice from configuration.
    pub fn set_version(&mut self, version: Version) {
        debug_assert_eq!(self.role, Role::Client);
        self.versions.set_initial(version);
        self.local.set_versions(self.role, &self.versions)
    }

    pub fn remote(&self) -> &TransportParameters {
        match (self.remote.as_ref(), self.remote_0rtt.as_ref()) {
            (Some(tp), _) | (_, Some(tp)) => tp,
            _ => panic!("no transport parameters from peer"),
        }
    }

    /// Get the version as set (or as determined by a compatible upgrade).
    pub fn version(&self) -> Version {
        self.versions.initial()
    }

    fn compatible_upgrade(&mut self, remote_tp: &TransportParameters) -> Res<()> {
        if let Some((current, other)) = remote_tp.get_versions() {
            qtrace!(
                "Peer versions: {:x} {:x?}; config {:?}",
                current,
                other,
                self.versions,
            );

            if self.role == Role::Client {
                let chosen = Version::try_from(current)?;
                if self.versions.compatible().any(|&v| v == chosen) {
                    Ok(())
                } else {
                    qinfo!(
                        "Chosen version {:x} is not compatible with initial version {:x}",
                        current,
                        self.versions.initial().wire_version(),
                    );
                    Err(Error::TransportParameterError)
                }
            } else {
                if current != self.versions.initial().wire_version() {
                    qinfo!(
                        "Current version {:x} != own version {:x}",
                        current,
                        self.versions.initial().wire_version(),
                    );
                    return Err(Error::TransportParameterError);
                }

                if let Some(preferred) = self.versions.preferred_compatible(other) {
                    if preferred != self.versions.initial() {
                        qinfo!(
                            "Compatible upgrade {:?} ==> {:?}",
                            self.versions.initial(),
                            preferred
                        );
                        self.versions.set_initial(preferred);
                        self.local.compatible_upgrade(preferred);
                    }
                    Ok(())
                } else {
                    qinfo!("Unable to find any compatible version");
                    Err(Error::TransportParameterError)
                }
            }
        } else {
            Ok(())
        }
    }
}

impl ExtensionHandler for TransportParametersHandler {
    fn write(&mut self, msg: HandshakeMessage, d: &mut [u8]) -> ExtensionWriterResult {
        if !matches!(msg, TLS_HS_CLIENT_HELLO | TLS_HS_ENCRYPTED_EXTENSIONS) {
            return ExtensionWriterResult::Skip;
        }

        qdebug!("Writing transport parameters, msg={:?}", msg);

        // TODO(ekr@rtfm.com): Modify to avoid a copy.
        let mut enc = Encoder::default();
        self.local.encode(&mut enc);
        assert!(enc.len() <= d.len());
        d[..enc.len()].copy_from_slice(enc.as_ref());
        ExtensionWriterResult::Write(enc.len())
    }

    fn handle(&mut self, msg: HandshakeMessage, d: &[u8]) -> ExtensionHandlerResult {
        qtrace!(
            "Handling transport parameters, msg={:?} value={}",
            msg,
            hex(d),
        );

        if !matches!(msg, TLS_HS_CLIENT_HELLO | TLS_HS_ENCRYPTED_EXTENSIONS) {
            return ExtensionHandlerResult::Alert(110); // unsupported_extension
        }

        let mut dec = Decoder::from(d);
        match TransportParameters::decode(&mut dec) {
            Ok(tp) => {
                if self.compatible_upgrade(&tp).is_ok() {
                    self.remote = Some(tp);
                    ExtensionHandlerResult::Ok
                } else {
                    ExtensionHandlerResult::Alert(47)
                }
            }
            _ => ExtensionHandlerResult::Alert(47), // illegal_parameter
        }
    }
}

#[derive(Debug)]
pub(crate) struct TpZeroRttChecker<T> {
    handler: Rc<RefCell<TransportParametersHandler>>,
    app_checker: T,
}

impl<T> TpZeroRttChecker<T>
where
    T: ZeroRttChecker + 'static,
{
    pub fn wrap(
        handler: Rc<RefCell<TransportParametersHandler>>,
        app_checker: T,
    ) -> Box<dyn ZeroRttChecker> {
        Box::new(Self {
            handler,
            app_checker,
        })
    }
}

impl<T> ZeroRttChecker for TpZeroRttChecker<T>
where
    T: ZeroRttChecker,
{
    fn check(&self, token: &[u8]) -> ZeroRttCheckResult {
        // Reject 0-RTT if there is no token.
        if token.is_empty() {
            qdebug!("0-RTT: no token, no 0-RTT");
            return ZeroRttCheckResult::Reject;
        }
        let mut dec = Decoder::from(token);
        let tpslice = if let Some(v) = dec.decode_vvec() {
            v
        } else {
            qinfo!("0-RTT: token code error");
            return ZeroRttCheckResult::Fail;
        };
        let mut dec_tp = Decoder::from(tpslice);
        let remembered = if let Ok(v) = TransportParameters::decode(&mut dec_tp) {
            v
        } else {
            qinfo!("0-RTT: transport parameter decode error");
            return ZeroRttCheckResult::Fail;
        };
        if self.handler.borrow().local.ok_for_0rtt(&remembered) {
            qinfo!("0-RTT: transport parameters OK, passing to application checker");
            self.app_checker.check(dec.decode_remainder())
        } else {
            qinfo!("0-RTT: transport parameters bad, rejecting");
            ZeroRttCheckResult::Reject
        }
    }
}

#[cfg(test)]
#[allow(unused_variables)]
mod tests {
    use super::*;

    #[test]
    fn basic_tps() {
        const RESET_TOKEN: &[u8] = &[1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8];
        let mut tps = TransportParameters::default();
        tps.set(
            STATELESS_RESET_TOKEN,
            TransportParameter::Bytes(RESET_TOKEN.to_vec()),
        );
        tps.params
            .insert(INITIAL_MAX_STREAMS_BIDI, TransportParameter::Integer(10));

        let mut enc = Encoder::default();
        tps.encode(&mut enc);

        let tps2 = TransportParameters::decode(&mut enc.as_decoder()).expect("Couldn't decode");
        assert_eq!(tps, tps2);

        println!("TPS = {:?}", tps);
        assert_eq!(tps2.get_integer(IDLE_TIMEOUT), 0); // Default
        assert_eq!(tps2.get_integer(MAX_ACK_DELAY), 25); // Default
        assert_eq!(tps2.get_integer(ACTIVE_CONNECTION_ID_LIMIT), 2); // Default
        assert_eq!(tps2.get_integer(INITIAL_MAX_STREAMS_BIDI), 10); // Sent
        assert_eq!(tps2.get_bytes(STATELESS_RESET_TOKEN), Some(RESET_TOKEN));
        assert_eq!(tps2.get_bytes(ORIGINAL_DESTINATION_CONNECTION_ID), None);
        assert_eq!(tps2.get_bytes(INITIAL_SOURCE_CONNECTION_ID), None);
        assert_eq!(tps2.get_bytes(RETRY_SOURCE_CONNECTION_ID), None);
        assert!(!tps2.has_value(ORIGINAL_DESTINATION_CONNECTION_ID));
        assert!(!tps2.has_value(INITIAL_SOURCE_CONNECTION_ID));
        assert!(!tps2.has_value(RETRY_SOURCE_CONNECTION_ID));
        assert!(tps2.has_value(STATELESS_RESET_TOKEN));

        let mut enc = Encoder::default();
        tps.encode(&mut enc);

        let tps2 = TransportParameters::decode(&mut enc.as_decoder()).expect("Couldn't decode");
    }

    fn make_spa() -> TransportParameter {
        TransportParameter::PreferredAddress {
            v4: Some(SocketAddrV4::new(Ipv4Addr::from(0xc000_0201), 443)),
            v6: Some(SocketAddrV6::new(
                Ipv6Addr::from(0xfe80_0000_0000_0000_0000_0000_0000_0001),
                443,
                0,
                0,
            )),
            cid: ConnectionId::from(&[1, 2, 3, 4, 5]),
            srt: [3; 16],
        }
    }

    #[test]
    fn preferred_address_encode_decode() {
        const ENCODED: &[u8] = &[
            0x0d, 0x2e, 0xc0, 0x00, 0x02, 0x01, 0x01, 0xbb, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0xbb, 0x05, 0x01,
            0x02, 0x03, 0x04, 0x05, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
            0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        ];
        let spa = make_spa();
        let mut enc = Encoder::new();
        spa.encode(&mut enc, PREFERRED_ADDRESS);
        assert_eq!(enc.as_ref(), ENCODED);

        let mut dec = enc.as_decoder();
        let (id, decoded) = TransportParameter::decode(&mut dec).unwrap().unwrap();
        assert_eq!(id, PREFERRED_ADDRESS);
        assert_eq!(decoded, spa);
    }

    fn mutate_spa<F>(wrecker: F) -> TransportParameter
    where
        F: FnOnce(&mut Option<SocketAddrV4>, &mut Option<SocketAddrV6>, &mut ConnectionId),
    {
        let mut spa = make_spa();
        if let TransportParameter::PreferredAddress {
            ref mut v4,
            ref mut v6,
            ref mut cid,
            ..
        } = &mut spa
        {
            wrecker(v4, v6, cid);
        } else {
            unreachable!();
        }
        spa
    }

    /// This takes a `TransportParameter::PreferredAddress` that has been mutilated.
    /// It then encodes it, working from the knowledge that the `encode` function
    /// doesn't care about validity, and decodes it.  The result should be failure.
    fn assert_invalid_spa(spa: TransportParameter) {
        let mut enc = Encoder::new();
        spa.encode(&mut enc, PREFERRED_ADDRESS);
        assert_eq!(
            TransportParameter::decode(&mut enc.as_decoder()).unwrap_err(),
            Error::TransportParameterError
        );
    }

    /// This is for those rare mutations that are acceptable.
    fn assert_valid_spa(spa: TransportParameter) {
        let mut enc = Encoder::new();
        spa.encode(&mut enc, PREFERRED_ADDRESS);
        let mut dec = enc.as_decoder();
        let (id, decoded) = TransportParameter::decode(&mut dec).unwrap().unwrap();
        assert_eq!(id, PREFERRED_ADDRESS);
        assert_eq!(decoded, spa);
    }

    #[test]
    fn preferred_address_zero_address() {
        // Either port being zero is bad.
        assert_invalid_spa(mutate_spa(|v4, _, _| {
            v4.as_mut().unwrap().set_port(0);
        }));
        assert_invalid_spa(mutate_spa(|_, v6, _| {
            v6.as_mut().unwrap().set_port(0);
        }));
        // Either IP being zero is bad.
        assert_invalid_spa(mutate_spa(|v4, _, _| {
            v4.as_mut().unwrap().set_ip(Ipv4Addr::from(0));
        }));
        assert_invalid_spa(mutate_spa(|_, v6, _| {
            v6.as_mut().unwrap().set_ip(Ipv6Addr::from(0));
        }));
        // Either address being absent is OK.
        assert_valid_spa(mutate_spa(|v4, _, _| {
            *v4 = None;
        }));
        assert_valid_spa(mutate_spa(|_, v6, _| {
            *v6 = None;
        }));
        // Both addresses being absent is bad.
        assert_invalid_spa(mutate_spa(|v4, v6, _| {
            *v4 = None;
            *v6 = None;
        }));
    }

    #[test]
    fn preferred_address_bad_cid() {
        assert_invalid_spa(mutate_spa(|_, _, cid| {
            *cid = ConnectionId::from(&[]);
        }));
        assert_invalid_spa(mutate_spa(|_, _, cid| {
            *cid = ConnectionId::from(&[0x0c; 21]);
        }));
    }

    #[test]
    fn preferred_address_truncated() {
        let spa = make_spa();
        let mut enc = Encoder::new();
        spa.encode(&mut enc, PREFERRED_ADDRESS);
        let mut dec = Decoder::from(&enc.as_ref()[..enc.len() - 1]);
        assert_eq!(
            TransportParameter::decode(&mut dec).unwrap_err(),
            Error::NoMoreData
        );
    }

    #[test]
    #[should_panic(expected = "v4.is_some() || v6.is_some()")]
    fn preferred_address_neither() {
        _ = PreferredAddress::new(None, None);
    }

    #[test]
    #[should_panic(expected = ".is_unspecified")]
    fn preferred_address_v4_unspecified() {
        _ = PreferredAddress::new(Some(SocketAddrV4::new(Ipv4Addr::from(0), 443)), None);
    }

    #[test]
    #[should_panic(expected = "left != right")]
    fn preferred_address_v4_zero_port() {
        _ = PreferredAddress::new(
            Some(SocketAddrV4::new(Ipv4Addr::from(0xc000_0201), 0)),
            None,
        );
    }

    #[test]
    #[should_panic(expected = ".is_unspecified")]
    fn preferred_address_v6_unspecified() {
        _ = PreferredAddress::new(None, Some(SocketAddrV6::new(Ipv6Addr::from(0), 443, 0, 0)));
    }

    #[test]
    #[should_panic(expected = "left != right")]
    fn preferred_address_v6_zero_port() {
        _ = PreferredAddress::new(None, Some(SocketAddrV6::new(Ipv6Addr::from(1), 0, 0, 0)));
    }

    #[test]
    fn compatible_0rtt_ignored_values() {
        let mut tps_a = TransportParameters::default();
        tps_a.set(
            STATELESS_RESET_TOKEN,
            TransportParameter::Bytes(vec![1, 2, 3]),
        );
        tps_a.set(IDLE_TIMEOUT, TransportParameter::Integer(10));
        tps_a.set(MAX_ACK_DELAY, TransportParameter::Integer(22));
        tps_a.set(ACTIVE_CONNECTION_ID_LIMIT, TransportParameter::Integer(33));

        let mut tps_b = TransportParameters::default();
        assert!(tps_a.ok_for_0rtt(&tps_b));
        assert!(tps_b.ok_for_0rtt(&tps_a));

        tps_b.set(
            STATELESS_RESET_TOKEN,
            TransportParameter::Bytes(vec![8, 9, 10]),
        );
        tps_b.set(IDLE_TIMEOUT, TransportParameter::Integer(100));
        tps_b.set(MAX_ACK_DELAY, TransportParameter::Integer(2));
        tps_b.set(ACTIVE_CONNECTION_ID_LIMIT, TransportParameter::Integer(44));
        assert!(tps_a.ok_for_0rtt(&tps_b));
        assert!(tps_b.ok_for_0rtt(&tps_a));
    }

    #[test]
    fn compatible_0rtt_integers() {
        let mut tps_a = TransportParameters::default();
        const INTEGER_KEYS: &[TransportParameterId] = &[
            INITIAL_MAX_DATA,
            INITIAL_MAX_STREAM_DATA_BIDI_LOCAL,
            INITIAL_MAX_STREAM_DATA_BIDI_REMOTE,
            INITIAL_MAX_STREAM_DATA_UNI,
            INITIAL_MAX_STREAMS_BIDI,
            INITIAL_MAX_STREAMS_UNI,
            MAX_UDP_PAYLOAD_SIZE,
            MIN_ACK_DELAY,
            MAX_DATAGRAM_FRAME_SIZE,
        ];
        for i in INTEGER_KEYS {
            tps_a.set(*i, TransportParameter::Integer(12));
        }

        let tps_b = tps_a.clone();
        assert!(tps_a.ok_for_0rtt(&tps_b));
        assert!(tps_b.ok_for_0rtt(&tps_a));

        // For each integer key, choose a new value that will be accepted.
        for i in INTEGER_KEYS {
            let mut tps_b = tps_a.clone();
            // Set a safe new value; reducing MIN_ACK_DELAY instead.
            let safe_value = if *i == MIN_ACK_DELAY { 11 } else { 13 };
            tps_b.set(*i, TransportParameter::Integer(safe_value));
            // If the new value is not safe relative to the remembered value,
            // then we can't attempt 0-RTT with these parameters.
            assert!(!tps_a.ok_for_0rtt(&tps_b));
            // The opposite situation is fine.
            assert!(tps_b.ok_for_0rtt(&tps_a));
        }

        // Drop integer values and check that that is OK.
        for i in INTEGER_KEYS {
            let mut tps_b = tps_a.clone();
            tps_b.remove(*i);
            // A value that is missing from what is rememebered is OK.
            assert!(tps_a.ok_for_0rtt(&tps_b));
            // A value that is rememebered, but not current is not OK.
            assert!(!tps_b.ok_for_0rtt(&tps_a));
        }
    }

    /// `ACTIVE_CONNECTION_ID_LIMIT` can't be less than 2.
    #[test]
    fn active_connection_id_limit_min_2() {
        let mut tps = TransportParameters::default();

        // Intentionally set an invalid value for the ACTIVE_CONNECTION_ID_LIMIT transport parameter.
        tps.params
            .insert(ACTIVE_CONNECTION_ID_LIMIT, TransportParameter::Integer(1));

        let mut enc = Encoder::default();
        tps.encode(&mut enc);

        // When decoding a set of transport parameters with an invalid ACTIVE_CONNECTION_ID_LIMIT
        // the result should be an error.
        let invalid_decode_result = TransportParameters::decode(&mut enc.as_decoder());
        assert!(invalid_decode_result.is_err());
    }

    #[test]
    fn versions_encode_decode() {
        const ENCODED: &[u8] = &[
            0x80, 0xff, 0x73, 0xdb, 0x0c, 0x00, 0x00, 0x00, 0x01, 0x1a, 0x2a, 0x3a, 0x4a, 0x5a,
            0x6a, 0x7a, 0x8a,
        ];
        let vn = TransportParameter::Versions {
            current: Version::Version1.wire_version(),
            other: vec![0x1a2a_3a4a, 0x5a6a_7a8a],
        };

        let mut enc = Encoder::new();
        vn.encode(&mut enc, VERSION_NEGOTIATION);
        assert_eq!(enc.as_ref(), ENCODED);

        let mut dec = enc.as_decoder();
        let (id, decoded) = TransportParameter::decode(&mut dec).unwrap().unwrap();
        assert_eq!(id, VERSION_NEGOTIATION);
        assert_eq!(decoded, vn);
    }

    #[test]
    fn versions_truncated() {
        const TRUNCATED: &[u8] = &[
            0x80, 0xff, 0x73, 0xdb, 0x0c, 0x00, 0x00, 0x00, 0x01, 0x1a, 0x2a, 0x3a, 0x4a, 0x5a,
            0x6a, 0x7a,
        ];
        let mut dec = Decoder::from(&TRUNCATED);
        assert_eq!(
            TransportParameter::decode(&mut dec).unwrap_err(),
            Error::NoMoreData
        );
    }

    #[test]
    fn versions_zero() {
        const ZERO1: &[u8] = &[0x80, 0xff, 0x73, 0xdb, 0x04, 0x00, 0x00, 0x00, 0x00];
        const ZERO2: &[u8] = &[
            0x80, 0xff, 0x73, 0xdb, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        ];

        let mut dec = Decoder::from(&ZERO1);
        assert_eq!(
            TransportParameter::decode(&mut dec).unwrap_err(),
            Error::TransportParameterError
        );
        let mut dec = Decoder::from(&ZERO2);
        assert_eq!(
            TransportParameter::decode(&mut dec).unwrap_err(),
            Error::TransportParameterError
        );
    }

    #[test]
    fn versions_equal_0rtt() {
        let mut current = TransportParameters::default();
        current.set(
            VERSION_NEGOTIATION,
            TransportParameter::Versions {
                current: Version::Version1.wire_version(),
                other: vec![0x1a2a_3a4a],
            },
        );

        let mut remembered = TransportParameters::default();
        // It's OK to not remember having versions.
        assert!(current.ok_for_0rtt(&remembered));
        // But it is bad in the opposite direction.
        assert!(!remembered.ok_for_0rtt(&current));

        // If the version matches, it's OK to use 0-RTT.
        remembered.set(
            VERSION_NEGOTIATION,
            TransportParameter::Versions {
                current: Version::Version1.wire_version(),
                other: vec![0x5a6a_7a8a, 0x9aaa_baca],
            },
        );
        assert!(current.ok_for_0rtt(&remembered));
        assert!(remembered.ok_for_0rtt(&current));

        // An apparent "upgrade" is still cause to reject 0-RTT.
        remembered.set(
            VERSION_NEGOTIATION,
            TransportParameter::Versions {
                current: Version::Version1.wire_version() + 1,
                other: vec![],
            },
        );
        assert!(!current.ok_for_0rtt(&remembered));
        assert!(!remembered.ok_for_0rtt(&current));
    }
}
