/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::schema;
use crate::storage::{ClientRemoteTabs, RemoteTab, TABS_CLIENT_TTL};
use crate::store::TabsStore;
use crate::sync::record::{TabsRecord, TabsRecordTab};
use crate::Result;
use std::collections::HashMap;
use std::sync::{Arc, Mutex, Weak};
use sync15::bso::{IncomingBso, OutgoingBso, OutgoingEnvelope};
use sync15::engine::{
    CollSyncIds, CollectionRequest, EngineSyncAssociation, IncomingChangeset, OutgoingChangeset,
    SyncEngine, SyncEngineId,
};
use sync15::{telemetry, ClientData, DeviceType, RemoteClient, ServerTimestamp};
use sync_guid::Guid;

// Our "sync manager" will use whatever is stashed here.
lazy_static::lazy_static! {
    // Mutex: just taken long enough to update the inner stuff
    static ref STORE_FOR_MANAGER: Mutex<Weak<TabsStore>> = Mutex::new(Weak::new());
}

/// Called by the sync manager to get a sync engine via the store previously
/// registered with the sync manager.
pub fn get_registered_sync_engine(engine_id: &SyncEngineId) -> Option<Box<dyn SyncEngine>> {
    let weak = STORE_FOR_MANAGER.lock().unwrap();
    match weak.upgrade() {
        None => None,
        Some(store) => match engine_id {
            SyncEngineId::Tabs => Some(Box::new(TabsEngine::new(Arc::clone(&store)))),
            // panicing here seems reasonable - it's a static error if this
            // it hit, not something that runtime conditions can influence.
            _ => unreachable!("can't provide unknown engine: {}", engine_id),
        },
    }
}

impl ClientRemoteTabs {
    pub(crate) fn from_record_with_remote_client(
        client_id: String,
        last_modified: ServerTimestamp,
        remote_client: &RemoteClient,
        record: TabsRecord,
    ) -> Self {
        Self {
            client_id,
            client_name: remote_client.device_name.clone(),
            device_type: remote_client.device_type,
            last_modified: last_modified.as_millis(),
            remote_tabs: record.tabs.iter().map(RemoteTab::from_record_tab).collect(),
        }
    }

    // Note that this should die as part of https://github.com/mozilla/application-services/issues/5199
    // If we don't have a `RemoteClient` record, then we don't know whether the ID passed here is
    // the fxa_device_id (which is must be) or the client_id (which it will be if this ends up being
    // called for desktop records, where client_id != fxa_device_id)
    pub(crate) fn from_record(
        client_id: String,
        last_modified: ServerTimestamp,
        record: TabsRecord,
    ) -> Self {
        Self {
            client_id,
            client_name: record.client_name,
            device_type: DeviceType::Unknown,
            last_modified: last_modified.as_millis(),
            remote_tabs: record.tabs.iter().map(RemoteTab::from_record_tab).collect(),
        }
    }
    fn to_record(&self) -> TabsRecord {
        TabsRecord {
            id: self.client_id.clone(),
            client_name: self.client_name.clone(),
            tabs: self
                .remote_tabs
                .iter()
                .map(RemoteTab::to_record_tab)
                .collect(),
        }
    }
}

impl RemoteTab {
    pub(crate) fn from_record_tab(tab: &TabsRecordTab) -> Self {
        Self {
            title: tab.title.clone(),
            url_history: tab.url_history.clone(),
            icon: tab.icon.clone(),
            last_used: tab.last_used.checked_mul(1000).unwrap_or_default(),
        }
    }
    pub(super) fn to_record_tab(&self) -> TabsRecordTab {
        TabsRecordTab {
            title: self.title.clone(),
            url_history: self.url_history.clone(),
            icon: self.icon.clone(),
            last_used: self.last_used.checked_div(1000).unwrap_or_default(),
        }
    }
}

// This is the implementation of syncing, which is used by the 2 different "sync engines"
// (We hope to get these 2 engines even closer in the future, but for now, we suck this up)
pub struct TabsSyncImpl {
    pub(super) store: Arc<TabsStore>,
    pub(super) local_id: String,
}

impl TabsSyncImpl {
    pub fn new(store: Arc<TabsStore>) -> Self {
        Self {
            store,
            local_id: Default::default(),
        }
    }

    pub fn prepare_for_sync(&mut self, client_data: ClientData) -> Result<()> {
        let mut storage = self.store.storage.lock().unwrap();
        // We only know the client list at sync time, but need to return tabs potentially
        // at any time -- so we store the clients in the meta table to be able to properly
        // return a ClientRemoteTab struct
        storage.put_meta(
            schema::REMOTE_CLIENTS_KEY,
            &serde_json::to_string(&client_data.recent_clients)?,
        )?;
        self.local_id = client_data.local_client_id;
        Ok(())
    }

