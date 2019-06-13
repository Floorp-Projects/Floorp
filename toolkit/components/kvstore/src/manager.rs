/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A manager of handles to Rkv environments that ensures that multiple calls
//! to `nsIKeyValueService.get_or_create()` with the same path reuse the same
//! environment handle.
//!
//! The Rkv crate itself provides this functionality, but it calls
//! `Path.canonicalize()` on the path, which crashes in Firefox on Android
//! because of https://bugzilla.mozilla.org/show_bug.cgi?id=1531887.
//!
//! Since kvstore consumers use canonical paths in practice, this manager
//! works around that bug by not doing so itself.

use error::KeyValueError;
use lmdb::Error as LmdbError;
use rkv::{migrate::Migrator, Rkv, StoreError};
use std::collections::HashMap;
use std::collections::hash_map::Entry;
use std::fs::copy;
use std::path::{
    Path,
    PathBuf,
};
use std::sync::{
    Arc,
    RwLock,
};
use tempfile::tempdir;

lazy_static! {
    static ref MANAGER: RwLock<Manager> = RwLock::new(Manager::new());
}

pub(crate) struct Manager {
    environments: HashMap<PathBuf, Arc<RwLock<Rkv>>>,
}

impl Manager {
    fn new() -> Manager {
        Manager {
            environments: Default::default(),
        }
    }

    pub(crate) fn singleton() -> &'static RwLock<Manager> {
        &*MANAGER
    }

    /// Return the open env at `path`, or create it by calling `f`.
    pub fn get_or_create<'p, F, P>(&mut self, path: P, f: F) -> Result<Arc<RwLock<Rkv>>, KeyValueError>
    where
        F: Fn(&Path) -> Result<Rkv, StoreError>,
        P: Into<&'p Path>,
    {
        let path = path.into();
        Ok(match self.environments.entry(path.to_path_buf()) {
            Entry::Occupied(e) => e.get().clone(),
            Entry::Vacant(e) => {
                let env = match f(e.key().as_path()) {
                    Ok(env) => env,
                    Err(StoreError::LmdbError(LmdbError::Invalid)) => {
                        let temp_env = tempdir()?;
                        let mut migrator = Migrator::new(path)?;
                        migrator.migrate(temp_env.path())?;
                        copy(temp_env.path().join("data.mdb"), path.join("data.mdb"))?;
                        copy(temp_env.path().join("lock.mdb"), path.join("lock.mdb"))?;
                        f(e.key().as_path())?
                    },
                    Err(err) => return Err(err.into()),
                };
                let k = Arc::new(RwLock::new(env));
                e.insert(k).clone()
            },
        })
    }
}
