// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

//! A simple rkv demo that showcases the basic usage (put/get/delete) of rkv.
//!
//! You can test this out by running:
//!
//!     cargo run --example simple-store

use rkv::{
    Manager,
    MultiStore,
    Rkv,
    StoreOptions,
    Value,
    Writer,
};
use tempfile::Builder;

use std::fs;

fn getput<'env, 's>(store: MultiStore, writer: &'env mut Writer, ids: &'s mut Vec<String>) {
    let keys = vec!["str1", "str2", "str3"];
    // we convert the writer into a cursor so that we can safely read
    for k in keys.iter() {
        // this is a multi-valued database, so get returns an iterator
        let mut iter = store.get(writer, k).unwrap();
        while let Some(Ok((_key, val))) = iter.next() {
            if let Value::Str(s) = val.unwrap() {
                ids.push(s.to_owned());
            } else {
                panic!("didn't get a string back!");
            }
        }
    }
    for id in ids {
        store.put(writer, &id, &Value::Blob(b"weeeeeee")).unwrap();
    }
}

fn delete(store: MultiStore, writer: &mut Writer) {
    let keys = vec!["str1", "str2", "str3"];
    let vals = vec!["string uno", "string quatro", "string siete"];
    // we convert the writer into a cursor so that we can safely read
    for i in 0..keys.len() {
        store.delete(writer, &keys[i], &Value::Str(vals[i])).unwrap();
    }
}

fn main() {
    let root = Builder::new().prefix("simple-db").tempdir().unwrap();
    fs::create_dir_all(root.path()).unwrap();
    let p = root.path();

    // The manager enforces that each process opens the same lmdb environment at most once
    let created_arc = Manager::singleton().write().unwrap().get_or_create(p, Rkv::new).unwrap();
    let k = created_arc.read().unwrap();

    // Creates a store called "store"
    let store = k.open_single("store", StoreOptions::create()).unwrap();

    let multistore = k.open_multi("multistore", StoreOptions::create()).unwrap();

    println!("Inserting data...");
    {
        // Use a writer to mutate the store
        let mut writer = k.write().unwrap();
        store.put(&mut writer, "int", &Value::I64(1234)).unwrap();
        store.put(&mut writer, "uint", &Value::U64(1234_u64)).unwrap();
        store.put(&mut writer, "float", &Value::F64(1234.0.into())).unwrap();
        store.put(&mut writer, "instant", &Value::Instant(1_528_318_073_700)).unwrap();
        store.put(&mut writer, "boolean", &Value::Bool(true)).unwrap();
        store.put(&mut writer, "string", &Value::Str("héllo, yöu")).unwrap();
        store.put(&mut writer, "json", &Value::Json(r#"{"foo":"bar", "number": 1}"#)).unwrap();
        store.put(&mut writer, "blob", &Value::Blob(b"blob")).unwrap();
        writer.commit().unwrap();
    }

    println!("Testing getput");
    {
        let mut ids = Vec::new();
        let mut writer = k.write().unwrap();
        multistore.put(&mut writer, "str1", &Value::Str("string uno")).unwrap();
        multistore.put(&mut writer, "str1", &Value::Str("string dos")).unwrap();
        multistore.put(&mut writer, "str1", &Value::Str("string tres")).unwrap();
        multistore.put(&mut writer, "str2", &Value::Str("string quatro")).unwrap();
        multistore.put(&mut writer, "str2", &Value::Str("string cinco")).unwrap();
        multistore.put(&mut writer, "str2", &Value::Str("string seis")).unwrap();
        multistore.put(&mut writer, "str3", &Value::Str("string siete")).unwrap();
        multistore.put(&mut writer, "str3", &Value::Str("string ocho")).unwrap();
        multistore.put(&mut writer, "str3", &Value::Str("string nueve")).unwrap();
        getput(multistore, &mut writer, &mut ids);
        writer.commit().unwrap();
        let mut writer = k.write().unwrap();
        delete(multistore, &mut writer);
        writer.commit().unwrap();
    }
    println!("Looking up keys...");
    {
        // Use a reader to query the store
        let reader = k.read().unwrap();
        println!("Get int {:?}", store.get(&reader, "int").unwrap());
        println!("Get uint {:?}", store.get(&reader, "uint").unwrap());
        println!("Get float {:?}", store.get(&reader, "float").unwrap());
        println!("Get instant {:?}", store.get(&reader, "instant").unwrap());
        println!("Get boolean {:?}", store.get(&reader, "boolean").unwrap());
        println!("Get string {:?}", store.get(&reader, "string").unwrap());
        println!("Get json {:?}", store.get(&reader, "json").unwrap());
        println!("Get blob {:?}", store.get(&reader, "blob").unwrap());
        println!("Get non-existent {:?}", store.get(&reader, "non-existent").unwrap());
    }

    println!("Looking up keys via Writer.get()...");
    {
        let mut writer = k.write().unwrap();
        store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
        store.put(&mut writer, "bar", &Value::Str("baz")).unwrap();
        store.delete(&mut writer, "foo").unwrap();
        println!("It should be None! ({:?})", store.get(&writer, "foo").unwrap());
        println!("Get bar ({:?})", store.get(&writer, "bar").unwrap());
        writer.commit().unwrap();
        let reader = k.read().expect("reader");
        println!("It should be None! ({:?})", store.get(&reader, "foo").unwrap());
        println!("Get bar {:?}", store.get(&reader, "bar").unwrap());
    }

    println!("Aborting transaction...");
    {
        // Aborting a write transaction rollbacks the change(s)
        let mut writer = k.write().unwrap();
        store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
        writer.abort();

        let reader = k.read().expect("reader");
        println!("It should be None! ({:?})", store.get(&reader, "foo").unwrap());
        // Explicitly aborting a transaction is not required unless an early
        // abort is desired, since both read and write transactions will
        // implicitly be aborted once they go out of scope.
    }

    println!("Deleting keys...");
    {
        // Deleting a key/value also requires a write transaction
        let mut writer = k.write().unwrap();
        store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
        store.delete(&mut writer, "foo").unwrap();
        println!("It should be None! ({:?})", store.get(&writer, "foo").unwrap());
        writer.commit().unwrap();

        // Committing a transaction consumes the writer, preventing you
        // from reusing it by failing and reporting a compile-time error.
        // This line would report error[E0382]: use of moved value: `writer`.
        // store.put(&mut writer, "baz", &Value::Str("buz")).unwrap();
    }

    println!("Clearing store...");
    {
        // Clearing a store deletes all the entries in that store
        let mut writer = k.write().unwrap();
        store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
        store.put(&mut writer, "bar", &Value::Str("baz")).unwrap();
        store.clear(&mut writer).unwrap();
        writer.commit().unwrap();

        let reader = k.read().expect("reader");
        println!("It should be None! ({:?})", store.get(&reader, "foo").unwrap());
        println!("It should be None! ({:?})", store.get(&reader, "bar").unwrap());
    }

    println!("Write and read on multiple stores...");
    {
        let another_store = k.open_single("another_store", StoreOptions::create()).unwrap();
        let mut writer = k.write().unwrap();
        store.put(&mut writer, "foo", &Value::Str("bar")).unwrap();
        another_store.put(&mut writer, "foo", &Value::Str("baz")).unwrap();
        writer.commit().unwrap();

        let reader = k.read().unwrap();
        println!("Get from store value: {:?}", store.get(&reader, "foo").unwrap());
        println!("Get from another store value: {:?}", another_store.get(&reader, "foo").unwrap());
    }
    println!("Environment statistics: btree depth = {}", k.stat().unwrap().depth());
}