    pub fn apply_incoming(
        &mut self,
        inbound: Vec<IncomingBso>,
        telem: &mut telemetry::Engine,
    ) -> Result<Vec<OutgoingBso>> {
        let local_id = self.local_id.clone();
        let mut remote_tabs = Vec::with_capacity(inbound.len());
        let mut incoming_telemetry = telemetry::EngineIncoming::new();

        let remote_clients: HashMap<String, RemoteClient> = {
            let mut storage = self.store.storage.lock().unwrap();
            match storage.get_meta::<String>(schema::REMOTE_CLIENTS_KEY)? {
                None => HashMap::default(),
                Some(json) => serde_json::from_str(&json).unwrap(),
            }
        };
        for incoming in inbound {
            if incoming.envelope.id == local_id {
                // That's our own record, ignore it.
                continue;
            }
            let modified = incoming.envelope.modified;
            let record = match incoming.into_content::<TabsRecord>().content() {
                Some(record) => record,
                None => {
                    // Invalid record or a "tombstone" which tabs don't have.
                    log::warn!("Ignoring incoming invalid tab");
                    incoming_telemetry.failed(1);
                    continue;
                }
            };
            remote_tabs.push((record, modified));
        }

        // We want to keep the mutex for as short as possible
        let local_tabs = {
            let mut storage = self.store.storage.lock().unwrap();
            // In desktop we might end up here with zero records when doing a quick-write, in
            // which case we don't want to wipe the DB.
            if !remote_tabs.is_empty() {
                storage.replace_remote_tabs(remote_tabs)?;
            }
            storage.remove_stale_clients()?;
            storage.prepare_local_tabs_for_upload()
        };
        let outgoing = if let Some(local_tabs) = local_tabs {
            let (client_name, device_type) = remote_clients
                .get(&local_id)
                .map(|client| (client.device_name.clone(), client.device_type))
                .unwrap_or_else(|| (String::new(), DeviceType::Unknown));
            let local_record = ClientRemoteTabs {
                client_id: local_id.clone(),
                client_name,
                device_type,
                last_modified: 0, // ignored for outgoing records.
                remote_tabs: local_tabs.to_vec(),
            };
            log::trace!("outgoing {:?}", local_record);
            let envelope = OutgoingEnvelope {
                id: local_id.into(),
                ttl: Some(TABS_CLIENT_TTL),
                ..Default::default()
            };
            vec![OutgoingBso::from_content(
                envelope,
                local_record.to_record(),
            )?]
        } else {
            vec![]
        };
        telem.incoming(incoming_telemetry);
        Ok(outgoing)
    }

    pub fn sync_finished(
        &mut self,
        new_timestamp: ServerTimestamp,
        records_synced: &[Guid],
    ) -> Result<()> {
        log::info!(
            "sync completed after uploading {} records",
            records_synced.len()
        );
        self.set_last_sync(new_timestamp)?;
        Ok(())
    }

    pub fn reset(&mut self, assoc: &EngineSyncAssociation) -> Result<()> {
        self.set_last_sync(ServerTimestamp(0))?;
        let mut storage = self.store.storage.lock().unwrap();
        storage.delete_meta(schema::REMOTE_CLIENTS_KEY)?;
        storage.wipe_remote_tabs()?;
        match assoc {
            EngineSyncAssociation::Disconnected => {
                storage.delete_meta(schema::GLOBAL_SYNCID_META_KEY)?;
                storage.delete_meta(schema::COLLECTION_SYNCID_META_KEY)?;
            }
            EngineSyncAssociation::Connected(ids) => {
                storage.put_meta(schema::GLOBAL_SYNCID_META_KEY, &ids.global.to_string())?;
                storage.put_meta(schema::COLLECTION_SYNCID_META_KEY, &ids.coll.to_string())?;
            }
        };
        Ok(())
    }

    pub fn wipe(&mut self) -> Result<()> {
        self.reset(&EngineSyncAssociation::Disconnected)?;
        // not clear why we need to wipe the local tabs - the app is just going
        // to re-add them?
        self.store.storage.lock().unwrap().wipe_local_tabs();
        Ok(())
    }

    pub fn get_sync_assoc(&self) -> Result<EngineSyncAssociation> {
        let mut storage = self.store.storage.lock().unwrap();
        let global = storage.get_meta::<String>(schema::GLOBAL_SYNCID_META_KEY)?;
        let coll = storage.get_meta::<String>(schema::COLLECTION_SYNCID_META_KEY)?;
        Ok(if let (Some(global), Some(coll)) = (global, coll) {
            EngineSyncAssociation::Connected(CollSyncIds {
                global: Guid::from_string(global),
                coll: Guid::from_string(coll),
            })
        } else {
            EngineSyncAssociation::Disconnected
        })
    }

