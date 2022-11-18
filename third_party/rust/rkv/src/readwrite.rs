// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use crate::{
    backend::{
        BackendDatabase, BackendRoCursor, BackendRoCursorTransaction, BackendRoTransaction,
        BackendRwCursorTransaction, BackendRwTransaction,
    },
    error::StoreError,
    helpers::read_transform,
    value::Value,
};

pub struct Reader<T>(T);
pub struct Writer<T>(T);

pub trait Readable<'r> {
    type Database: BackendDatabase;
    type RoCursor: BackendRoCursor<'r>;

    fn get<K>(&'r self, db: &Self::Database, k: &K) -> Result<Option<Value<'r>>, StoreError>
    where
        K: AsRef<[u8]>;

    fn open_ro_cursor(&'r self, db: &Self::Database) -> Result<Self::RoCursor, StoreError>;
}

impl<'r, T> Readable<'r> for Reader<T>
where
    T: BackendRoCursorTransaction<'r>,
{
    type Database = T::Database;
    type RoCursor = T::RoCursor;

    fn get<K>(&'r self, db: &T::Database, k: &K) -> Result<Option<Value<'r>>, StoreError>
    where
        K: AsRef<[u8]>,
    {
        let bytes = self.0.get(db, k.as_ref()).map_err(|e| e.into());
        match read_transform(bytes).map(Some) {
            Err(StoreError::KeyValuePairNotFound) => Ok(None),
            result => result,
        }
    }

    fn open_ro_cursor(&'r self, db: &T::Database) -> Result<T::RoCursor, StoreError> {
        self.0.open_ro_cursor(db).map_err(|e| e.into())
    }
}

impl<T> Reader<T> {
    pub(crate) fn new(txn: T) -> Reader<T> {
        Reader(txn)
    }
}

impl<T> Reader<T>
where
    T: BackendRoTransaction,
{
    pub fn abort(self) {
        self.0.abort();
    }
}

impl<'r, T> Readable<'r> for Writer<T>
where
    T: BackendRwCursorTransaction<'r>,
{
    type Database = T::Database;
    type RoCursor = T::RoCursor;

    fn get<K>(&'r self, db: &T::Database, k: &K) -> Result<Option<Value<'r>>, StoreError>
    where
        K: AsRef<[u8]>,
    {
        let bytes = self.0.get(db, k.as_ref()).map_err(|e| e.into());
        match read_transform(bytes).map(Some) {
            Err(StoreError::KeyValuePairNotFound) => Ok(None),
            result => result,
        }
    }

    fn open_ro_cursor(&'r self, db: &T::Database) -> Result<T::RoCursor, StoreError> {
        self.0.open_ro_cursor(db).map_err(|e| e.into())
    }
}

impl<T> Writer<T> {
    pub(crate) fn new(txn: T) -> Writer<T> {
        Writer(txn)
    }
}

impl<T> Writer<T>
where
    T: BackendRwTransaction,
{
    pub fn commit(self) -> Result<(), StoreError> {
        self.0.commit().map_err(|e| e.into())
    }

    pub fn abort(self) {
        self.0.abort();
    }

    pub(crate) fn put<K>(
        &mut self,
        db: &T::Database,
        k: &K,
        v: &Value,
        flags: T::Flags,
    ) -> Result<(), StoreError>
    where
        K: AsRef<[u8]>,
    {
        // TODO: don't allocate twice.
        self.0
            .put(db, k.as_ref(), &v.to_bytes()?, flags)
            .map_err(|e| e.into())
    }

    #[cfg(not(feature = "db-dup-sort"))]
    pub(crate) fn delete<K>(&mut self, db: &T::Database, k: &K) -> Result<(), StoreError>
    where
        K: AsRef<[u8]>,
    {
        self.0.del(db, k.as_ref()).map_err(|e| e.into())
    }

    #[cfg(feature = "db-dup-sort")]
    pub(crate) fn delete<K>(
        &mut self,
        db: &T::Database,
        k: &K,
        v: Option<&[u8]>,
    ) -> Result<(), StoreError>
    where
        K: AsRef<[u8]>,
    {
        self.0.del(db, k.as_ref(), v).map_err(|e| e.into())
    }

    pub(crate) fn clear(&mut self, db: &T::Database) -> Result<(), StoreError> {
        self.0.clear_db(db).map_err(|e| e.into())
    }
}
