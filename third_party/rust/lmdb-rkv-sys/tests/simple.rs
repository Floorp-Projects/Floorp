extern crate lmdb_sys;

use lmdb_sys::*;

use std::ffi::{c_void, CString};
use std::fs::File;
use std::ptr;

// https://github.com/victorporof/lmdb/blob/mdb.master/libraries/liblmdb/moz-test.c

macro_rules! E {
    ($expr:expr) => {{
        match $expr {
            ::MDB_SUCCESS => (),
            err_code => assert!(false, "Failed with code {}", err_code),
        }
    }};
}

macro_rules! str {
    ($expr:expr) => {
        ::CString::new($expr).unwrap().as_ptr()
    };
}

#[test]
#[cfg(target_pointer_width = "32")]
fn test_simple_32() {
    test_simple("./tests/fixtures/testdb-32")
}

#[test]
#[cfg(target_pointer_width = "64")]
fn test_simple_64() {
    test_simple("./tests/fixtures/testdb")
}

#[cfg(windows)]
fn get_file_fd(file: &File) -> std::os::windows::io::RawHandle {
    use std::os::windows::io::AsRawHandle;
    file.as_raw_handle()
}

#[cfg(unix)]
fn get_file_fd(file: &File) -> std::os::unix::io::RawFd {
    use std::os::unix::io::AsRawFd;
    file.as_raw_fd()
}

fn test_simple(env_path: &str) {
    let mut env: *mut MDB_env = ptr::null_mut();
    let mut dbi: MDB_dbi = 0;
    let mut key = MDB_val {
        mv_size: 0,
        mv_data: ptr::null_mut(),
    };
    let mut data = MDB_val {
        mv_size: 0,
        mv_data: ptr::null_mut(),
    };
    let mut txn: *mut MDB_txn = ptr::null_mut();
    let sval = str!("foo") as *mut c_void;
    let dval = str!("bar") as *mut c_void;

    unsafe {
        E!(mdb_env_create(&mut env));
        E!(mdb_env_set_maxdbs(env, 2));
        E!(mdb_env_open(env, str!(env_path), 0, 0664));

        E!(mdb_txn_begin(env, ptr::null_mut(), 0, &mut txn));
        E!(mdb_dbi_open(txn, str!("subdb"), MDB_CREATE, &mut dbi));
        E!(mdb_txn_commit(txn));

        key.mv_size = 3;
        key.mv_data = sval;
        data.mv_size = 3;
        data.mv_data = dval;

        E!(mdb_txn_begin(env, ptr::null_mut(), 0, &mut txn));
        E!(mdb_put(txn, dbi, &mut key, &mut data, 0));
        E!(mdb_txn_commit(txn));
    }

    let file = File::create("./tests/fixtures/copytestdb.mdb").unwrap();

    unsafe {
        let fd = get_file_fd(&file);
        E!(mdb_env_copyfd(env, fd));

        mdb_dbi_close(env, dbi);
        mdb_env_close(env);
    }
}