    pub fn set_last_sync(&self, last_sync: ServerTimestamp) -> Result<()> {
        let mut storage = self.store.storage.lock().unwrap();
        log::debug!("Updating last sync to {}", last_sync);
        let last_sync_millis = last_sync.as_millis();
        storage.put_meta(schema::LAST_SYNC_META_KEY, &last_sync_millis)
    }

    pub fn get_last_sync(&self) -> Result<Option<ServerTimestamp>> {
        let mut storage = self.store.storage.lock().unwrap();
        let millis = storage.get_meta::<i64>(schema::LAST_SYNC_META_KEY)?;
        Ok(millis.map(ServerTimestamp))
    }
}

// This is the "SyncEngine" used when syncing via the Sync Manager.
pub struct TabsEngine {
    pub sync_impl: Mutex<TabsSyncImpl>,
}

impl TabsEngine {
    pub fn new(store: Arc<TabsStore>) -> Self {
        Self {
            sync_impl: Mutex::new(TabsSyncImpl::new(store)),
        }
    }
}

impl SyncEngine for TabsEngine {
    fn collection_name(&self) -> std::borrow::Cow<'static, str> {
        "tabs".into()
    }

    fn prepare_for_sync(&self, get_client_data: &dyn Fn() -> ClientData) -> anyhow::Result<()> {
        Ok(self
            .sync_impl
            .lock()
            .unwrap()
            .prepare_for_sync(get_client_data())?)
    }

    fn apply_incoming(
        &self,
        inbound: Vec<IncomingChangeset>,
        telem: &mut telemetry::Engine,
    ) -> anyhow::Result<OutgoingChangeset> {
        assert_eq!(inbound.len(), 1, "only requested one set of records");
        let inbound = inbound.into_iter().next().unwrap();
        let outgoing_records = self
            .sync_impl
            .lock()
            .unwrap()
            .apply_incoming(inbound.changes, telem)?;

        Ok(OutgoingChangeset::new_with_changes(
            "tabs",
            inbound.timestamp,
            outgoing_records,
        ))
    }

    fn sync_finished(
        &self,
        new_timestamp: ServerTimestamp,
        records_synced: Vec<Guid>,
    ) -> anyhow::Result<()> {
        Ok(self
            .sync_impl
            .lock()
            .unwrap()
            .sync_finished(new_timestamp, &records_synced)?)
    }

    fn get_collection_requests(
        &self,
        server_timestamp: ServerTimestamp,
    ) -> anyhow::Result<Vec<CollectionRequest>> {
        let since = self
            .sync_impl
            .lock()
            .unwrap()
            .get_last_sync()?
            .unwrap_or_default();
        Ok(if since == server_timestamp {
            vec![]
        } else {
            vec![CollectionRequest::new("tabs").full().newer_than(since)]
        })
    }

    fn get_sync_assoc(&self) -> anyhow::Result<EngineSyncAssociation> {
        Ok(self.sync_impl.lock().unwrap().get_sync_assoc()?)
    }

    fn reset(&self, assoc: &EngineSyncAssociation) -> anyhow::Result<()> {
        Ok(self.sync_impl.lock().unwrap().reset(assoc)?)
    }

    fn wipe(&self) -> anyhow::Result<()> {
        Ok(self.sync_impl.lock().unwrap().wipe()?)
    }
}

impl crate::TabsStore {
    // This allows the embedding app to say "make this instance available to
    // the sync manager". The implementation is more like "offer to sync mgr"
    // (thereby avoiding us needing to link with the sync manager) but
    // `register_with_sync_manager()` is logically what's happening so that's
    // the name it gets.
    pub fn register_with_sync_manager(self: Arc<Self>) {
        let mut state = STORE_FOR_MANAGER.lock().unwrap();
        *state = Arc::downgrade(&self);
    }
}

#[cfg(test)]
pub mod test {
    use super::*;
    use serde_json::json;
    use sync15::bso::IncomingBso;

