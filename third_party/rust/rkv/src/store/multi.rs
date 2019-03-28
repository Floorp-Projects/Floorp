// Copyright 2018 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use crate::{
    error::StoreError,
    read_transform,
    readwrite::{
        Readable,
        Writer,
    },
    value::Value,
};
use lmdb::{
    Cursor,
    Database,
    Iter as LmdbIter,
    //    IterDup as LmdbIterDup,
    RoCursor,
    WriteFlags,
};

#[derive(Copy, Clone)]
pub struct MultiStore {
    db: Database,
}

pub struct Iter<'env> {
    iter: LmdbIter<'env>,
    cursor: RoCursor<'env>,
}

impl MultiStore {
    pub(crate) fn new(db: Database) -> MultiStore {
        MultiStore {
            db,
        }
    }

    /// Provides a cursor to all of the values for the duplicate entries that match this key
    pub fn get<T: Readable, K: AsRef<[u8]>>(self, reader: &T, k: K) -> Result<Iter, StoreError> {
        let mut cursor = reader.open_ro_cursor(self.db)?;
        let iter = cursor.iter_dup_of(k);
        Ok(Iter {
            iter,
            cursor,
        })
    }

    /// Provides the first value that matches this key
    pub fn get_first<T: Readable, K: AsRef<[u8]>>(self, reader: &T, k: K) -> Result<Option<Value>, StoreError> {
        reader.get(self.db, &k)
    }

    /// Insert a value at the specified key.
    /// This put will allow duplicate entries.  If you wish to have duplicate entries
    /// rejected, use the `put_with_flags` function and specify NO_DUP_DATA
    pub fn put<K: AsRef<[u8]>>(self, writer: &mut Writer, k: K, v: &Value) -> Result<(), StoreError> {
        writer.put(self.db, &k, v, WriteFlags::empty())
    }

    pub fn put_with_flags<K: AsRef<[u8]>>(
        self,
        writer: &mut Writer,
        k: K,
        v: &Value,
        flags: WriteFlags,
    ) -> Result<(), StoreError> {
        writer.put(self.db, &k, v, flags)
    }

    pub fn delete_all<K: AsRef<[u8]>>(self, writer: &mut Writer, k: K) -> Result<(), StoreError> {
        writer.delete(self.db, &k, None)
    }

    pub fn delete<K: AsRef<[u8]>>(self, writer: &mut Writer, k: K, v: &Value) -> Result<(), StoreError> {
        writer.delete(self.db, &k, Some(&v.to_bytes()?))
    }

    /* TODO - Figure out how to solve the need to have the cursor stick around when
     *        we are producing iterators from MultiIter
    /// Provides an iterator starting at the lexographically smallest value in the store
    pub fn iter_start(&self, store: MultiStore) -> Result<MultiIter, StoreError> {
        let mut cursor = self.tx.open_ro_cursor(store.0).map_err(StoreError::LmdbError)?;

        // We call Cursor.iter() instead of Cursor.iter_start() because
        // the latter panics at "called `Result::unwrap()` on an `Err` value:
        // NotFound" when there are no items in the store, whereas the former
        // returns an iterator that yields no items.
        //
        // And since we create the Cursor and don't change its position, we can
        // be sure that a call to Cursor.iter() will start at the beginning.
        //
        let iter = cursor.iter_dup();

        Ok(MultiIter {
            iter,
            cursor,
        })
    }
    */

    pub fn clear(self, writer: &mut Writer) -> Result<(), StoreError> {
        writer.clear(self.db)
    }
}

/*
fn read_transform_owned(val: Result<&[u8], lmdb::Error>) -> Result<Option<OwnedValue>, StoreError> {
    match val {
        Ok(bytes) => Value::from_tagged_slice(bytes).map(|v| Some(OwnedValue::from(&v))).map_err(StoreError::DataError),
        Err(lmdb::Error::NotFound) => Ok(None),
        Err(e) => Err(StoreError::LmdbError(e)),
    }
}

impl<'env> Iterator for MultiIter<'env> {
    type Item = Iter<'env>;

    fn next(&mut self) -> Option<Self::Item> {
        match self.iter.next() {
            None => None,
            Some(iter) => Some(Iter {
                iter,
                cursor,
            }),
        }
    }
}
*/

impl<'env> Iterator for Iter<'env> {
    type Item = Result<(&'env [u8], Option<Value<'env>>), StoreError>;

    fn next(&mut self) -> Option<Self::Item> {
        match self.iter.next() {
            None => None,
            Some(Ok((key, bytes))) => match read_transform(Ok(bytes)) {
                Ok(val) => Some(Ok((key, val))),
                Err(err) => Some(Err(err)),
            },
            Some(Err(err)) => Some(Err(StoreError::LmdbError(err))),
        }
    }
}
