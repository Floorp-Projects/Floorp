use std::mem;

use serde::ser;
use Value;
use super::{Encoder, Error, EncoderState, State};

impl Encoder {
    fn table_begin(&mut self) -> Result<Self, Error> {
        match self.state {
            State::NextMapKey => Err(Error::InvalidMapKeyLocation),
            _ => Ok(mem::replace(self, Encoder::new()))
        }
    }

    fn table_end(&mut self, mut state: Self) -> Result<(), Error> {
        match state.state {
            State::NextKey(key) => {
                mem::swap(&mut self.toml, &mut state.toml);
                self.toml.insert(key, Value::Table(state.toml));
            },
            State::NextArray(mut arr) => {
                mem::swap(&mut self.toml, &mut state.toml);
                arr.push(Value::Table(state.toml));
                self.state = State::NextArray(arr);
            },
            State::Start => {},
            State::NextMapKey => unreachable!(),
        }
        Ok(())
    }
}

impl ser::Serializer for Encoder {
    type Error = Error;
    type MapState = Self;
    type StructState = Self;
    type StructVariantState = Self;
    type SeqState = EncoderState;
    type TupleState = EncoderState;
    type TupleStructState = EncoderState;
    type TupleVariantState = EncoderState;

    fn serialize_bool(&mut self, v: bool) -> Result<(), Error> {
        self.emit_value(Value::Boolean(v))
    }

    fn serialize_i64(&mut self, v: i64) -> Result<(), Error> {
        self.emit_value(Value::Integer(v))
    }

    // TODO: checked casts

    fn serialize_u64(&mut self, v: u64) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_isize(&mut self, v: isize) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_usize(&mut self, v: usize) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i8(&mut self, v: i8) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_u8(&mut self, v: u8) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i16(&mut self, v: i16) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_u16(&mut self, v: u16) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i32(&mut self, v: i32) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_u32(&mut self, v: u32) -> Result<(), Error> {
        self.serialize_i64(v as i64)
    }

    fn serialize_f32(&mut self, v: f32) -> Result<(), Error> {
        self.serialize_f64(v as f64)
    }

    fn serialize_f64(&mut self, v: f64) -> Result<(), Error> {
        self.emit_value(Value::Float(v))
    }

    fn serialize_str(&mut self, value: &str) -> Result<(), Error> {
        self.emit_value(Value::String(value.to_string()))
    }

    fn serialize_unit_struct(&mut self, _name: &'static str) -> Result<(), Error> {
        Ok(())
    }

