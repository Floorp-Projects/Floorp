/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::storage::{ClientRemoteTabs, RemoteTab, TabsStorage};
use crate::{ApiResult, PendingCommand, RemoteCommand};
use std::path::Path;
use std::sync::{Arc, Mutex};

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

    pub fn new_remote_command_store(self: Arc<Self>) -> Arc<RemoteCommandStore> {
        Arc::new(RemoteCommandStore {
            store: Arc::clone(&self),
        })
    }
}

pub struct RemoteCommandStore {
    // it's a shame we can't hold a TabsStorage.
    store: Arc<TabsStore>,
}

impl RemoteCommandStore {
    // Info about remote tab commands.
    // We record a local timestamp and a state of "pending". The app must arrange to deliver and
    // mark then as "sent". Thus it also serves as a persistent queue of commands to send while
    // handling unreliable delivery.

    // Commands here will influence what TabsStore::remote_tabs() returns for the device in an
    // attempt the pretend the command has remotely executed and succeeded before it actually has.
    // The policies for when we should stop pretending the command has executed is up to the app via
    // removing the command.
    #[error_support::handle_error(crate::Error)]
    pub fn add_remote_command(&self, device_id: &str, command: &RemoteCommand) -> ApiResult<bool> {
        self.store
            .storage
            .lock()
            .unwrap()
            .add_remote_tab_command(device_id, command)
    }

    #[error_support::handle_error(crate::Error)]
    pub fn add_remote_command_at(
        &self,
        device_id: &str,
        command: &RemoteCommand,
        when: types::Timestamp,
    ) -> ApiResult<bool> {
        self.store
            .storage
            .lock()
            .unwrap()
            .add_remote_tab_command_at(device_id, command, when)
    }

    // Remove all information about a command.
    #[error_support::handle_error(crate::Error)]
    pub fn remove_remote_command(
        &self,
        device_id: &str,
        command: &RemoteCommand,
    ) -> ApiResult<bool> {
        self.store
            .storage
            .lock()
            .unwrap()
            .remove_remote_tab_command(device_id, command)
    }

    #[error_support::handle_error(crate::Error)]
    pub fn get_unsent_commands(&self) -> ApiResult<Vec<PendingCommand>> {
        self.store.storage.lock().unwrap().get_unsent_commands()
    }

    #[error_support::handle_error(crate::Error)]
    pub fn set_pending_command_sent(&self, command: &PendingCommand) -> ApiResult<bool> {
        self.store
            .storage
            .lock()
            .unwrap()
            .set_pending_command_sent(command)
    }
}
