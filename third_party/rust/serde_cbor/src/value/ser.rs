// Copyright 2017 Serde Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::collections::BTreeMap;

use crate::error::Error;
use serde::{self, Serialize};

use crate::tags::Tagged;
use crate::value::Value;

impl serde::Serialize for Value {
    #[inline]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        match *self {
            Value::Integer(v) => serializer.serialize_i128(v),
            Value::Bytes(ref v) => serializer.serialize_bytes(&v),
            Value::Text(ref v) => serializer.serialize_str(&v),
            Value::Array(ref v) => v.serialize(serializer),
            Value::Map(ref v) => v.serialize(serializer),
            Value::Tag(tag, ref v) => Tagged::new(Some(tag), v).serialize(serializer),
            Value::Float(v) => serializer.serialize_f64(v),
            Value::Bool(v) => serializer.serialize_bool(v),
            Value::Null => serializer.serialize_unit(),
            Value::__Hidden => unreachable!(),
        }
    }
}

struct Serializer;

impl serde::Serializer for Serializer {
    type Ok = Value;
    type Error = Error;

    type SerializeSeq = SerializeVec;
    type SerializeTuple = SerializeVec;
    type SerializeTupleStruct = SerializeVec;
    type SerializeTupleVariant = SerializeTupleVariant;
    type SerializeMap = SerializeMap;
    type SerializeStruct = SerializeMap;
    type SerializeStructVariant = SerializeStructVariant;

    #[inline]
    fn serialize_bool(self, value: bool) -> Result<Value, Error> {
        Ok(Value::Bool(value))
    }

    #[inline]
    fn serialize_i8(self, value: i8) -> Result<Value, Error> {
        self.serialize_i64(i64::from(value))
    }

    #[inline]
    fn serialize_i16(self, value: i16) -> Result<Value, Error> {
        self.serialize_i64(i64::from(value))
    }

    #[inline]
    fn serialize_i32(self, value: i32) -> Result<Value, Error> {
        self.serialize_i64(i64::from(value))
    }

    #[inline]
    fn serialize_i64(self, value: i64) -> Result<Value, Error> {
        self.serialize_i128(i128::from(value))
    }

    fn serialize_i128(self, value: i128) -> Result<Value, Error> {
        Ok(Value::Integer(value))
    }

    #[inline]
    fn serialize_u8(self, value: u8) -> Result<Value, Error> {
        self.serialize_u64(u64::from(value))
    }

    #[inline]
    fn serialize_u16(self, value: u16) -> Result<Value, Error> {
        self.serialize_u64(u64::from(value))
    }

    #[inline]
    fn serialize_u32(self, value: u32) -> Result<Value, Error> {
        self.serialize_u64(u64::from(value))
    }

    #[inline]
    fn serialize_u64(self, value: u64) -> Result<Value, Error> {
        Ok(Value::Integer(value.into()))
    }

    #[inline]
    fn serialize_f32(self, value: f32) -> Result<Value, Error> {
        self.serialize_f64(f64::from(value))
    }

    #[inline]
    fn serialize_f64(self, value: f64) -> Result<Value, Error> {
        Ok(Value::Float(value))
    }

    #[inline]
    fn serialize_char(self, value: char) -> Result<Value, Error> {
        let mut s = String::new();
        s.push(value);
        self.serialize_str(&s)
    }

    #[inline]
    fn serialize_str(self, value: &str) -> Result<Value, Error> {
        Ok(Value::Text(value.to_owned()))
    }

    fn serialize_bytes(self, value: &[u8]) -> Result<Value, Error> {
        Ok(Value::Bytes(value.to_vec()))
    }

    #[inline]
    fn serialize_unit(self) -> Result<Value, Error> {
        Ok(Value::Null)
    }

    #[inline]
    fn serialize_unit_struct(self, _name: &'static str) -> Result<Value, Error> {
        self.serialize_unit()
    }

    #[inline]
    fn serialize_unit_variant(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
    ) -> Result<Value, Error> {
        self.serialize_str(variant)
    }

    #[inline]
    fn serialize_newtype_struct<T: ?Sized>(
        self,
        _name: &'static str,
        value: &T,
    ) -> Result<Value, Error>
    where
        T: Serialize,
    {
        value.serialize(self)
    }

    fn serialize_newtype_variant<T: ?Sized>(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        value: &T,
    ) -> Result<Value, Error>
    where
        T: Serialize,
    {
        let mut values = BTreeMap::new();
        values.insert(Value::from(variant.to_owned()), to_value(&value)?);
        Ok(Value::Map(values))
    }

    #[inline]
    fn serialize_none(self) -> Result<Value, Error> {
        self.serialize_unit()
    }

    #[inline]
    fn serialize_some<T: ?Sized>(self, value: &T) -> Result<Value, Error>
    where
        T: Serialize,
    {
        value.serialize(self)
    }

    fn serialize_seq(self, len: Option<usize>) -> Result<Self::SerializeSeq, Error> {
        Ok(SerializeVec {
            vec: Vec::with_capacity(len.unwrap_or(0)),
        })
    }

