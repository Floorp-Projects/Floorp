// Copyright 2018-2019 Mozilla

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::{error, fmt, result, str::Utf8Error, string::FromUtf16Error};

use crate::guid::Guid;
use crate::tree::Kind;

pub type Result<T> = result::Result<T, Error>;

#[derive(Debug)]
pub struct Error(ErrorKind);

impl Error {
    pub fn kind(&self) -> &ErrorKind {
        &self.0
    }
}

impl error::Error for Error {
    fn source(&self) -> Option<&(dyn error::Error + 'static)> {
        match self.kind() {
            ErrorKind::MalformedString(err) => Some(err.as_ref()),
            _ => None,
        }
    }
}

impl From<ErrorKind> for Error {
    fn from(kind: ErrorKind) -> Error {
        Error(kind)
    }
}

impl From<FromUtf16Error> for Error {
    fn from(error: FromUtf16Error) -> Error {
        Error(ErrorKind::MalformedString(error.into()))
    }
}

impl From<Utf8Error> for Error {
    fn from(error: Utf8Error) -> Error {
        Error(ErrorKind::MalformedString(error.into()))
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.kind() {
            ErrorKind::MismatchedItemKind(local_kind, remote_kind) => write!(
                f,
                "Can't merge local kind {} and remote kind {}",
                local_kind, remote_kind
            ),
            ErrorKind::DuplicateItem(guid) => write!(f, "Item {} already exists in tree", guid),
            ErrorKind::MissingItem(guid) => write!(f, "Item {} doesn't exist in tree", guid),
            ErrorKind::InvalidParent(child_guid, parent_guid) => write!(
                f,
                "Can't insert item {} into non-folder {}",
                child_guid, parent_guid
            ),
            ErrorKind::MissingParent(child_guid, parent_guid) => write!(
                f,
                "Can't insert item {} into nonexistent parent {}",
                child_guid, parent_guid
            ),
            ErrorKind::Cycle(guid) => write!(f, "Item {} can't contain itself", guid),
            ErrorKind::MergeConflict => write!(f, "Local tree changed during merge"),
            ErrorKind::UnmergedLocalItems => {
                write!(f, "Merged tree doesn't mention all items from local tree")
            }
            ErrorKind::UnmergedRemoteItems => {
                write!(f, "Merged tree doesn't mention all items from remote tree")
            }
            ErrorKind::InvalidGuid(invalid_guid) => {
                write!(f, "Merged tree contains invalid GUID {}", invalid_guid)
            }
            ErrorKind::InvalidByte(b) => write!(f, "Invalid byte {} in UTF-16 encoding", b),
            ErrorKind::MalformedString(err) => err.fmt(f),
            ErrorKind::Abort => write!(f, "Operation aborted"),
        }
    }
}

#[derive(Debug)]
pub enum ErrorKind {
    MismatchedItemKind(Kind, Kind),
    DuplicateItem(Guid),
    InvalidParent(Guid, Guid),
    MissingParent(Guid, Guid),
    MissingItem(Guid),
    Cycle(Guid),
    MergeConflict,
    UnmergedLocalItems,
    UnmergedRemoteItems,
    InvalidGuid(Guid),
    InvalidByte(u16),
    MalformedString(Box<dyn error::Error + Send + Sync + 'static>),
    Abort,
}
