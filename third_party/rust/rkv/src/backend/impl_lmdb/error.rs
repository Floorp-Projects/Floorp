// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{fmt, io, path::PathBuf};

use crate::{backend::traits::BackendError, error::StoreError};

#[derive(Debug)]
pub enum ErrorImpl {
    LmdbError(lmdb::Error),
    UnsuitableEnvironmentPath(PathBuf),
    IoError(io::Error),
}

impl BackendError for ErrorImpl {}

impl fmt::Display for ErrorImpl {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self {
            ErrorImpl::LmdbError(e) => e.fmt(fmt),
            ErrorImpl::UnsuitableEnvironmentPath(_) => write!(fmt, "UnsuitableEnvironmentPath"),
            ErrorImpl::IoError(e) => e.fmt(fmt),
        }
    }
}

impl Into<StoreError> for ErrorImpl {
    fn into(self) -> StoreError {
        match self {
            ErrorImpl::LmdbError(lmdb::Error::Corrupted) => StoreError::DatabaseCorrupted,
            ErrorImpl::LmdbError(lmdb::Error::NotFound) => StoreError::KeyValuePairNotFound,
            ErrorImpl::LmdbError(lmdb::Error::BadValSize) => StoreError::KeyValuePairBadSize,
            ErrorImpl::LmdbError(lmdb::Error::Invalid) => StoreError::FileInvalid,
            ErrorImpl::LmdbError(lmdb::Error::MapFull) => StoreError::MapFull,
            ErrorImpl::LmdbError(lmdb::Error::DbsFull) => StoreError::DbsFull,
            ErrorImpl::LmdbError(lmdb::Error::ReadersFull) => StoreError::ReadersFull,
            ErrorImpl::LmdbError(error) => StoreError::LmdbError(error),
            ErrorImpl::UnsuitableEnvironmentPath(path) => {
                StoreError::UnsuitableEnvironmentPath(path)
            }
            ErrorImpl::IoError(error) => StoreError::IoError(error),
        }
    }
}

impl From<io::Error> for ErrorImpl {
    fn from(e: io::Error) -> ErrorImpl {
        ErrorImpl::IoError(e)
    }
}
