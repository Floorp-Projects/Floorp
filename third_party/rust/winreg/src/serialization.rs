// Copyright 2015, Igor Shaula
// Licensed under the MIT License <LICENSE or
// http://opensource.org/licenses/MIT>. This file
// may not be copied, modified, or distributed
// except according to those terms.

//! Registry keys parsing and serialization
use std::mem;
use std::io;
use super::{RegKey};
use super::enums::*;
use super::transaction::Transaction;
use super::rustc_serialize;
use super::winapi;
use self::EncoderState::{Start, NextKey/*, NextMapKey */};


#[derive(Debug)]
pub enum EncoderError{
    EncodeNotImplemented(String),
    IoError(io::Error),
    NoFieldName,
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

const ENCODER_SAM: winapi::DWORD = KEY_CREATE_SUB_KEY|KEY_SET_VALUE;

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

impl rustc_serialize::Encoder for Encoder {
    type Error = EncoderError;

    fn emit_nil(&mut self) -> EncodeResult<()> {
        no_impl!("nil")
    }

    fn emit_usize(&mut self, v: usize) -> EncodeResult<()> {
        self.emit_u64(v as u64)
    }

    fn emit_u64(&mut self, v: u64) -> EncodeResult<()> {
        emit_value!(self, v)
    }

    fn emit_u32(&mut self, v: u32) -> EncodeResult<()> {
        emit_value!(self, v)
    }

    fn emit_u16(&mut self, v: u16) -> EncodeResult<()> {
        self.emit_u32(v as u32)
    }

    fn emit_u8(&mut self, v: u8) -> EncodeResult<()> {
        self.emit_u32(v as u32)
    }

    fn emit_isize(&mut self, v: isize) -> EncodeResult<()> {
        self.emit_i64(v as i64)
    }

    fn emit_i64(&mut self, v: i64) -> EncodeResult<()> {
        let s = v.to_string();
        emit_value!(self, s)
    }

    fn emit_i32(&mut self, v: i32) -> EncodeResult<()> {
        self.emit_i64(v as i64)
    }

    fn emit_i16(&mut self, v: i16) -> EncodeResult<()> {
        self.emit_i64(v as i64)
    }

    fn emit_i8(&mut self, v: i8) -> EncodeResult<()> {
        self.emit_i64(v as i64)
    }

    fn emit_bool(&mut self, v: bool) -> EncodeResult<()> {
        self.emit_u32(v as u32)
    }

    fn emit_f64(&mut self, v: f64) -> EncodeResult<()> {
        let s = v.to_string();
        emit_value!(self, s)
    }

    fn emit_f32(&mut self, v: f32) -> EncodeResult<()> {
        let s = v.to_string();
        emit_value!(self, s)
    }

    fn emit_char(&mut self, _v: char) -> EncodeResult<()> {
        no_impl!("char")
    }

    fn emit_str(&mut self, v: &str) -> EncodeResult<()> {
        emit_value!(self, v)
    }

    fn emit_enum<F>(&mut self, _name: &str, _f: F) -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("enum")
    }

    fn emit_enum_variant<F>(&mut self, _name: &str, _id: usize, _cnt: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("enum_variant")
    }

    fn emit_enum_variant_arg<F>(&mut self, _: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("enum_variant_arg")
    }

    fn emit_enum_struct_variant<F>(&mut self, _name: &str, _id: usize, _cnt: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("enum_struct_variant")
    }

    fn emit_enum_struct_variant_field<F>(&mut self, _name: &str, _: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("enum_struct_variant_field")
    }

    fn emit_struct<F>(&mut self, _name: &str, _len: usize, f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        match mem::replace(&mut self.state, Start) {
            Start => { // root structure
                f(self)
            },
            NextKey(ref s) => { // nested structure
                match self.keys[self.keys.len()-1].create_subkey_transacted_with_flags(&s, &self.tr, ENCODER_SAM) {
                    Ok(subkey) => {
                        self.keys.push(subkey);
                        let res = f(self);
                        self.keys.pop();
                        res
                    },
                    Err(err) => Err(EncoderError::IoError(err))
                }
            }
        }
    }

