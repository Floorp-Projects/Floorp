use libc::c_uint;
use std::ffi::CString;
use std::ptr;

use ffi;

use error::{Result, lmdb_result};

/// A handle to an individual database in an environment.
///
/// A database handle denotes the name and parameters of a database in an environment.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Database {
    dbi: ffi::MDB_dbi,
}

impl Database {

    /// Opens a new database handle in the given transaction.
    ///
    /// Prefer using `Environment::open_db`, `Environment::create_db`, `TransactionExt::open_db`,
    /// or `RwTransaction::create_db`.
    pub(crate) unsafe fn new(txn: *mut ffi::MDB_txn,
                             name: Option<&str>,
                             flags: c_uint)
                             -> Result<Database> {
        let c_name = name.map(|n| CString::new(n).unwrap());
        let name_ptr = if let Some(ref c_name) = c_name { c_name.as_ptr() } else { ptr::null() };
        let mut dbi: ffi::MDB_dbi = 0;
        lmdb_result(ffi::mdb_dbi_open(txn, name_ptr, flags, &mut dbi))?;
        Ok(Database { dbi: dbi })
    }

    pub(crate) fn freelist_db() -> Database {
        Database {
            dbi: 0,
        }
    }

    /// Returns the underlying LMDB database handle.
    ///
    /// The caller **must** ensure that the handle is not used after the lifetime of the
    /// environment, or after the database has been closed.
    pub fn dbi(&self) -> ffi::MDB_dbi {
        self.dbi
    }
}

unsafe impl Sync for Database {}
unsafe impl Send for Database {}
