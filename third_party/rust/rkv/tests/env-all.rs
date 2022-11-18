// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::fs;

use tempfile::Builder;

use rkv::{
    backend::{Lmdb, SafeMode},
    Rkv, StoreOptions, Value,
};

#[test]
fn test_open_safe_same_dir_as_lmdb() {
    let root = Builder::new()
        .prefix("test_open_safe_same_dir_as_lmdb")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // Create database of type A and save to disk.
    {
        let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

        let mut writer = k.write().expect("writer");
        sk.put(&mut writer, "foo", &Value::I64(1234))
            .expect("wrote");
        sk.put(&mut writer, "bar", &Value::Bool(true))
            .expect("wrote");
        sk.put(&mut writer, "baz", &Value::Str("héllo, yöu"))
            .expect("wrote");
        assert_eq!(
            sk.get(&writer, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            sk.get(&writer, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&writer, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
        writer.commit().expect("committed");
        k.sync(true).expect("synced");
    }
    // Verify that database of type A was written to disk.
    {
        let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let sk = k
            .open_single("sk", StoreOptions::default())
            .expect("opened");

        let reader = k.read().expect("reader");
        assert_eq!(
            sk.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            sk.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
    // Create database of type B and verify that it is empty.
    {
        let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let _ = k
            .open_single("sk", StoreOptions::default())
            .expect_err("not opened");
    }
    // Verify that database of type A wasn't changed.
    {
        let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let sk = k
            .open_single("sk", StoreOptions::default())
            .expect("opened");

        let reader = k.read().expect("reader");
        assert_eq!(
            sk.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            sk.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
    // Create database of type B and save to disk (type A exists at the same path).
    {
        let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

        let mut writer = k.write().expect("writer");
        sk.put(&mut writer, "foo1", &Value::I64(5678))
            .expect("wrote");
        sk.put(&mut writer, "bar1", &Value::Bool(false))
            .expect("wrote");
        sk.put(&mut writer, "baz1", &Value::Str("héllo~ yöu"))
            .expect("wrote");
        assert_eq!(
            sk.get(&writer, "foo1").expect("read"),
            Some(Value::I64(5678))
        );
        assert_eq!(
            sk.get(&writer, "bar1").expect("read"),
            Some(Value::Bool(false))
        );
        assert_eq!(
            sk.get(&writer, "baz1").expect("read"),
            Some(Value::Str("héllo~ yöu"))
        );
        writer.commit().expect("committed");
        k.sync(true).expect("synced");
    }
    // Verify that database of type B was written to disk.
    {
        let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let sk = k
            .open_single("sk", StoreOptions::default())
            .expect("opened");

        let reader = k.read().expect("reader");
        assert_eq!(
            sk.get(&reader, "foo1").expect("read"),
            Some(Value::I64(5678))
        );
        assert_eq!(
            sk.get(&reader, "bar1").expect("read"),
            Some(Value::Bool(false))
        );
        assert_eq!(
            sk.get(&reader, "baz1").expect("read"),
            Some(Value::Str("héllo~ yöu"))
        );
    }
    // Verify that database of type A still wasn't changed.
    {
        let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let sk = k
            .open_single("sk", StoreOptions::default())
            .expect("opened");

        let reader = k.read().expect("reader");
        assert_eq!(
            sk.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            sk.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
}

#[test]
fn test_open_lmdb_same_dir_as_safe() {
    let root = Builder::new()
        .prefix("test_open_lmdb_same_dir_as_safe")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    // Create database of type A and save to disk.
    {
        let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

        let mut writer = k.write().expect("writer");
        sk.put(&mut writer, "foo", &Value::I64(1234))
            .expect("wrote");
        sk.put(&mut writer, "bar", &Value::Bool(true))
            .expect("wrote");
        sk.put(&mut writer, "baz", &Value::Str("héllo, yöu"))
            .expect("wrote");
        assert_eq!(
            sk.get(&writer, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            sk.get(&writer, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&writer, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
        writer.commit().expect("committed");
        k.sync(true).expect("synced");
    }
    // Verify that database of type A was written to disk.
    {
        let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let sk = k
            .open_single("sk", StoreOptions::default())
            .expect("opened");

        let reader = k.read().expect("reader");
        assert_eq!(
            sk.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            sk.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
    // Create database of type B and verify that it is empty.
    {
        let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let _ = k
            .open_single("sk", StoreOptions::default())
            .expect_err("not opened");
    }
    // Verify that database of type A wasn't changed.
    {
        let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let sk = k
            .open_single("sk", StoreOptions::default())
            .expect("opened");

        let reader = k.read().expect("reader");
        assert_eq!(
            sk.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            sk.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
    // Create database of type B and save to disk (type A exists at the same path).
    {
        let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

        let mut writer = k.write().expect("writer");
        sk.put(&mut writer, "foo1", &Value::I64(5678))
            .expect("wrote");
        sk.put(&mut writer, "bar1", &Value::Bool(false))
            .expect("wrote");
        sk.put(&mut writer, "baz1", &Value::Str("héllo~ yöu"))
            .expect("wrote");
        assert_eq!(
            sk.get(&writer, "foo1").expect("read"),
            Some(Value::I64(5678))
        );
        assert_eq!(
            sk.get(&writer, "bar1").expect("read"),
            Some(Value::Bool(false))
        );
        assert_eq!(
            sk.get(&writer, "baz1").expect("read"),
            Some(Value::Str("héllo~ yöu"))
        );
        writer.commit().expect("committed");
        k.sync(true).expect("synced");
    }
    // Verify that database of type B was written to disk.
    {
        let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
        let sk = k
            .open_single("sk", StoreOptions::default())
            .expect("opened");

        let reader = k.read().expect("reader");
        assert_eq!(
            sk.get(&reader, "foo1").expect("read"),
            Some(Value::I64(5678))
        );
        assert_eq!(
            sk.get(&reader, "bar1").expect("read"),
            Some(Value::Bool(false))
        );
        assert_eq!(
            sk.get(&reader, "baz1").expect("read"),
            Some(Value::Str("héllo~ yöu"))
        );
    }
    // Verify that database of type A still wasn't changed.
    {
        let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
        let sk = k
            .open_single("sk", StoreOptions::default())
            .expect("opened");

        let reader = k.read().expect("reader");
        assert_eq!(
            sk.get(&reader, "foo").expect("read"),
            Some(Value::I64(1234))
        );
        assert_eq!(
            sk.get(&reader, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&reader, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }
}
