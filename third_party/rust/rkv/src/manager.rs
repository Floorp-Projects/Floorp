// Copyright 2018 Mozilla
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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

use error::StoreError;

use Rkv;

/// A process is only permitted to have one open handle to each Rkv environment.
/// This manager exists to enforce that constraint: don't open environments directly.
lazy_static! {
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
    extern crate tempfile;

    use self::tempfile::Builder;
    use std::fs;

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
