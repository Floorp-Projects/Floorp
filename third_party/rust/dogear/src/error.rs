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
use crate::Item;

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
        // We format the guid-specific params with <guid: {}> to make it easier on the
        // telemetry side to parse out the user-specific guid and normalize the errors
        // to better aggregate the data
        match self.kind() {
            ErrorKind::MismatchedItemKind(local_item, remote_item) => write!(
                f,
                "Can't merge local {} <guid: {}> and remote {} <guid: {}>",
                local_item.kind, local_item.guid, remote_item.kind, remote_item.guid,
            ),
            ErrorKind::DuplicateItem(guid) => {
                write!(f, "Item <guid: {}> already exists in tree", guid)
            }
            ErrorKind::MissingItem(guid) => {
                write!(f, "Item <guid: {}> doesn't exist in tree", guid)
            }
            ErrorKind::InvalidParent(child, parent) => write!(
                f,
                "Can't insert {} <guid: {}> into {} <guid: {}>",
                child.kind, child.guid, parent.kind, parent.guid,
            ),
            ErrorKind::InvalidParentForUnknownChild(child_guid, parent) => write!(
                f,
                "Can't insert unknown child <guid: {}> into {} <guid: {}>",
                child_guid, parent.kind, parent.guid,
            ),
            ErrorKind::MissingParent(child, parent_guid) => write!(
                f,
                "Can't insert {} <guid: {}> into nonexistent parent <guid: {}>",
                child.kind, child.guid, parent_guid,
            ),
            ErrorKind::MissingParentForUnknownChild(child_guid, parent_guid) => write!(
                f,
                "Can't insert unknown child <guid: {}> into nonexistent parent <guid: {}>",
                child_guid, parent_guid,
            ),
            ErrorKind::Cycle(guid) => write!(f, "Item <guid: {}> can't contain itself", guid),
            ErrorKind::MergeConflict => write!(f, "Local tree changed during merge"),
            ErrorKind::UnmergedLocalItems => {
                write!(f, "Merged tree doesn't mention all items from local tree")
            }
            ErrorKind::UnmergedRemoteItems => {
                write!(f, "Merged tree doesn't mention all items from remote tree")
            }
            ErrorKind::InvalidGuid(invalid_guid) => {
                write!(
                    f,
                    "Merged tree contains invalid GUID <guid: {}>",
                    invalid_guid
                )
            }
            ErrorKind::InvalidByte(b) => write!(f, "Invalid byte <byte: {}> in UTF-16 encoding", b),
            ErrorKind::MalformedString(err) => err.fmt(f),
            ErrorKind::Abort => write!(f, "Operation aborted"),
        }
    }
}

#[derive(Debug)]
pub enum ErrorKind {
    MismatchedItemKind(Item, Item),
    DuplicateItem(Guid),
    InvalidParent(Item, Item),
    InvalidParentForUnknownChild(Guid, Item),
    MissingParent(Item, Guid),
    MissingParentForUnknownChild(Guid, Guid),
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
