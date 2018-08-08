// Copyright 2018 Mozilla
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
use lmdb;

use value::Type;

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