    fn emit_struct_field<F>(&mut self, f_name: &str, _f_idx: usize, f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>
    {
        self.state = NextKey(f_name.to_owned());
        f(self)
    }

    fn emit_tuple<F>(&mut self, _: usize, _f: F) -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>
    {
        no_impl!("tuple")
    }

    fn emit_tuple_arg<F>(&mut self, _: usize, _f: F) -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>
    {
        no_impl!("tuple_arg")
    }

    fn emit_tuple_struct<F>(&mut self, _: &str, _: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>
    {
        no_impl!("tuple_struct")
    }

    fn emit_tuple_struct_arg<F>(&mut self, _: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>
    {
        no_impl!("tuple_struct_arg")
    }

    fn emit_option<F>(&mut self, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>
    {
        no_impl!("Option")
    }

    fn emit_option_none(&mut self) -> EncodeResult<()> {
        no_impl!("Option::None")
    }

    fn emit_option_some<F>(&mut self, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("Option::Some")
    }

    fn emit_seq<F>(&mut self, _: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("seq")
    }

    fn emit_seq_elt<F>(&mut self, _: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("seq_elt")
    }

    fn emit_map<F>(&mut self, _: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("map")
    }

    fn emit_map_elt_key<F>(&mut self, _: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("map_elt_key")
    }

    fn emit_map_elt_val<F>(&mut self, _idx: usize, _f: F)
        -> EncodeResult<()> where
        F: FnOnce(&mut Self) -> EncodeResult<()>,
    {
        no_impl!("map_elt_val")
    }
}

//=====================================================================

#[derive(Debug)]
pub enum DecoderError{
    DecodeNotImplemented(String),
    IoError(io::Error),
    ParseError(String),
    NoFieldName,
}

pub type DecodeResult<T> = Result<T, DecoderError>;

#[derive(Debug)]
pub struct Decoder {
    key: RegKey,
    f_name: Option<String>,
}

const DECODER_SAM: winapi::DWORD = KEY_QUERY_VALUE;

impl Decoder {
    pub fn from_key(key: &RegKey) -> DecodeResult<Decoder> {
        key.open_subkey_with_flags("", DECODER_SAM)
            .map(Decoder::new)
            .map_err(DecoderError::IoError)
    }

    fn new(key: RegKey) -> Decoder {
        Decoder{
            key: key,
            f_name: None,
        }
    }
}

macro_rules! read_value{
    ($s:ident) => (
        match mem::replace(&mut $s.f_name, None) {
            Some(ref s) => {
                $s.key.get_value(s)
                    .map_err(DecoderError::IoError)
            },
            None => Err(DecoderError::NoFieldName)
        }
    )
}

macro_rules! parse_string{
    ($s:ident) => ({
        let s: String = try!(read_value!($s));
        s.parse().map_err(|e| DecoderError::ParseError(format!("{:?}", e)))
    })
}

macro_rules! no_impl {
    ($e:expr) => (
        Err(DecoderError::DecodeNotImplemented($e.to_owned()))
    )
}

impl rustc_serialize::Decoder for Decoder {
    type Error = DecoderError;
    fn read_nil(&mut self) -> DecodeResult<()> {
        no_impl!("nil")
    }

    fn read_usize(&mut self) -> DecodeResult<usize> {
        self.read_u64().map(|v| v as usize)
    }

    fn read_u64(&mut self) -> DecodeResult<u64> {
        read_value!(self)
    }

    fn read_u32(&mut self) -> DecodeResult<u32> {
        read_value!(self)
    }

    fn read_u16(&mut self) -> DecodeResult<u16> {
        self.read_u32().map(|v| v as u16)
    }

    fn read_u8(&mut self) -> DecodeResult<u8> {
        self.read_u32().map(|v| v as u8)
    }

    fn read_isize(&mut self) -> DecodeResult<isize> {
        self.read_i64().map(|v| v as isize)
    }

    fn read_i64(&mut self) -> DecodeResult<i64> {
        parse_string!(self)
    }

    fn read_i32(&mut self) -> DecodeResult<i32> {
        parse_string!(self)
    }

    fn read_i16(&mut self) -> DecodeResult<i16> {
        parse_string!(self)
    }

    fn read_i8(&mut self) -> DecodeResult<i8> {
        parse_string!(self)
    }

    fn read_bool(&mut self) -> DecodeResult<bool> {
        self.read_u32().map(|v| v > 0)
    }

    fn read_f64(&mut self) -> DecodeResult<f64> {
        parse_string!(self)
    }

    fn read_f32(&mut self) -> DecodeResult<f32> {
        parse_string!(self)
    }

    fn read_char(&mut self) -> DecodeResult<char> {
        no_impl!("char")
    }

    fn read_str(&mut self) -> DecodeResult<String> {
        read_value!(self)
    }

    fn read_enum<T, F>(&mut self, _name: &str, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("enum")
    }

    fn read_enum_variant<T, F>(&mut self, _names: &[&str], _f: F)
        -> DecodeResult<T> where
        F: FnMut(&mut Self, usize) -> DecodeResult<T>
    {
        no_impl!("enum_variant")
    }

    fn read_enum_variant_arg<T, F>(&mut self, _a_idx: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("enum_variant_arg")
    }

    fn read_enum_struct_variant<T, F>(&mut self, _names: &[&str], _f: F)
        -> DecodeResult<T> where
        F: FnMut(&mut Self, usize) -> DecodeResult<T>
    {
        no_impl!("enum_struct_variant")
    }

    fn read_enum_struct_variant_field<T, F>(&mut self, _f_name: &str, _f_idx: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("enum_struct_variant_field")
    }

    fn read_struct<T, F>(&mut self, _s_name: &str, _len: usize, f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        match mem::replace(&mut self.f_name, None) {
            None => { // root structure
                f(self)
            },
            Some(ref s) => { // nested structure
                match self.key.open_subkey_with_flags(&s, DECODER_SAM) {
                    Ok(subkey) => {
                        let mut nested = Decoder::new(subkey);
                        f(&mut nested)
                    },
                    Err(err) => Err(DecoderError::IoError(err))
                }
            }
        }
    }

    fn read_struct_field<T, F>(&mut self, f_name: &str, _f_idx: usize, f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        self.f_name = Some(f_name.to_owned());
        f(self)
    }

    fn read_tuple<T, F>(&mut self, _len: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("tuple")
    }

    fn read_tuple_arg<T, F>(&mut self, _a_idx: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("tuple_arg")
    }

    fn read_tuple_struct<T, F>(&mut self, _s_name: &str, _len: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("tuple_struct")
    }

    fn read_tuple_struct_arg<T, F>(&mut self, _a_idx: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("tuple_struct_arg")
    }

    fn read_option<T, F>(&mut self, _f: F)
        -> DecodeResult<T> where
        F: FnMut(&mut Self, bool) -> DecodeResult<T>
    {
        no_impl!("option")
    }

    fn read_seq<T, F>(&mut self, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self, usize) -> DecodeResult<T>
    {
        no_impl!("seq")
    }

    fn read_seq_elt<T, F>(&mut self, _idx: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("seq_elt")
    }

    fn read_map<T, F>(&mut self, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self, usize) -> DecodeResult<T>
    {
        no_impl!("map")
    }

    fn read_map_elt_key<T, F>(&mut self, _idx: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("map_elt_key")
    }

    fn read_map_elt_val<T, F>(&mut self, _idx: usize, _f: F)
        -> DecodeResult<T> where
        F: FnOnce(&mut Self) -> DecodeResult<T>
    {
        no_impl!("map_elt_val")
    }

    fn error(&mut self, err: &str) -> Self::Error {
        DecoderError::ParseError(err.to_owned())
    }
}
