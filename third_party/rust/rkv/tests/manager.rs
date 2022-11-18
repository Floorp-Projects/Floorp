// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{fs, sync::Arc};

use tempfile::Builder;

#[cfg(feature = "lmdb")]
use rkv::backend::{Lmdb, LmdbEnvironment};
use rkv::{
    backend::{BackendEnvironmentBuilder, SafeMode, SafeModeEnvironment},
    CloseOptions, Rkv, StoreOptions, Value,
};

/// Test that a manager can be created with simple type inference.
#[cfg(feature = "lmdb")]
#[test]
#[allow(clippy::let_underscore_lock)]
fn test_simple() {
    type Manager = rkv::Manager<LmdbEnvironment>;

    let _unused = Manager::singleton().write().unwrap();
}

/// Test that a manager can be created with simple type inference.
#[test]
#[allow(clippy::let_underscore_lock)]
fn test_simple_safe() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let _unused = Manager::singleton().write().unwrap();
}

/// Test that a shared Rkv instance can be created with simple type inference.
#[cfg(feature = "lmdb")]
#[test]
fn test_simple_2() {
    type Manager = rkv::Manager<LmdbEnvironment>;

    let root = Builder::new()
        .prefix("test_simple_2")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut manager = Manager::singleton().write().unwrap();
    let _ = manager
        .get_or_create(root.path(), Rkv::new::<Lmdb>)
        .unwrap();
}

/// Test that a shared Rkv instance can be created with simple type inference.
#[test]
fn test_simple_safe_2() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let root = Builder::new()
        .prefix("test_simple_safe_2")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut manager = Manager::singleton().write().unwrap();
    let _ = manager
        .get_or_create(root.path(), Rkv::new::<SafeMode>)
        .unwrap();
}

/// Test that the manager will return the same Rkv instance each time for each path.
#[cfg(feature = "lmdb")]
#[test]
fn test_same() {
    type Manager = rkv::Manager<LmdbEnvironment>;

    let root = Builder::new()
        .prefix("test_same")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let p = root.path();
    assert!(Manager::singleton()
        .read()
        .unwrap()
        .get(p)
        .expect("success")
        .is_none());

    let created_arc = Manager::singleton()
        .write()
        .unwrap()
        .get_or_create(p, Rkv::new::<Lmdb>)
        .expect("created");
    let fetched_arc = Manager::singleton()
        .read()
        .unwrap()
        .get(p)
        .expect("success")
        .expect("existed");
    assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
}

/// Test that the manager will return the same Rkv instance each time for each path.
#[test]
fn test_same_safe() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let root = Builder::new()
        .prefix("test_same_safe")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let p = root.path();
    assert!(Manager::singleton()
        .read()
        .unwrap()
        .get(p)
        .expect("success")
        .is_none());

    let created_arc = Manager::singleton()
        .write()
        .unwrap()
        .get_or_create(p, Rkv::new::<SafeMode>)
        .expect("created");
    let fetched_arc = Manager::singleton()
        .read()
        .unwrap()
        .get(p)
        .expect("success")
        .expect("existed");
    assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
}

/// Test that the manager will return the same Rkv instance each time for each path.
#[cfg(feature = "lmdb")]
#[test]
fn test_same_with_capacity() {
    type Manager = rkv::Manager<LmdbEnvironment>;

    let root = Builder::new()
        .prefix("test_same_with_capacity")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut manager = Manager::singleton().write().unwrap();

    let p = root.path();
    assert!(manager.get(p).expect("success").is_none());

    let created_arc = manager
        .get_or_create_with_capacity(p, 10, Rkv::with_capacity::<Lmdb>)
        .expect("created");
    let fetched_arc = manager.get(p).expect("success").expect("existed");
    assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
}

/// Test that the manager will return the same Rkv instance each time for each path.
#[test]
fn test_same_with_capacity_safe() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let root = Builder::new()
        .prefix("test_same_with_capacity_safe")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut manager = Manager::singleton().write().unwrap();

    let p = root.path();
    assert!(manager.get(p).expect("success").is_none());

    let created_arc = manager
        .get_or_create_with_capacity(p, 10, Rkv::with_capacity::<SafeMode>)
        .expect("created");
    let fetched_arc = manager.get(p).expect("success").expect("existed");
    assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
}

