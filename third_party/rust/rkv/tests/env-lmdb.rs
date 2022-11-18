// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

// TODO: change this back to `clippy::cognitive_complexity` when Clippy stable
// deprecates `clippy::cyclomatic_complexity`.
#![allow(clippy::complexity)]

use std::{
    fs,
    path::Path,
    str,
    sync::{Arc, RwLock},
    thread,
};

use byteorder::{ByteOrder, LittleEndian};
use tempfile::Builder;

use rkv::{
    backend::{
        BackendEnvironmentBuilder, BackendInfo, BackendStat, Lmdb, LmdbDatabase, LmdbEnvironment,
        LmdbRwTransaction,
    },
    EnvironmentFlags, Rkv, SingleStore, StoreError, StoreOptions, Value, Writer,
};

fn check_rkv(k: &Rkv<LmdbEnvironment>) {
    let _ = k
        .open_single(None, StoreOptions::create())
        .expect("created default");

    let s = k.open_single("s", StoreOptions::create()).expect("opened");
    let reader = k.read().expect("reader");

    let result = s.get(&reader, "foo");
    assert_eq!(None, result.expect("success but no value"));
}

// The default size is 1MB.
const DEFAULT_SIZE: usize = 1024 * 1024;

/// We can't open a directory that doesn't exist.
#[test]
fn test_open_fails() {
    let root = Builder::new()
        .prefix("test_open_fails")
        .tempdir()
        .expect("tempdir");
    assert!(root.path().exists());

    let nope = root.path().join("nope/");
    assert!(!nope.exists());

    let pb = nope.to_path_buf();
    match Rkv::new::<Lmdb>(nope.as_path()).err() {
        Some(StoreError::UnsuitableEnvironmentPath(p)) => {
            assert_eq!(pb, p);
        }
        _ => panic!("expected error"),
    };
}

#[test]
fn test_open() {
    let root = Builder::new()
        .prefix("test_open")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    check_rkv(&k);
}

#[test]
fn test_open_from_builder() {
    let root = Builder::new()
        .prefix("test_open_from_builder")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let mut builder = Rkv::environment_builder::<Lmdb>();
    builder.set_max_dbs(2);

    let k = Rkv::from_builder(root.path(), builder).expect("rkv");
    check_rkv(&k);
}

#[test]
fn test_open_from_builder_with_no_subdir_1() {
    let root = Builder::new()
        .prefix("test_open_from_builder")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    {
        let mut builder = Rkv::environment_builder::<Lmdb>();
        builder.set_max_dbs(2);

        let k = Rkv::from_builder(root.path(), builder).expect("rkv");
        check_rkv(&k);
    }
    {
        let mut builder = Rkv::environment_builder::<Lmdb>();
        builder.set_flags(EnvironmentFlags::NO_SUB_DIR);
        builder.set_max_dbs(2);

        let mut datamdb = root.path().to_path_buf();
        datamdb.push("data.mdb");

        let k = Rkv::from_builder(&datamdb, builder).expect("rkv");
        check_rkv(&k);
    }
}

#[test]
#[should_panic(expected = "rkv: UnsuitableEnvironmentPath")]
fn test_open_from_builder_with_no_subdir_2() {
    let root = Builder::new()
        .prefix("test_open_from_builder")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    {
        let mut builder = Rkv::environment_builder::<Lmdb>();
        builder.set_max_dbs(2);

        let k = Rkv::from_builder(root.path(), builder).expect("rkv");
        check_rkv(&k);
    }
    {
        let mut builder = Rkv::environment_builder::<Lmdb>();
        builder.set_flags(EnvironmentFlags::NO_SUB_DIR);
        builder.set_max_dbs(2);

        let mut datamdb = root.path().to_path_buf();
        datamdb.push("bogus.mdb");

        let k = Rkv::from_builder(&datamdb, builder).expect("rkv");
        check_rkv(&k);
    }
}

#[test]
fn test_open_from_builder_with_dir_1() {
    let root = Builder::new()
        .prefix("test_open_from_builder")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());

    let mut builder = Rkv::environment_builder::<Lmdb>();
    builder.set_max_dbs(2);
    builder.set_make_dir_if_needed(true);

    let k = Rkv::from_builder(root.path(), builder).expect("rkv");
    check_rkv(&k);
}

#[test]
#[should_panic(expected = "rkv: UnsuitableEnvironmentPath(\"bogus\")")]
fn test_open_from_builder_with_dir_2() {
    let root = Path::new("bogus");
    println!("Root path: {root:?}");
    assert!(!root.is_dir());

    let mut builder = Rkv::environment_builder::<Lmdb>();
    builder.set_max_dbs(2);

    let k = Rkv::from_builder(root, builder).expect("rkv");
    check_rkv(&k);
}

#[test]
#[should_panic(expected = "opened: DbsFull")]
fn test_create_with_capacity_1() {
    let root = Builder::new()
        .prefix("test_create_with_capacity")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::with_capacity::<Lmdb>(root.path(), 1).expect("rkv");
    check_rkv(&k);

    // This errors with "opened: DbsFull" because we specified a capacity of one (database),
    // and check_rkv already opened one (plus the default database, which doesn't count
    // against the limit).
    let _zzz = k
        .open_single("zzz", StoreOptions::create())
        .expect("opened");
}

#[test]
fn test_create_with_capacity_2() {
    let root = Builder::new()
        .prefix("test_create_with_capacity")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::with_capacity::<Lmdb>(root.path(), 1).expect("rkv");
    check_rkv(&k);

    // This doesn't error with "opened: DbsFull" with because even though we specified a
    // capacity of one (database), and check_rkv already opened one, the default database
    // doesn't count against the limit.
    let _zzz = k.open_single(None, StoreOptions::create()).expect("opened");
}

