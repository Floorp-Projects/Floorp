// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::io;
use std::num;
use std::path::PathBuf;
use std::str;
use std::thread;
use std::thread::ThreadId;

use failure::Fail;

pub use crate::backend::SafeModeError;
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
    #[fail(display = "database corrupted")]
    DatabaseCorrupted,

    #[fail(display = "key/value pair not found")]
    KeyValuePairNotFound,

    #[fail(display = "unsupported size of key/DB name/data")]
    KeyValuePairBadSize,

    #[fail(display = "file is not a valid database")]
    FileInvalid,

    #[fail(display = "environment mapsize reached")]
    MapFull,

    #[fail(display = "environment maxdbs reached")]
    DbsFull,

    #[fail(display = "environment maxreaders reached")]
    ReadersFull,

    #[fail(display = "I/O error: {:?}", _0)]
    IoError(io::Error),

    #[fail(display = "directory does not exist or not a directory: {:?}", _0)]
    DirectoryDoesNotExistError(PathBuf),

    #[fail(display = "data error: {:?}", _0)]
    DataError(DataError),

    #[fail(display = "lmdb backend error: {}", _0)]
    LmdbError(lmdb::Error),

    #[fail(display = "safe mode backend error: {}", _0)]
    SafeModeError(SafeModeError),

    #[fail(display = "read transaction already exists in thread {:?}", _0)]
    ReadTransactionAlreadyExists(ThreadId),

    #[fail(display = "attempted to open DB during transaction in thread {:?}", _0)]
    OpenAttemptedDuringTransaction(ThreadId),

    #[fail(display = "other backing store error: {}", _0)]
    OtherError(i32),
}

impl StoreError {
    pub fn open_during_transaction() -> StoreError {
        StoreError::OpenAttemptedDuringTransaction(thread::current().id())
    }

    pub fn read_transaction_already_exists() -> StoreError {
        StoreError::ReadTransactionAlreadyExists(thread::current().id())
    }
}

impl From<DataError> for StoreError {
    fn from(e: DataError) -> StoreError {
        StoreError::DataError(e)
    }
}

impl From<io::Error> for StoreError {
    fn from(e: io::Error) -> StoreError {
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
    IoError(io::Error),

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

    #[fail(display = "lmdb backend error: {}", _0)]
    LmdbError(lmdb::Error),

    #[fail(display = "safe mode backend error: {}", _0)]
    SafeModeError(SafeModeError),

    #[fail(display = "string conversion error")]
    StringConversionError,

    #[fail(display = "TryFromInt error: {:?}", _0)]
    TryFromIntError(num::TryFromIntError),

    #[fail(display = "unexpected Page variant")]
    UnexpectedPageVariant,

    #[fail(display = "unexpected PageHeader variant")]
    UnexpectedPageHeaderVariant,

    #[fail(display = "unsupported PageHeader variant")]
    UnsupportedPageHeaderVariant,

    #[fail(display = "UTF8 error: {:?}", _0)]
    Utf8Error(str::Utf8Error),
}

impl From<io::Error> for MigrateError {
    fn from(e: io::Error) -> MigrateError {
        MigrateError::IoError(e)
    }
}

impl From<str::Utf8Error> for MigrateError {
    fn from(e: str::Utf8Error) -> MigrateError {
        MigrateError::Utf8Error(e)
    }
}

impl From<num::TryFromIntError> for MigrateError {
    fn from(e: num::TryFromIntError) -> MigrateError {
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
        MigrateError::LmdbError(e)
    }
}

impl From<SafeModeError> for MigrateError {
    fn from(e: SafeModeError) -> MigrateError {
        MigrateError::SafeModeError(e)
    }
}
