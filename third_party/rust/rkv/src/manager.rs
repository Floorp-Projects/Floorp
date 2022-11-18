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
    collections::{btree_map::Entry, BTreeMap},
    os::raw::c_uint,
    path::{Path, PathBuf},
    result,
    sync::{Arc, RwLock},
};

use lazy_static::lazy_static;

#[cfg(feature = "lmdb")]
use crate::backend::LmdbEnvironment;
use crate::{
    backend::{BackendEnvironment, BackendEnvironmentBuilder, SafeModeEnvironment},
    error::{CloseError, StoreError},
    helpers::canonicalize_path,
    store::CloseOptions,
    Rkv,
};

type Result<T> = result::Result<T, StoreError>;
type CloseResult<T> = result::Result<T, CloseError>;
type SharedRkv<E> = Arc<RwLock<Rkv<E>>>;

#[cfg(feature = "lmdb")]
lazy_static! {
    static ref MANAGER_LMDB: RwLock<Manager<LmdbEnvironment>> = RwLock::new(Manager::new());
}

lazy_static! {
    static ref MANAGER_SAFE_MODE: RwLock<Manager<SafeModeEnvironment>> =
        RwLock::new(Manager::new());
}

/// A process is only permitted to have one open handle to each Rkv environment. This
/// manager exists to enforce that constraint: don't open environments directly.
///
/// By default, path canonicalization is enabled for identifying RKV instances. This
/// is true by default, because it helps enforce the constraints guaranteed by
/// this manager. However, path canonicalization might crash in some fringe
/// circumstances, so the `no-canonicalize-path` feature offers the possibility of
/// disabling it. See: https://bugzilla.mozilla.org/show_bug.cgi?id=1531887
///
/// When path canonicalization is disabled, you *must* ensure an RKV environment is
/// always created or retrieved with the same path.
pub struct Manager<E> {
    environments: BTreeMap<PathBuf, SharedRkv<E>>,
}

impl<'e, E> Manager<E>
where
    E: BackendEnvironment<'e>,
{
    fn new() -> Manager<E> {
        Manager {
            environments: Default::default(),
        }
    }

    /// Return the open env at `path`, returning `None` if it has not already been opened.
    pub fn get<'p, P>(&self, path: P) -> Result<Option<SharedRkv<E>>>
    where
        P: Into<&'p Path>,
    {
        let canonical = if cfg!(feature = "no-canonicalize-path") {
            path.into().to_path_buf()
        } else {
            canonicalize_path(path)?
        };
        Ok(self.environments.get(&canonical).cloned())
    }

    /// Return the open env at `path`, or create it by calling `f`.
    pub fn get_or_create<'p, F, P>(&mut self, path: P, f: F) -> Result<SharedRkv<E>>
    where
        F: FnOnce(&Path) -> Result<Rkv<E>>,
        P: Into<&'p Path>,
    {
        let canonical = if cfg!(feature = "no-canonicalize-path") {
            path.into().to_path_buf()
        } else {
            canonicalize_path(path)?
        };
        Ok(match self.environments.entry(canonical) {
            Entry::Occupied(e) => e.get().clone(),
            Entry::Vacant(e) => {
                let k = Arc::new(RwLock::new(f(e.key().as_path())?));
                e.insert(k).clone()
            }
        })
    }

    /// Return the open env at `path` with `capacity`, or create it by calling `f`.
    pub fn get_or_create_with_capacity<'p, F, P>(
        &mut self,
        path: P,
        capacity: c_uint,
        f: F,
    ) -> Result<SharedRkv<E>>
    where
        F: FnOnce(&Path, c_uint) -> Result<Rkv<E>>,
        P: Into<&'p Path>,
    {
        let canonical = if cfg!(feature = "no-canonicalize-path") {
            path.into().to_path_buf()
        } else {
            canonicalize_path(path)?
        };
        Ok(match self.environments.entry(canonical) {
            Entry::Occupied(e) => e.get().clone(),
            Entry::Vacant(e) => {
                let k = Arc::new(RwLock::new(f(e.key().as_path(), capacity)?));
                e.insert(k).clone()
            }
        })
    }

    /// Return a new Rkv environment from the builder, or create it by calling `f`.
    pub fn get_or_create_from_builder<'p, F, P, B>(
        &mut self,
        path: P,
        builder: B,
        f: F,
    ) -> Result<SharedRkv<E>>
    where
        F: FnOnce(&Path, B) -> Result<Rkv<E>>,
        P: Into<&'p Path>,
        B: BackendEnvironmentBuilder<'e, Environment = E>,
    {
        let canonical = if cfg!(feature = "no-canonicalize-path") {
            path.into().to_path_buf()
        } else {
            canonicalize_path(path)?
        };
        Ok(match self.environments.entry(canonical) {
            Entry::Occupied(e) => e.get().clone(),
            Entry::Vacant(e) => {
                let k = Arc::new(RwLock::new(f(e.key().as_path(), builder)?));
                e.insert(k).clone()
            }
        })
    }

    /// Tries to close the specified environment.
    /// Returns an error when other users of this environment still exist.
    pub fn try_close<'p, P>(&mut self, path: P, options: CloseOptions) -> CloseResult<()>
    where
        P: Into<&'p Path>,
    {
        let canonical = if cfg!(feature = "no-canonicalize-path") {
            path.into().to_path_buf()
        } else {
            canonicalize_path(path)?
        };
        match self.environments.entry(canonical) {
            Entry::Vacant(_) => Ok(()),
            Entry::Occupied(e) if Arc::strong_count(e.get()) > 1 => {
                Err(CloseError::EnvironmentStillOpen)
            }
            Entry::Occupied(e) => {
                let env = Arc::try_unwrap(e.remove())
                    .map_err(|_| CloseError::UnknownEnvironmentStillOpen)?;
                env.into_inner()?.close(options)?;
                Ok(())
            }
        }
    }
}

