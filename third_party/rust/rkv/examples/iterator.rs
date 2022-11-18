// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

//! A demo that showcases the basic usage of iterators in rkv.
//!
//! You can test this out by running:
//!
//!     cargo run --example iterator

use std::{fs, str};

use tempfile::Builder;

use rkv::{
    backend::{SafeMode, SafeModeDatabase, SafeModeEnvironment},
    Manager, Rkv, SingleStore, StoreError, StoreOptions, Value,
};

fn main() {
    let root = Builder::new().prefix("iterator").tempdir().unwrap();
    fs::create_dir_all(root.path()).unwrap();
    let p = root.path();

    let mut manager = Manager::<SafeModeEnvironment>::singleton().write().unwrap();
    let created_arc = manager.get_or_create(p, Rkv::new::<SafeMode>).unwrap();
    let k = created_arc.read().unwrap();
    let store = k.open_single("store", StoreOptions::create()).unwrap();

    populate_store(&k, store).unwrap();

    let reader = k.read().unwrap();

    println!("Iterating from the beginning...");
    // Reader::iter_start() iterates from the first item in the store, and
    // returns the (key, value) tuples in order.
    let mut iter = store.iter_start(&reader).unwrap();
    while let Some(Ok((country, city))) = iter.next() {
        let country = str::from_utf8(country).unwrap();
        println!("{country}, {city:?}");
    }

    println!();
    println!("Iterating from the given key...");
    // Reader::iter_from() iterates from the first key equal to or greater
    // than the given key.
    let mut iter = store.iter_from(&reader, "Japan").unwrap();
    while let Some(Ok((country, city))) = iter.next() {
        println!("{}, {city:?}", str::from_utf8(country).unwrap());
    }

    println!();
    println!("Iterating from the given prefix...");
    let mut iter = store.iter_from(&reader, "Un").unwrap();
    while let Some(Ok((country, city))) = iter.next() {
        println!("{}, {city:?}", str::from_utf8(country).unwrap());
    }
}

fn populate_store(
    k: &Rkv<SafeModeEnvironment>,
    store: SingleStore<SafeModeDatabase>,
) -> Result<(), StoreError> {
    let mut writer = k.write()?;
    for (country, city) in vec![
        ("Canada", Value::Str("Ottawa")),
        ("United States of America", Value::Str("Washington")),
        ("Germany", Value::Str("Berlin")),
        ("France", Value::Str("Paris")),
        ("Italy", Value::Str("Rome")),
        ("United Kingdom", Value::Str("London")),
        ("Japan", Value::Str("Tokyo")),
    ] {
        store.put(&mut writer, country, &city)?;
    }
    writer.commit()
}
