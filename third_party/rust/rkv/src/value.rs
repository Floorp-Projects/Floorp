// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::fmt;

use arrayref::array_ref;
use bincode::{deserialize, serialize, serialized_size};
use ordered_float::OrderedFloat;
use uuid::{Bytes, Uuid};

use crate::error::DataError;

/// We define a set of types, associated with simple integers, to annotate values stored
/// in LMDB. This is to avoid an accidental 'cast' from a value of one type to another.
/// For this reason we don't simply use `deserialize` from the `bincode` crate.
#[repr(u8)]
#[derive(Debug, PartialEq, Eq)]
pub enum Type {
    Bool = 1,
    U64 = 2,
    I64 = 3,
    F64 = 4,
    Instant = 5, // Millisecond-precision timestamp.
    Uuid = 6,
    Str = 7,
    Json = 8,
    Blob = 9,
}

/// We use manual tagging, because <https://github.com/serde-rs/serde/issues/610>.
impl Type {
    pub fn from_tag(tag: u8) -> Result<Type, DataError> {
        #![allow(clippy::unnecessary_lazy_evaluations)]
        Type::from_primitive(tag).ok_or_else(|| DataError::UnknownType(tag))
    }

    #[allow(clippy::wrong_self_convention)]
    pub fn to_tag(self) -> u8 {
        self as u8
    }

    fn from_primitive(p: u8) -> Option<Type> {
        match p {
            1 => Some(Type::Bool),
            2 => Some(Type::U64),
            3 => Some(Type::I64),
            4 => Some(Type::F64),
            5 => Some(Type::Instant),
            6 => Some(Type::Uuid),
            7 => Some(Type::Str),
            8 => Some(Type::Json),
            9 => Some(Type::Blob),
            _ => None,
        }
    }
}

impl fmt::Display for Type {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        f.write_str(match *self {
            Type::Bool => "bool",
            Type::U64 => "u64",
            Type::I64 => "i64",
            Type::F64 => "f64",
            Type::Instant => "instant",
            Type::Uuid => "uuid",
            Type::Str => "str",
            Type::Json => "json",
            Type::Blob => "blob",
        })
    }
}