/// Some storage drivers are able to discard when the database is corrupted at runtime.
/// Test how these managers can discard corrupted databases and re-open.
#[test]
fn test_safe_mode_corrupt_while_open_1() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let root = Builder::new()
        .prefix("test_safe_mode_corrupt_while_open_1")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // Create environment.
    let mut manager = Manager::singleton().write().unwrap();
    let shared_env = manager
        .get_or_create(root.path(), Rkv::new::<SafeMode>)
        .expect("created");
    let env = shared_env.read().unwrap();

    // Write some data.
    let store = env
        .open_single("store", StoreOptions::create())
        .expect("opened");
    let mut writer = env.write().expect("writer");
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
    env.sync(true).expect("synced");

    // Verify it was flushed to disk.
    let mut safebin = root.path().to_path_buf();
    safebin.push("data.safe.bin");
    assert!(safebin.exists());

    // Oops, corruption.
    fs::write(&safebin, "bogus").expect("dbfile corrupted");

    // Close everything.
    drop(env);
    drop(shared_env);
    manager
        .try_close(root.path(), CloseOptions::default())
        .expect("closed without deleting");
    assert!(manager.get(root.path()).expect("success").is_none());

    // Recreating environment fails.
    manager
        .get_or_create(root.path(), Rkv::new::<SafeMode>)
        .expect_err("not created");
    assert!(manager.get(root.path()).expect("success").is_none());

    // But we can use a builder and pass `discard_if_corrupted` to deal with it.
    let mut builder = Rkv::environment_builder::<SafeMode>();
    builder.set_discard_if_corrupted(true);
    manager
        .get_or_create_from_builder(root.path(), builder, Rkv::from_builder::<SafeMode>)
        .expect("created");
    assert!(manager.get(root.path()).expect("success").is_some());
}

/// Some storage drivers are able to recover when the database is corrupted at runtime.
/// Test how these managers can recover corrupted databases while open.
#[test]
fn test_safe_mode_corrupt_while_open_2() {
    type Manager = rkv::Manager<SafeModeEnvironment>;

    let root = Builder::new()
        .prefix("test_safe_mode_corrupt_while_open_2")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // Create environment.
    let mut manager = Manager::singleton().write().unwrap();
    let shared_env = manager
        .get_or_create(root.path(), Rkv::new::<SafeMode>)
        .expect("created");
    let env = shared_env.read().unwrap();

    // Write some data.
    let store = env
        .open_single("store", StoreOptions::create())
        .expect("opened");
    let mut writer = env.write().expect("writer");
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
    env.sync(true).expect("synced");

    // Verify it was flushed to disk.
    let mut safebin = root.path().to_path_buf();
    safebin.push("data.safe.bin");
    assert!(safebin.exists());

    // Oops, corruption.
    fs::write(&safebin, "bogus").expect("dbfile corrupted");

    // Reading still works. Magic.
    let store = env
        .open_single("store", StoreOptions::default())
        .expect("opened");
    let reader = env.read().expect("reader");
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
    reader.abort();

    // Writing still works, dbfile will be un-corrupted.
    let store = env
        .open_single("store", StoreOptions::default())
        .expect("opened");
    let mut writer = env.write().expect("writer");
    store
        .put(&mut writer, "foo2", &Value::I64(5678))
        .expect("wrote");
    store
        .put(&mut writer, "bar2", &Value::Bool(false))
        .expect("wrote");
    store
        .put(&mut writer, "baz2", &Value::Str("byé, yöu"))
        .expect("wrote");
    writer.commit().expect("committed");
    env.sync(true).expect("synced");

    // Close everything.
    drop(env);
    drop(shared_env);
    manager
        .try_close(root.path(), CloseOptions::default())
        .expect("closed without deleting");
    assert!(manager.get(root.path()).expect("success").is_none());

    // Recreate environment.
    let shared_env = manager
        .get_or_create(root.path(), Rkv::new::<SafeMode>)
        .expect("created");
    let env = shared_env.read().unwrap();

    // Verify that the dbfile is not corrupted.
    let store = env
        .open_single("store", StoreOptions::default())
        .expect("opened");
    let reader = env.read().expect("reader");
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
    assert_eq!(
        store.get(&reader, "foo2").expect("read"),
        Some(Value::I64(5678))
    );
    assert_eq!(
        store.get(&reader, "bar2").expect("read"),
        Some(Value::Bool(false))
    );
    assert_eq!(
        store.get(&reader, "baz2").expect("read"),
        Some(Value::Str("byé, yöu"))
    );
}
