// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{fs, path::Path};

use tempfile::Builder;

use rkv::{
    backend::{Lmdb, LmdbEnvironment, SafeMode, SafeModeEnvironment},
    Manager, Migrator, Rkv, StoreOptions, Value,
};

macro_rules! populate_store {
    ($env:expr) => {
        let store = $env
            .open_single("store", StoreOptions::create())
            .expect("opened");
        let mut writer = $env.write().expect("writer");
        store
            .put(&mut writer, "foo", &Value::I64(1234))
            .expect("wrote");
        store
            .put(&mut writer, "bar", &Value::Bool(true))
            .expect("wrote");
        store
            .put(&mut writer, "baz", &Value::Str("héllo, yöu"))
            .expect("wrote");
        writer.commit().expect("committed");
    };
}

#[test]
fn test_open_migrator_lmdb_to_safe() {
    let root = Builder::new()
        .prefix("test_open_migrator_lmdb_to_safe")
        .tempdir()
        .expect("tempdir");
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
        let store = src_env
            .open_single("store", StoreOptions::default())
            .expect("opened");
        let reader = src_env.read().expect("reader");
        assert_eq!(
            store.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            store.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            store.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
    // Open and migrate.
    {
        let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        Migrator::open_and_migrate_lmdb_to_safe_mode(root.path(), |builder| builder, &dst_env)
            .expect("migrated");
    }
    // Verify that the database was indeed migrated.
    {
        let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let store = dst_env
            .open_single("store", StoreOptions::default())
            .expect("opened");
        let reader = dst_env.read().expect("reader");
        assert_eq!(
            store.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            store.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            store.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
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
fn test_open_migrator_safe_to_lmdb() {
    let root = Builder::new()
        .prefix("test_open_migrator_safe_to_lmdb")
        .tempdir()
        .expect("tempdir");
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
        let store = src_env
            .open_single("store", StoreOptions::default())
            .expect("opened");
        let reader = src_env.read().expect("reader");
        assert_eq!(
            store.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            store.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            store.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
    // Open and migrate.
    {
        let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        Migrator::open_and_migrate_safe_mode_to_lmdb(root.path(), |builder| builder, &dst_env)
            .expect("migrated");
    }
    // Verify that the database was indeed migrated.
    {
        let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let store = dst_env
            .open_single("store", StoreOptions::default())
            .expect("opened");
        let reader = dst_env.read().expect("reader");
        assert_eq!(
            store.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            store.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            store.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
    // Check if the old files were deleted from disk.
    {
        let mut safebin = root.path().to_path_buf();
        safebin.push("data.safe.bin");
        assert!(!safebin.exists());
    }
}

#[test]
fn test_open_migrator_round_trip() {
    let root = Builder::new()
        .prefix("test_open_migrator_lmdb_to_safe")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // Populate source environment and persist to disk.
    {
        let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        populate_store!(&src_env);
        src_env.sync(true).expect("synced");
    }
    // Open and migrate.
    {
        let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        Migrator::open_and_migrate_lmdb_to_safe_mode(root.path(), |builder| builder, &dst_env)
            .expect("migrated");
    }
    // Open and migrate back.
    {
        let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        Migrator::open_and_migrate_safe_mode_to_lmdb(root.path(), |builder| builder, &dst_env)
            .expect("migrated");
    }
    // Verify that the database was indeed migrated twice.
    {
        let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let store = dst_env
            .open_single("store", StoreOptions::default())
            .expect("opened");
        let reader = dst_env.read().expect("reader");
        assert_eq!(
            store.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            store.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            store.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
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
fn test_easy_migrator_no_dir_1() {
    let root = Builder::new()
        .prefix("test_easy_migrator_no_dir")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // This won't fail with IoError even though the path is a bogus path, because this
    // is the "easy mode" migration which automatically handles (ignores) this error.
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    Migrator::easy_migrate_lmdb_to_safe_mode(Path::new("bogus"), &dst_env).expect("migrated");

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
fn test_easy_migrator_no_dir_2() {
    let root = Builder::new()
        .prefix("test_easy_migrator_no_dir")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // This won't fail with IoError even though the path is a bogus path, because this
    // is the "easy mode" migration which automatically handles (ignores) this error.
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    Migrator::easy_migrate_safe_mode_to_lmdb(Path::new("bogus"), &dst_env).expect("migrated");

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
fn test_easy_migrator_invalid_1() {
    let root = Builder::new()
        .prefix("test_easy_migrator_invalid")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let dbfile = root.path().join("data.mdb");
    fs::write(dbfile, "bogus").expect("dbfile created");

    // This won't fail with FileInvalid even though the database is a bogus file, because this
    // is the "easy mode" migration which automatically handles (ignores) this error.
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    Migrator::easy_migrate_lmdb_to_safe_mode(root.path(), &dst_env).expect("migrated");

    let mut datamdb = root.path().to_path_buf();
    let mut lockmdb = root.path().to_path_buf();
    let mut safebin = root.path().to_path_buf();
    datamdb.push("data.mdb");
    lockmdb.push("lock.mdb");
    safebin.push("data.safe.bin");
    assert!(datamdb.exists()); // corrupted db isn't deleted
    assert!(lockmdb.exists());
    assert!(!safebin.exists());
}

#[test]
fn test_easy_migrator_invalid_2() {
    let root = Builder::new()
        .prefix("test_easy_migrator_invalid")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let dbfile = root.path().join("data.safe.bin");
    fs::write(dbfile, "bogus").expect("dbfile created");

    // This won't fail with FileInvalid even though the database is a bogus file, because this
    // is the "easy mode" migration which automatically handles (ignores) this error.
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    Migrator::easy_migrate_safe_mode_to_lmdb(root.path(), &dst_env).expect("migrated");

    let mut datamdb = root.path().to_path_buf();
    let mut lockmdb = root.path().to_path_buf();
    let mut safebin = root.path().to_path_buf();
    datamdb.push("data.mdb");
    lockmdb.push("lock.mdb");
    safebin.push("data.safe.bin");
    assert!(datamdb.exists()); // lmdb writes an empty db to disk
    assert!(lockmdb.exists());
    assert!(safebin.exists()); // corrupted db isn't deleted
}

#[test]
#[should_panic(expected = "migrated: SourceEmpty")]
fn test_migrator_lmdb_to_safe_1() {
    let root = Builder::new()
        .prefix("test_migrate_lmdb_to_safe")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    Migrator::migrate_lmdb_to_safe_mode(&src_env, &dst_env).expect("migrated");
}

#[test]
#[should_panic(expected = "migrated: DestinationNotEmpty")]
fn test_migrator_lmdb_to_safe_2() {
    let root = Builder::new()
        .prefix("test_migrate_lmdb_to_safe")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    populate_store!(&dst_env);
    Migrator::migrate_lmdb_to_safe_mode(&src_env, &dst_env).expect("migrated");
}

#[test]
fn test_migrator_lmdb_to_safe_3() {
    let root = Builder::new()
        .prefix("test_migrate_lmdb_to_safe")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    Migrator::migrate_lmdb_to_safe_mode(&src_env, &dst_env).expect("migrated");

    let store = dst_env
        .open_single("store", StoreOptions::default())
        .expect("opened");
    let reader = dst_env.read().expect("reader");
    assert_eq!(
        store.get(&reader, "foo").expect("read"),
        Some(Value::I64(1234))
    );
    assert_eq!(
        store.get(&reader, "bar").expect("read"),
        Some(Value::Bool(true))
    );
    assert_eq!(
        store.get(&reader, "baz").expect("read"),
        Some(Value::Str("héllo, yöu"))
    );
}

#[test]
#[should_panic(expected = "migrated: SourceEmpty")]
fn test_migrator_safe_to_lmdb_1() {
    let root = Builder::new()
        .prefix("test_migrate_safe_to_lmdb")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    Migrator::migrate_safe_mode_to_lmdb(&src_env, &dst_env).expect("migrated");
}

#[test]
#[should_panic(expected = "migrated: DestinationNotEmpty")]
fn test_migrator_safe_to_lmdb_2() {
    let root = Builder::new()
        .prefix("test_migrate_safe_to_lmdb")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    populate_store!(&dst_env);
    Migrator::migrate_safe_mode_to_lmdb(&src_env, &dst_env).expect("migrated");
}

#[test]
fn test_migrator_safe_to_lmdb_3() {
    let root = Builder::new()
        .prefix("test_migrate_safe_to_lmdb")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    Migrator::migrate_safe_mode_to_lmdb(&src_env, &dst_env).expect("migrated");

    let store = dst_env
        .open_single("store", StoreOptions::default())
        .expect("opened");
    let reader = dst_env.read().expect("reader");
    assert_eq!(
        store.get(&reader, "foo").expect("read"),
        Some(Value::I64(1234))
    );
    assert_eq!(
        store.get(&reader, "bar").expect("read"),
        Some(Value::Bool(true))
    );
    assert_eq!(
        store.get(&reader, "baz").expect("read"),
        Some(Value::Str("héllo, yöu"))
    );
}

#[test]
fn test_easy_migrator_failed_migration_1() {
    let root = Builder::new()
        .prefix("test_easy_migrator_failed_migration_1")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let dbfile = root.path().join("data.mdb");
    fs::write(&dbfile, "bogus").expect("bogus dbfile created");

    // This won't fail with FileInvalid even though the database is a bogus file, because this
    // is the "easy mode" migration which automatically handles (ignores) this error.
    let dst_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    Migrator::easy_migrate_lmdb_to_safe_mode(root.path(), &dst_env).expect("migrated");

    // Populate destination environment and persist to disk.
    populate_store!(&dst_env);
    dst_env.sync(true).expect("synced");

    // Delete bogus file and create a valid source environment in its place.
    fs::remove_file(&dbfile).expect("bogus dbfile removed");
    let src_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    src_env.sync(true).expect("synced");

    // Attempt to migrate again. This should *NOT* fail with DestinationNotEmpty.
    Migrator::easy_migrate_lmdb_to_safe_mode(root.path(), &dst_env).expect("migrated");
}

#[test]
fn test_easy_migrator_failed_migration_2() {
    let root = Builder::new()
        .prefix("test_easy_migrator_failed_migration_2")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let dbfile = root.path().join("data.safe.bin");
    fs::write(&dbfile, "bogus").expect("bogus dbfile created");

    // This won't fail with FileInvalid even though the database is a bogus file, because this
    // is the "easy mode" migration which automatically handles (ignores) this error.
    let dst_env = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    Migrator::easy_migrate_safe_mode_to_lmdb(root.path(), &dst_env).expect("migrated");

    // Populate destination environment and persist to disk.
    populate_store!(&dst_env);
    dst_env.sync(true).expect("synced");

    // Delete bogus file and create a valid source environment in its place.
    fs::remove_file(&dbfile).expect("bogus dbfile removed");
    let src_env = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    populate_store!(&src_env);
    src_env.sync(true).expect("synced");

    // Attempt to migrate again. This should *NOT* fail with DestinationNotEmpty.
    Migrator::easy_migrate_safe_mode_to_lmdb(root.path(), &dst_env).expect("migrated");
}

fn test_easy_migrator_from_manager_failed_migration_1() {
    let root = Builder::new()
        .prefix("test_easy_migrator_from_manager_failed_migration_1")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    {
        let mut src_manager = Manager::<LmdbEnvironment>::singleton().write().unwrap();
        let created_src_arc = src_manager
            .get_or_create(root.path(), Rkv::new::<Lmdb>)
            .unwrap();
        let src_env = created_src_arc.read().unwrap();
        populate_store!(&src_env);
        src_env.sync(true).expect("synced");
    }
    {
        let mut dst_manager = Manager::<SafeModeEnvironment>::singleton().write().unwrap();
        let created_dst_arc_1 = dst_manager
            .get_or_create(root.path(), Rkv::new::<SafeMode>)
            .unwrap();
        let dst_env_1 = created_dst_arc_1.read().unwrap();
        populate_store!(&dst_env_1);
        dst_env_1.sync(true).expect("synced");
    }

    // Attempt to migrate again in a new env. This should *NOT* fail with DestinationNotEmpty.
    let dst_manager = Manager::<SafeModeEnvironment>::singleton().read().unwrap();
    let created_dst_arc_2 = dst_manager.get(root.path()).unwrap().unwrap();
    let dst_env_2 = created_dst_arc_2.read().unwrap();
    Migrator::easy_migrate_lmdb_to_safe_mode(root.path(), dst_env_2).expect("migrated");
}

fn test_easy_migrator_from_manager_failed_migration_2() {
    let root = Builder::new()
        .prefix("test_easy_migrator_from_manager_failed_migration_2")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    {
        let mut src_manager = Manager::<SafeModeEnvironment>::singleton().write().unwrap();
        let created_src_arc = src_manager
            .get_or_create(root.path(), Rkv::new::<SafeMode>)
            .unwrap();
        let src_env = created_src_arc.read().unwrap();
        populate_store!(&src_env);
        src_env.sync(true).expect("synced");
    }
    {
        let mut dst_manager = Manager::<LmdbEnvironment>::singleton().write().unwrap();
        let created_dst_arc_1 = dst_manager
            .get_or_create(root.path(), Rkv::new::<Lmdb>)
            .unwrap();
        let dst_env_1 = created_dst_arc_1.read().unwrap();
        populate_store!(&dst_env_1);
        dst_env_1.sync(true).expect("synced");
    }

    // Attempt to migrate again in a new env. This should *NOT* fail with DestinationNotEmpty.
    let dst_manager = Manager::<LmdbEnvironment>::singleton().read().unwrap();
    let created_dst_arc_2 = dst_manager.get(root.path()).unwrap().unwrap();
    let dst_env_2 = created_dst_arc_2.read().unwrap();
    Migrator::easy_migrate_safe_mode_to_lmdb(root.path(), dst_env_2).expect("migrated");
}

#[test]
fn test_easy_migrator_from_manager_failed_migration() {
    test_easy_migrator_from_manager_failed_migration_1();
    test_easy_migrator_from_manager_failed_migration_2();
}
