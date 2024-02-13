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
    fs,
    path::{Path, PathBuf},
};

use lmdb::Error as LmdbError;

use super::{
    DatabaseFlagsImpl, DatabaseImpl, EnvironmentFlagsImpl, ErrorImpl, InfoImpl, RoTransactionImpl,
    RwTransactionImpl, StatImpl,
};
use crate::backend::common::RecoveryStrategy;
use crate::backend::traits::{
    BackendEnvironment, BackendEnvironmentBuilder, BackendInfo, BackendIter, BackendRoCursor,
    BackendRoCursorTransaction, BackendStat,
};

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct EnvironmentBuilderImpl {
    builder: lmdb::EnvironmentBuilder,
    env_path_type: EnvironmentPathType,
    env_lock_type: EnvironmentLockType,
    env_db_type: EnvironmentDefaultDbType,
    make_dir_if_needed: bool,
}

impl<'b> BackendEnvironmentBuilder<'b> for EnvironmentBuilderImpl {
    type Environment = EnvironmentImpl;
    type Error = ErrorImpl;
    type Flags = EnvironmentFlagsImpl;

    fn new() -> EnvironmentBuilderImpl {
        EnvironmentBuilderImpl {
            builder: lmdb::Environment::new(),
            env_path_type: EnvironmentPathType::SubDir,
            env_lock_type: EnvironmentLockType::Lockfile,
            env_db_type: EnvironmentDefaultDbType::SingleDatabase,
            make_dir_if_needed: false,
        }
    }

    fn set_flags<T>(&mut self, flags: T) -> &mut Self
    where
        T: Into<Self::Flags>,
    {
        let flags = flags.into();
        if flags.0 == lmdb::EnvironmentFlags::NO_SUB_DIR {
            self.env_path_type = EnvironmentPathType::NoSubDir;
        }
        if flags.0 == lmdb::EnvironmentFlags::NO_LOCK {
            self.env_lock_type = EnvironmentLockType::NoLockfile;
        }
        self.builder.set_flags(flags.0);
        self
    }

    fn set_max_readers(&mut self, max_readers: u32) -> &mut Self {
        self.builder.set_max_readers(max_readers);
        self
    }

    fn set_max_dbs(&mut self, max_dbs: u32) -> &mut Self {
        if max_dbs > 0 {
            self.env_db_type = EnvironmentDefaultDbType::MultipleNamedDatabases
        }
        self.builder.set_max_dbs(max_dbs);
        self
    }

    fn set_map_size(&mut self, size: usize) -> &mut Self {
        self.builder.set_map_size(size);
        self
    }

    fn set_make_dir_if_needed(&mut self, make_dir_if_needed: bool) -> &mut Self {
        self.make_dir_if_needed = make_dir_if_needed;
        self
    }

    /// **UNIMPLEMENTED.** Will panic at runtime.
    fn set_corruption_recovery_strategy(&mut self, _strategy: RecoveryStrategy) -> &mut Self {
        // Unfortunately, when opening a database, LMDB doesn't handle all the ways it could have
        // been corrupted. Prefer using the `SafeMode` backend if this is important.
        unimplemented!();
    }

    fn open(&self, path: &Path) -> Result<Self::Environment, Self::Error> {
        match self.env_path_type {
            EnvironmentPathType::NoSubDir => {
                if !path.is_file() {
                    return Err(ErrorImpl::UnsuitableEnvironmentPath(path.into()));
                }
            }
            EnvironmentPathType::SubDir => {
                if !path.is_dir() {
                    if !self.make_dir_if_needed {
                        return Err(ErrorImpl::UnsuitableEnvironmentPath(path.into()));
                    }
                    fs::create_dir_all(path)?;
                }
            }
        }

        self.builder
            .open(path)
            .map_err(ErrorImpl::LmdbError)
            .and_then(|lmdbenv| {
                EnvironmentImpl::new(
                    path,
                    self.env_path_type,
                    self.env_lock_type,
                    self.env_db_type,
                    lmdbenv,
                )
            })
    }
}

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum EnvironmentPathType {
    SubDir,
    NoSubDir,
}

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum EnvironmentLockType {
    Lockfile,
    NoLockfile,
}

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub enum EnvironmentDefaultDbType {
    SingleDatabase,
    MultipleNamedDatabases,
}

#[derive(Debug)]
pub struct EnvironmentImpl {
    path: PathBuf,
    env_path_type: EnvironmentPathType,
    env_lock_type: EnvironmentLockType,
    env_db_type: EnvironmentDefaultDbType,
    lmdbenv: lmdb::Environment,
}

