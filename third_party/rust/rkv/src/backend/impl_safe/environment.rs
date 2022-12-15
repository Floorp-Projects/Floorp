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
    borrow::Cow,
    collections::HashMap,
    fs,
    ops::DerefMut,
    path::{Path, PathBuf},
    sync::{Arc, RwLock, RwLockReadGuard, RwLockWriteGuard},
};

use id_arena::Arena;
use log::warn;

use super::{
    database::Database, DatabaseFlagsImpl, DatabaseImpl, EnvironmentFlagsImpl, ErrorImpl, InfoImpl,
    RoTransactionImpl, RwTransactionImpl, StatImpl,
};
use crate::backend::traits::{BackendEnvironment, BackendEnvironmentBuilder};

const DEFAULT_DB_FILENAME: &str = "data.safe.bin";

type DatabaseArena = Arena<Database>;
type DatabaseNameMap = HashMap<Option<String>, DatabaseImpl>;

#[derive(Debug, PartialEq, Eq, Copy, Clone)]
pub struct EnvironmentBuilderImpl {
    flags: EnvironmentFlagsImpl,
    max_readers: Option<usize>,
    max_dbs: Option<usize>,
    map_size: Option<usize>,
    make_dir_if_needed: bool,
    discard_if_corrupted: bool,
}

impl<'b> BackendEnvironmentBuilder<'b> for EnvironmentBuilderImpl {
    type Environment = EnvironmentImpl;
    type Error = ErrorImpl;
    type Flags = EnvironmentFlagsImpl;

    fn new() -> EnvironmentBuilderImpl {
        EnvironmentBuilderImpl {
            flags: EnvironmentFlagsImpl::empty(),
            max_readers: None,
            max_dbs: None,
            map_size: None,
            make_dir_if_needed: false,
            discard_if_corrupted: false,
        }
    }

    fn set_flags<T>(&mut self, flags: T) -> &mut Self
    where
        T: Into<Self::Flags>,
    {
        self.flags = flags.into();
        self
    }

    fn set_max_readers(&mut self, max_readers: u32) -> &mut Self {
        self.max_readers = Some(max_readers as usize);
        self
    }

    fn set_max_dbs(&mut self, max_dbs: u32) -> &mut Self {
        self.max_dbs = Some(max_dbs as usize);
        self
    }

    fn set_map_size(&mut self, map_size: usize) -> &mut Self {
        self.map_size = Some(map_size);
        self
    }

    fn set_make_dir_if_needed(&mut self, make_dir_if_needed: bool) -> &mut Self {
        self.make_dir_if_needed = make_dir_if_needed;
        self
    }

    fn set_discard_if_corrupted(&mut self, discard_if_corrupted: bool) -> &mut Self {
        self.discard_if_corrupted = discard_if_corrupted;
        self
    }

    fn open(&self, path: &Path) -> Result<Self::Environment, Self::Error> {
        // Technically NO_SUB_DIR should change these checks here, but they're both currently
        // unimplemented with this storage backend.
        if !path.is_dir() {
            if !self.make_dir_if_needed {
                return Err(ErrorImpl::UnsuitableEnvironmentPath(path.into()));
            }
            fs::create_dir_all(path)?;
        }
        let mut env = EnvironmentImpl::new(
            path,
            self.flags,
            self.max_readers,
            self.max_dbs,
            self.map_size,
        )?;
        env.read_from_disk(self.discard_if_corrupted)?;
        Ok(env)
    }
}

#[derive(Debug)]
pub(crate) struct EnvironmentDbs {
    pub(crate) arena: DatabaseArena,
    pub(crate) name_map: DatabaseNameMap,
}

#[derive(Debug)]
pub(crate) struct EnvironmentDbsRefMut<'a> {
    pub(crate) arena: &'a mut DatabaseArena,
    pub(crate) name_map: &'a mut DatabaseNameMap,
}

impl<'a> From<&'a mut EnvironmentDbs> for EnvironmentDbsRefMut<'a> {
    fn from(dbs: &mut EnvironmentDbs) -> EnvironmentDbsRefMut {
        EnvironmentDbsRefMut {
            arena: &mut dbs.arena,
            name_map: &mut dbs.name_map,
        }
    }
}

#[derive(Debug)]
pub struct EnvironmentImpl {
    path: PathBuf,
    max_dbs: usize,
    dbs: RwLock<EnvironmentDbs>,
    ro_txns: Arc<()>,
    rw_txns: Arc<()>,
}

impl EnvironmentImpl {
    fn serialize(&self) -> Result<Vec<u8>, ErrorImpl> {
        let dbs = self.dbs.read().map_err(|_| ErrorImpl::EnvPoisonError)?;
        let data: HashMap<_, _> = dbs
            .name_map
            .iter()
            .map(|(name, id)| (name, &dbs.arena[id.0]))
            .collect();
        Ok(bincode::serialize(&data)?)
    }

    fn deserialize(
        bytes: &[u8],
        discard_if_corrupted: bool,
    ) -> Result<(DatabaseArena, DatabaseNameMap), ErrorImpl> {
        let mut arena = DatabaseArena::new();
        let mut name_map = HashMap::new();
        let data: HashMap<_, _> = match bincode::deserialize(bytes) {
            Err(_) if discard_if_corrupted => Ok(HashMap::new()),
            result => result,
        }?;
        for (name, db) in data {
            name_map.insert(name, DatabaseImpl(arena.alloc(db)));
        }
        Ok((arena, name_map))
    }
}

