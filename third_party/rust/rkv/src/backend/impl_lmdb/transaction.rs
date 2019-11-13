// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use lmdb::Transaction;

use super::{
    DatabaseImpl,
    ErrorImpl,
    RoCursorImpl,
    WriteFlagsImpl,
};
use crate::backend::traits::{
    BackendRoCursorTransaction,
    BackendRoTransaction,
    BackendRwCursorTransaction,
    BackendRwTransaction,
};

#[derive(Debug)]
pub struct RoTransactionImpl<'env>(pub(crate) lmdb::RoTransaction<'env>);

impl<'env> BackendRoTransaction for RoTransactionImpl<'env> {
    type Error = ErrorImpl;
    type Database = DatabaseImpl;

    fn get(&self, db: &Self::Database, key: &[u8]) -> Result<&[u8], Self::Error> {
        self.0.get(db.0, &key).map_err(ErrorImpl)
    }

    fn abort(self) {
        self.0.abort()
    }
}

impl<'env> BackendRoCursorTransaction<'env> for RoTransactionImpl<'env> {
    type RoCursor = RoCursorImpl<'env>;

    fn open_ro_cursor(&'env self, db: &Self::Database) -> Result<Self::RoCursor, Self::Error> {
        self.0.open_ro_cursor(db.0).map(RoCursorImpl).map_err(ErrorImpl)
    }
}

#[derive(Debug)]
pub struct RwTransactionImpl<'env>(pub(crate) lmdb::RwTransaction<'env>);

impl<'env> BackendRwTransaction for RwTransactionImpl<'env> {
    type Error = ErrorImpl;
    type Database = DatabaseImpl;
    type Flags = WriteFlagsImpl;

    fn get(&self, db: &Self::Database, key: &[u8]) -> Result<&[u8], Self::Error> {
        self.0.get(db.0, &key).map_err(ErrorImpl)
    }

    fn put(&mut self, db: &Self::Database, key: &[u8], value: &[u8], flags: Self::Flags) -> Result<(), Self::Error> {
        self.0.put(db.0, &key, &value, flags.0).map_err(ErrorImpl)
    }

    #[cfg(not(feature = "db-dup-sort"))]
    fn del(&mut self, db: &Self::Database, key: &[u8]) -> Result<(), Self::Error> {
        self.0.del(db.0, &key, None).map_err(ErrorImpl)
    }

    #[cfg(feature = "db-dup-sort")]
    fn del(&mut self, db: &Self::Database, key: &[u8], value: Option<&[u8]>) -> Result<(), Self::Error> {
        self.0.del(db.0, &key, value).map_err(ErrorImpl)
    }

    fn clear_db(&mut self, db: &Self::Database) -> Result<(), Self::Error> {
        self.0.clear_db(db.0).map_err(ErrorImpl)
    }

    fn commit(self) -> Result<(), Self::Error> {
        self.0.commit().map_err(ErrorImpl)
    }

    fn abort(self) {
        self.0.abort()
    }
}

impl<'env> BackendRwCursorTransaction<'env> for RwTransactionImpl<'env> {
    type RoCursor = RoCursorImpl<'env>;

    fn open_ro_cursor(&'env self, db: &Self::Database) -> Result<Self::RoCursor, Self::Error> {
        self.0.open_ro_cursor(db.0).map(RoCursorImpl).map_err(ErrorImpl)
    }
}