    fn serialize_unit(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn serialize_none(&mut self) -> Result<(), Error> {
        self.emit_none()
    }

    fn serialize_char(&mut self, c: char) -> Result<(), Error> {
        self.serialize_str(&c.to_string())
    }

    fn serialize_some<V>(&mut self, value: V) -> Result<(), Error>
        where V: ser::Serialize
    {
        value.serialize(self)
    }

    fn serialize_bytes(&mut self, v: &[u8]) -> Result<(), Error> {
        let mut state = try!(self.serialize_seq(Some(v.len())));
        for c in v {
            try!(self.serialize_seq_elt(&mut state, c));
        }
        self.serialize_seq_end(state)
    }

    fn serialize_seq_fixed_size(&mut self, len: usize)
                                -> Result<EncoderState, Error> {
        self.serialize_seq(Some(len))
    }

    fn serialize_seq(&mut self, _len: Option<usize>)
                     -> Result<EncoderState, Error> {
        self.seq_begin().map(|s| EncoderState { inner: s })
    }

    fn serialize_seq_elt<T>(&mut self,
                            _state: &mut EncoderState,
                            value: T) -> Result<(), Error>
        where T: ser::Serialize
    {
        value.serialize(self)
    }

    fn serialize_seq_end(&mut self, state: EncoderState) -> Result<(), Error> {
        self.seq_end(state.inner)
    }

    fn serialize_tuple(&mut self, len: usize)
                       -> Result<EncoderState, Error> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple_elt<T>(&mut self,
                              state: &mut EncoderState,
                              value: T) -> Result<(), Error>
        where T: ser::Serialize
    {
        self.serialize_seq_elt(state, value)
    }

    fn serialize_tuple_end(&mut self, state: EncoderState) -> Result<(), Error> {
        self.serialize_seq_end(state)
    }

    fn serialize_tuple_struct(&mut self,
                              _name: &'static str,
                              len: usize) -> Result<EncoderState, Error> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple_struct_elt<T>(&mut self,
                                     state: &mut EncoderState,
                                     value: T) -> Result<(), Error>
        where T: ser::Serialize
    {
        self.serialize_seq_elt(state, value)
    }

    fn serialize_tuple_struct_end(&mut self, state: EncoderState)
                                  -> Result<(), Error> {
        self.serialize_seq_end(state)
    }

    fn serialize_tuple_variant(&mut self,
                               _name: &'static str,
                               _id: usize,
                               _variant: &'static str,
                               len: usize) -> Result<EncoderState, Error> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple_variant_elt<T>(&mut self,
                                      state: &mut EncoderState,
                                      value: T) -> Result<(), Error>
        where T: ser::Serialize
    {
        self.serialize_seq_elt(state, value)
    }

    fn serialize_tuple_variant_end(&mut self, state: EncoderState)
                                   -> Result<(), Error> {
        self.serialize_seq_end(state)
    }

    fn serialize_map(&mut self, _len: Option<usize>) -> Result<Self, Error> {
        self.table_begin()
    }

    fn serialize_map_key<K>(&mut self,
                            _state: &mut Encoder,
                            key: K) -> Result<(), Error>
        where K: ser::Serialize
    {
        self.table_key(|me| key.serialize(me))
    }

    fn serialize_map_value<V>(&mut self,
                              _state: &mut Encoder,
                              value: V) -> Result<(), Error>
        where V: ser::Serialize
    {
        value.serialize(self)
    }

    fn serialize_map_end(&mut self, state: Self) -> Result<(), Error> {
        self.table_end(state)
    }

    fn serialize_struct(&mut self,
                        _name: &'static str,
                        len: usize) -> Result<Self, Error> {
        self.serialize_map(Some(len))
    }

    fn serialize_struct_elt<V>(&mut self,
                               state: &mut Encoder,
                               key: &'static str,
                               value: V) -> Result<(), Error>
        where V: ser::Serialize
    {
        try!(self.serialize_map_key(state, key));
        self.serialize_map_value(state, value)
    }

    fn serialize_struct_end(&mut self, state: Self) -> Result<(), Error> {
        self.serialize_map_end(state)
    }

    fn serialize_struct_variant(&mut self,
                                _name: &'static str,
                                _id: usize,
                                _variant: &'static str,
                                len: usize) -> Result<Self, Error> {
        self.serialize_map(Some(len))
    }

    fn serialize_struct_variant_elt<V>(&mut self,
                                       state: &mut Encoder,
                                       key: &'static str,
                                       value: V) -> Result<(), Error>
        where V: ser::Serialize
    {
        try!(self.serialize_map_key(state, key));
        self.serialize_map_value(state, value)
    }

    fn serialize_struct_variant_end(&mut self, state: Self) -> Result<(), Error> {
        self.serialize_map_end(state)
    }

    fn serialize_newtype_struct<T>(&mut self,
                                   _name: &'static str,
                                   value: T) -> Result<(), Self::Error>
        where T: ser::Serialize,
    {
        // Don't serialize the newtype struct in a tuple.
        value.serialize(self)
    }

    fn serialize_newtype_variant<T>(&mut self,
                                    _name: &'static str,
                                    _variant_index: usize,
                                    _variant: &'static str,
                                    value: T) -> Result<(), Self::Error>
        where T: ser::Serialize,
    {
        // Don't serialize the newtype struct variant in a tuple.
        value.serialize(self)
    }

    fn serialize_unit_variant(&mut self,
                               _name: &'static str,
                               _variant_index: usize,
                               _variant: &'static str,
                               ) -> Result<(), Self::Error>
    {
        Ok(())
    }
}

impl ser::Serialize for Value {
    fn serialize<E>(&self, e: &mut E) -> Result<(), E::Error>
        where E: ser::Serializer
    {
        match *self {
            Value::String(ref s) => e.serialize_str(s),
            Value::Integer(i) => e.serialize_i64(i),
            Value::Float(f) => e.serialize_f64(f),
            Value::Boolean(b) => e.serialize_bool(b),
            Value::Datetime(ref s) => e.serialize_str(s),
            Value::Array(ref a) => {
                let mut state = try!(e.serialize_seq(Some(a.len())));
                for el in a.iter() {
                    try!(e.serialize_seq_elt(&mut state, el));
                }
                e.serialize_seq_end(state)
            }
            Value::Table(ref t) => {
                let mut state = try!(e.serialize_map(Some(t.len())));
                for (k, v) in t.iter() {
                    try!(e.serialize_map_key(&mut state, k));
                    try!(e.serialize_map_value(&mut state, v));
                }
                e.serialize_map_end(state)
            }
        }
    }
}

impl ser::Error for Error {
    fn custom<T: Into<String>>(msg: T) -> Error {
        Error::Custom(msg.into())
    }
}