impl EnvironmentImpl {
    pub(crate) fn new(
        path: &Path,
        flags: EnvironmentFlagsImpl,
        max_readers: Option<usize>,
        max_dbs: Option<usize>,
        map_size: Option<usize>,
    ) -> Result<EnvironmentImpl, ErrorImpl> {
        if !flags.is_empty() {
            warn!("Ignoring `flags={:?}`", flags);
        }
        if let Some(max_readers) = max_readers {
            warn!("Ignoring `max_readers={}`", max_readers);
        }
        if let Some(map_size) = map_size {
            warn!("Ignoring `map_size={}`", map_size);
        }

        Ok(EnvironmentImpl {
            path: path.to_path_buf(),
            max_dbs: max_dbs.unwrap_or(std::usize::MAX),
            dbs: RwLock::new(EnvironmentDbs {
                arena: DatabaseArena::new(),
                name_map: HashMap::new(),
            }),
            ro_txns: Arc::new(()),
            rw_txns: Arc::new(()),
        })
    }

    pub(crate) fn read_from_disk(&mut self, discard_if_corrupted: bool) -> Result<(), ErrorImpl> {
        let mut path = Cow::from(&self.path);
        if fs::metadata(&path)?.is_dir() {
            path.to_mut().push(DEFAULT_DB_FILENAME);
        };
        if fs::metadata(&path).is_err() {
            return Ok(());
        };
        let (arena, name_map) = Self::deserialize(&fs::read(&path)?, discard_if_corrupted)?;
        self.dbs = RwLock::new(EnvironmentDbs { arena, name_map });
        Ok(())
    }

    pub(crate) fn write_to_disk(&self) -> Result<(), ErrorImpl> {
        let mut path = Cow::from(&self.path);
        if fs::metadata(&path)?.is_dir() {
            path.to_mut().push(DEFAULT_DB_FILENAME);
        };

        // Write to a temp file first.
        let tmp_path = path.with_extension("tmp");
        fs::write(&tmp_path, self.serialize()?)?;

        // Atomically move that file to the database file.
        fs::rename(tmp_path, path)?;
        Ok(())
    }

    pub(crate) fn dbs(&self) -> Result<RwLockReadGuard<EnvironmentDbs>, ErrorImpl> {
        self.dbs.read().map_err(|_| ErrorImpl::EnvPoisonError)
    }

    pub(crate) fn dbs_mut(&self) -> Result<RwLockWriteGuard<EnvironmentDbs>, ErrorImpl> {
        self.dbs.write().map_err(|_| ErrorImpl::EnvPoisonError)
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
        let dbs = self.dbs.read().map_err(|_| ErrorImpl::EnvPoisonError)?;
        Ok(dbs.name_map.keys().map(|key| key.to_owned()).collect())
    }

    fn open_db(&self, name: Option<&str>) -> Result<Self::Database, Self::Error> {
        if Arc::strong_count(&self.ro_txns) > 1 {
            return Err(ErrorImpl::DbsIllegalOpen);
        }
        // TOOD: don't reallocate `name`.
        let key = name.map(String::from);
        let dbs = self.dbs.read().map_err(|_| ErrorImpl::EnvPoisonError)?;
        let db = dbs.name_map.get(&key).ok_or(ErrorImpl::DbNotFoundError)?;
        Ok(*db)
    }

    fn create_db(
        &self,
        name: Option<&str>,
        flags: Self::Flags,
    ) -> Result<Self::Database, Self::Error> {
        if Arc::strong_count(&self.ro_txns) > 1 {
            return Err(ErrorImpl::DbsIllegalOpen);
        }
        // TOOD: don't reallocate `name`.
        let key = name.map(String::from);
        let mut dbs = self.dbs.write().map_err(|_| ErrorImpl::EnvPoisonError)?;
        if dbs.name_map.keys().filter_map(|k| k.as_ref()).count() >= self.max_dbs && name.is_some() {
            return Err(ErrorImpl::DbsFull);
        }
        let parts = EnvironmentDbsRefMut::from(dbs.deref_mut());
        let arena = parts.arena;
        let name_map = parts.name_map;
        let id = name_map
            .entry(key)
            .or_insert_with(|| DatabaseImpl(arena.alloc(Database::new(Some(flags), None))));
        Ok(*id)
    }

    fn begin_ro_txn(&'e self) -> Result<Self::RoTransaction, Self::Error> {
        RoTransactionImpl::new(self, self.ro_txns.clone())
    }

    fn begin_rw_txn(&'e self) -> Result<Self::RwTransaction, Self::Error> {
        RwTransactionImpl::new(self, self.rw_txns.clone())
    }

    fn sync(&self, force: bool) -> Result<(), Self::Error> {
        warn!("Ignoring `force={}`", force);
        self.write_to_disk()
    }

    fn stat(&self) -> Result<Self::Stat, Self::Error> {
        Ok(StatImpl)
    }

    fn info(&self) -> Result<Self::Info, Self::Error> {
        Ok(InfoImpl)
    }

    fn freelist(&self) -> Result<usize, Self::Error> {
        unimplemented!()
    }

    fn load_ratio(&self) -> Result<Option<f32>, Self::Error> {
        warn!("`load_ratio()` is irrelevant for this storage backend.");
        Ok(None)
    }

    fn set_map_size(&self, size: usize) -> Result<(), Self::Error> {
        warn!(
            "`set_map_size({})` is ignored by this storage backend.",
            size
        );
        Ok(())
    }

    fn get_files_on_disk(&self) -> Vec<PathBuf> {
        // Technically NO_SUB_DIR and NO_LOCK should change this output, but
        // they're both currently unimplemented with this storage backend.
        let mut db_filename = self.path.clone();
        db_filename.push(DEFAULT_DB_FILENAME);
        vec![db_filename]
    }
}
