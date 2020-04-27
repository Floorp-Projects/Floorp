/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use failure::Fail;
use interrupt_support::Interrupted;

#[derive(Debug)]
pub enum QuotaReason {
    TotalBytes,
    ItemBytes,
    MaxItems,
}

#[derive(Debug, Fail)]
pub enum ErrorKind {
    #[fail(display = "Quota exceeded: {:?}", _0)]
    QuotaError(QuotaReason),

    #[fail(display = "Error parsing JSON data: {}", _0)]
    JsonError(#[fail(cause)] serde_json::Error),

    #[fail(display = "Error executing SQL: {}", _0)]
    SqlError(#[fail(cause)] rusqlite::Error),

    #[fail(display = "A connection of this type is already open")]
    ConnectionAlreadyOpen,

    #[fail(display = "An invalid connection type was specified")]
    InvalidConnectionType,

    #[fail(display = "IO error: {}", _0)]
    IoError(#[fail(cause)] std::io::Error),

    #[fail(display = "Operation interrupted")]
    InterruptedError(#[fail(cause)] Interrupted),

    #[fail(display = "Tried to close connection on wrong StorageApi instance")]
    WrongApiForClose,

    // This will happen if you provide something absurd like
    // "/" or "" as your database path. For more subtley broken paths,
    // we'll likely return an IoError.
    #[fail(display = "Illegal database path: {:?}", _0)]
    IllegalDatabasePath(std::path::PathBuf),

    #[fail(display = "UTF8 Error: {}", _0)]
    Utf8Error(#[fail(cause)] std::str::Utf8Error),

    #[fail(display = "Database cannot be upgraded")]
    DatabaseUpgradeError,

    #[fail(display = "Database version {} is not supported", _0)]
    UnsupportedDatabaseVersion(i64),
}

error_support::define_error! {
    ErrorKind {
        (JsonError, serde_json::Error),
        (SqlError, rusqlite::Error),
        (IoError, std::io::Error),
        (InterruptedError, Interrupted),
        (Utf8Error, std::str::Utf8Error),
    }
}
