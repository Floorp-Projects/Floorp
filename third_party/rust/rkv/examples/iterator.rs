// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

//! A demo that showcases the basic usage of iterators in rkv.
//!
//! You can test this out by running:
//!
//!     cargo run --example iterator

extern crate rkv;
extern crate tempfile;

use rkv::{
    Manager,
    Rkv,
    Store,
    StoreError,
    Value,
};
use tempfile::Builder;

use std::fs;
use std::str;

fn main() {
    let root = Builder::new().prefix("iterator").tempdir().unwrap();
    fs::create_dir_all(root.path()).unwrap();
    let p = root.path();

    let created_arc = Manager::singleton().write().unwrap().get_or_create(p, Rkv::new).unwrap();
    let k = created_arc.read().unwrap();
    let store = k.open_or_create("store").unwrap();

    populate_store(&k, &store).unwrap();

    let reader = k.read().unwrap();

    println!("Iterating from the beginning...");
    // Reader::iter_start() iterates from the first item in the store, and
    // returns the (key, value) tuples in order.
    let mut iter = reader.iter_start(&store).unwrap();
    while let Some((country, city)) = iter.next() {
        println!("{}, {:?}", str::from_utf8(country).unwrap(), city);
    }

    println!("");
    println!("Iterating from the given key...");
    // Reader::iter_from() iterates from the first key equal to or greater
    // than the given key.
    let mut iter = reader.iter_from(&store, "Japan").unwrap();
    while let Some((country, city)) = iter.next() {
        println!("{}, {:?}", str::from_utf8(country).unwrap(), city);
    }

    println!("");
    println!("Iterating from the given prefix...");
    let mut iter = reader.iter_from(&store, "Un").unwrap();
    while let Some((country, city)) = iter.next() {
        println!("{}, {:?}", str::from_utf8(country).unwrap(), city);
    }
}

fn populate_store(k: &Rkv, store: &Store) -> Result<(), StoreError> {
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
        writer.put(store, country, &city)?;
    }
    writer.commit()
}
