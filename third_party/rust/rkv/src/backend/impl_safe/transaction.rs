// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
#![allow(dead_code)] // TODO: Get rid of unused struct members

use std::{collections::HashMap, sync::Arc};

use super::{
    snapshot::Snapshot, DatabaseImpl, EnvironmentImpl, ErrorImpl, RoCursorImpl, WriteFlagsImpl,
};
use crate::backend::traits::{
    BackendRoCursorTransaction, BackendRoTransaction, BackendRwCursorTransaction,
    BackendRwTransaction,
};

#[derive(Debug)]
pub struct RoTransactionImpl<'t> {
    env: &'t EnvironmentImpl,
    snapshots: HashMap<DatabaseImpl, Snapshot>,
    idx: Arc<()>,
}

impl<'t> RoTransactionImpl<'t> {
    pub(crate) fn new(
        env: &'t EnvironmentImpl,
        idx: Arc<()>,
    ) -> Result<RoTransactionImpl<'t>, ErrorImpl> {
        let snapshots = env
            .dbs()?
            .arena
            .iter()
            .map(|(id, db)| (DatabaseImpl(id), db.snapshot()))
            .collect();
        Ok(RoTransactionImpl {
            env,
            snapshots,
            idx,
        })
    }
}

impl<'t> BackendRoTransaction for RoTransactionImpl<'t> {
    type Database = DatabaseImpl;
    type Error = ErrorImpl;

    fn get(&self, db: &Self::Database, key: &[u8]) -> Result<&[u8], Self::Error> {
        let snapshot = self.snapshots.get(db).ok_or(ErrorImpl::DbIsForeignError)?;
        snapshot.get(key).ok_or(ErrorImpl::KeyValuePairNotFound)
    }

    fn abort(self) {
        // noop
    }
}

impl<'t> BackendRoCursorTransaction<'t> for RoTransactionImpl<'t> {
    type RoCursor = RoCursorImpl<'t>;

    fn open_ro_cursor(&'t self, db: &Self::Database) -> Result<Self::RoCursor, Self::Error> {
        let snapshot = self.snapshots.get(db).ok_or(ErrorImpl::DbIsForeignError)?;
        Ok(RoCursorImpl(snapshot))
    }
}

#[derive(Debug)]
pub struct RwTransactionImpl<'t> {
    env: &'t EnvironmentImpl,
    snapshots: HashMap<DatabaseImpl, Snapshot>,
    idx: Arc<()>,
}

impl<'t> RwTransactionImpl<'t> {
    pub(crate) fn new(
        env: &'t EnvironmentImpl,
        idx: Arc<()>,
    ) -> Result<RwTransactionImpl<'t>, ErrorImpl> {
        let snapshots = env
            .dbs()?
            .arena
            .iter()
            .map(|(id, db)| (DatabaseImpl(id), db.snapshot()))
            .collect();
        Ok(RwTransactionImpl {
            env,
            snapshots,
            idx,
        })
    }
}

impl<'t> BackendRwTransaction for RwTransactionImpl<'t> {
    type Database = DatabaseImpl;
    type Error = ErrorImpl;
    type Flags = WriteFlagsImpl;

    fn get(&self, db: &Self::Database, key: &[u8]) -> Result<&[u8], Self::Error> {
        let snapshot = self.snapshots.get(db).ok_or(ErrorImpl::DbIsForeignError)?;
        snapshot.get(key).ok_or(ErrorImpl::KeyValuePairNotFound)
    }

    #[cfg(not(feature = "db-dup-sort"))]
    fn put(
        &mut self,
        db: &Self::Database,
        key: &[u8],
        value: &[u8],
        _flags: Self::Flags,
    ) -> Result<(), Self::Error> {
        let snapshot = self
            .snapshots
            .get_mut(db)
            .ok_or_else(|| ErrorImpl::DbIsForeignError)?;
        snapshot.put(key, value);
        Ok(())
    }

    #[cfg(feature = "db-dup-sort")]
    fn put(
        &mut self,
        db: &Self::Database,
        key: &[u8],
        value: &[u8],
        _flags: Self::Flags,
    ) -> Result<(), Self::Error> {
        use super::DatabaseFlagsImpl;
        let snapshot = self
            .snapshots
            .get_mut(db)
            .ok_or(ErrorImpl::DbIsForeignError)?;
        if snapshot.flags().contains(DatabaseFlagsImpl::DUP_SORT) {
            snapshot.put_dup(key, value);
        } else {
            snapshot.put(key, value);
        }
        Ok(())
    }

    #[cfg(not(feature = "db-dup-sort"))]
    fn del(&mut self, db: &Self::Database, key: &[u8]) -> Result<(), Self::Error> {
        let snapshot = self
            .snapshots
            .get_mut(db)
            .ok_or_else(|| ErrorImpl::DbIsForeignError)?;
        let deleted = snapshot.del(key);
        Ok(deleted.ok_or_else(|| ErrorImpl::KeyValuePairNotFound)?)
    }

    #[cfg(feature = "db-dup-sort")]
    fn del(
        &mut self,
        db: &Self::Database,
        key: &[u8],
        value: Option<&[u8]>,
    ) -> Result<(), Self::Error> {
        use super::DatabaseFlagsImpl;
        let snapshot = self
            .snapshots
            .get_mut(db)
            .ok_or(ErrorImpl::DbIsForeignError)?;
        let deleted = match (value, snapshot.flags()) {
            (Some(value), flags) if flags.contains(DatabaseFlagsImpl::DUP_SORT) => {
                snapshot.del_exact(key, value)
            }
            _ => snapshot.del(key),
        };
        deleted.ok_or(ErrorImpl::KeyValuePairNotFound)
    }

    fn clear_db(&mut self, db: &Self::Database) -> Result<(), Self::Error> {
        let snapshot = self
            .snapshots
            .get_mut(db)
            .ok_or(ErrorImpl::DbIsForeignError)?;
        snapshot.clear();
        Ok(())
    }

    fn commit(self) -> Result<(), Self::Error> {
        let mut dbs = self.env.dbs_mut()?;

        for (id, snapshot) in self.snapshots {
            let db = dbs.arena.get_mut(id.0).ok_or(ErrorImpl::DbIsForeignError)?;
            db.replace(snapshot);
        }

        drop(dbs);
        self.env.write_to_disk()
    }

    fn abort(self) {
        // noop
    }
}

impl<'t> BackendRwCursorTransaction<'t> for RwTransactionImpl<'t> {
    type RoCursor = RoCursorImpl<'t>;

    fn open_ro_cursor(&'t self, db: &Self::Database) -> Result<Self::RoCursor, Self::Error> {
        let snapshot = self.snapshots.get(db).ok_or(ErrorImpl::DbIsForeignError)?;
        Ok(RoCursorImpl(snapshot))
    }
}