    #[test]
    fn test_incoming_tabs() {
        env_logger::try_init().ok();

        let engine = TabsEngine::new(Arc::new(TabsStore::new_with_mem_path("test-incoming")));

        let records = vec![
            json!({
                "id": "device-no-tabs",
                "clientName": "device with no tabs",
                "tabs": [],
            }),
            json!({
                "id": "device-with-a-tab",
                "clientName": "device with a tab",
                "tabs": [{
                    "title": "the title",
                    "urlHistory": [
                        "https://mozilla.org/"
                    ],
                    "icon": "https://mozilla.org/icon",
                    "lastUsed": 1643764207
                }]
            }),
            // test an updated payload will replace the previous record
            json!({
                "id": "device-with-a-tab",
                "clientName": "device with an updated tab",
                "tabs": [{
                    "title": "the new title",
                    "urlHistory": [
                        "https://mozilla.org/"
                    ],
                    "icon": "https://mozilla.org/icon",
                    "lastUsed": 1643764208
                }]
            }),
            // This has the main payload as OK but the tabs part invalid.
            json!({
                "id": "device-with-invalid-tab",
                "clientName": "device with a tab",
                "tabs": [{
                    "foo": "bar",
                }]
            }),
            // We want this to be a valid payload but an invalid tab - so it needs an ID.
            json!({
                "id": "invalid-tab",
                "foo": "bar"
            }),
        ];

        let incoming = IncomingChangeset::new_with_changes(
            engine.collection_name(),
            ServerTimestamp(0),
            records
                .into_iter()
                .map(IncomingBso::from_test_content)
                .collect(),
        );
        let outgoing = engine
            .apply_incoming(vec![incoming], &mut telemetry::Engine::new("tabs"))
            .expect("Should apply incoming and stage outgoing records");

        assert!(outgoing.changes.is_empty());

        // now check the store has what we think it has.
        let sync_impl = engine.sync_impl.lock().unwrap();
        let mut storage = sync_impl.store.storage.lock().unwrap();
        let mut crts = storage.get_remote_tabs().expect("should work");
        crts.sort_by(|a, b| a.client_name.partial_cmp(&b.client_name).unwrap());
        assert_eq!(crts.len(), 2, "we currently include devices with no tabs");
        let crt = &crts[0];
        assert_eq!(crt.client_name, "device with an updated tab");
        assert_eq!(crt.device_type, DeviceType::Unknown);
        assert_eq!(crt.remote_tabs.len(), 1);
        assert_eq!(crt.remote_tabs[0].title, "the new title");

        let crt = &crts[1];
        assert_eq!(crt.client_name, "device with no tabs");
        assert_eq!(crt.device_type, DeviceType::Unknown);
        assert_eq!(crt.remote_tabs.len(), 0);
    }

    #[test]
    fn test_no_incoming_doesnt_write() {
        env_logger::try_init().ok();

        let engine = TabsEngine::new(Arc::new(TabsStore::new_with_mem_path(
            "test_no_incoming_doesnt_write",
        )));

        let records = vec![json!({
            "id": "device-with-a-tab",
            "clientName": "device with a tab",
            "tabs": [{
                "title": "the title",
                "urlHistory": [
                    "https://mozilla.org/"
                ],
                "icon": "https://mozilla.org/icon",
                "lastUsed": 1643764207
            }]
        })];

        let incoming = IncomingChangeset::new_with_changes(
            engine.collection_name(),
            ServerTimestamp(0),
            records
                .into_iter()
                .map(IncomingBso::from_test_content)
                .collect(),
        );
        engine
            .apply_incoming(vec![incoming], &mut telemetry::Engine::new("tabs"))
            .expect("Should apply incoming and stage outgoing records");

        // now check the store has what we think it has.
        {
            let sync_impl = engine.sync_impl.lock().unwrap();
            let mut storage = sync_impl.store.storage.lock().unwrap();
            assert_eq!(storage.get_remote_tabs().expect("should work").len(), 1);
        }

        // Now another sync with zero incoming records, should still be able to get back
        // our one client.
        let incoming = IncomingChangeset::new_with_changes(
            engine.collection_name(),
            ServerTimestamp(0),
            vec![],
        );
        engine
            .apply_incoming(vec![incoming], &mut telemetry::Engine::new("tabs"))
            .expect("Should succeed applying zero records");

        {
            let sync_impl = engine.sync_impl.lock().unwrap();
            let mut storage = sync_impl.store.storage.lock().unwrap();
            assert_eq!(storage.get_remote_tabs().expect("should work").len(), 1);
        }
    }

    #[test]
    fn test_sync_manager_registration() {
        let store = Arc::new(TabsStore::new_with_mem_path("test-registration"));
        assert_eq!(Arc::strong_count(&store), 1);
        assert_eq!(Arc::weak_count(&store), 0);
        Arc::clone(&store).register_with_sync_manager();
        assert_eq!(Arc::strong_count(&store), 1);
        assert_eq!(Arc::weak_count(&store), 1);
        let registered = STORE_FOR_MANAGER
            .lock()
            .unwrap()
            .upgrade()
            .expect("should upgrade");
        assert!(Arc::ptr_eq(&store, &registered));
        drop(registered);
        // should be no new references
        assert_eq!(Arc::strong_count(&store), 1);
        assert_eq!(Arc::weak_count(&store), 1);
        // dropping the registered object should drop the registration.
        drop(store);
        assert!(STORE_FOR_MANAGER.lock().unwrap().upgrade().is_none());
    }
}
