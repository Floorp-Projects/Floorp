// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{
    fs,
    path::Path,
};

use tempfile::Builder;

use rkv::{
    backend::{
        Lmdb,
        SafeMode,
    },
    Migrator,
    Rkv,
    StoreOptions,
    Value,
};

macro_rules! populate_store {
    ($env:expr) => {
        let store = $env.open_single("store", StoreOptions::create()).expect("opened");
        let mut writer = $env.write().expect("writer");
        store.put(&mut writer, "foo", &Value::I64(1234)).expect("wrote");
        store.put(&mut writer, "bar", &Value::Bool(true)).expect("wrote");
        store.put(&mut writer, "baz", &Value::Str("héllo, yöu")).expect("wrote");
        writer.commit().expect("committed");
    };
}

#[test]
fn test_simple_migrator_lmdb_to_safe() {
    let root = Builder::new().prefix("test_simple_migrator_lmdb_to_safe").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // Populate source environment and persist to disk.
    {
        let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        populate_store!(&src_env);
        src_env.sync(true).expect("synced");
    }
    // Check if the files were written to disk.
    {
        let mut datamdb = root.path().to_path_buf();
        let mut lockmdb = root.path().to_path_buf();
        datamdb.push("data.mdb");
        lockmdb.push("lock.mdb");
        assert!(datamdb.exists());
        assert!(lockmdb.exists());
    }
    // Verify that database was written to disk.
    {
        let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let store = src_env.open_single("store", StoreOptions::default()).expect("opened");
        let reader = src_env.read().expect("reader");
        assert_eq!(store.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
        assert_eq!(store.get(&reader, "bar").expect("read"), Some(Value::Bool(true)));
        assert_eq!(store.get(&reader, "baz").expect("read"), Some(Value::Str("héllo, yöu")));
    }
    // Easy migrate.
    {
        let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        Migrator::auto_migrate_lmdb_to_safe_mode(root.path(), |builder| builder, &dst_env).expect("migrated");
    }
    // Verify that the database was indeed migrated.
    {
        let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let store = dst_env.open_single("store", StoreOptions::default()).expect("opened");
        let reader = dst_env.read().expect("reader");
        assert_eq!(store.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
        assert_eq!(store.get(&reader, "bar").expect("read"), Some(Value::Bool(true)));
        assert_eq!(store.get(&reader, "baz").expect("read"), Some(Value::Str("héllo, yöu")));
    }
    // Check if the old files were deleted from disk.
    {
        let mut datamdb = root.path().to_path_buf();
        let mut lockmdb = root.path().to_path_buf();
        datamdb.push("data.mdb");
        lockmdb.push("lock.mdb");
        assert!(!datamdb.exists());
        assert!(!lockmdb.exists());
    }
}

#[test]
fn test_simple_migrator_safe_to_lmdb() {
    let root = Builder::new().prefix("test_simple_migrator_safe_to_lmdb").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // Populate source environment and persist to disk.
    {
        let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        populate_store!(&src_env);
        src_env.sync(true).expect("synced");
    }
    // Check if the files were written to disk.
    {
        let mut safebin = root.path().to_path_buf();
        safebin.push("data.safe.bin");
        assert!(safebin.exists());
    }
    // Verify that database was written to disk.
    {
        let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let store = src_env.open_single("store", StoreOptions::default()).expect("opened");
        let reader = src_env.read().expect("reader");
        assert_eq!(store.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
        assert_eq!(store.get(&reader, "bar").expect("read"), Some(Value::Bool(true)));
        assert_eq!(store.get(&reader, "baz").expect("read"), Some(Value::Str("héllo, yöu")));
    }
    // Easy migrate.
    {
        let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        Migrator::auto_migrate_safe_mode_to_lmdb(root.path(), |builder| builder, &dst_env).expect("migrated");
    }
    // Verify that the database was indeed migrated.
    {
        let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let store = dst_env.open_single("store", StoreOptions::default()).expect("opened");
        let reader = dst_env.read().expect("reader");
        assert_eq!(store.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
        assert_eq!(store.get(&reader, "bar").expect("read"), Some(Value::Bool(true)));
        assert_eq!(store.get(&reader, "baz").expect("read"), Some(Value::Str("héllo, yöu")));
    }
    // Check if the old files were deleted from disk.
    {
        let mut safebin = root.path().to_path_buf();
        safebin.push("data.safe.bin");
        assert!(!safebin.exists());
    }
}

#[test]
fn test_migrator_round_trip() {
    let root = Builder::new().prefix("test_simple_migrator_lmdb_to_safe").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // Populate source environment and persist to disk.
    {
        let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        populate_store!(&src_env);
        src_env.sync(true).expect("synced");
    }
    // Easy migrate.
    {
        let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        Migrator::auto_migrate_lmdb_to_safe_mode(root.path(), |builder| builder, &dst_env).expect("migrated");
    }
    // Easy migrate back.
    {
        let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        Migrator::auto_migrate_safe_mode_to_lmdb(root.path(), |builder| builder, &dst_env).expect("migrated");
    }
    // Verify that the database was indeed migrated twice.
    {
        let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let store = dst_env.open_single("store", StoreOptions::default()).expect("opened");
        let reader = dst_env.read().expect("reader");
        assert_eq!(store.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
        assert_eq!(store.get(&reader, "bar").expect("read"), Some(Value::Bool(true)));
        assert_eq!(store.get(&reader, "baz").expect("read"), Some(Value::Str("héllo, yöu")));
    }
    // Check if the right files are finally present on disk.
    {
        let mut datamdb = root.path().to_path_buf();
        let mut lockmdb = root.path().to_path_buf();
        let mut safebin = root.path().to_path_buf();
        datamdb.push("data.mdb");
        lockmdb.push("lock.mdb");
        safebin.push("data.safe.bin");
        assert!(datamdb.exists());
        assert!(lockmdb.exists());
        assert!(!safebin.exists());
    }
}

#[test]
fn test_migrator_no_dir_1() {
    let root = Builder::new().prefix("test_migrator_no_dir").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    Migrator::auto_migrate_lmdb_to_safe_mode(Path::new("bogus"), |builder| builder, &dst_env).expect("migrated");

    let mut datamdb = root.path().to_path_buf();
    let mut lockmdb = root.path().to_path_buf();
    let mut safebin = root.path().to_path_buf();
    datamdb.push("data.mdb");
    lockmdb.push("lock.mdb");
    safebin.push("data.safe.bin");
    assert!(!datamdb.exists());
    assert!(!lockmdb.exists());
    assert!(!safebin.exists()); // safe mode doesn't write an empty db to disk
}

#[test]
fn test_migrator_no_dir_2() {
    let root = Builder::new().prefix("test_migrator_no_dir").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    Migrator::auto_migrate_safe_mode_to_lmdb(Path::new("bogus"), |builder| builder, &dst_env).expect("migrated");

    let mut datamdb = root.path().to_path_buf();
    let mut lockmdb = root.path().to_path_buf();
    let mut safebin = root.path().to_path_buf();
    datamdb.push("data.mdb");
    lockmdb.push("lock.mdb");
    safebin.push("data.safe.bin");
    assert!(datamdb.exists()); // lmdb writes an empty db to disk
    assert!(lockmdb.exists());
    assert!(!safebin.exists());
}

#[test]
#[should_panic(expected = "migrated: SourceEmpty")]
fn test_migrator_lmdb_to_safe_1() {
    let root = Builder::new().prefix("test_migrate_lmdb_to_safe").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    Migrator::migrate_lmdb_to_safe_mode(&src_env, &dst_env).expect("migrated");
}

#[test]
#[should_panic(expected = "migrated: DestinationNotEmpty")]
fn test_migrator_lmdb_to_safe_2() {
    let root = Builder::new().prefix("test_migrate_lmdb_to_safe").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    populate_store!(&dst_env);
    Migrator::migrate_lmdb_to_safe_mode(&src_env, &dst_env).expect("migrated");
}

#[test]
fn test_migrator_lmdb_to_safe_3() {
    let root = Builder::new().prefix("test_migrate_lmdb_to_safe").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    Migrator::migrate_lmdb_to_safe_mode(&src_env, &dst_env).expect("migrated");

    let store = dst_env.open_single("store", StoreOptions::default()).expect("opened");
    let reader = dst_env.read().expect("reader");
    assert_eq!(store.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
    assert_eq!(store.get(&reader, "bar").expect("read"), Some(Value::Bool(true)));
    assert_eq!(store.get(&reader, "baz").expect("read"), Some(Value::Str("héllo, yöu")));
}

#[test]
#[should_panic(expected = "migrated: SourceEmpty")]
fn test_migrator_safe_to_lmdb_1() {
    let root = Builder::new().prefix("test_migrate_safe_to_lmdb").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    Migrator::migrate_safe_mode_to_lmdb(&src_env, &dst_env).expect("migrated");
}

#[test]
#[should_panic(expected = "migrated: DestinationNotEmpty")]
fn test_migrator_safe_to_lmdb_2() {
    let root = Builder::new().prefix("test_migrate_safe_to_lmdb").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    populate_store!(&dst_env);
    Migrator::migrate_safe_mode_to_lmdb(&src_env, &dst_env).expect("migrated");
}

#[test]
fn test_migrator_safe_to_lmdb_3() {
    let root = Builder::new().prefix("test_migrate_safe_to_lmdb").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    Migrator::migrate_safe_mode_to_lmdb(&src_env, &dst_env).expect("migrated");

    let store = dst_env.open_single("store", StoreOptions::default()).expect("opened");
    let reader = dst_env.read().expect("reader");
    assert_eq!(store.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
    assert_eq!(store.get(&reader, "bar").expect("read"), Some(Value::Bool(true)));
    assert_eq!(store.get(&reader, "baz").expect("read"), Some(Value::Str("héllo, yöu")));
}
