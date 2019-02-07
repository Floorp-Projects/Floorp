// Copyright 2018 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use lmdb;

use std::marker::PhantomData;

use lmdb::{
    Cursor,
    Database,
    Iter as LmdbIter,
    RoCursor,
    RoTransaction,
    RwTransaction,
    Transaction,
};

use lmdb::WriteFlags;

use error::StoreError;

use value::Value;

fn read_transform(val: Result<&[u8], lmdb::Error>) -> Result<Option<Value>, StoreError> {
    match val {
        Ok(bytes) => Value::from_tagged_slice(bytes).map(Some).map_err(StoreError::DataError),
        Err(lmdb::Error::NotFound) => Ok(None),
        Err(e) => Err(StoreError::LmdbError(e)),
    }
}

pub struct Writer<'env, K>
where
    K: AsRef<[u8]>,
{
    tx: RwTransaction<'env>,
    phantom: PhantomData<K>,
}

pub struct Reader<'env, K>
where
    K: AsRef<[u8]>,
{
    tx: RoTransaction<'env>,
    phantom: PhantomData<K>,
}

pub struct Iter<'env> {
    iter: LmdbIter<'env>,
    cursor: RoCursor<'env>,
}

impl<'env, K> Writer<'env, K>
where
    K: AsRef<[u8]>,
{
    pub(crate) fn new(txn: RwTransaction) -> Writer<K> {
        Writer {
            tx: txn,
            phantom: PhantomData,
        }
    }

    pub fn get(&self, store: Store, k: K) -> Result<Option<Value>, StoreError> {
        let bytes = self.tx.get(store.0, &k);
        read_transform(bytes)
    }

    // TODO: flags
    pub fn put(&mut self, store: Store, k: K, v: &Value) -> Result<(), StoreError> {
        // TODO: don't allocate twice.
        let bytes = v.to_bytes()?;
        self.tx.put(store.0, &k, &bytes, WriteFlags::empty()).map_err(StoreError::LmdbError)
    }

    pub fn delete(&mut self, store: Store, k: K) -> Result<(), StoreError> {
        self.tx.del(store.0, &k, None).map_err(StoreError::LmdbError)
    }

    pub fn delete_value(&mut self, _store: Store, _k: K, _v: &Value) -> Result<(), StoreError> {
        // Even better would be to make this a method only on a dupsort store —
        // it would need a little bit of reorganizing of types and traits,
        // but when I see "If the database does not support sorted duplicate
        // data items (MDB_DUPSORT) the data parameter is ignored" in the docs,
        // I see a footgun that we can avoid by using the type system.
        unimplemented!();
    }

    pub fn commit(self) -> Result<(), StoreError> {
        self.tx.commit().map_err(StoreError::LmdbError)
    }

    pub fn abort(self) {
        self.tx.abort();
    }
}

impl<'env, K> Reader<'env, K>
where
    K: AsRef<[u8]>,
{
    pub(crate) fn new(txn: RoTransaction) -> Reader<K> {
        Reader {
            tx: txn,
            phantom: PhantomData,
        }
    }

    pub fn get(&self, store: Store, k: K) -> Result<Option<Value>, StoreError> {
        let bytes = self.tx.get(store.0, &k);
        read_transform(bytes)
    }

    pub fn abort(self) {
        self.tx.abort();
    }

    pub fn iter_start(&self, store: Store) -> Result<Iter, StoreError> {
        let mut cursor = self.tx.open_ro_cursor(store.0).map_err(StoreError::LmdbError)?;

        // We call Cursor.iter() instead of Cursor.iter_start() because
        // the latter panics at "called `Result::unwrap()` on an `Err` value:
        // NotFound" when there are no items in the store, whereas the former
        // returns an iterator that yields no items.
        //
        // And since we create the Cursor and don't change its position, we can
        // be sure that a call to Cursor.iter() will start at the beginning.
        //
        let iter = cursor.iter();

        Ok(Iter {
            iter,
            cursor,
        })
    }

    pub fn iter_from(&self, store: Store, k: K) -> Result<Iter, StoreError> {
        let mut cursor = self.tx.open_ro_cursor(store.0).map_err(StoreError::LmdbError)?;
        let iter = cursor.iter_from(k);
        Ok(Iter {
            iter,
            cursor,
        })
    }
}

impl<'env> Iterator for Iter<'env> {
    type Item = (&'env [u8], Result<Option<Value<'env>>, StoreError>);

    fn next(&mut self) -> Option<Self::Item> {
        match self.iter.next() {
            None => None,
            Some((key, bytes)) => Some((key, read_transform(Ok(bytes)))),
        }
    }
}

/// New type around an `lmdb::Database`.  At this time, the underlying LMDB
/// handle (within lmdb-rs::Database) is a C integer, so Copy is automatic.
#[derive(Copy, Clone)]
pub struct Store(Database);

impl Store {
    pub fn new(db: Database) -> Store {
        Store(db)
    }
}