#[test]
#[should_panic(expected = "opened: DbsFull")]
fn test_open_with_capacity_1() {
    let root = Builder::new()
        .prefix("test_open_with_capacity")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::with_capacity::<Lmdb>(root.path(), 1).expect("rkv");
    check_rkv(&k);

    let _zzz = k
        .open_single("zzz", StoreOptions::default())
        .expect("opened");
}

#[test]
fn test_open_with_capacity_2() {
    let root = Builder::new()
        .prefix("test_open_with_capacity")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::with_capacity::<Lmdb>(root.path(), 1).expect("rkv");
    check_rkv(&k);

    let _zzz = k
        .open_single(None, StoreOptions::default())
        .expect("opened");
}

#[test]
fn test_list_dbs_1() {
    let root = Builder::new()
        .prefix("test_list_dbs")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::with_capacity::<Lmdb>(root.path(), 1).expect("rkv");
    check_rkv(&k);

    let dbs = k.get_dbs().unwrap();
    assert_eq!(dbs, vec![Some("s".to_owned())]);
}

#[test]
fn test_list_dbs_2() {
    let root = Builder::new()
        .prefix("test_list_dbs")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::with_capacity::<Lmdb>(root.path(), 2).expect("rkv");
    check_rkv(&k);

    let _ = k
        .open_single("zzz", StoreOptions::create())
        .expect("opened");

    let dbs = k.get_dbs().unwrap();
    assert_eq!(dbs, vec![Some("s".to_owned()), Some("zzz".to_owned())]);
}

#[test]
fn test_list_dbs_3() {
    let root = Builder::new()
        .prefix("test_list_dbs")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::with_capacity::<Lmdb>(root.path(), 0).expect("rkv");

    let _ = k.open_single(None, StoreOptions::create()).expect("opened");

    let dbs = k.get_dbs().unwrap();
    assert_eq!(dbs, vec![None]);
}

fn get_larger_than_default_map_size_value() -> usize {
    // The LMDB C library and lmdb Rust crate docs for setting the map size
    // <http://www.lmdb.tech/doc/group__mdb.html#gaa2506ec8dab3d969b0e609cd82e619e5>
    // <https://docs.rs/lmdb/0.8.0/lmdb/struct.EnvironmentBuilder.html#method.set_map_size>
    // both say that the default map size is 10,485,760 bytes, i.e. 10MiB.
    //
    // But the DEFAULT_MAPSIZE define in the LMDB code
    // https://github.com/LMDB/lmdb/blob/26c7df88e44e31623d0802a564f24781acdefde3/libraries/liblmdb/mdb.c#L729
    // sets the default map size to 1,048,576 bytes, i.e. 1MiB.
    //
    DEFAULT_SIZE + 1 /* 1,048,576 + 1 bytes, i.e. 1MiB + 1 byte */
}

#[test]
#[should_panic(expected = "wrote: MapFull")]
fn test_exceed_map_size() {
    let root = Builder::new()
        .prefix("test_exceed_map_size")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k
        .open_single("test", StoreOptions::create())
        .expect("opened");

    // Writing a large enough value should cause LMDB to fail on MapFull.
    // We write a string that is larger than the default map size.
    let val = "x".repeat(get_larger_than_default_map_size_value());
    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, "foo", &Value::Str(&val))
        .expect("wrote");
}

#[test]
#[should_panic(expected = "wrote: KeyValuePairBadSize")]
fn test_exceed_key_size_limit() {
    let root = Builder::new()
        .prefix("test_exceed_key_size_limit")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k
        .open_single("test", StoreOptions::create())
        .expect("opened");

    let key = "k".repeat(512);
    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, key, &Value::Str("val")).expect("wrote");
}

#[test]
fn test_increase_map_size() {
    let root = Builder::new()
        .prefix("test_open_with_map_size")
        .tempdir()
        .expect("tempdir");
    println!("Root path: {:?}", root.path());
    fs::create_dir_all(root.path()).expect("dir created");
    assert!(root.path().is_dir());

    let mut builder = Rkv::environment_builder::<Lmdb>();
    // Set the map size to the size of the value we'll store in it + 100KiB,
    // which ensures that there's enough space for the value and metadata.
    builder.set_map_size(
        get_larger_than_default_map_size_value() + 100 * 1024, /* 100KiB */
    );
    builder.set_max_dbs(2);
    let k = Rkv::from_builder(root.path(), builder).unwrap();
    let sk = k
        .open_single("test", StoreOptions::create())
        .expect("opened");
    let val = "x".repeat(get_larger_than_default_map_size_value());

    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, "foo", &Value::Str(&val))
        .expect("wrote");
    writer.commit().expect("committed");

    let reader = k.read().unwrap();
    assert_eq!(
        sk.get(&reader, "foo").expect("read"),
        Some(Value::Str(&val))
    );
}

