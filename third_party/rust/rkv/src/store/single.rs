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
    RoCursor,
    WriteFlags,
};

#[derive(Copy, Clone)]
pub struct SingleStore {
    db: Database,
}

pub struct Iter<'env> {
    iter: LmdbIter<'env>,
    cursor: RoCursor<'env>,
}

impl SingleStore {
    pub(crate) fn new(db: Database) -> SingleStore {
        SingleStore {
            db,
        }
    }

    pub fn get<T: Readable, K: AsRef<[u8]>>(self, reader: &T, k: K) -> Result<Option<Value>, StoreError> {
        reader.get(self.db, &k)
    }

    // TODO: flags
    pub fn put<K: AsRef<[u8]>>(self, writer: &mut Writer, k: K, v: &Value) -> Result<(), StoreError> {
        writer.put(self.db, &k, v, WriteFlags::empty())
    }

    pub fn delete<K: AsRef<[u8]>>(self, writer: &mut Writer, k: K) -> Result<(), StoreError> {
        writer.delete(self.db, &k, None)
    }

    pub fn iter_start<T: Readable>(self, reader: &T) -> Result<Iter, StoreError> {
        let mut cursor = reader.open_ro_cursor(self.db)?;

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

    pub fn iter_from<T: Readable, K: AsRef<[u8]>>(self, reader: &T, k: K) -> Result<Iter, StoreError> {
        let mut cursor = reader.open_ro_cursor(self.db)?;
        let iter = cursor.iter_from(k);
        Ok(Iter {
            iter,
            cursor,
        })
    }

    pub fn clear(self, writer: &mut Writer) -> Result<(), StoreError> {
        writer.clear(self.db)
    }
}

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
