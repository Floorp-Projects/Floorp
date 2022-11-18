// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use std::{
    fmt::{Debug, Display},
    path::{Path, PathBuf},
};

use crate::{
    backend::common::{DatabaseFlags, EnvironmentFlags, WriteFlags},
    error::StoreError,
};

pub trait BackendError: Debug + Display + Into<StoreError> {}

pub trait BackendDatabase: Debug + Eq + PartialEq + Copy + Clone {}

pub trait BackendFlags: Debug + Eq + PartialEq + Copy + Clone + Default {
    fn empty() -> Self;
}

pub trait BackendEnvironmentFlags: BackendFlags {
    fn set(&mut self, flag: EnvironmentFlags, value: bool);
}

pub trait BackendDatabaseFlags: BackendFlags {
    fn set(&mut self, flag: DatabaseFlags, value: bool);
}

pub trait BackendWriteFlags: BackendFlags {
    fn set(&mut self, flag: WriteFlags, value: bool);
}

pub trait BackendStat {
    fn page_size(&self) -> usize;

    fn depth(&self) -> usize;

    fn branch_pages(&self) -> usize;

    fn leaf_pages(&self) -> usize;

    fn overflow_pages(&self) -> usize;

    fn entries(&self) -> usize;
}

pub trait BackendInfo {
    fn map_size(&self) -> usize;

    fn last_pgno(&self) -> usize;

    fn last_txnid(&self) -> usize;

    fn max_readers(&self) -> usize;

    fn num_readers(&self) -> usize;
}

pub trait BackendEnvironmentBuilder<'b>: Debug + Eq + PartialEq + Copy + Clone {
    type Error: BackendError;
    type Environment: BackendEnvironment<'b>;
    type Flags: BackendEnvironmentFlags;

    fn new() -> Self;

    fn set_flags<T>(&mut self, flags: T) -> &mut Self
    where
        T: Into<Self::Flags>;

    fn set_max_dbs(&mut self, max_dbs: u32) -> &mut Self;

    fn set_max_readers(&mut self, max_readers: u32) -> &mut Self;

    fn set_map_size(&mut self, size: usize) -> &mut Self;

    fn set_make_dir_if_needed(&mut self, make_dir_if_needed: bool) -> &mut Self;

    fn set_discard_if_corrupted(&mut self, discard_if_corrupted: bool) -> &mut Self;

    fn open(&self, path: &Path) -> Result<Self::Environment, Self::Error>;
}

pub trait BackendEnvironment<'e>: Debug {
    type Error: BackendError;
    type Database: BackendDatabase;
    type Flags: BackendDatabaseFlags;
    type Stat: BackendStat;
    type Info: BackendInfo;
    type RoTransaction: BackendRoCursorTransaction<'e, Database = Self::Database>;
    type RwTransaction: BackendRwCursorTransaction<'e, Database = Self::Database>;

    fn get_dbs(&self) -> Result<Vec<Option<String>>, Self::Error>;

    fn open_db(&self, name: Option<&str>) -> Result<Self::Database, Self::Error>;

    fn create_db(
        &self,
        name: Option<&str>,
        flags: Self::Flags,
    ) -> Result<Self::Database, Self::Error>;

    fn begin_ro_txn(&'e self) -> Result<Self::RoTransaction, Self::Error>;

    fn begin_rw_txn(&'e self) -> Result<Self::RwTransaction, Self::Error>;

    fn sync(&self, force: bool) -> Result<(), Self::Error>;

    fn stat(&self) -> Result<Self::Stat, Self::Error>;

    fn info(&self) -> Result<Self::Info, Self::Error>;

    fn freelist(&self) -> Result<usize, Self::Error>;

    fn load_ratio(&self) -> Result<Option<f32>, Self::Error>;

    fn set_map_size(&self, size: usize) -> Result<(), Self::Error>;

    fn get_files_on_disk(&self) -> Vec<PathBuf>;
}

pub trait BackendRoTransaction: Debug {
    type Error: BackendError;
    type Database: BackendDatabase;

    fn get(&self, db: &Self::Database, key: &[u8]) -> Result<&[u8], Self::Error>;

    fn abort(self);
}

pub trait BackendRwTransaction: Debug {
    type Error: BackendError;
    type Database: BackendDatabase;
    type Flags: BackendWriteFlags;

    fn get(&self, db: &Self::Database, key: &[u8]) -> Result<&[u8], Self::Error>;

    fn put(
        &mut self,
        db: &Self::Database,
        key: &[u8],
        value: &[u8],
        flags: Self::Flags,
    ) -> Result<(), Self::Error>;

    #[cfg(not(feature = "db-dup-sort"))]
    fn del(&mut self, db: &Self::Database, key: &[u8]) -> Result<(), Self::Error>;

    #[cfg(feature = "db-dup-sort")]
    fn del(
        &mut self,
        db: &Self::Database,
        key: &[u8],
        value: Option<&[u8]>,
    ) -> Result<(), Self::Error>;

    fn clear_db(&mut self, db: &Self::Database) -> Result<(), Self::Error>;

    fn commit(self) -> Result<(), Self::Error>;

    fn abort(self);
}

pub trait BackendRoCursorTransaction<'t>: BackendRoTransaction {
    type RoCursor: BackendRoCursor<'t>;

    fn open_ro_cursor(&'t self, db: &Self::Database) -> Result<Self::RoCursor, Self::Error>;
}

pub trait BackendRwCursorTransaction<'t>: BackendRwTransaction {
    type RoCursor: BackendRoCursor<'t>;

    fn open_ro_cursor(&'t self, db: &Self::Database) -> Result<Self::RoCursor, Self::Error>;
}

pub trait BackendRoCursor<'c>: Debug {
    type Iter: BackendIter<'c>;

    fn into_iter(self) -> Self::Iter;

    fn into_iter_from<K>(self, key: K) -> Self::Iter
    where
        K: AsRef<[u8]> + 'c;

    fn into_iter_dup_of<K>(self, key: K) -> Self::Iter
    where
        K: AsRef<[u8]> + 'c;
}

pub trait BackendIter<'i> {
    type Error: BackendError;

    #[allow(clippy::type_complexity)]
    fn next(&mut self) -> Option<Result<(&'i [u8], &'i [u8]), Self::Error>>;
}