#[test]
fn test_round_trip_and_transactions() {
    let root = Builder::new()
        .prefix("test_round_trip_and_transactions")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    {
        let mut writer = k.write().expect("writer");
        sk.put(&mut writer, "foo", &Value::I64(1234))
            .expect("wrote");
        sk.put(&mut writer, "noo", &Value::F64(1234.0.into()))
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
            sk.get(&writer, "noo").expect("read"),
            Some(Value::F64(1234.0.into()))
        );
        assert_eq!(
            sk.get(&writer, "bar").expect("read"),
            Some(Value::Bool(true))
        );
        assert_eq!(
            sk.get(&writer, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );

        // Isolation. Reads won't return values.
        let r = &k.read().unwrap();
        assert_eq!(sk.get(r, "foo").expect("read"), None);
        assert_eq!(sk.get(r, "bar").expect("read"), None);
        assert_eq!(sk.get(r, "baz").expect("read"), None);
    }

    // Dropped: tx rollback. Reads will still return nothing.

    {
        let r = &k.read().unwrap();
        assert_eq!(sk.get(r, "foo").expect("read"), None);
        assert_eq!(sk.get(r, "bar").expect("read"), None);
        assert_eq!(sk.get(r, "baz").expect("read"), None);
    }

    {
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
    }

    // Committed. Reads will succeed.

    {
        let r = k.read().unwrap();
        assert_eq!(sk.get(&r, "foo").expect("read"), Some(Value::I64(1234)));
        assert_eq!(sk.get(&r, "bar").expect("read"), Some(Value::Bool(true)));
        assert_eq!(
            sk.get(&r, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }

    {
        let mut writer = k.write().expect("writer");
        sk.delete(&mut writer, "foo").expect("deleted");
        sk.delete(&mut writer, "bar").expect("deleted");
        sk.delete(&mut writer, "baz").expect("deleted");
        assert_eq!(sk.get(&writer, "foo").expect("read"), None);
        assert_eq!(sk.get(&writer, "bar").expect("read"), None);
        assert_eq!(sk.get(&writer, "baz").expect("read"), None);

        // Isolation. Reads still return values.
        let r = k.read().unwrap();
        assert_eq!(sk.get(&r, "foo").expect("read"), Some(Value::I64(1234)));
        assert_eq!(sk.get(&r, "bar").expect("read"), Some(Value::Bool(true)));
        assert_eq!(
            sk.get(&r, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }

    // Dropped: tx rollback. Reads will still return values.

    {
        let r = k.read().unwrap();
        assert_eq!(sk.get(&r, "foo").expect("read"), Some(Value::I64(1234)));
        assert_eq!(sk.get(&r, "bar").expect("read"), Some(Value::Bool(true)));
        assert_eq!(
            sk.get(&r, "baz").expect("read"),
            Some(Value::Str("héllo, yöu"))
        );
    }

    {
        let mut writer = k.write().expect("writer");
        sk.delete(&mut writer, "foo").expect("deleted");
        sk.delete(&mut writer, "bar").expect("deleted");
        sk.delete(&mut writer, "baz").expect("deleted");
        assert_eq!(sk.get(&writer, "foo").expect("read"), None);
        assert_eq!(sk.get(&writer, "bar").expect("read"), None);
        assert_eq!(sk.get(&writer, "baz").expect("read"), None);

        writer.commit().expect("committed");
    }

    // Committed. Reads will succeed but return None to indicate a missing value.

    {
        let r = k.read().unwrap();
        assert_eq!(sk.get(&r, "foo").expect("read"), None);
        assert_eq!(sk.get(&r, "bar").expect("read"), None);
        assert_eq!(sk.get(&r, "baz").expect("read"), None);
    }
}

#[test]
fn test_single_store_clear() {
    let root = Builder::new()
        .prefix("test_single_store_clear")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    {
        let mut writer = k.write().expect("writer");
        sk.put(&mut writer, "foo", &Value::I64(1234))
            .expect("wrote");
        sk.put(&mut writer, "bar", &Value::Bool(true))
            .expect("wrote");
        sk.put(&mut writer, "baz", &Value::Str("héllo, yöu"))
            .expect("wrote");
        writer.commit().expect("committed");
    }

    {
        let mut writer = k.write().expect("writer");
        sk.clear(&mut writer).expect("cleared");
        writer.commit().expect("committed");
    }

    {
        let r = k.read().unwrap();
        let iter = sk.iter_start(&r).expect("iter");
        assert_eq!(iter.count(), 0);
    }
}

#[test]
#[should_panic(expected = "KeyValuePairNotFound")]
fn test_single_store_delete_nonexistent() {
    let root = Builder::new()
        .prefix("test_single_store_delete_nonexistent")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    let mut writer = k.write().expect("writer");
    sk.delete(&mut writer, "bogus").unwrap();
}

#[test]
#[cfg(feature = "db-dup-sort")]
fn test_multi_put_get_del() {
    let root = Builder::new()
        .prefix("test_multi_put_get_del")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let multistore = k.open_multi("multistore", StoreOptions::create()).unwrap();

    let mut writer = k.write().unwrap();
    multistore
        .put(&mut writer, "str1", &Value::Str("str1 foo"))
        .unwrap();
    multistore
        .put(&mut writer, "str1", &Value::Str("str1 bar"))
        .unwrap();
    multistore
        .put(&mut writer, "str2", &Value::Str("str2 foo"))
        .unwrap();
    multistore
        .put(&mut writer, "str2", &Value::Str("str2 bar"))
        .unwrap();
    multistore
        .put(&mut writer, "str3", &Value::Str("str3 foo"))
        .unwrap();
    multistore
        .put(&mut writer, "str3", &Value::Str("str3 bar"))
        .unwrap();
    writer.commit().unwrap();

    let writer = k.write().unwrap();
    {
        let mut iter = multistore.get(&writer, "str1").unwrap();
        let (id, val) = iter.next().unwrap().unwrap();
        assert_eq!((id, val), (&b"str1"[..], Value::Str("str1 bar")));
        let (id, val) = iter.next().unwrap().unwrap();
        assert_eq!((id, val), (&b"str1"[..], Value::Str("str1 foo")));
    }
    writer.commit().unwrap();

    let mut writer = k.write().unwrap();
    multistore
        .delete(&mut writer, "str1", &Value::Str("str1 foo"))
        .unwrap();
    assert_eq!(
        multistore.get_first(&writer, "str1").unwrap(),
        Some(Value::Str("str1 bar"))
    );
    multistore
        .delete(&mut writer, "str2", &Value::Str("str2 bar"))
        .unwrap();
    assert_eq!(
        multistore.get_first(&writer, "str2").unwrap(),
        Some(Value::Str("str2 foo"))
    );
    multistore.delete_all(&mut writer, "str3").unwrap();
    assert_eq!(multistore.get_first(&writer, "str3").unwrap(), None);
    writer.commit().unwrap();
}

#[test]
#[cfg(feature = "db-dup-sort")]
fn test_multiple_store_clear() {
    let root = Builder::new()
        .prefix("test_multiple_store_clear")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let multistore = k
        .open_multi("multistore", StoreOptions::create())
        .expect("opened");

    {
        let mut writer = k.write().expect("writer");
        multistore
            .put(&mut writer, "str1", &Value::Str("str1 foo"))
            .unwrap();
        multistore
            .put(&mut writer, "str1", &Value::Str("str1 bar"))
            .unwrap();
        multistore
            .put(&mut writer, "str2", &Value::Str("str2 foo"))
            .unwrap();
        multistore
            .put(&mut writer, "str2", &Value::Str("str2 bar"))
            .unwrap();
        multistore
            .put(&mut writer, "str3", &Value::Str("str3 foo"))
            .unwrap();
        multistore
            .put(&mut writer, "str3", &Value::Str("str3 bar"))
            .unwrap();
        writer.commit().expect("committed");
    }

    {
        let mut writer = k.write().expect("writer");
        multistore.clear(&mut writer).expect("cleared");
        writer.commit().expect("committed");
    }

    {
        let r = k.read().unwrap();
        assert_eq!(multistore.get_first(&r, "str1").expect("read"), None);
        assert_eq!(multistore.get_first(&r, "str2").expect("read"), None);
        assert_eq!(multistore.get_first(&r, "str3").expect("read"), None);
    }
}

#[test]
fn test_open_store_for_read() {
    let root = Builder::new()
        .prefix("test_open_store_for_read")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");

    // First create the store, and start a write transaction on it.
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");
    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, "foo", &Value::Str("bar"))
        .expect("write");

    // Open the same store for read, note that the write transaction is still in progress,
    // it should not block the reader though.
    let sk_readonly = k
        .open_single("sk", StoreOptions::default())
        .expect("opened");
    writer.commit().expect("commit");

    // Now the write transaction is committed, any followed reads should see its change.
    let reader = k.read().expect("reader");
    assert_eq!(
        sk_readonly.get(&reader, "foo").expect("read"),
        Some(Value::Str("bar"))
    );
}

#[test]
#[should_panic(expected = "open a missing store")]
fn test_open_a_missing_store() {
    let root = Builder::new()
        .prefix("test_open_a_missing_store")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let _sk = k
        .open_single("sk", StoreOptions::default())
        .expect("open a missing store");
}

#[test]
#[should_panic(expected = "new failed: FileInvalid")]
fn test_open_a_broken_store() {
    let root = Builder::new()
        .prefix("test_open_a_missing_store")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let dbfile = root.path().join("data.mdb");
    fs::write(dbfile, "bogus").expect("dbfile created");

    let _ = Rkv::new::<Lmdb>(root.path()).expect("new failed");
}

#[test]
fn test_open_fail_with_badrslot() {
    let root = Builder::new()
        .prefix("test_open_fail_with_badrslot")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");

    // First create the store
    let _sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    // Open a reader on this store
    let _reader = k.read().expect("reader");

    // Open the same store for read while the reader is in progress will panic
    let store = k.open_single("sk", StoreOptions::default());
    match store {
        Err(StoreError::OpenAttemptedDuringTransaction(_thread_id)) => (),
        _ => panic!("should panic"),
    }
}

#[test]
fn test_read_before_write_num() {
    let root = Builder::new()
        .prefix("test_read_before_write_num")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    // Test reading a number, modifying it, and then writing it back.
    // We have to be done with the Value::I64 before calling Writer::put,
    // as the Value::I64 borrows an immutable reference to the Writer.
    // So we extract and copy its primitive value.

    fn get_existing_foo(
        store: SingleStore<LmdbDatabase>,
        writer: &Writer<LmdbRwTransaction>,
    ) -> Option<i64> {
        match store.get(writer, "foo").expect("read") {
            Some(Value::I64(val)) => Some(val),
            _ => None,
        }
    }

    let mut writer = k.write().expect("writer");
    let mut existing = get_existing_foo(sk, &writer).unwrap_or(99);
    existing += 1;
    sk.put(&mut writer, "foo", &Value::I64(existing))
        .expect("success");

    let updated = get_existing_foo(sk, &writer).unwrap_or(99);
    assert_eq!(updated, 100);
    writer.commit().expect("commit");
}

#[test]
fn test_read_before_write_str() {
    let root = Builder::new()
        .prefix("test_read_before_write_str")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    // Test reading a string, modifying it, and then writing it back.
    // We have to be done with the Value::Str before calling Writer::put,
    // as the Value::Str (and its underlying &str) borrows an immutable
    // reference to the Writer.  So we copy it to a String.

    fn get_existing_foo(
        store: SingleStore<LmdbDatabase>,
        writer: &Writer<LmdbRwTransaction>,
    ) -> Option<String> {
        match store.get(writer, "foo").expect("read") {
            Some(Value::Str(val)) => Some(val.to_string()),
            _ => None,
        }
    }

    let mut writer = k.write().expect("writer");
    let mut existing = get_existing_foo(sk, &writer).unwrap_or_default();
    existing.push('…');
    sk.put(&mut writer, "foo", &Value::Str(&existing))
        .expect("write");

    let updated = get_existing_foo(sk, &writer).unwrap_or_default();
    assert_eq!(updated, "…");
    writer.commit().expect("commit");
}

#[test]
fn test_concurrent_read_transactions_prohibited() {
    let root = Builder::new()
        .prefix("test_concurrent_reads_prohibited")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let _first = k.read().expect("reader");
    let second = k.read();

    match second {
        Err(StoreError::ReadTransactionAlreadyExists(t)) => {
            println!("Thread was {t:?}");
        }
        Err(e) => {
            println!("Got error {e:?}");
        }
        _ => {
            panic!("Expected error.");
        }
    }
}

#[test]
fn test_isolation() {
    let root = Builder::new()
        .prefix("test_isolation")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let s = k.open_single("s", StoreOptions::create()).expect("opened");

    // Add one field.
    {
        let mut writer = k.write().expect("writer");
        s.put(&mut writer, "foo", &Value::I64(1234)).expect("wrote");
        writer.commit().expect("committed");
    }

    {
        let reader = k.read().unwrap();
        assert_eq!(s.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
    }

    // Establish a long-lived reader that outlasts a writer.
    let reader = k.read().expect("reader");
    assert_eq!(s.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));

    // Start a write transaction.
    let mut writer = k.write().expect("writer");
    s.put(&mut writer, "foo", &Value::I64(999)).expect("wrote");

    // The reader and writer are isolated.
    assert_eq!(s.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));
    assert_eq!(s.get(&writer, "foo").expect("read"), Some(Value::I64(999)));

    // If we commit the writer, we still have isolation.
    writer.commit().expect("committed");
    assert_eq!(s.get(&reader, "foo").expect("read"), Some(Value::I64(1234)));

    // A new reader sees the committed value. Note that LMDB doesn't allow two
    // read transactions to exist in the same thread, so we abort the previous one.
    reader.abort();
    let reader = k.read().expect("reader");
    assert_eq!(s.get(&reader, "foo").expect("read"), Some(Value::I64(999)));
}

#[test]
fn test_blob() {
    let root = Builder::new()
        .prefix("test_round_trip_blob")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    let mut writer = k.write().expect("writer");
    assert_eq!(sk.get(&writer, "foo").expect("read"), None);
    sk.put(&mut writer, "foo", &Value::Blob(&[1, 2, 3, 4]))
        .expect("wrote");
    assert_eq!(
        sk.get(&writer, "foo").expect("read"),
        Some(Value::Blob(&[1, 2, 3, 4]))
    );

    fn u16_to_u8(src: &[u16]) -> Vec<u8> {
        let mut dst = vec![0; 2 * src.len()];
        LittleEndian::write_u16_into(src, &mut dst);
        dst
    }

    fn u8_to_u16(src: &[u8]) -> Vec<u16> {
        let mut dst = vec![0; src.len() / 2];
        LittleEndian::read_u16_into(src, &mut dst);
        dst
    }

    // When storing UTF-16 strings as blobs, we'll need to convert
    // their [u16] backing storage to [u8].  Test that converting, writing,
    // reading, and converting back works as expected.
    let u16_array = [1000, 10000, 54321, 65535];
    assert_eq!(sk.get(&writer, "bar").expect("read"), None);
    sk.put(&mut writer, "bar", &Value::Blob(&u16_to_u8(&u16_array)))
        .expect("wrote");
    let u8_array = match sk.get(&writer, "bar").expect("read") {
        Some(Value::Blob(val)) => val,
        _ => &[],
    };
    assert_eq!(u8_to_u16(u8_array), u16_array);
}

#[test]
fn test_sync() {
    let root = Builder::new()
        .prefix("test_sync")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let mut builder = Rkv::environment_builder::<Lmdb>();
    builder.set_max_dbs(1);
    builder.set_flags(EnvironmentFlags::NO_SYNC);
    {
        let k = Rkv::from_builder(root.path(), builder).expect("new succeeded");
        let sk = k.open_single("sk", StoreOptions::create()).expect("opened");
        {
            let mut writer = k.write().expect("writer");
            sk.put(&mut writer, "foo", &Value::I64(1234))
                .expect("wrote");
            writer.commit().expect("committed");
            k.sync(true).expect("synced");
        }
    }
    let k = Rkv::from_builder(root.path(), builder).expect("new succeeded");
    let sk = k
        .open_single("sk", StoreOptions::default())
        .expect("opened");
    let reader = k.read().expect("reader");
    assert_eq!(
        sk.get(&reader, "foo").expect("read"),
        Some(Value::I64(1234))
    );
}

#[test]
#[cfg(feature = "db-int-key")]
fn test_stat() {
    let root = Builder::new()
        .prefix("test_stat")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    for i in 0..5 {
        let sk = k
            .open_integer(&format!("sk{i}")[..], StoreOptions::create())
            .expect("opened");
        {
            let mut writer = k.write().expect("writer");
            sk.put(&mut writer, i, &Value::I64(i64::from(i)))
                .expect("wrote");
            writer.commit().expect("committed");
        }
    }
    assert_eq!(k.stat().expect("stat").depth(), 1);
    assert_eq!(k.stat().expect("stat").entries(), 5);
    assert_eq!(k.stat().expect("stat").branch_pages(), 0);
    assert_eq!(k.stat().expect("stat").leaf_pages(), 1);
}

#[test]
fn test_info() {
    let root = Builder::new()
        .prefix("test_info")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, "foo", &Value::Str("bar"))
        .expect("wrote");
    writer.commit().expect("commited");

    let info = k.info().expect("info");

    // The default size is 1MB.
    assert_eq!(info.map_size(), DEFAULT_SIZE);
    // Should greater than 0 after the write txn.
    assert!(info.last_pgno() > 0);
    // A txn to open_single + a txn to write.
    assert_eq!(info.last_txnid(), 2);
    // The default max readers is 126.
    assert_eq!(info.max_readers(), 126);
    assert_eq!(info.num_readers(), 0);

    // A new reader should increment the reader counter.
    let _reader = k.read().expect("reader");
    let info = k.info().expect("info");

    assert_eq!(info.num_readers(), 1);
}

#[test]
fn test_load_ratio() {
    let root = Builder::new()
        .prefix("test_load_ratio")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, "foo", &Value::Str("bar"))
        .expect("wrote");
    writer.commit().expect("commited");
    let ratio = k.load_ratio().expect("ratio").unwrap();
    assert!(ratio > 0.0_f32 && ratio < 1.0_f32);

    // Put data to database should increase the load ratio.
    let mut writer = k.write().expect("writer");
    sk.put(
        &mut writer,
        "bar",
        &Value::Str(&"more-than-4KB".repeat(1000)),
    )
    .expect("wrote");
    writer.commit().expect("commited");
    let new_ratio = k.load_ratio().expect("ratio").unwrap();
    assert!(new_ratio > ratio);

    // Clear the database so that all the used pages should go to freelist, hence the ratio
    // should decrease.
    let mut writer = k.write().expect("writer");
    sk.clear(&mut writer).expect("clear");
    writer.commit().expect("commited");
    let after_clear_ratio = k.load_ratio().expect("ratio").unwrap();
    assert!(after_clear_ratio < new_ratio);
}

