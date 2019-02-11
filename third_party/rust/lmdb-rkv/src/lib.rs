//! Idiomatic and safe APIs for interacting with the
//! [Lightning Memory-mapped Database (LMDB)](https://symas.com/lmdb).

#![cfg_attr(test, feature(test))]
#![deny(missing_docs)]
#![doc(html_root_url = "https://docs.rs/lmdb-rkv/0.11.2")]

extern crate libc;
extern crate lmdb_rkv_sys as ffi;

#[cfg(test)] extern crate rand;
#[cfg(test)] extern crate tempdir;
#[cfg(test)] extern crate test;
#[macro_use] extern crate bitflags;

pub use cursor::{
    Cursor,
    RoCursor,
    RwCursor,
    Iter,
    IterDup,
};
pub use database::Database;
pub use environment::{Environment, Stat, EnvironmentBuilder};
pub use error::{Error, Result};
pub use flags::*;
pub use transaction::{
    InactiveTransaction,
    RoTransaction,
    RwTransaction,
    Transaction,
};

macro_rules! lmdb_try {
    ($expr:expr) => ({
        match $expr {
            ::ffi::MDB_SUCCESS => (),
            err_code => return Err(::Error::from_err_code(err_code)),
        }
    })
}

macro_rules! lmdb_try_with_cleanup {
    ($expr:expr, $cleanup:expr) => ({
        match $expr {
            ::ffi::MDB_SUCCESS => (),
            err_code => {
                let _ = $cleanup;
                return Err(::Error::from_err_code(err_code))
            },
        }
    })
}

mod flags;
mod cursor;
mod database;
mod environment;
mod error;
mod transaction;

#[cfg(test)]
mod test_utils {

    extern crate byteorder;

    use self::byteorder::{ByteOrder, LittleEndian};
    use tempdir::TempDir;

    use super::*;

    pub fn get_key(n: u32) -> String {
        format!("key{}", n)
    }

    pub fn get_data(n: u32) -> String {
        format!("data{}", n)
    }

    pub fn setup_bench_db<'a>(num_rows: u32) -> (TempDir, Environment) {
        let dir = TempDir::new("test").unwrap();
        let env = Environment::new().open(dir.path()).unwrap();

        {
            let db = env.open_db(None).unwrap();
            let mut txn = env.begin_rw_txn().unwrap();
            for i in 0..num_rows {
                txn.put(db, &get_key(i), &get_data(i), WriteFlags::empty()).unwrap();
            }
            txn.commit().unwrap();
        }
        (dir, env)
    }

    /// Regression test for https://github.com/danburkert/lmdb-rs/issues/21.
    /// This test reliably segfaults when run against lmbdb compiled with opt level -O3 and newer
    /// GCC compilers.
    #[test]
    fn issue_21_regression() {
        const HEIGHT_KEY: [u8; 1] = [0];

        let dir = TempDir::new("test").unwrap();

        let env = {
            let mut builder = Environment::new();
            builder.set_max_dbs(2);
            builder.set_map_size(1_000_000);
            builder.open(dir.path()).expect("open lmdb env")
        };
        let index = env.create_db(None, DatabaseFlags::DUP_SORT).expect("open index db");

        for height in 0..1000 {
            let mut value = [0u8; 8];
            LittleEndian::write_u64(&mut value, height);
            let mut tx = env.begin_rw_txn().expect("begin_rw_txn");
            tx.put(index,
                   &HEIGHT_KEY,
                   &value,
                   WriteFlags::empty()).expect("tx.put");
            tx.commit().expect("tx.commit")
        }
    }
}
