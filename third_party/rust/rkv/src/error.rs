// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{io, path::PathBuf, sync, thread, thread::ThreadId};

use thiserror::Error;

pub use crate::backend::SafeModeError;
use crate::value::Type;

#[derive(Debug, Error)]
pub enum DataError {
    #[error("unknown type tag: {0}")]
    UnknownType(u8),

    #[error("unexpected type tag: expected {expected}, got {actual}")]
    UnexpectedType { expected: Type, actual: Type },

    #[error("empty data; expected tag")]
    Empty,

    #[error("invalid value for type {value_type}: {err}")]
    DecodingError {
        value_type: Type,
        err: Box<bincode::ErrorKind>,
    },

    #[error("couldn't encode value: {0}")]
    EncodingError(#[from] Box<bincode::ErrorKind>),

    #[error("invalid uuid bytes")]
    InvalidUuid,
}

#[derive(Debug, Error)]
pub enum StoreError {
    #[error("manager poisoned")]
    ManagerPoisonError,

    #[error("database corrupted")]
    DatabaseCorrupted,

    #[error("key/value pair not found")]
    KeyValuePairNotFound,

    #[error("unsupported size of key/DB name/data")]
    KeyValuePairBadSize,

    #[error("file is not a valid database")]
    FileInvalid,

    #[error("environment mapsize reached")]
    MapFull,

    #[error("environment maxdbs reached")]
    DbsFull,

    #[error("environment maxreaders reached")]
    ReadersFull,

    #[error("I/O error: {0:?}")]
    IoError(#[from] io::Error),

    #[error("environment path does not exist or not the right type: {0:?}")]
    UnsuitableEnvironmentPath(PathBuf),

    #[error("data error: {0:?}")]
    DataError(#[from] DataError),

    #[cfg(feature = "lmdb")]
    #[error("lmdb backend error: {0}")]
    LmdbError(lmdb::Error),

    #[error("safe mode backend error: {0}")]
    SafeModeError(SafeModeError),

    #[error("read transaction already exists in thread {0:?}")]
    ReadTransactionAlreadyExists(ThreadId),

    #[error("attempted to open DB during transaction in thread {0:?}")]
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

impl<T> From<sync::PoisonError<T>> for StoreError {
    fn from(_: sync::PoisonError<T>) -> StoreError {
        StoreError::ManagerPoisonError
    }
}

#[derive(Debug, Error)]
pub enum CloseError {
    #[error("manager poisoned")]
    ManagerPoisonError,

    #[error("close attempted while manager has an environment still open")]
    EnvironmentStillOpen,

    #[error("close attempted while an environment not known to the manager is still open")]
    UnknownEnvironmentStillOpen,

    #[error("I/O error: {0:?}")]
    IoError(#[from] io::Error),
}

impl<T> From<sync::PoisonError<T>> for CloseError {
    fn from(_: sync::PoisonError<T>) -> CloseError {
        CloseError::ManagerPoisonError
    }
}

#[derive(Debug, Error)]
pub enum MigrateError {
    #[error("store error: {0}")]
    StoreError(#[from] StoreError),

    #[error("close error: {0}")]
    CloseError(#[from] CloseError),

    #[error("manager poisoned")]
    ManagerPoisonError,

    #[error("source is empty")]
    SourceEmpty,

    #[error("destination is not empty")]
    DestinationNotEmpty,
}

impl<T> From<sync::PoisonError<T>> for MigrateError {
    fn from(_: sync::PoisonError<T>) -> MigrateError {
        MigrateError::ManagerPoisonError
    }
}
