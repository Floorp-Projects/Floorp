use libc::c_int;
use std::error::Error as StdError;
use std::ffi::CStr;
use std::os::raw::c_char;
use std::{fmt, result, str};

use ffi;

/// An LMDB error kind.
#[derive(Debug, Eq, PartialEq, Copy, Clone)]
pub enum Error {
    /// key/data pair already exists.
    KeyExist,
    /// key/data pair not found (EOF).
    NotFound,
    /// Requested page not found - this usually indicates corruption.
    PageNotFound,
    /// Located page was wrong type.
    Corrupted,
    /// Update of meta page failed or environment had fatal error.
    Panic,
    /// Environment version mismatch.
    VersionMismatch,
    /// File is not a valid LMDB file.
    Invalid,
    /// Environment mapsize reached.
    MapFull,
    /// Environment maxdbs reached.
    DbsFull,
    /// Environment maxreaders reached.
    ReadersFull,
    /// Too many TLS keys in use - Windows only.
    TlsFull,
    /// Txn has too many dirty pages.
    TxnFull,
    /// Cursor stack too deep - internal error.
    CursorFull,
    /// Page has not enough space - internal error.
    PageFull,
    /// Database contents grew beyond environment mapsize.
    MapResized,
    /// MDB_Incompatible: Operation and DB incompatible, or DB flags changed.
    Incompatible,
    /// Invalid reuse of reader locktable slot.
    BadRslot,
    /// Transaction cannot recover - it must be aborted.
    BadTxn,
    /// Unsupported size of key/DB name/data, or wrong DUP_FIXED size.
    BadValSize,
    /// The specified DBI was changed unexpectedly.
    BadDbi,
    /// Other error.
    Other(c_int),
}

impl Error {

    /// Converts a raw error code to an `Error`.
    pub fn from_err_code(err_code: c_int) -> Error {
        match err_code {
            ffi::MDB_KEYEXIST         => Error::KeyExist,
            ffi::MDB_NOTFOUND         => Error::NotFound,
            ffi::MDB_PAGE_NOTFOUND    => Error::PageNotFound,
            ffi::MDB_CORRUPTED        => Error::Corrupted,
            ffi::MDB_PANIC            => Error::Panic,
            ffi::MDB_VERSION_MISMATCH => Error::VersionMismatch,
            ffi::MDB_INVALID          => Error::Invalid,
            ffi::MDB_MAP_FULL         => Error::MapFull,
            ffi::MDB_DBS_FULL         => Error::DbsFull,
            ffi::MDB_READERS_FULL     => Error::ReadersFull,
            ffi::MDB_TLS_FULL         => Error::TlsFull,
            ffi::MDB_TXN_FULL         => Error::TxnFull,
            ffi::MDB_CURSOR_FULL      => Error::CursorFull,
            ffi::MDB_PAGE_FULL        => Error::PageFull,
            ffi::MDB_MAP_RESIZED      => Error::MapResized,
            ffi::MDB_INCOMPATIBLE     => Error::Incompatible,
            ffi::MDB_BAD_RSLOT        => Error::BadRslot,
            ffi::MDB_BAD_TXN          => Error::BadTxn,
            ffi::MDB_BAD_VALSIZE      => Error::BadValSize,
            ffi::MDB_BAD_DBI          => Error::BadDbi,
            other                     => Error::Other(other),
        }
    }

    /// Converts an `Error` to the raw error code.
    pub fn to_err_code(&self) -> c_int {
        match *self {
            Error::KeyExist        => ffi::MDB_KEYEXIST,
            Error::NotFound        => ffi::MDB_NOTFOUND,
            Error::PageNotFound    => ffi::MDB_PAGE_NOTFOUND,
            Error::Corrupted       => ffi::MDB_CORRUPTED,
            Error::Panic           => ffi::MDB_PANIC,
            Error::VersionMismatch => ffi::MDB_VERSION_MISMATCH,
            Error::Invalid         => ffi::MDB_INVALID,
            Error::MapFull         => ffi::MDB_MAP_FULL,
            Error::DbsFull         => ffi::MDB_DBS_FULL,
            Error::ReadersFull     => ffi::MDB_READERS_FULL,
            Error::TlsFull         => ffi::MDB_TLS_FULL,
            Error::TxnFull         => ffi::MDB_TXN_FULL,
            Error::CursorFull      => ffi::MDB_CURSOR_FULL,
            Error::PageFull        => ffi::MDB_PAGE_FULL,
            Error::MapResized      => ffi::MDB_MAP_RESIZED,
            Error::Incompatible    => ffi::MDB_INCOMPATIBLE,
            Error::BadRslot        => ffi::MDB_BAD_RSLOT,
            Error::BadTxn          => ffi::MDB_BAD_TXN,
            Error::BadValSize      => ffi::MDB_BAD_VALSIZE,
            Error::BadDbi          => ffi::MDB_BAD_DBI,
            Error::Other(err_code) => err_code,
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{}", self.description())
    }
}

impl StdError for Error {
    fn description(&self) -> &str {
        unsafe {
            // This is safe since the error messages returned from mdb_strerror are static.
            let err: *const c_char = ffi::mdb_strerror(self.to_err_code()) as *const c_char;
            str::from_utf8_unchecked(CStr::from_ptr(err).to_bytes())
        }
    }
}

/// An LMDB result.
pub type Result<T> = result::Result<T, Error>;

pub fn lmdb_result(err_code: c_int) -> Result<()> {
    if err_code == ffi::MDB_SUCCESS {
        Ok(())
    } else {
        Err(Error::from_err_code(err_code))
    }
}

#[cfg(test)]
mod test {

    use std::error::Error as StdError;

    use super::*;

    #[test]
    fn test_description() {
        assert_eq!("Permission denied",
                   Error::from_err_code(13).description());
        assert_eq!("MDB_NOTFOUND: No matching key/data pair found",
                   Error::NotFound.description());
    }
}
