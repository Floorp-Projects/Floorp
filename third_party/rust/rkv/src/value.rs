// Copyright 2018 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use ordered_float::OrderedFloat;

use bincode::{
    deserialize,
    serialize,
};

use uuid::{
    Uuid,
    UuidBytes,
};

use error::DataError;

/// We define a set of types, associated with simple integers, to annotate values
/// stored in LMDB. This is to avoid an accidental 'cast' from a value of one type
/// to another. For this reason we don't simply use `deserialize` from the `bincode`
/// crate.
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
        Type::from_primitive(tag).ok_or(DataError::UnknownType(tag))
    }

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

impl ::std::fmt::Display for Type {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> Result<(), ::std::fmt::Error> {
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
pub enum Value<'s> {
    Bool(bool),
    U64(u64),
    I64(i64),
    F64(OrderedFloat<f64>),
    Instant(i64), // Millisecond-precision timestamp.
    Uuid(&'s UuidBytes),
    Str(&'s str),
    Json(&'s str),
    Blob(&'s [u8]),
}

// TODO: implement conversion between the two types of `Value` wrapper.
// This might be unnecessary: we'll probably jump straight to primitives.
enum OwnedValue {
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

fn uuid<'s>(bytes: &'s [u8]) -> Result<Value<'s>, DataError> {
    if bytes.len() == 16 {
        Ok(Value::Uuid(array_ref![bytes, 0, 16]))
    } else {
        Err(DataError::InvalidUuid)
    }
}

impl<'s> Value<'s> {
    fn expected_from_tagged_slice(expected: Type, slice: &'s [u8]) -> Result<Value<'s>, DataError> {
        let (tag, data) = slice.split_first().ok_or(DataError::Empty)?;
        let t = Type::from_tag(*tag)?;
        if t == expected {
            return Err(DataError::UnexpectedType {
                expected,
                actual: t,
            });
        }
        Value::from_type_and_data(t, data)
    }

    pub fn from_tagged_slice(slice: &'s [u8]) -> Result<Value<'s>, DataError> {
        let (tag, data) = slice.split_first().ok_or(DataError::Empty)?;
        let t = Type::from_tag(*tag)?;
        Value::from_type_and_data(t, data)
    }

    fn from_type_and_data(t: Type, data: &'s [u8]) -> Result<Value<'s>, DataError> {
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
            },
        }.map_err(|e| DataError::DecodingError {
            value_type: t,
            err: e,
        })
    }

    pub fn to_bytes(&self) -> Result<Vec<u8>, DataError> {
        match self {
            Value::Bool(ref v) => serialize(&(Type::Bool.to_tag(), *v)),
            Value::U64(ref v) => serialize(&(Type::U64.to_tag(), *v)),
            Value::I64(ref v) => serialize(&(Type::I64.to_tag(), *v)),
            Value::F64(ref v) => serialize(&(Type::F64.to_tag(), v.0)),
            Value::Instant(ref v) => serialize(&(Type::Instant.to_tag(), *v)),
            Value::Str(ref v) => serialize(&(Type::Str.to_tag(), v)),
            Value::Json(ref v) => serialize(&(Type::Json.to_tag(), v)),
            Value::Blob(ref v) => serialize(&(Type::Blob.to_tag(), v)),
            Value::Uuid(ref v) => {
                // Processed above to avoid verbose duplication of error transforms.
                serialize(&(Type::Uuid.to_tag(), v))
            },
        }.map_err(DataError::EncodingError)
    }
}