impl EnvironmentImpl {
    pub(crate) fn new(
        path: &Path,
        env_path_type: EnvironmentPathType,
        env_lock_type: EnvironmentLockType,
        env_db_type: EnvironmentDefaultDbType,
        lmdbenv: lmdb::Environment,
    ) -> Result<EnvironmentImpl, ErrorImpl> {
        Ok(EnvironmentImpl {
            path: path.to_path_buf(),
            env_path_type,
            env_lock_type,
            env_db_type,
            lmdbenv,
        })
    }
}

impl<'e> BackendEnvironment<'e> for EnvironmentImpl {
    type Database = DatabaseImpl;
    type Error = ErrorImpl;
    type Flags = DatabaseFlagsImpl;
    type Info = InfoImpl;
    type RoTransaction = RoTransactionImpl<'e>;
    type RwTransaction = RwTransactionImpl<'e>;
    type Stat = StatImpl;

    fn get_dbs(&self) -> Result<Vec<Option<String>>, Self::Error> {
        if self.env_db_type == EnvironmentDefaultDbType::SingleDatabase {
            return Ok(vec![None]);
        }
        let db = self
            .lmdbenv
            .open_db(None)
            .map(DatabaseImpl)
            .map_err(ErrorImpl::LmdbError)?;
        let reader = self.begin_ro_txn()?;
        let cursor = reader.open_ro_cursor(&db)?;
        let mut iter = cursor.into_iter();
        let mut store = vec![];
        while let Some(result) = iter.next() {
            let (key, _) = result?;
            let name = String::from_utf8(key.to_owned())
                .map_err(|_| ErrorImpl::LmdbError(lmdb::Error::Corrupted))?;
            store.push(Some(name));
        }
        Ok(store)
    }

    fn open_db(&self, name: Option<&str>) -> Result<Self::Database, Self::Error> {
        self.lmdbenv
            .open_db(name)
            .map(DatabaseImpl)
            .map_err(ErrorImpl::LmdbError)
    }

    fn create_db(
        &self,
        name: Option<&str>,
        flags: Self::Flags,
    ) -> Result<Self::Database, Self::Error> {
        self.lmdbenv
            .create_db(name, flags.0)
            .map(DatabaseImpl)
            .map_err(ErrorImpl::LmdbError)
    }

    fn begin_ro_txn(&'e self) -> Result<Self::RoTransaction, Self::Error> {
        self.lmdbenv
            .begin_ro_txn()
            .map(RoTransactionImpl)
            .map_err(ErrorImpl::LmdbError)
    }

    fn begin_rw_txn(&'e self) -> Result<Self::RwTransaction, Self::Error> {
        self.lmdbenv
            .begin_rw_txn()
            .map(RwTransactionImpl)
            .map_err(ErrorImpl::LmdbError)
    }

    fn sync(&self, force: bool) -> Result<(), Self::Error> {
        self.lmdbenv.sync(force).map_err(ErrorImpl::LmdbError)
    }

    fn stat(&self) -> Result<Self::Stat, Self::Error> {
        self.lmdbenv
            .stat()
            .map(StatImpl)
            .map_err(ErrorImpl::LmdbError)
    }

    fn info(&self) -> Result<Self::Info, Self::Error> {
        self.lmdbenv
            .info()
            .map(InfoImpl)
            .map_err(ErrorImpl::LmdbError)
    }

    fn freelist(&self) -> Result<usize, Self::Error> {
        self.lmdbenv.freelist().map_err(ErrorImpl::LmdbError)
    }

    fn load_ratio(&self) -> Result<Option<f32>, Self::Error> {
        let stat = self.stat()?;
        let info = self.info()?;
        let freelist = self.freelist()?;

        let last_pgno = info.last_pgno() + 1; // pgno is 0 based.
        let total_pgs = info.map_size() / stat.page_size();
        if freelist > last_pgno {
            return Err(ErrorImpl::LmdbError(LmdbError::Corrupted));
        }
        let used_pgs = last_pgno - freelist;
        Ok(Some(used_pgs as f32 / total_pgs as f32))
    }

    fn set_map_size(&self, size: usize) -> Result<(), Self::Error> {
        self.lmdbenv
            .set_map_size(size)
            .map_err(ErrorImpl::LmdbError)
    }

    fn get_files_on_disk(&self) -> Vec<PathBuf> {
        let mut store = vec![];

        if self.env_path_type == EnvironmentPathType::NoSubDir {
            // The option NO_SUB_DIR could change the default directory layout; therefore this should
            // probably return the path used to create environment, along with the custom lockfile
            // when available.
            unimplemented!();
        }

        let mut db_filename = self.path.clone();
        db_filename.push("data.mdb");
        store.push(db_filename);

        if self.env_lock_type == EnvironmentLockType::Lockfile {
            let mut lock_filename = self.path.clone();
            lock_filename.push("lock.mdb");
            store.push(lock_filename);
        }

        store
    }
}
