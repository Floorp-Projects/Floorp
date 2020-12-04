// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{
    io,
    path::PathBuf,
    str,
    sync,
    thread,
    thread::ThreadId,
};

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
    #[fail(display = "manager poisoned")]
    ManagerPoisonError,

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

    #[fail(display = "environment path does not exist or not the right type: {:?}", _0)]
    UnsuitableEnvironmentPath(PathBuf),

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

impl<T> From<sync::PoisonError<T>> for StoreError {
    fn from(_: sync::PoisonError<T>) -> StoreError {
        StoreError::ManagerPoisonError
    }
}

#[derive(Debug, Fail)]
pub enum MigrateError {
    #[fail(display = "store error: {}", _0)]
    StoreError(StoreError),

    #[fail(display = "manager poisoned")]
    ManagerPoisonError,

    #[fail(display = "source is empty")]
    SourceEmpty,

    #[fail(display = "destination is not empty")]
    DestinationNotEmpty,
}

impl From<StoreError> for MigrateError {
    fn from(e: StoreError) -> MigrateError {
        MigrateError::StoreError(e)
    }
}

impl<T> From<sync::PoisonError<T>> for MigrateError {
    fn from(_: sync::PoisonError<T>) -> MigrateError {
        MigrateError::ManagerPoisonError
    }
}
