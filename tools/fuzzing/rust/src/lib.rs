// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
extern crate libc;
extern crate rkv;
extern crate tempfile;

use std::fs;
use std::fs::File;
use std::io::Write;
use std::path::Path;
use tempfile::Builder;

fn eat_lmdb_err<T>(value: Result<T, rkv::StoreError>) -> Result<Option<T>, rkv::StoreError> {
    match value {
        Ok(value) => Ok(Some(value)),
        Err(rkv::StoreError::LmdbError(_)) => Ok(None),
        Err(err) => {
            println!("Not a crash, but an error outside LMDB: {}", err);
            println!("A refined fuzzing test, or changes to RKV, might be required.");
            Err(err)
        },
    }
}

#[no_mangle]
pub extern "C" fn fuzz_rkv_db_file(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let data = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize) };

    // First 8192 bytes are for the lock file.
    if data.len() < 8192 {
        return 0;
    }
    let (lock, db) = data.split_at(8192);
    let mut lock_file = File::create("data.lock").unwrap();
    lock_file.write_all(lock).unwrap();
    let mut db_file = File::create("data.mdb").unwrap();
    db_file.write_all(db).unwrap();

    let &mut builder = rkv::Rkv::environment_builder().set_max_dbs(2);
    let env = rkv::Rkv::from_env(Path::new("."), builder).unwrap();
    let store = env.open_single("test", rkv::StoreOptions::create()).unwrap();

    let reader = env.read().unwrap();
    eat_lmdb_err(store.get(&reader, &[0])).unwrap();


    0
}

#[no_mangle]
pub extern "C" fn fuzz_rkv_key_write(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let data = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize) };

    let root = Builder::new().prefix("fuzz_rkv_key_write").tempdir().unwrap();
    fs::create_dir_all(root.path()).unwrap();

    let env = rkv::Rkv::new(root.path()).unwrap();
    let store = env.open_single("test", rkv::StoreOptions::create()).unwrap();

    let mut writer = env.write().unwrap();
    // Some data are invalid values, and are handled as store errors.
    // Ignore those errors, but not others.
    eat_lmdb_err(store.put(&mut writer, data, &rkv::Value::Str("val"))).unwrap();

    0
}

#[no_mangle]
pub extern "C" fn fuzz_rkv_val_write(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let data = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize) };

    let root = Builder::new().prefix("fuzz_rkv_val_write").tempdir().unwrap();
    fs::create_dir_all(root.path()).unwrap();

    let env = rkv::Rkv::new(root.path()).unwrap();
    let store = env.open_single("test", rkv::StoreOptions::create()).unwrap();

    let mut writer = env.write().unwrap();
    let string = String::from_utf8_lossy(data);
    let value = rkv::Value::Str(&string);
    store.put(&mut writer, "key", &value).unwrap();

    0
}