    fn serialize_tuple(self, len: usize) -> Result<Self::SerializeTuple, Error> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple_struct(
        self,
        _name: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleStruct, Error> {
        self.serialize_tuple(len)
    }

    fn serialize_tuple_variant(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleVariant, Error> {
        Ok(SerializeTupleVariant {
            name: String::from(variant),
            vec: Vec::with_capacity(len),
        })
    }

    fn serialize_map(self, _len: Option<usize>) -> Result<Self::SerializeMap, Error> {
        Ok(SerializeMap {
            map: BTreeMap::new(),
            next_key: None,
        })
    }

    fn serialize_struct(
        self,
        _name: &'static str,
        len: usize,
    ) -> Result<Self::SerializeStruct, Error> {
        self.serialize_map(Some(len))
    }

    fn serialize_struct_variant(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        _len: usize,
    ) -> Result<Self::SerializeStructVariant, Error> {
        Ok(SerializeStructVariant {
            name: String::from(variant),
            map: BTreeMap::new(),
        })
    }

    #[inline]
    fn is_human_readable(&self) -> bool {
        false
    }
}

pub struct SerializeVec {
    vec: Vec<Value>,
}

pub struct SerializeTupleVariant {
    name: String,
    vec: Vec<Value>,
}

pub struct SerializeMap {
    map: BTreeMap<Value, Value>,
    next_key: Option<Value>,
}

pub struct SerializeStructVariant {
    name: String,
    map: BTreeMap<Value, Value>,
}

impl serde::ser::SerializeSeq for SerializeVec {
    type Ok = Value;
    type Error = Error;

    fn serialize_element<T: ?Sized>(&mut self, value: &T) -> Result<(), Error>
    where
        T: Serialize,
    {
        self.vec.push(to_value(&value)?);
        Ok(())
    }

    fn end(self) -> Result<Value, Error> {
        Ok(Value::Array(self.vec))
    }
}

impl serde::ser::SerializeTuple for SerializeVec {
    type Ok = Value;
    type Error = Error;

    fn serialize_element<T: ?Sized>(&mut self, value: &T) -> Result<(), Error>
    where
        T: Serialize,
    {
        serde::ser::SerializeSeq::serialize_element(self, value)
    }

    fn end(self) -> Result<Value, Error> {
        serde::ser::SerializeSeq::end(self)
    }
}

impl serde::ser::SerializeTupleStruct for SerializeVec {
    type Ok = Value;
    type Error = Error;

    fn serialize_field<T: ?Sized>(&mut self, value: &T) -> Result<(), Error>
    where
        T: Serialize,
    {
        serde::ser::SerializeSeq::serialize_element(self, value)
    }

    fn end(self) -> Result<Value, Error> {
        serde::ser::SerializeSeq::end(self)
    }
}

impl serde::ser::SerializeTupleVariant for SerializeTupleVariant {
    type Ok = Value;
    type Error = Error;

    fn serialize_field<T: ?Sized>(&mut self, value: &T) -> Result<(), Error>
    where
        T: Serialize,
    {
        self.vec.push(to_value(&value)?);
        Ok(())
    }

    fn end(self) -> Result<Value, Error> {
        let mut object = BTreeMap::new();

        object.insert(Value::from(self.name), Value::Array(self.vec));

        Ok(Value::Map(object))
    }
}

impl serde::ser::SerializeMap for SerializeMap {
    type Ok = Value;
    type Error = Error;

    fn serialize_key<T: ?Sized>(&mut self, key: &T) -> Result<(), Error>
    where
        T: Serialize,
    {
        self.next_key = Some(to_value(&key)?);
        Ok(())
    }

    fn serialize_value<T: ?Sized>(&mut self, value: &T) -> Result<(), Error>
    where
        T: Serialize,
    {
        let key = self.next_key.take();
        // Panic because this indicates a bug in the program rather than an
        // expected failure.
        let key = key.expect("serialize_value called before serialize_key");
        self.map.insert(key, to_value(&value)?);
        Ok(())
    }

    fn end(self) -> Result<Value, Error> {
        Ok(Value::Map(self.map))
    }
}

impl serde::ser::SerializeStruct for SerializeMap {
    type Ok = Value;
    type Error = Error;

    fn serialize_field<T: ?Sized>(&mut self, key: &'static str, value: &T) -> Result<(), Error>
    where
        T: Serialize,
    {
        serde::ser::SerializeMap::serialize_key(self, key)?;
        serde::ser::SerializeMap::serialize_value(self, value)
    }

    fn end(self) -> Result<Value, Error> {
        serde::ser::SerializeMap::end(self)
    }
}

impl serde::ser::SerializeStructVariant for SerializeStructVariant {
    type Ok = Value;
    type Error = Error;

    fn serialize_field<T: ?Sized>(&mut self, key: &'static str, value: &T) -> Result<(), Error>
    where
        T: Serialize,
    {
        self.map
            .insert(Value::from(String::from(key)), to_value(&value)?);
        Ok(())
    }

    fn end(self) -> Result<Value, Error> {
        let mut object = BTreeMap::new();

        object.insert(Value::from(self.name), Value::Map(self.map));

        Ok(Value::Map(object))
    }
}

/// Convert a `T` into `serde_cbor::Value` which is an enum that can represent
/// any valid CBOR data.
///
/// ```rust
/// extern crate serde;
///
/// #[macro_use]
/// extern crate serde_derive;
/// extern crate serde_cbor;
///
/// use std::error::Error;
///
/// #[derive(Serialize)]
/// struct User {
///     fingerprint: String,
///     location: String,
/// }
///
/// fn main() {
///     let u = User {
///         fingerprint: "0xF9BA143B95FF6D82".to_owned(),
///         location: "Menlo Park, CA".to_owned(),
///     };
///
///     let v = serde_cbor::value::to_value(u).unwrap();
/// }
/// ```
#[allow(clippy::needless_pass_by_value)]
// Taking by value is more friendly to iterator adapters, option and result
pub fn to_value<T>(value: T) -> Result<Value, Error>
where
    T: Serialize,
{
    value.serialize(Serializer)
}