#[cfg(feature = "lmdb")]
impl Manager<LmdbEnvironment> {
    pub fn singleton() -> &'static RwLock<Manager<LmdbEnvironment>> {
        &MANAGER_LMDB
    }
}

impl Manager<SafeModeEnvironment> {
    pub fn singleton() -> &'static RwLock<Manager<SafeModeEnvironment>> {
        &MANAGER_SAFE_MODE
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::*;

    use std::fs;

    use tempfile::Builder;

    #[cfg(feature = "lmdb")]
    use backend::Lmdb;

    /// Test that one can mutate managed Rkv instances in surprising ways.
    #[cfg(feature = "lmdb")]
    #[test]
    fn test_mutate_managed_rkv() {
        let mut manager = Manager::<LmdbEnvironment>::new();

        let root1 = Builder::new()
            .prefix("test_mutate_managed_rkv_1")
            .tempdir()
            .expect("tempdir");
        fs::create_dir_all(root1.path()).expect("dir created");
        let path1 = root1.path();
        let arc = manager
            .get_or_create(path1, Rkv::new::<Lmdb>)
            .expect("created");

        // Arc<RwLock<>> has interior mutability, so we can replace arc's Rkv instance with a new
        // instance that has a different path.
        let root2 = Builder::new()
            .prefix("test_mutate_managed_rkv_2")
            .tempdir()
            .expect("tempdir");
        fs::create_dir_all(root2.path()).expect("dir created");
        let path2 = root2.path();
        {
            let mut rkv = arc.write().expect("guard");
            let rkv2 = Rkv::new::<Lmdb>(path2).expect("Rkv");
            *rkv = rkv2;
        }

        // Arc now has a different internal Rkv with path2, but it's still mapped to path1 in
        // manager, so its pointer is equal to a new Arc for path1.
        let path1_arc = manager.get(path1).expect("success").expect("existed");
        assert!(Arc::ptr_eq(&path1_arc, &arc));

        // Meanwhile, a new Arc for path2 has a different pointer, even though its Rkv's path is
        // the same as arc's current path.
        let path2_arc = manager
            .get_or_create(path2, Rkv::new::<Lmdb>)
            .expect("success");
        assert!(!Arc::ptr_eq(&path2_arc, &arc));
    }
}