#[test]
fn test_set_map_size() {
    let root = Builder::new()
        .prefix("test_size_map_size")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    assert_eq!(k.info().expect("info").map_size(), DEFAULT_SIZE);

    k.set_map_size(2 * DEFAULT_SIZE).expect("resized");

    // Should be able to write.
    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, "foo", &Value::Str("bar"))
        .expect("wrote");
    writer.commit().expect("commited");

    assert_eq!(k.info().expect("info").map_size(), 2 * DEFAULT_SIZE);
}

#[test]
fn test_iter() {
    let root = Builder::new()
        .prefix("test_iter")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    // An iterator over an empty store returns no values.
    {
        let reader = k.read().unwrap();
        let mut iter = sk.iter_start(&reader).unwrap();
        assert!(iter.next().is_none());
    }

    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, "foo", &Value::I64(1234))
        .expect("wrote");
    sk.put(&mut writer, "noo", &Value::F64(1234.0.into()))
        .expect("wrote");
    sk.put(&mut writer, "bar", &Value::Bool(true))
        .expect("wrote");
    sk.put(&mut writer, "baz", &Value::Str("héllo, yöu"))
        .expect("wrote");
    sk.put(&mut writer, "héllò, töűrîst", &Value::Str("Emil.RuleZ!"))
        .expect("wrote");
    sk.put(&mut writer, "你好，遊客", &Value::Str("米克規則"))
        .expect("wrote");
    writer.commit().expect("committed");

    let reader = k.read().unwrap();

    // Reader.iter() returns (key, value) tuples ordered by key.
    let mut iter = sk.iter_start(&reader).unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "bar");
    assert_eq!(val, Value::Bool(true));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "baz");
    assert_eq!(val, Value::Str("héllo, yöu"));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "foo");
    assert_eq!(val, Value::I64(1234));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "héllò, töűrîst");
    assert_eq!(val, Value::Str("Emil.RuleZ!"));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());

    // Iterators don't loop.  Once one returns None, additional calls
    // to its next() method will always return None.
    assert!(iter.next().is_none());

    // Reader.iter_from() begins iteration at the first key equal to
    // or greater than the given key.
    let mut iter = sk.iter_from(&reader, "moo").unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());

    // Reader.iter_from() works as expected when the given key is a prefix
    // of a key in the store.
    let mut iter = sk.iter_from(&reader, "no").unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());
}

