// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#[macro_use]
extern crate lazy_static;
extern crate libc;
extern crate rkv;
extern crate tempfile;

use std::fs;
use std::fs::File;
use std::io::Write;
use std::iter;
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
pub extern "C" fn fuzz_rkv_db_name(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let data = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize) };

    let root = Builder::new().prefix("fuzz_rkv_db_name").tempdir().unwrap();
    fs::create_dir_all(root.path()).unwrap();

    let env = rkv::Rkv::new(root.path()).unwrap();
    let name = String::from_utf8_lossy(data);
    println!("Checking string: '{:?}'", name);
    // Some strings are invalid database names, and are handled as store errors.
    // Ignore those errors, but not others.
    let store = eat_lmdb_err(env.open_single(name.as_ref(), rkv::StoreOptions::create())).unwrap();

    if let Some(store) = store {
        let reader = env.read().unwrap();
        eat_lmdb_err(store.get(&reader, &[0])).unwrap();
    };

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

lazy_static! {
    static ref STATIC_DATA: Vec<String> = {
        let sizes = vec![4, 16, 128, 512, 1024];
        let mut data = Vec::new();

        for (i, s) in sizes.into_iter().enumerate() {
            let bytes = iter::repeat('a' as u8 + i as u8).take(s).collect();
            data.push(String::from_utf8(bytes).unwrap());
        }

        data
    };
}

#[no_mangle]
pub extern "C" fn fuzz_rkv_calls(raw_data: *const u8, size: libc::size_t) -> libc::c_int {
    let mut fuzz = unsafe { std::slice::from_raw_parts(raw_data as *const u8, size as usize).to_vec() };

    fn maybe_do(fuzz: &mut Vec<u8>, f: impl FnOnce(&mut Vec<u8>) -> ()) -> Option<()> {
        match fuzz.pop().map(|byte| byte % 2) {
            Some(0) => Some(f(fuzz)),
            _ => None
        }
    }

    fn maybe_abort(fuzz: &mut Vec<u8>, read: rkv::Reader) -> Result<(), rkv::StoreError>  {
        match fuzz.pop().map(|byte| byte % 2) {
            Some(0) => Ok(read.abort()),
            _ => Ok(())
        }
    };

    fn maybe_commit(fuzz: &mut Vec<u8>, write: rkv::Writer) -> Result<(), rkv::StoreError>  {
        match fuzz.pop().map(|byte| byte % 3) {
            Some(0) => write.commit(),
            Some(1) => Ok(write.abort()),
            _ => Ok(())
        }
    };

    fn get_static_data<'a>(fuzz: &mut Vec<u8>) -> Option<&'a String> {
        fuzz.pop().map(|byte| {
            let data = &*STATIC_DATA;
            let n = byte as usize;
            data.get(n % data.len()).unwrap()
        })
    };

    let root = Builder::new().prefix("fuzz_rkv_calls").tempdir().unwrap();
    fs::create_dir_all(root.path()).unwrap();

    let mut builder = rkv::Rkv::environment_builder();
    builder.set_max_dbs(1); // need at least one db

    maybe_do(&mut fuzz, |fuzz| {
        let n = fuzz.pop().unwrap_or(126) as u32; // default
        builder.set_max_readers(1 + n);
    });
    maybe_do(&mut fuzz, |fuzz| {
        let n = fuzz.pop().unwrap_or(1) as u32;
        builder.set_max_dbs(1 + n);
    });
    maybe_do(&mut fuzz, |fuzz| {
        let n = fuzz.pop().unwrap_or(1) as usize;
        builder.set_map_size(1_048_576 * (n % 100)); // 1,048,576 bytes, i.e. 1MiB.
    });

    let env = rkv::Rkv::from_env(root.path(), builder).unwrap();
    let store = env.open_single("test", rkv::StoreOptions::create()).unwrap();

    loop {
        match fuzz.pop().map(|byte| byte % 4) {
            Some(0) => {
                let key = match get_static_data(&mut fuzz) {
                    Some(value) => value,
                    None => break
                };
                let value = match get_static_data(&mut fuzz) {
                    Some(value) => value,
                    None => break
                };
                let mut writer = env.write().unwrap();
                eat_lmdb_err(store.put(&mut writer, key, &rkv::Value::Str(value))).unwrap();
                maybe_commit(&mut fuzz, writer).unwrap();
            }
            Some(1) => {
                let key = match get_static_data(&mut fuzz) {
                    Some(value) => value,
                    None => break
                };
                let mut reader = env.read().unwrap();
                eat_lmdb_err(store.get(&mut reader, key)).unwrap();
                maybe_abort(&mut fuzz, reader).unwrap();
            }
            Some(2) => {
                let key = match get_static_data(&mut fuzz) {
                    Some(value) => value,
                    None => break
                };
                let mut writer = env.write().unwrap();
                eat_lmdb_err(store.delete(&mut writer, key)).unwrap();
                maybe_commit(&mut fuzz, writer).unwrap();
            }
            Some(3) => {
                // Any attempt to set a size smaller than the space already
                // consumed by the environment will be silently changed to the
                // current size of the used space.
                let n = fuzz.pop().unwrap_or(1) as usize;
                builder.set_map_size(1_048_576 * (n % 100)); // 1,048,576 bytes, i.e. 1MiB.
            }
            _ => {
                break
            }
        }
    }

    0
}
