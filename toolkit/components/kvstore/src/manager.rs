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

use rkv::{Rkv, StoreError};
use std::collections::HashMap;
use std::collections::hash_map::Entry;
use std::path::{
    Path,
    PathBuf,
};
use std::sync::{
    Arc,
    RwLock,
};

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
    pub fn get_or_create<'p, F, P>(&mut self, path: P, f: F) -> Result<Arc<RwLock<Rkv>>, StoreError>
    where
        F: FnOnce(&Path) -> Result<Rkv, StoreError>,
        P: Into<&'p Path>,
    {
        Ok(match self.environments.entry(path.into().to_path_buf()) {
            Entry::Occupied(e) => e.get().clone(),
            Entry::Vacant(e) => {
                let k = Arc::new(RwLock::new(f(e.key().as_path())?));
                e.insert(k).clone()
            },
        })
    }
}