#[test]
fn test_iter_from_key_greater_than_existing() {
    let root = Builder::new()
        .prefix("test_iter_from_key_greater_than_existing")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");
    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let sk = k.open_single("sk", StoreOptions::create()).expect("opened");

    let mut writer = k.write().expect("writer");
    sk.put(&mut writer, "foo", &Value::I64(1234))
        .expect("wrote");
    sk.put(&mut writer, "noo", &Value::F64(1234.0.into()))
        .expect("wrote");
    sk.put(&mut writer, "bar", &Value::Bool(true))
        .expect("wrote");
    sk.put(&mut writer, "baz", &Value::Str("héllo, yöu"))
        .expect("wrote");
    writer.commit().expect("committed");

    let reader = k.read().unwrap();
    let mut iter = sk.iter_from(&reader, "nuu").unwrap();
    assert!(iter.next().is_none());
}

#[test]
fn test_multiple_store_read_write() {
    let root = Builder::new()
        .prefix("test_multiple_store_read_write")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let s1 = k
        .open_single("store_1", StoreOptions::create())
        .expect("opened");
    let s2 = k
        .open_single("store_2", StoreOptions::create())
        .expect("opened");
    let s3 = k
        .open_single("store_3", StoreOptions::create())
        .expect("opened");

    let mut writer = k.write().expect("writer");
    s1.put(&mut writer, "foo", &Value::Str("bar"))
        .expect("wrote");
    s2.put(&mut writer, "foo", &Value::I64(123)).expect("wrote");
    s3.put(&mut writer, "foo", &Value::Bool(true))
        .expect("wrote");

    assert_eq!(
        s1.get(&writer, "foo").expect("read"),
        Some(Value::Str("bar"))
    );
    assert_eq!(s2.get(&writer, "foo").expect("read"), Some(Value::I64(123)));
    assert_eq!(
        s3.get(&writer, "foo").expect("read"),
        Some(Value::Bool(true))
    );

    writer.commit().expect("committed");

    let reader = k.read().expect("unbound_reader");
    assert_eq!(
        s1.get(&reader, "foo").expect("read"),
        Some(Value::Str("bar"))
    );
    assert_eq!(s2.get(&reader, "foo").expect("read"), Some(Value::I64(123)));
    assert_eq!(
        s3.get(&reader, "foo").expect("read"),
        Some(Value::Bool(true))
    );
    reader.abort();

    // test delete across multiple stores
    let mut writer = k.write().expect("writer");
    s1.delete(&mut writer, "foo").expect("deleted");
    s2.delete(&mut writer, "foo").expect("deleted");
    s3.delete(&mut writer, "foo").expect("deleted");
    writer.commit().expect("committed");

    let reader = k.read().expect("reader");
    assert_eq!(s1.get(&reader, "key").expect("value"), None);
    assert_eq!(s2.get(&reader, "key").expect("value"), None);
    assert_eq!(s3.get(&reader, "key").expect("value"), None);
}

