// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{io, num, str};

use thiserror::Error;

#[derive(Debug, Error)]
pub enum MigrateError {
    #[error("database not found: {0:?}")]
    DatabaseNotFound(String),

    #[error("{0}")]
    FromString(String),

    #[error("couldn't determine bit depth")]
    IndeterminateBitDepth,

    #[error("I/O error: {0:?}")]
    IoError(#[from] io::Error),

    #[error("invalid DatabaseFlags bits")]
    InvalidDatabaseBits,

    #[error("invalid data version")]
    InvalidDataVersion,

    #[error("invalid magic number")]
    InvalidMagicNum,

    #[error("invalid NodeFlags bits")]
    InvalidNodeBits,

    #[error("invalid PageFlags bits")]
    InvalidPageBits,

    #[error("invalid page number")]
    InvalidPageNum,

    #[error("lmdb backend error: {0}")]
    LmdbError(#[from] lmdb::Error),

    #[error("string conversion error")]
    StringConversionError,

    #[error("TryFromInt error: {0:?}")]
    TryFromIntError(#[from] num::TryFromIntError),

    #[error("unexpected Page variant")]
    UnexpectedPageVariant,

    #[error("unexpected PageHeader variant")]
    UnexpectedPageHeaderVariant,

    #[error("unsupported PageHeader variant")]
    UnsupportedPageHeaderVariant,

    #[error("UTF8 error: {0:?}")]
    Utf8Error(#[from] str::Utf8Error),
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
