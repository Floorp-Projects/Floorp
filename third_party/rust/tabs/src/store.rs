/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::storage::{ClientRemoteTabs, RemoteTab, TabsStorage};
use std::path::Path;
use std::sync::Mutex;

pub struct TabsStore {
    pub storage: Mutex<TabsStorage>,
}

impl TabsStore {
    pub fn new(db_path: impl AsRef<Path>) -> Self {
        Self {
            storage: Mutex::new(TabsStorage::new(db_path)),
        }
    }

    pub fn new_with_mem_path(db_path: &str) -> Self {
        Self {
            storage: Mutex::new(TabsStorage::new_with_mem_path(db_path)),
        }
    }

    pub fn set_local_tabs(&self, local_state: Vec<RemoteTab>) {
        self.storage.lock().unwrap().update_local_state(local_state);
    }

    // like remote_tabs, but serves the uniffi layer
    pub fn get_all(&self) -> Vec<ClientRemoteTabs> {
        match self.remote_tabs() {
            Some(list) => list,
            None => vec![],
        }
    }

    pub fn remote_tabs(&self) -> Option<Vec<ClientRemoteTabs>> {
        self.storage.lock().unwrap().get_remote_tabs()
    }
}