#[test]
fn test_multiple_store_iter() {
    let root = Builder::new()
        .prefix("test_multiple_store_iter")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let k = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let s1 = k
        .open_single("store_1", StoreOptions::create())
        .expect("opened");
    let s2 = k
        .open_single("store_2", StoreOptions::create())
        .expect("opened");

    let mut writer = k.write().expect("writer");
    // Write to "s1"
    s1.put(&mut writer, "foo", &Value::I64(1234))
        .expect("wrote");
    s1.put(&mut writer, "noo", &Value::F64(1234.0.into()))
        .expect("wrote");
    s1.put(&mut writer, "bar", &Value::Bool(true))
        .expect("wrote");
    s1.put(&mut writer, "baz", &Value::Str("héllo, yöu"))
        .expect("wrote");
    s1.put(&mut writer, "héllò, töűrîst", &Value::Str("Emil.RuleZ!"))
        .expect("wrote");
    s1.put(&mut writer, "你好，遊客", &Value::Str("米克規則"))
        .expect("wrote");
    // &mut writer to "s2"
    s2.put(&mut writer, "foo", &Value::I64(1234))
        .expect("wrote");
    s2.put(&mut writer, "noo", &Value::F64(1234.0.into()))
        .expect("wrote");
    s2.put(&mut writer, "bar", &Value::Bool(true))
        .expect("wrote");
    s2.put(&mut writer, "baz", &Value::Str("héllo, yöu"))
        .expect("wrote");
    s2.put(&mut writer, "héllò, töűrîst", &Value::Str("Emil.RuleZ!"))
        .expect("wrote");
    s2.put(&mut writer, "你好，遊客", &Value::Str("米克規則"))
        .expect("wrote");
    writer.commit().expect("committed");

    let reader = k.read().unwrap();

    // Iterate through the whole store in "s1"
    let mut iter = s1.iter_start(&reader).unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "bar");
    assert_eq!(val, Value::Bool(true));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "baz");
    assert_eq!(val, Value::Str("héllo, yöu"));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "foo");
    assert_eq!(val, Value::I64(1234));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "héllò, töűrîst");
    assert_eq!(val, Value::Str("Emil.RuleZ!"));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());

    // Iterate through the whole store in "s2"
    let mut iter = s2.iter_start(&reader).unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "bar");
    assert_eq!(val, Value::Bool(true));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "baz");
    assert_eq!(val, Value::Str("héllo, yöu"));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "foo");
    assert_eq!(val, Value::I64(1234));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "héllò, töűrîst");
    assert_eq!(val, Value::Str("Emil.RuleZ!"));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());

    // Iterate from a given key in "s1"
    let mut iter = s1.iter_from(&reader, "moo").unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());

    // Iterate from a given key in "s2"
    let mut iter = s2.iter_from(&reader, "moo").unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());

    // Iterate from a given prefix in "s1"
    let mut iter = s1.iter_from(&reader, "no").unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());

    // Iterate from a given prefix in "s2"
    let mut iter = s2.iter_from(&reader, "no").unwrap();
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "noo");
    assert_eq!(val, Value::F64(1234.0.into()));
    let (key, val) = iter.next().unwrap().unwrap();
    assert_eq!(str::from_utf8(key).expect("key"), "你好，遊客");
    assert_eq!(val, Value::Str("米克規則"));
    assert!(iter.next().is_none());
}

