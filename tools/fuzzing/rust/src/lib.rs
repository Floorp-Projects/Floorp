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

macro_rules! try_or_ret {
    ($expr:expr) => {{
        match $expr {
            Ok(v) => v,
            Err(_) => return 0,
        }
    }};
}

#[no_mangle]
pub extern "C" fn fuzz_rkv_db_file(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let data = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize) };

    // First 8192 bytes are for the lock file.
    if data.len() < 8192 {
        return 0;
    }
    let (lock, db) = data.split_at(8192);
    let mut lock_file = try_or_ret!(File::create("data.lock"));
    try_or_ret!(lock_file.write_all(lock));
    let mut db_file = try_or_ret!(File::create("data.mdb"));
    try_or_ret!(db_file.write_all(db));

    let &mut builder = rkv::Rkv::environment_builder().set_max_dbs(2);
    let env = try_or_ret!(rkv::Rkv::from_env(Path::new("."), builder));
    let store = try_or_ret!(env.open_single("test", rkv::StoreOptions::create()));

    let reader = try_or_ret!(env.read());
    try_or_ret!(store.get(&reader, &[0]));

    0
}

#[no_mangle]
pub extern "C" fn fuzz_rkv_key_write(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let data = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize) };

    let root = try_or_ret!(Builder::new().prefix("fuzz_rkv_key_write").tempdir());
    try_or_ret!(fs::create_dir_all(root.path()));

    let env = try_or_ret!(rkv::Rkv::new(root.path()));
    let store: rkv::SingleStore = try_or_ret!(env.open_single("test", rkv::StoreOptions::create()));

    let mut writer = try_or_ret!(env.write());
    try_or_ret!(store.put(&mut writer, data, &rkv::Value::Str("val")));

    0
}

#[no_mangle]
pub extern "C" fn fuzz_rkv_val_write(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let data = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize) };

    let root = try_or_ret!(Builder::new().prefix("fuzz_rkv_val_write").tempdir());
    try_or_ret!(fs::create_dir_all(root.path()));

    let env = try_or_ret!(rkv::Rkv::new(root.path()));
    let store: rkv::SingleStore = try_or_ret!(env.open_single("test", rkv::StoreOptions::create()));

    let mut writer = try_or_ret!(env.write());
    let value = rkv::Value::Str(try_or_ret!(std::str::from_utf8(data)));
    try_or_ret!(store.put(&mut writer, "key", &value));

    0
}
