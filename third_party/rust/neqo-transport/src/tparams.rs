// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Transport parameters. See -transport section 7.3.

#![allow(dead_code)]
use crate::{Error, Res};
use neqo_common::{hex, qdebug, qinfo, qtrace, Decoder, Encoder};
use neqo_crypto::constants::{TLS_HS_CLIENT_HELLO, TLS_HS_ENCRYPTED_EXTENSIONS};
use neqo_crypto::ext::{ExtensionHandler, ExtensionHandlerResult, ExtensionWriterResult};
use neqo_crypto::{HandshakeMessage, ZeroRttCheckResult, ZeroRttChecker};
use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;

struct PreferredAddress {
    // TODO(ekr@rtfm.com): Implement.
}

pub type TransportParameterId = u64;
macro_rules! tpids {
        { $($n:ident = $v:expr),+ $(,)? } => {
            $(pub const $n: TransportParameterId = $v as TransportParameterId;)+
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
}

#[derive(Clone, Debug, PartialEq)]
pub enum TransportParameter {
    Bytes(Vec<u8>),
    Integer(u64),
    Empty,
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
        };
    }

    fn decode(dec: &mut Decoder) -> Res<Option<(TransportParameterId, Self)>> {
        let tp = match dec.decode_varint() {
            Some(v) => v,
            _ => return Err(Error::NoMoreData),
        };
        let content = match dec.decode_vvec() {
            Some(v) => v,
            _ => return Err(Error::NoMoreData),
        };
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
            | MAX_ACK_DELAY => match d.decode_varint() {
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

#[derive(Clone, Debug, Default, PartialEq)]
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
            | INITIAL_MAX_STREAMS_UNI => 0,
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
            | ACTIVE_CONNECTION_ID_LIMIT => {
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
            Some(TransportParameter::Bytes(x)) => Some(&x),
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
            ) {
                continue;
            }
            match self.params.get(k) {
                Some(v_self) => match (v_self, v_rem) {
                    (TransportParameter::Integer(i_self), TransportParameter::Integer(i_rem)) => {
                        if *i_self < *i_rem {
                            return false;
                        }
                    }
                    (TransportParameter::Empty, TransportParameter::Empty) => {}
                    _ => return false,
                },
                _ => return false,
            }
        }
        true
    }

    fn was_sent(&self, tp: TransportParameterId) -> bool {
        self.params.contains_key(&tp)
    }
}

#[derive(Default, Debug)]
pub struct TransportParametersHandler {
    pub(crate) local: TransportParameters,
    pub(crate) remote: Option<TransportParameters>,
    pub(crate) remote_0rtt: Option<TransportParameters>,
}

impl TransportParametersHandler {
    pub fn remote(&self) -> &TransportParameters {
        match (self.remote.as_ref(), self.remote_0rtt.as_ref()) {
            (Some(tp), _) | (_, Some(tp)) => tp,
            _ => panic!("no transport parameters from peer"),
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
        d[..enc.len()].copy_from_slice(&enc);
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
                self.remote = Some(tp);
                ExtensionHandlerResult::Ok
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

// TODO(ekr@rtfm.com): Need to write more TP unit tests.
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
        assert_eq!(tps2.was_sent(ORIGINAL_DESTINATION_CONNECTION_ID), false);
        assert_eq!(tps2.was_sent(INITIAL_SOURCE_CONNECTION_ID), false);
        assert_eq!(tps2.was_sent(RETRY_SOURCE_CONNECTION_ID), false);
        assert_eq!(tps2.was_sent(STATELESS_RESET_TOKEN), true);

        let mut enc = Encoder::default();
        tps.encode(&mut enc);

        let tps2 = TransportParameters::decode(&mut enc.as_decoder()).expect("Couldn't decode");
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
        ];
        for i in INTEGER_KEYS {
            tps_a.set(*i, TransportParameter::Integer(12));
        }

        let tps_b = tps_a.clone();
        assert!(tps_a.ok_for_0rtt(&tps_b));
        assert!(tps_b.ok_for_0rtt(&tps_a));

        // For each integer key, increase the value by one.
        for i in INTEGER_KEYS {
            let mut tps_b = tps_a.clone();
            tps_b.set(*i, TransportParameter::Integer(13));
            // If an increased value is remembered, then we can't attempt 0-RTT with these parameters.
            assert!(!tps_a.ok_for_0rtt(&tps_b));
            // If an increased value is lower, then we can attempt 0-RTT with these parameters.
            assert!(tps_b.ok_for_0rtt(&tps_a));
        }

        // Drop integer values and check.
        for i in INTEGER_KEYS {
            let mut tps_b = tps_a.clone();
            tps_b.remove(*i);
            // A value that is missing from what is rememebered is OK.
            assert!(tps_a.ok_for_0rtt(&tps_b));
            // A value that is rememebered, but not current is not OK.
            assert!(!tps_b.ok_for_0rtt(&tps_a));
        }
    }
}
