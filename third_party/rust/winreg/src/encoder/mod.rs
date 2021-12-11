// Copyright 2017, Igor Shaula
// Licensed under the MIT License <LICENSE or
// http://opensource.org/licenses/MIT>. This file
// may not be copied, modified, or distributed
// except according to those terms.
use std::io;
use std::fmt;
use std::error::Error;
use winapi::shared::minwindef::DWORD;
use super::RegKey;
use super::enums::*;
use super::transaction::Transaction;
use self::EncoderState::*;

macro_rules! emit_value{
    ($s:ident, $v:ident) => (
        match mem::replace(&mut $s.state, Start) {
            NextKey(ref s) => {
                $s.keys[$s.keys.len()-1].set_value(s, &$v)
                    .map_err(EncoderError::IoError)
            },
            Start => Err(EncoderError::NoFieldName)
        }
    )
}

macro_rules! no_impl {
    ($e:expr) => (
        Err(EncoderError::EncodeNotImplemented($e.to_owned()))
    )
}

#[cfg(feature = "serialization-serde")] mod serialization_serde;

#[derive(Debug)]
pub enum EncoderError{
    EncodeNotImplemented(String),
    SerializerError(String),
    IoError(io::Error),
    NoFieldName,
}

impl fmt::Display for EncoderError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}", self)
    }
}

impl Error for EncoderError {
    fn description(&self) -> &str {
        use self::EncoderError::*;
        match *self {
            EncodeNotImplemented(ref s) | SerializerError(ref s) => s,
            IoError(ref e) => e.description(),
            NoFieldName => "No field name"
        }
    }
}

pub type EncodeResult<T> = Result<T, EncoderError>;

impl From<io::Error> for EncoderError {
    fn from(err: io::Error) -> EncoderError {
        EncoderError::IoError(err)
    }
}

#[derive(Debug)]
enum EncoderState {
    Start,
    NextKey(String),
    // NextMapKey,
}

#[derive(Debug)]
pub struct Encoder {
    keys: Vec<RegKey>,
    tr: Transaction,
    state: EncoderState,
}

const ENCODER_SAM: DWORD = KEY_CREATE_SUB_KEY|KEY_SET_VALUE;

impl Encoder {
    pub fn from_key(key: &RegKey) -> EncodeResult<Encoder> {
        let tr = try!(Transaction::new());
        key.open_subkey_transacted_with_flags("", &tr, ENCODER_SAM)
            .map(|k| Encoder::new(k, tr))
            .map_err(EncoderError::IoError)
    }

    fn new(key: RegKey, tr: Transaction) -> Encoder {
        let mut keys = Vec::with_capacity(5);
        keys.push(key);
        Encoder{
            keys: keys,
            tr: tr,
            state: Start,
        }
    }

    pub fn commit(&mut self) -> EncodeResult<()> {
        self.tr.commit().map_err(EncoderError::IoError)
    }
}
