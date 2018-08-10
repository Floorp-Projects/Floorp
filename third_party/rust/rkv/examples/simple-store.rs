// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

//! A simple rkv demo that showcases the basic usage (put/get/delete) of rkv.
//!
//! You can test this out by running:
//!
//!     cargo run --example simple-store

extern crate rkv;
extern crate tempfile;

use rkv::{
    Manager,
    Rkv,
    Value,
};
use tempfile::Builder;

use std::fs;

fn main() {
    let root = Builder::new().prefix("simple-db").tempdir().unwrap();
    fs::create_dir_all(root.path()).unwrap();
    let p = root.path();

    // The manager enforces that each process opens the same lmdb environment at most once
    let created_arc = Manager::singleton().write().unwrap().get_or_create(p, Rkv::new).unwrap();
    let k = created_arc.read().unwrap();

    // Creates a store called "store"
    let store = k.open_or_create("store").unwrap();

    println!("Inserting data...");
    {
        // Use a writer to mutate the store
        let mut writer = k.write().unwrap();
        writer.put(&store, "int", &Value::I64(1234)).unwrap();
        writer.put(&store, "uint", &Value::U64(1234_u64)).unwrap();
        writer.put(&store, "float", &Value::F64(1234.0.into())).unwrap();
        writer.put(&store, "instant", &Value::Instant(1528318073700)).unwrap();
        writer.put(&store, "boolean", &Value::Bool(true)).unwrap();
        writer.put(&store, "string", &Value::Str("héllo, yöu")).unwrap();
        writer.put(&store, "json", &Value::Json(r#"{"foo":"bar", "number": 1}"#)).unwrap();
        writer.put(&store, "blob", &Value::Blob(b"blob")).unwrap();
        writer.commit().unwrap();
    }

    println!("Looking up keys...");
    {
        // Use a reader to query the store
        let reader = k.read().unwrap();
        println!("Get int {:?}", reader.get(&store, "int").unwrap());
        println!("Get uint {:?}", reader.get(&store, "uint").unwrap());
        println!("Get float {:?}", reader.get(&store, "float").unwrap());
        println!("Get instant {:?}", reader.get(&store, "instant").unwrap());
        println!("Get boolean {:?}", reader.get(&store, "boolean").unwrap());
        println!("Get string {:?}", reader.get(&store, "string").unwrap());
        println!("Get json {:?}", reader.get(&store, "json").unwrap());
        println!("Get blob {:?}", reader.get(&store, "blob").unwrap());
        println!("Get non-existent {:?}", reader.get(&store, "non-existent").unwrap());
    }

    println!("Looking up keys via Writer.get()...");
    {
        let mut writer = k.write().unwrap();
        writer.put(&store, "foo", &Value::Str("bar")).unwrap();
        writer.put(&store, "bar", &Value::Str("baz")).unwrap();
        writer.delete(&store, "foo").unwrap();
        println!("It should be None! ({:?})", writer.get(&store, "foo").unwrap());
        println!("Get bar ({:?})", writer.get(&store, "bar").unwrap());
        writer.commit().unwrap();
        let reader = k.read().expect("reader");
        println!("It should be None! ({:?})", reader.get(&store, "foo").unwrap());
        println!("Get bar {:?}", reader.get(&store, "bar").unwrap());
    }

    println!("Aborting transaction...");
    {
        // Aborting a write transaction rollbacks the change(s)
        let mut writer = k.write().unwrap();
        writer.put(&store, "foo", &Value::Str("bar")).unwrap();
        writer.abort();

        let reader = k.read().expect("reader");
        println!("It should be None! ({:?})", reader.get(&store, "foo").unwrap());
        // Explicitly aborting a transaction is not required unless an early
        // abort is desired, since both read and write transactions will
        // implicitly be aborted once they go out of scope.
    }

    println!("Deleting keys...");
    {
        // Deleting a key/value also requires a write transaction
        let mut writer = k.write().unwrap();
        writer.put(&store, "foo", &Value::Str("bar")).unwrap();
        writer.delete(&store, "foo").unwrap();
        println!("It should be None! ({:?})", writer.get(&store, "foo").unwrap());
        writer.commit().unwrap();

        // Committing a transaction consumes the writer, preventing you
        // from reusing it by failing and reporting a compile-time error.
        // This line would report error[E0382]: use of moved value: `writer`.
        // writer.put(&store, "baz", &Value::Str("buz")).unwrap();
    }

    println!("Write and read on multiple stores...");
    {
        let another_store = k.open_or_create("another_store").unwrap();
        let mut writer = k.write().unwrap();
        writer.put(&store, "foo", &Value::Str("bar")).unwrap();
        writer.put(&another_store, "foo", &Value::Str("baz")).unwrap();
        writer.commit().unwrap();

        let reader = k.read().unwrap();
        println!("Get from store value: {:?}", reader.get(&store, "foo").unwrap());
        println!("Get from another store value: {:?}", reader.get(&another_store, "foo").unwrap());
    }
}
