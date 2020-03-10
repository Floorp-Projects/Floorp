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

use crate::backend::traits::BackendError;
use crate::error::StoreError;

#[derive(Debug)]
pub struct ErrorImpl(pub(crate) lmdb::Error);

impl BackendError for ErrorImpl {}

impl fmt::Display for ErrorImpl {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(fmt)
    }
}

impl Into<StoreError> for ErrorImpl {
    fn into(self) -> StoreError {
        match self.0 {
            lmdb::Error::NotFound => StoreError::KeyValuePairNotFound,
            lmdb::Error::BadValSize => StoreError::KeyValuePairBadSize,
            lmdb::Error::Invalid => StoreError::FileInvalid,
            lmdb::Error::MapFull => StoreError::MapFull,
            lmdb::Error::DbsFull => StoreError::DbsFull,
            lmdb::Error::ReadersFull => StoreError::ReadersFull,
            _ => StoreError::LmdbError(self.0),
        }
    }
}
