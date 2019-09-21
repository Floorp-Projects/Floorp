// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
extern crate libc;
extern crate rkv;

use std::fs::File;
use std::io::Write;
use std::path::Path;

#[no_mangle]
pub extern fn fuzz_rkv(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let data = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize) };

    // First 8192 bytes are for the lock file.
    if data.len() < 8192 {
        return 0;
    }
    let (lock, db) = data.split_at(8192);
    let mut lock_file = match File::create("data.mdb") {
        Ok(lock_file) => lock_file,
        Err(_) => return 0,
    };
    match lock_file.write_all(lock) {
        Ok(_) => {}
        Err(_) => return 0,
    };
    let mut db_file = match File::create("data.mdb") {
        Ok(db_file) => db_file,
        Err(_) => return 0,
    };
    match db_file.write_all(db) {
        Ok(_) => {}
        Err(_) => return 0,
    };
    let env = {
        let mut builder = rkv::Rkv::environment_builder();
        builder.set_max_dbs(2);
        match rkv::Rkv::from_env(Path::new("."), builder) {
            Ok(env) => env,
            Err(_) => return 0,
        }
    };
    let store = match env.open_single("cert_storage", rkv::StoreOptions::create()) {
        Ok(store) => store,
        Err(_) => return 0,
    };
    let reader = match env.read() {
        Ok(reader) => reader,
        Err(_) => return 0,
    };
    match store.get(&reader, &[0]) {
        Ok(_) => {}
        Err(_) => {}
    };
    0
}
