/// consider a struct like this
/// struct Sample {
///     id: u64,
///     value: String,
///     date: String,
/// }
/// We would like to index all of the fields so that we can search for the struct not only by ID
/// but also by value and date.  When we index the fields individually in their own tables, it
/// is important that we run all operations within a single transaction to ensure coherence of
/// the indices
/// This test features helper functions for reading and writing the parts of the struct.
/// Note that the reader functions take `Readable` because they might run within a Read
/// Transaction or a Write Transaction.  The test demonstrates fetching values via both.
use rkv::{
    MultiStore,
    Readable,
    Rkv,
    SingleStore,
    StoreOptions,
    Value,
    Writer,
};

use tempfile::Builder;

use std::fs;

#[test]
fn read_many() {
    let root = Builder::new().prefix("test_txns").tempdir().expect("tempdir");
    fs::create_dir_all(root.path()).expect("dir created");
    let k = Rkv::new(root.path()).expect("new succeeded");
    let samplestore = k.open_single("s", StoreOptions::create()).expect("open");
    let datestore = k.open_multi("m", StoreOptions::create()).expect("open");
    let valuestore = k.open_multi("m", StoreOptions::create()).expect("open");

    {
        let mut writer = k.write().expect("env write lock");

        for id in 0..30_u64 {
            let value = format!("value{}", id);
            let date = format!("2019-06-{}", id);
            put_id_field(&mut writer, datestore, &date, id);
            put_id_field(&mut writer, valuestore, &value, id);
            put_sample(&mut writer, samplestore, id, &value);
        }

        // now we read in the same transaction
        for id in 0..30_u64 {
            let value = format!("value{}", id);
            let date = format!("2019-06-{}", id);
            let ids = get_ids_by_field(&writer, datestore, &date);
            let ids2 = get_ids_by_field(&writer, valuestore, &value);
            let samples = get_samples(&writer, samplestore, &ids);
            let samples2 = get_samples(&writer, samplestore, &ids2);
            println!("{:?}, {:?}", samples, samples2);
        }
    }

    {
        let reader = k.read().expect("env read lock");
        for id in 0..30_u64 {
            let value = format!("value{}", id);
            let date = format!("2019-06-{}", id);
            let ids = get_ids_by_field(&reader, datestore, &date);
            let ids2 = get_ids_by_field(&reader, valuestore, &value);
            let samples = get_samples(&reader, samplestore, &ids);
            let samples2 = get_samples(&reader, samplestore, &ids2);
            println!("{:?}, {:?}", samples, samples2);
        }
    }
}

fn get_ids_by_field<Txn: Readable>(txn: &Txn, store: MultiStore, field: &str) -> Vec<u64> {
    store
        .get(txn, field)
        .expect("get iterator")
        .map(|id| match id.expect("field") {
            (_, Some(Value::U64(id))) => id,
            _ => panic!("getting value in iter"),
        })
        .collect::<Vec<u64>>()
}

fn get_samples<Txn: Readable>(txn: &Txn, samplestore: SingleStore, ids: &[u64]) -> Vec<String> {
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

fn put_sample(txn: &mut Writer, samplestore: SingleStore, id: u64, value: &str) {
    let idbytes = id.to_be_bytes();
    samplestore.put(txn, &idbytes, &Value::Str(value)).expect("put id");
}

fn put_id_field(txn: &mut Writer, store: MultiStore, field: &str, id: u64) {
    store.put(txn, field, &Value::U64(id)).expect("put id");
}
