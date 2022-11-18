// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
#![cfg(feature = "db-dup-sort")]

use std::fs;

use tempfile::Builder;

use rkv::{
    backend::{SafeMode, SafeModeDatabase, SafeModeRoCursor, SafeModeRwTransaction},
    Readable, Rkv, StoreOptions, Value, Writer,
};

/// Consider a struct like this:
/// struct Sample {
///     id: u64,
///     value: String,
///     date: String,
/// }
/// We would like to index all of the fields so that we can search for the struct not only
/// by ID but also by value and date.  When we index the fields individually in their own
/// tables, it is important that we run all operations within a single transaction to
/// ensure coherence of the indices.
/// This test features helper functions for reading and writing the parts of the struct.
/// Note that the reader functions take `Readable` because they might run within a Read
/// Transaction or a Write Transaction.  The test demonstrates fetching values via both.

type SingleStore = rkv::SingleStore<SafeModeDatabase>;
type MultiStore = rkv::MultiStore<SafeModeDatabase>;

#[test]
fn read_many() {
    let root = Builder::new()
        .prefix("test_txns")
        .tempdir()
        .expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");
    let k = Rkv::new::<SafeMode>(root.path()).expect("new succeeded");
    let samplestore = k.open_single("s", StoreOptions::create()).expect("open");
    let datestore = k.open_multi("m", StoreOptions::create()).expect("open");
    let valuestore = k.open_multi("m", StoreOptions::create()).expect("open");

    {
        let mut writer = k.write().expect("env write lock");

        for id in 0..30_u64 {
            let value = format!("value{id}");
            let date = format!("2019-06-{id}");
            put_id_field(&mut writer, datestore, &date, id);
            put_id_field(&mut writer, valuestore, &value, id);
            put_sample(&mut writer, samplestore, id, &value);
        }

        // now we read in the same transaction
        for id in 0..30_u64 {
            let value = format!("value{id}");
            let date = format!("2019-06-{id}");
            let ids = get_ids_by_field(&writer, datestore, &date);
            let ids2 = get_ids_by_field(&writer, valuestore, &value);
            let samples = get_samples(&writer, samplestore, &ids);
            let samples2 = get_samples(&writer, samplestore, &ids2);
            println!("{samples:?}, {samples2:?}");
        }
    }

    {
        let reader = k.read().expect("env read lock");
        for id in 0..30_u64 {
            let value = format!("value{id}");
            let date = format!("2019-06-{id}");
            let ids = get_ids_by_field(&reader, datestore, &date);
            let ids2 = get_ids_by_field(&reader, valuestore, &value);
            let samples = get_samples(&reader, samplestore, &ids);
            let samples2 = get_samples(&reader, samplestore, &ids2);
            println!("{samples:?}, {samples2:?}");
        }
    }
}

fn get_ids_by_field<'t, T>(txn: &'t T, store: MultiStore, field: &'t str) -> Vec<u64>
where
    T: Readable<'t, Database = SafeModeDatabase, RoCursor = SafeModeRoCursor<'t>>,
{
    store
        .get(txn, field)
        .expect("get iterator")
        .map(|id| match id.expect("field") {
            (_, Value::U64(id)) => id,
            _ => panic!("getting value in iter"),
        })
        .collect::<Vec<u64>>()
}

fn get_samples<'t, T>(txn: &'t T, samplestore: SingleStore, ids: &[u64]) -> Vec<String>
where
    T: Readable<'t, Database = SafeModeDatabase, RoCursor = SafeModeRoCursor<'t>>,
{
    ids.iter()
        .map(|id| {
            let bytes = id.to_be_bytes();
            match samplestore.get(txn, &bytes).expect("fetch sample") {
                Some(Value::Str(sample)) => String::from(sample),
                Some(_) => panic!("wrong type"),
                None => panic!("no sample for this id!"),
            }
        })
        .collect::<Vec<String>>()
}

fn put_sample(
    txn: &mut Writer<SafeModeRwTransaction>,
    samplestore: SingleStore,
    id: u64,
    value: &str,
) {
    let idbytes = id.to_be_bytes();
    samplestore
        .put(txn, &idbytes, &Value::Str(value))
        .expect("put id");
}

fn put_id_field(txn: &mut Writer<SafeModeRwTransaction>, store: MultiStore, field: &str, id: u64) {
    store.put(txn, field, &Value::U64(id)).expect("put id");
}