#[test]
fn test_store_multiple_thread() {
    let root = Builder::new()
        .prefix("test_multiple_thread")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");

    let rkv_arc = Arc::new(RwLock::new(
        Rkv::new::<Lmdb>(root.path()).expect("new succeeded"),
    ));
    let store = rkv_arc
        .read()
        .unwrap()
        .open_single("test", StoreOptions::create())
        .expect("opened");

    let num_threads = 10;
    let mut write_handles = Vec::with_capacity(num_threads as usize);
    let mut read_handles = Vec::with_capacity(num_threads as usize);

    // Note that this isn't intended to demonstrate a good use of threads.
    // For this shape of data, it would be more performant to write/read
    // all values using one transaction in a single thread. The point here
    // is just to confirm that a store can be shared by multiple threads.

    // For each KV pair, spawn a thread that writes it to the store.
    for i in 0..num_threads {
        let rkv_arc = rkv_arc.clone();
        write_handles.push(thread::spawn(move || {
            let rkv = rkv_arc.write().expect("rkv");
            let mut writer = rkv.write().expect("writer");
            store
                .put(&mut writer, i.to_string(), &Value::U64(i))
                .expect("written");
            writer.commit().unwrap();
        }));
    }
    for handle in write_handles {
        handle.join().expect("joined");
    }

    // For each KV pair, spawn a thread that reads it from the store
    // and returns its value.
    for i in 0..num_threads {
        let rkv_arc = rkv_arc.clone();
        read_handles.push(thread::spawn(move || {
            let rkv = rkv_arc.read().expect("rkv");
            let reader = rkv.read().expect("reader");
            let value = match store.get(&reader, i.to_string()) {
                Ok(Some(Value::U64(value))) => value,
                Ok(Some(_)) => panic!("value type unexpected"),
                Ok(None) => panic!("value not found"),
                Err(err) => panic!("{}", err),
            };
            assert_eq!(value, i);
            value
        }));
    }

    // Sum the values returned from the threads and confirm that they're
    // equal to the sum of values written to the threads.
    let thread_sum: u64 = read_handles
        .into_iter()
        .map(|handle| handle.join().expect("value"))
        .sum();
    assert_eq!(thread_sum, (0..num_threads).sum());
}

