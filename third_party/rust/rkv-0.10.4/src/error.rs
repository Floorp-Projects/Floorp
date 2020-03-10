// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::path::PathBuf;

use bincode;
use failure::Fail;
use lmdb;

use crate::value::Type;

#[derive(Debug, Fail)]
pub enum DataError {
    #[fail(display = "unknown type tag: {}", _0)]
    UnknownType(u8),

    #[fail(display = "unexpected type tag: expected {}, got {}", expected, actual)]
    UnexpectedType {
        expected: Type,
        actual: Type,
    },

    #[fail(display = "empty data; expected tag")]
    Empty,

    #[fail(display = "invalid value for type {}: {}", value_type, err)]
    DecodingError {
        value_type: Type,
        err: Box<bincode::ErrorKind>,
    },

    #[fail(display = "couldn't encode value: {}", _0)]
    EncodingError(Box<bincode::ErrorKind>),

    #[fail(display = "invalid uuid bytes")]
    InvalidUuid,
}

impl From<Box<bincode::ErrorKind>> for DataError {
    fn from(e: Box<bincode::ErrorKind>) -> DataError {
        DataError::EncodingError(e)
    }
}

#[derive(Debug, Fail)]
pub enum StoreError {
    #[fail(display = "I/O error: {:?}", _0)]
    IoError(::std::io::Error),

    #[fail(display = "directory does not exist or not a directory: {:?}", _0)]
    DirectoryDoesNotExistError(PathBuf),

    #[fail(display = "data error: {:?}", _0)]
    DataError(DataError),

    #[fail(display = "lmdb error: {}", _0)]
    LmdbError(lmdb::Error),

    #[fail(display = "read transaction already exists in thread {:?}", _0)]
    ReadTransactionAlreadyExists(::std::thread::ThreadId),

    #[fail(display = "attempted to open DB during transaction in thread {:?}", _0)]
    OpenAttemptedDuringTransaction(::std::thread::ThreadId),
}

impl StoreError {
    pub fn open_during_transaction() -> StoreError {
        StoreError::OpenAttemptedDuringTransaction(::std::thread::current().id())
    }
}

impl From<lmdb::Error> for StoreError {
    fn from(e: lmdb::Error) -> StoreError {
        match e {
            lmdb::Error::BadRslot => StoreError::ReadTransactionAlreadyExists(::std::thread::current().id()),
            e => StoreError::LmdbError(e),
        }
    }
}

impl From<DataError> for StoreError {
    fn from(e: DataError) -> StoreError {
        StoreError::DataError(e)
    }
}

impl From<::std::io::Error> for StoreError {
    fn from(e: ::std::io::Error) -> StoreError {
        StoreError::IoError(e)
    }
}

#[derive(Debug, Fail)]
pub enum MigrateError {
    #[fail(display = "database not found: {:?}", _0)]
    DatabaseNotFound(String),

    #[fail(display = "{}", _0)]
    FromString(String),

    #[fail(display = "couldn't determine bit depth")]
    IndeterminateBitDepth,

    #[fail(display = "I/O error: {:?}", _0)]
    IoError(::std::io::Error),

    #[fail(display = "invalid DatabaseFlags bits")]
    InvalidDatabaseBits,

    #[fail(display = "invalid data version")]
    InvalidDataVersion,

    #[fail(display = "invalid magic number")]
    InvalidMagicNum,

    #[fail(display = "invalid NodeFlags bits")]
    InvalidNodeBits,

    #[fail(display = "invalid PageFlags bits")]
    InvalidPageBits,

    #[fail(display = "invalid page number")]
    InvalidPageNum,

    #[fail(display = "lmdb error: {}", _0)]
    LmdbError(lmdb::Error),

    #[fail(display = "string conversion error")]
    StringConversionError,

    #[fail(display = "TryFromInt error: {:?}", _0)]
    TryFromIntError(::std::num::TryFromIntError),

    #[fail(display = "unexpected Page variant")]
    UnexpectedPageVariant,

    #[fail(display = "unexpected PageHeader variant")]
    UnexpectedPageHeaderVariant,

    #[fail(display = "unsupported PageHeader variant")]
    UnsupportedPageHeaderVariant,

    #[fail(display = "UTF8 error: {:?}", _0)]
    Utf8Error(::std::str::Utf8Error),
}

impl From<::std::io::Error> for MigrateError {
    fn from(e: ::std::io::Error) -> MigrateError {
        MigrateError::IoError(e)
    }
}

impl From<::std::str::Utf8Error> for MigrateError {
    fn from(e: ::std::str::Utf8Error) -> MigrateError {
        MigrateError::Utf8Error(e)
    }
}

impl From<::std::num::TryFromIntError> for MigrateError {
    fn from(e: ::std::num::TryFromIntError) -> MigrateError {
        MigrateError::TryFromIntError(e)
    }
}

impl From<&str> for MigrateError {
    fn from(e: &str) -> MigrateError {
        MigrateError::FromString(e.to_string())
    }
}

impl From<String> for MigrateError {
    fn from(e: String) -> MigrateError {
        MigrateError::FromString(e)
    }
}

impl From<lmdb::Error> for MigrateError {
    fn from(e: lmdb::Error) -> MigrateError {
        match e {
            e => MigrateError::LmdbError(e),
        }
    }
}
