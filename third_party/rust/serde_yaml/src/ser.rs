//! YAML Serialization
//!
//! This module provides YAML serialization with the type `Serializer`.

use std::{fmt, io, num, str};

use yaml_rust::{yaml, Yaml, YamlEmitter};

use serde::ser;

use super::error::{Error, Result};
use private;

pub struct Serializer;

impl ser::Serializer for Serializer {
    type Ok = Yaml;
    type Error = Error;

    type SerializeSeq = SerializeArray;
    type SerializeTuple = SerializeArray;
    type SerializeTupleStruct = SerializeArray;
    type SerializeTupleVariant = SerializeTupleVariant;
    type SerializeMap = SerializeMap;
    type SerializeStruct = SerializeStruct;
    type SerializeStructVariant = SerializeStructVariant;

    fn serialize_bool(self, v: bool) -> Result<Yaml> {
        Ok(Yaml::Boolean(v))
    }

    fn serialize_i8(self, v: i8) -> Result<Yaml> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i16(self, v: i16) -> Result<Yaml> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i32(self, v: i32) -> Result<Yaml> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i64(self, v: i64) -> Result<Yaml> {
        Ok(Yaml::Integer(v))
    }

    serde_if_integer128! {
        #[cfg_attr(feature = "cargo-clippy", allow(cast_possible_truncation))]
        fn serialize_i128(self, v: i128) -> Result<Yaml> {
            if v <= i64::max_value() as i128 && v >= i64::min_value() as i128 {
                self.serialize_i64(v as i64)
            } else {
                Ok(Yaml::Real(v.to_string()))
            }
        }
    }

    fn serialize_u8(self, v: u8) -> Result<Yaml> {
        self.serialize_i64(v as i64)
    }

    fn serialize_u16(self, v: u16) -> Result<Yaml> {
        self.serialize_i64(v as i64)
    }

    fn serialize_u32(self, v: u32) -> Result<Yaml> {
        self.serialize_i64(v as i64)
    }

    fn serialize_u64(self, v: u64) -> Result<Yaml> {
        if v <= i64::max_value() as u64 {
            self.serialize_i64(v as i64)
        } else {
            Ok(Yaml::Real(v.to_string()))
        }
    }

    serde_if_integer128! {
        #[cfg_attr(feature = "cargo-clippy", allow(cast_possible_truncation))]
        fn serialize_u128(self, v: u128) -> Result<Yaml> {
            if v <= i64::max_value() as u128 {
                self.serialize_i64(v as i64)
            } else {
                Ok(Yaml::Real(v.to_string()))
            }
        }
    }

    fn serialize_f32(self, v: f32) -> Result<Yaml> {
        self.serialize_f64(v as f64)
    }

    fn serialize_f64(self, v: f64) -> Result<Yaml> {
        Ok(Yaml::Real(match v.classify() {
            num::FpCategory::Infinite if v.is_sign_positive() => ".inf".into(),
            num::FpCategory::Infinite => "-.inf".into(),
            num::FpCategory::Nan => ".nan".into(),
            _ => {
                let mut buf = vec![];
                ::dtoa::write(&mut buf, v).unwrap();
                ::std::str::from_utf8(&buf).unwrap().into()
            }
        }))
    }

    fn serialize_char(self, value: char) -> Result<Yaml> {
        Ok(Yaml::String(value.to_string()))
    }

    fn serialize_str(self, value: &str) -> Result<Yaml> {
        Ok(Yaml::String(value.to_owned()))
    }

    fn serialize_bytes(self, value: &[u8]) -> Result<Yaml> {
        let vec = value.iter().map(|&b| Yaml::Integer(b as i64)).collect();
        Ok(Yaml::Array(vec))
    }

    fn serialize_unit(self) -> Result<Yaml> {
        Ok(Yaml::Null)
    }

    fn serialize_unit_struct(self, _name: &'static str) -> Result<Yaml> {
        self.serialize_unit()
    }

    fn serialize_unit_variant(
        self,
        _name: &str,
        _variant_index: u32,
        variant: &str,
    ) -> Result<Yaml> {
        Ok(Yaml::String(variant.to_owned()))
    }

