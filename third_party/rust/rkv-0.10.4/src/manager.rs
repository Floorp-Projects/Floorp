// Copyright 2018-2019 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

use lazy_static::lazy_static;
use std::collections::BTreeMap;

use std::io::{
    self,
    Error,
    ErrorKind,
};

use std::collections::btree_map::Entry;

use std::os::raw::c_uint;

use std::path::{
    Path,
    PathBuf,
};

use std::sync::{
    Arc,
    RwLock,
};

use url::Url;

use crate::error::StoreError;

use crate::Rkv;

lazy_static! {
    /// A process is only permitted to have one open handle to each Rkv environment.
    /// This manager exists to enforce that constraint: don't open environments directly.
    static ref MANAGER: RwLock<Manager> = RwLock::new(Manager::new());
}

// Workaround the UNC path on Windows, see https://github.com/rust-lang/rust/issues/42869.
// Otherwise, `Env::from_env()` will panic with error_no(123).
fn canonicalize_path<'p, P>(path: P) -> io::Result<PathBuf>
where
    P: Into<&'p Path>,
{
    let canonical = path.into().canonicalize()?;
    if cfg!(target_os = "windows") {
        let url = Url::from_file_path(&canonical).map_err(|_e| Error::new(ErrorKind::Other, "URL passing error"))?;
        return url.to_file_path().map_err(|_e| Error::new(ErrorKind::Other, "path canonicalization error"));
    }
    Ok(canonical)
}

/// A process is only permitted to have one open handle to each Rkv environment.
/// This manager exists to enforce that constraint: don't open environments directly.
pub struct Manager {
    environments: BTreeMap<PathBuf, Arc<RwLock<Rkv>>>,
}

impl Manager {
    fn new() -> Manager {
        Manager {
            environments: Default::default(),
        }
    }

    pub fn singleton() -> &'static RwLock<Manager> {
        &*MANAGER
    }

    /// Return the open env at `path`, returning `None` if it has not already been opened.
    pub fn get<'p, P>(&self, path: P) -> Result<Option<Arc<RwLock<Rkv>>>, ::std::io::Error>
    where
        P: Into<&'p Path>,
    {
        let canonical = canonicalize_path(path)?;
        Ok(self.environments.get(&canonical).cloned())
    }

    /// Return the open env at `path`, or create it by calling `f`.
    pub fn get_or_create<'p, F, P>(&mut self, path: P, f: F) -> Result<Arc<RwLock<Rkv>>, StoreError>
    where
        F: FnOnce(&Path) -> Result<Rkv, StoreError>,
        P: Into<&'p Path>,
    {
        let canonical = canonicalize_path(path)?;
        Ok(match self.environments.entry(canonical) {
            Entry::Occupied(e) => e.get().clone(),
            Entry::Vacant(e) => {
                let k = Arc::new(RwLock::new(f(e.key().as_path())?));
                e.insert(k).clone()
            },
        })
    }

    /// Return the open env at `path` with capacity `capacity`,
    /// or create it by calling `f`.
    pub fn get_or_create_with_capacity<'p, F, P>(
        &mut self,
        path: P,
        capacity: c_uint,
        f: F,
    ) -> Result<Arc<RwLock<Rkv>>, StoreError>
    where
        F: FnOnce(&Path, c_uint) -> Result<Rkv, StoreError>,
        P: Into<&'p Path>,
    {
        let canonical = canonicalize_path(path)?;
        Ok(match self.environments.entry(canonical) {
            Entry::Occupied(e) => e.get().clone(),
            Entry::Vacant(e) => {
                let k = Arc::new(RwLock::new(f(e.key().as_path(), capacity)?));
                e.insert(k).clone()
            },
        })
    }
}

#[cfg(test)]
mod tests {
    use std::fs;
    use tempfile::Builder;

    use super::*;

    /// Test that the manager will return the same Rkv instance each time for each path.
    #[test]
    fn test_same() {
        let root = Builder::new().prefix("test_same").tempdir().expect("tempdir");
        fs::create_dir_all(root.path()).expect("dir created");

        let mut manager = Manager::new();

        let p = root.path();
        assert!(manager.get(p).expect("success").is_none());

        let created_arc = manager.get_or_create(p, Rkv::new).expect("created");
        let fetched_arc = manager.get(p).expect("success").expect("existed");
        assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
    }

    /// Test that one can mutate managed Rkv instances in surprising ways.
    #[test]
    fn test_mutate_managed_rkv() {
        let mut manager = Manager::new();

        let root1 = Builder::new().prefix("test_mutate_managed_rkv_1").tempdir().expect("tempdir");
        fs::create_dir_all(root1.path()).expect("dir created");
        let path1 = root1.path();
        let arc = manager.get_or_create(path1, Rkv::new).expect("created");

        // Arc<RwLock<>> has interior mutability, so we can replace arc's Rkv
        // instance with a new instance that has a different path.
        let root2 = Builder::new().prefix("test_mutate_managed_rkv_2").tempdir().expect("tempdir");
        fs::create_dir_all(root2.path()).expect("dir created");
        let path2 = root2.path();
        {
            let mut rkv = arc.write().expect("guard");
            let rkv2 = Rkv::new(path2).expect("Rkv");
            *rkv = rkv2;
        }

        // arc now has a different internal Rkv with path2, but it's still
        // mapped to path1 in manager, so its pointer is equal to a new Arc
        // for path1.
        let path1_arc = manager.get(path1).expect("success").expect("existed");
        assert!(Arc::ptr_eq(&path1_arc, &arc));

        // Meanwhile, a new Arc for path2 has a different pointer, even though
        // its Rkv's path is the same as arc's current path.
        let path2_arc = manager.get_or_create(path2, Rkv::new).expect("success");
        assert!(!Arc::ptr_eq(&path2_arc, &arc));
    }

    /// Test that the manager will return the same Rkv instance each time for each path.
    #[test]
    fn test_same_with_capacity() {
        let root = Builder::new().prefix("test_same").tempdir().expect("tempdir");
        fs::create_dir_all(root.path()).expect("dir created");

        let mut manager = Manager::new();

        let p = root.path();
        assert!(manager.get(p).expect("success").is_none());

        let created_arc = manager.get_or_create_with_capacity(p, 10, Rkv::with_capacity).expect("created");
        let fetched_arc = manager.get(p).expect("success").expect("existed");
        assert!(Arc::ptr_eq(&created_arc, &fetched_arc));
    }
}