#[derive(Debug, Eq, PartialEq)]
pub enum Value<'v> {
    Bool(bool),
    U64(u64),
    I64(i64),
    F64(OrderedFloat<f64>),
    Instant(i64), // Millisecond-precision timestamp.
    Uuid(&'v Bytes),
    Str(&'v str),
    Json(&'v str),
    Blob(&'v [u8]),
}

#[derive(Clone, Debug, PartialEq)]
pub enum OwnedValue {
    Bool(bool),
    U64(u64),
    I64(i64),
    F64(f64),
    Instant(i64), // Millisecond-precision timestamp.
    Uuid(Uuid),
    Str(String),
    Json(String), // TODO
    Blob(Vec<u8>),
}

fn uuid(bytes: &[u8]) -> Result<Value, DataError> {
    if bytes.len() == 16 {
        Ok(Value::Uuid(array_ref![bytes, 0, 16]))
    } else {
        Err(DataError::InvalidUuid)
    }
}

impl<'v> Value<'v> {
    pub fn from_tagged_slice(slice: &'v [u8]) -> Result<Value<'v>, DataError> {
        let (tag, data) = slice.split_first().ok_or(DataError::Empty)?;
        let t = Type::from_tag(*tag)?;
        Value::from_type_and_data(t, data)
    }

    fn from_type_and_data(t: Type, data: &'v [u8]) -> Result<Value<'v>, DataError> {
        if t == Type::Uuid {
            return deserialize(data)
                .map_err(|e| DataError::DecodingError {
                    value_type: t,
                    err: e,
                })
                .map(uuid)?;
        }

        match t {
            Type::Bool => deserialize(data).map(Value::Bool),
            Type::U64 => deserialize(data).map(Value::U64),
            Type::I64 => deserialize(data).map(Value::I64),
            Type::F64 => deserialize(data).map(OrderedFloat).map(Value::F64),
            Type::Instant => deserialize(data).map(Value::Instant),
            Type::Str => deserialize(data).map(Value::Str),
            Type::Json => deserialize(data).map(Value::Json),
            Type::Blob => deserialize(data).map(Value::Blob),
            Type::Uuid => {
                // Processed above to avoid verbose duplication of error transforms.
                unreachable!()
            }
        }
        .map_err(|e| DataError::DecodingError {
            value_type: t,
            err: e,
        })
    }

    pub fn to_bytes(&self) -> Result<Vec<u8>, DataError> {
        match self {
            Value::Bool(v) => serialize(&(Type::Bool.to_tag(), *v)),
            Value::U64(v) => serialize(&(Type::U64.to_tag(), *v)),
            Value::I64(v) => serialize(&(Type::I64.to_tag(), *v)),
            Value::F64(v) => serialize(&(Type::F64.to_tag(), v.0)),
            Value::Instant(v) => serialize(&(Type::Instant.to_tag(), *v)),
            Value::Str(v) => serialize(&(Type::Str.to_tag(), v)),
            Value::Json(v) => serialize(&(Type::Json.to_tag(), v)),
            Value::Blob(v) => serialize(&(Type::Blob.to_tag(), v)),
            Value::Uuid(v) => serialize(&(Type::Uuid.to_tag(), v)),
        }
        .map_err(DataError::EncodingError)
    }

    pub fn serialized_size(&self) -> Result<u64, DataError> {
        match self {
            Value::Bool(v) => serialized_size(&(Type::Bool.to_tag(), *v)),
            Value::U64(v) => serialized_size(&(Type::U64.to_tag(), *v)),
            Value::I64(v) => serialized_size(&(Type::I64.to_tag(), *v)),
            Value::F64(v) => serialized_size(&(Type::F64.to_tag(), v.0)),
            Value::Instant(v) => serialized_size(&(Type::Instant.to_tag(), *v)),
            Value::Str(v) => serialized_size(&(Type::Str.to_tag(), v)),
            Value::Json(v) => serialized_size(&(Type::Json.to_tag(), v)),
            Value::Blob(v) => serialized_size(&(Type::Blob.to_tag(), v)),
            Value::Uuid(v) => serialized_size(&(Type::Uuid.to_tag(), v)),
        }
        .map_err(DataError::EncodingError)
    }
}

impl<'v> From<&'v Value<'v>> for OwnedValue {
    fn from(value: &Value) -> OwnedValue {
        match value {
            Value::Bool(v) => OwnedValue::Bool(*v),
            Value::U64(v) => OwnedValue::U64(*v),
            Value::I64(v) => OwnedValue::I64(*v),
            Value::F64(v) => OwnedValue::F64(**v),
            Value::Instant(v) => OwnedValue::Instant(*v),
            Value::Uuid(v) => OwnedValue::Uuid(Uuid::from_bytes(**v)),
            Value::Str(v) => OwnedValue::Str((*v).to_string()),
            Value::Json(v) => OwnedValue::Json((*v).to_string()),
            Value::Blob(v) => OwnedValue::Blob(v.to_vec()),
        }
    }
}

impl<'v> From<&'v OwnedValue> for Value<'v> {
    fn from(value: &OwnedValue) -> Value {
        match value {
            OwnedValue::Bool(v) => Value::Bool(*v),
            OwnedValue::U64(v) => Value::U64(*v),
            OwnedValue::I64(v) => Value::I64(*v),
            OwnedValue::F64(v) => Value::F64(OrderedFloat::from(*v)),
            OwnedValue::Instant(v) => Value::Instant(*v),
            OwnedValue::Uuid(v) => Value::Uuid(v.as_bytes()),
            OwnedValue::Str(v) => Value::Str(v),
            OwnedValue::Json(v) => Value::Json(v),
            OwnedValue::Blob(v) => Value::Blob(v),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_value_serialized_size() {
        // | Value enum    | tag: 1 byte   |     value_payload        |
        // |----------------------------------------------------------|
        // |   I64         |     1         |       8                  |
        // |   U64         |     1         |       8                  |
        // |   Bool        |     1         |       1                  |
        // |   Instant     |     1         |       8                  |
        // |   F64         |     1         |       8                  |
        // |   Uuid        |     1         |       16                 |
        // | Str/Blob/Json |     1         |(8: len + sizeof(payload))|
        assert_eq!(Value::I64(-1000).serialized_size().unwrap(), 9);
        assert_eq!(Value::U64(1000u64).serialized_size().unwrap(), 9);
        assert_eq!(Value::Bool(true).serialized_size().unwrap(), 2);
        assert_eq!(
            Value::Instant(1_558_020_865_224).serialized_size().unwrap(),
            9
        );
        assert_eq!(
            Value::F64(OrderedFloat(10000.1)).serialized_size().unwrap(),
            9
        );
        assert_eq!(Value::Str("hello!").serialized_size().unwrap(), 15);
        assert_eq!(Value::Str("¡Hola").serialized_size().unwrap(), 15);
        assert_eq!(Value::Blob(b"hello!").serialized_size().unwrap(), 15);
        assert_eq!(
            uuid(b"\x9f\xe2\xc4\xe9\x3f\x65\x4f\xdb\xb2\x4c\x02\xb1\x52\x59\x71\x6c")
                .unwrap()
                .serialized_size()
                .unwrap(),
            17
        );
    }
}