    fn serialize_newtype_struct<T: ?Sized>(self, _name: &'static str, value: &T) -> Result<Yaml>
    where
        T: ser::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_newtype_variant<T: ?Sized>(
        self,
        _name: &str,
        _variant_index: u32,
        variant: &str,
        value: &T,
    ) -> Result<Yaml>
    where
        T: ser::Serialize,
    {
        Ok(singleton_hash(to_yaml(variant)?, to_yaml(value)?))
    }

    fn serialize_none(self) -> Result<Yaml> {
        self.serialize_unit()
    }

    fn serialize_some<V: ?Sized>(self, value: &V) -> Result<Yaml>
    where
        V: ser::Serialize,
    {
        value.serialize(self)
    }

    fn serialize_seq(self, len: Option<usize>) -> Result<SerializeArray> {
        let array = match len {
            None => yaml::Array::new(),
            Some(len) => yaml::Array::with_capacity(len),
        };
        Ok(SerializeArray { array: array })
    }

    fn serialize_tuple(self, len: usize) -> Result<SerializeArray> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple_struct(self, _name: &'static str, len: usize) -> Result<SerializeArray> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple_variant(
        self,
        _enum: &'static str,
        _idx: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<SerializeTupleVariant> {
        Ok(SerializeTupleVariant {
            name: variant,
            array: yaml::Array::with_capacity(len),
        })
    }

    fn serialize_map(self, _len: Option<usize>) -> Result<SerializeMap> {
        Ok(SerializeMap {
            hash: yaml::Hash::new(),
            next_key: None,
        })
    }

    fn serialize_struct(self, _name: &'static str, _len: usize) -> Result<SerializeStruct> {
        Ok(SerializeStruct {
            hash: yaml::Hash::new(),
        })
    }

    fn serialize_struct_variant(
        self,
        _enum: &'static str,
        _idx: u32,
        variant: &'static str,
        _len: usize,
    ) -> Result<SerializeStructVariant> {
        Ok(SerializeStructVariant {
            name: variant,
            hash: yaml::Hash::new(),
        })
    }
}

#[doc(hidden)]
pub struct SerializeArray {
    array: yaml::Array,
}

#[doc(hidden)]
pub struct SerializeTupleVariant {
    name: &'static str,
    array: yaml::Array,
}

#[doc(hidden)]
pub struct SerializeMap {
    hash: yaml::Hash,
    next_key: Option<yaml::Yaml>,
}

#[doc(hidden)]
pub struct SerializeStruct {
    hash: yaml::Hash,
}

#[doc(hidden)]
pub struct SerializeStructVariant {
    name: &'static str,
    hash: yaml::Hash,
}

impl ser::SerializeSeq for SerializeArray {
    type Ok = yaml::Yaml;
    type Error = Error;

    fn serialize_element<T: ?Sized>(&mut self, elem: &T) -> Result<()>
    where
        T: ser::Serialize,
    {
        self.array.push(to_yaml(elem)?);
        Ok(())
    }

    fn end(self) -> Result<Yaml> {
        Ok(Yaml::Array(self.array))
    }
}

impl ser::SerializeTuple for SerializeArray {
    type Ok = yaml::Yaml;
    type Error = Error;

    fn serialize_element<T: ?Sized>(&mut self, elem: &T) -> Result<()>
    where
        T: ser::Serialize,
    {
        ser::SerializeSeq::serialize_element(self, elem)
    }

    fn end(self) -> Result<Yaml> {
        ser::SerializeSeq::end(self)
    }
}

impl ser::SerializeTupleStruct for SerializeArray {
    type Ok = yaml::Yaml;
    type Error = Error;

    fn serialize_field<V: ?Sized>(&mut self, value: &V) -> Result<()>
    where
        V: ser::Serialize,
    {
        ser::SerializeSeq::serialize_element(self, value)
    }

    fn end(self) -> Result<Yaml> {
        ser::SerializeSeq::end(self)
    }
}

impl ser::SerializeTupleVariant for SerializeTupleVariant {
    type Ok = yaml::Yaml;
    type Error = Error;

    fn serialize_field<V: ?Sized>(&mut self, v: &V) -> Result<()>
    where
        V: ser::Serialize,
    {
        self.array.push(to_yaml(v)?);
        Ok(())
    }

    fn end(self) -> Result<Yaml> {
        Ok(singleton_hash(to_yaml(self.name)?, Yaml::Array(self.array)))
    }
}

impl ser::SerializeMap for SerializeMap {
    type Ok = yaml::Yaml;
    type Error = Error;

    fn serialize_key<T: ?Sized>(&mut self, key: &T) -> Result<()>
    where
        T: ser::Serialize,
    {
        self.next_key = Some(to_yaml(key)?);
        Ok(())
    }

    fn serialize_value<T: ?Sized>(&mut self, value: &T) -> Result<()>
    where
        T: ser::Serialize,
    {
        match self.next_key.take() {
            Some(key) => self.hash.insert(key, to_yaml(value)?),
            None => panic!("serialize_value called before serialize_key"),
        };
        Ok(())
    }

    fn serialize_entry<K: ?Sized, V: ?Sized>(&mut self, key: &K, value: &V) -> Result<()>
    where
        K: ser::Serialize,
        V: ser::Serialize,
    {
        self.hash.insert(to_yaml(key)?, to_yaml(value)?);
        Ok(())
    }

    fn end(self) -> Result<Yaml> {
        Ok(Yaml::Hash(self.hash))
    }
}

impl ser::SerializeStruct for SerializeStruct {
    type Ok = yaml::Yaml;
    type Error = Error;

    fn serialize_field<V: ?Sized>(&mut self, key: &'static str, value: &V) -> Result<()>
    where
        V: ser::Serialize,
    {
        self.hash.insert(to_yaml(key)?, to_yaml(value)?);
        Ok(())
    }

    fn end(self) -> Result<Yaml> {
        Ok(Yaml::Hash(self.hash))
    }
}

impl ser::SerializeStructVariant for SerializeStructVariant {
    type Ok = yaml::Yaml;
    type Error = Error;

    fn serialize_field<V: ?Sized>(&mut self, field: &'static str, v: &V) -> Result<()>
    where
        V: ser::Serialize,
    {
        self.hash.insert(to_yaml(field)?, to_yaml(v)?);
        Ok(())
    }

    fn end(self) -> Result<Yaml> {
        Ok(singleton_hash(to_yaml(self.name)?, Yaml::Hash(self.hash)))
    }
}

/// Serialize the given data structure as YAML into the IO stream.
///
/// Serialization can fail if `T`'s implementation of `Serialize` decides to
/// return an error.
pub fn to_writer<W, T: ?Sized>(writer: W, value: &T) -> Result<()>
where
    W: io::Write,
    T: ser::Serialize,
{
    let doc = to_yaml(value)?;
    let mut writer_adapter = FmtToIoWriter { writer: writer };
    YamlEmitter::new(&mut writer_adapter)
        .dump(&doc)
        .map_err(private::error_emitter)?;
    Ok(())
}

/// Serialize the given data structure as a YAML byte vector.
///
/// Serialization can fail if `T`'s implementation of `Serialize` decides to
/// return an error.
pub fn to_vec<T: ?Sized>(value: &T) -> Result<Vec<u8>>
where
    T: ser::Serialize,
{
    let mut vec = Vec::with_capacity(128);
    to_writer(&mut vec, value)?;
    Ok(vec)
}

/// Serialize the given data structure as a String of YAML.
///
/// Serialization can fail if `T`'s implementation of `Serialize` decides to
/// return an error.
pub fn to_string<T: ?Sized>(value: &T) -> Result<String>
where
    T: ser::Serialize,
{
    Ok(String::from_utf8(to_vec(value)?).map_err(private::error_string_utf8)?)
}

/// The yaml-rust library uses `fmt::Write` intead of `io::Write` so this is a
/// simple adapter.
struct FmtToIoWriter<W> {
    writer: W,
}

impl<W> fmt::Write for FmtToIoWriter<W>
where
    W: io::Write,
{
    fn write_str(&mut self, s: &str) -> fmt::Result {
        if self.writer.write(s.as_bytes()).is_err() {
            return Err(fmt::Error);
        }
        Ok(())
    }
}

fn to_yaml<T>(elem: T) -> Result<Yaml>
where
    T: ser::Serialize,
{
    elem.serialize(Serializer)
}

fn singleton_hash(k: Yaml, v: Yaml) -> Yaml {
    let mut hash = yaml::Hash::new();
    hash.insert(k, v);
    Yaml::Hash(hash)
}