#[test]
fn test_use_value_as_key() {
    let root = Builder::new()
        .prefix("test_use_value_as_key")
        .tempdir()
        .expect("tempdir");
    let rkv = Rkv::new::<Lmdb>(root.path()).expect("new succeeded");
    let store = rkv
        .open_single("store", StoreOptions::create())
        .expect("opened");

    {
        let mut writer = rkv.write().expect("writer");
        store
            .put(&mut writer, "foo", &Value::Str("bar"))
            .expect("wrote");
        store
            .put(&mut writer, "bar", &Value::Str("baz"))
            .expect("wrote");
        writer.commit().expect("committed");
    }

    // It's possible to retrieve a value with a Reader and then use it
    // as a key with a Writer.
    {
        let reader = &rkv.read().unwrap();
        if let Some(Value::Str(key)) = store.get(reader, "foo").expect("read") {
            let mut writer = rkv.write().expect("writer");
            store.delete(&mut writer, key).expect("deleted");
            writer.commit().expect("committed");
        }
    }

    {
        let mut writer = rkv.write().expect("writer");
        store
            .put(&mut writer, "bar", &Value::Str("baz"))
            .expect("wrote");
        writer.commit().expect("committed");
    }

    // You can also retrieve a Value with a Writer and then use it as a key
    // with the same Writer if you copy the value to an owned type
    // so the Writer isn't still being borrowed by the retrieved value
    // when you try to borrow the Writer again to modify that value.
    {
        let mut writer = rkv.write().expect("writer");
        if let Some(Value::Str(value)) = store.get(&writer, "foo").expect("read") {
            let key = value.to_owned();
            store.delete(&mut writer, key).expect("deleted");
            writer.commit().expect("committed");
        }
    }

    {
        let name1 = rkv
            .open_single("name1", StoreOptions::create())
            .expect("opened");
        let name2 = rkv
            .open_single("name2", StoreOptions::create())
            .expect("opened");
        let mut writer = rkv.write().expect("writer");
        name1
            .put(&mut writer, "key1", &Value::Str("bar"))
            .expect("wrote");
        name1
            .put(&mut writer, "bar", &Value::Str("baz"))
            .expect("wrote");
        name2
            .put(&mut writer, "key2", &Value::Str("bar"))
            .expect("wrote");
        name2
            .put(&mut writer, "bar", &Value::Str("baz"))
            .expect("wrote");
        writer.commit().expect("committed");
    }

    // You can also iterate (store, key) pairs to retrieve foreign keys,
    // then iterate those foreign keys to modify/delete them.
    //
    // You need to open the stores in advance, since opening a store
    // uses a write transaction internally, so opening them while a writer
    // is extant will hang.
    //
    // And you need to copy the values to an owned type so the Writer isn't
    // still being borrowed by a retrieved value when you try to borrow
    // the Writer again to modify another value.
    let fields = vec![
        (
            rkv.open_single("name1", StoreOptions::create())
                .expect("opened"),
            "key1",
        ),
        (
            rkv.open_single("name2", StoreOptions::create())
                .expect("opened"),
            "key2",
        ),
    ];
    {
        let mut foreignkeys = Vec::new();
        let mut writer = rkv.write().expect("writer");
        for (store, key) in fields.iter() {
            if let Some(Value::Str(value)) = store.get(&writer, key).expect("read") {
                foreignkeys.push((store, value.to_owned()));
            }
        }
        for (store, key) in foreignkeys.iter() {
            store.delete(&mut writer, key).expect("deleted");
        }
        writer.commit().expect("committed");
    }
}
