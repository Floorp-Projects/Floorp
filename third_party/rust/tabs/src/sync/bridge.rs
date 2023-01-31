/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::sync::{Arc, Mutex};

use crate::error::{ApiResult, Result, TabsApiError};
use crate::sync::engine::TabsSyncImpl;
use crate::TabsStore;
use error_support::handle_error;
use sync15::bso::{IncomingBso, OutgoingBso};
use sync15::engine::{ApplyResults, BridgedEngine, CollSyncIds, EngineSyncAssociation};
use sync15::{ClientData, ServerTimestamp};
use sync_guid::Guid as SyncGuid;

impl TabsStore {
    // Returns a bridged sync engine for Desktop for this store.
    pub fn bridged_engine(self: Arc<Self>) -> Arc<TabsBridgedEngine> {
        let bridge_impl = crate::sync::bridge::BridgedEngineImpl::new(&self);
        // This is a concrete struct exposed via uniffi.
        let concrete = TabsBridgedEngine::new(bridge_impl);
        Arc::new(concrete)
    }
}

/// A bridged engine implements all the methods needed to make the
/// `storage.sync` store work with Desktop's Sync implementation.
/// Conceptually it's very similar to our SyncEngine, and once we have SyncEngine
/// and BridgedEngine using the same types we can probably combine them (or at least
/// implement this bridged engine purely in terms of SyncEngine)
/// See also #2841 and #5139
pub struct BridgedEngineImpl {
    sync_impl: Mutex<TabsSyncImpl>,
    incoming: Mutex<Vec<IncomingBso>>,
}

impl BridgedEngineImpl {
    /// Creates a bridged engine for syncing.
    pub fn new(store: &Arc<TabsStore>) -> Self {
        Self {
            sync_impl: Mutex::new(TabsSyncImpl::new(store.clone())),
            incoming: Mutex::default(),
        }
    }
}

impl BridgedEngine for BridgedEngineImpl {
    type Error = TabsApiError;

    #[handle_error]
    fn last_sync(&self) -> ApiResult<i64> {
        Ok(self
            .sync_impl
            .lock()
            .unwrap()
            .get_last_sync()?
            .unwrap_or_default()
            .as_millis())
    }

    #[handle_error]
    fn set_last_sync(&self, last_sync_millis: i64) -> ApiResult<()> {
        self.sync_impl
            .lock()
            .unwrap()
            .set_last_sync(ServerTimestamp::from_millis(last_sync_millis))?;
        Ok(())
    }

    #[handle_error]
    fn sync_id(&self) -> ApiResult<Option<String>> {
        Ok(match self.sync_impl.lock().unwrap().get_sync_assoc()? {
            EngineSyncAssociation::Connected(id) => Some(id.coll.to_string()),
            EngineSyncAssociation::Disconnected => None,
        })
    }

    #[handle_error]
    fn reset_sync_id(&self) -> ApiResult<String> {
        let new_id = SyncGuid::random().to_string();
        let new_coll_ids = CollSyncIds {
            global: SyncGuid::empty(),
            coll: new_id.clone().into(),
        };
        self.sync_impl
            .lock()
            .unwrap()
            .reset(&EngineSyncAssociation::Connected(new_coll_ids))?;
        Ok(new_id)
    }

    #[handle_error]
    fn ensure_current_sync_id(&self, sync_id: &str) -> ApiResult<String> {
        let mut sync_impl = self.sync_impl.lock().unwrap();
        let assoc = sync_impl.get_sync_assoc()?;
        if matches!(assoc, EngineSyncAssociation::Connected(c) if c.coll == sync_id) {
            log::debug!("ensure_current_sync_id is current");
        } else {
            let new_coll_ids = CollSyncIds {
                global: SyncGuid::empty(),
                coll: sync_id.into(),
            };
            sync_impl.reset(&EngineSyncAssociation::Connected(new_coll_ids))?;
        }
        Ok(sync_id.to_string()) // this is a bit odd, why the result?
    }

    #[handle_error]
    fn prepare_for_sync(&self, client_data: &str) -> ApiResult<()> {
        let data: ClientData = serde_json::from_str(client_data)?;
        self.sync_impl.lock().unwrap().prepare_for_sync(data)
    }

    fn sync_started(&self) -> ApiResult<()> {
        // This is a no-op for the Tabs Engine
        Ok(())
    }

    #[handle_error]
    fn store_incoming(&self, incoming: Vec<IncomingBso>) -> ApiResult<()> {
        // Store the incoming payload in memory so we can use it in apply
        *(self.incoming.lock().unwrap()) = incoming;
        Ok(())
    }

    #[handle_error]
    fn apply(&self) -> ApiResult<ApplyResults> {
        let mut incoming = self.incoming.lock().unwrap();
        // We've a reference to a Vec<> but it's owned by the mutex - swap the mutex owned
        // value for an empty vec so we can consume the original.
        let mut records = Vec::new();
        std::mem::swap(&mut records, &mut *incoming);
        let mut telem = sync15::telemetry::Engine::new("tabs");

        let mut sync_impl = self.sync_impl.lock().unwrap();
        let outgoing = sync_impl.apply_incoming(records, &mut telem)?;

        Ok(ApplyResults {
            records: outgoing,
            num_reconciled: Some(0),
        })
    }

    #[handle_error]
    fn set_uploaded(&self, server_modified_millis: i64, ids: &[SyncGuid]) -> ApiResult<()> {
        self.sync_impl
            .lock()
            .unwrap()
            .sync_finished(ServerTimestamp::from_millis(server_modified_millis), ids)
    }

    #[handle_error]
    fn sync_finished(&self) -> ApiResult<()> {
        *(self.incoming.lock().unwrap()) = Vec::default();
        Ok(())
    }

    #[handle_error]
    fn reset(&self) -> ApiResult<()> {
        self.sync_impl
            .lock()
            .unwrap()
            .reset(&EngineSyncAssociation::Disconnected)?;
        Ok(())
    }

    #[handle_error]
    fn wipe(&self) -> ApiResult<()> {
        self.sync_impl.lock().unwrap().wipe()?;
        Ok(())
    }
}

// This is for uniffi to expose, and does nothing than delegate back to the trait.
pub struct TabsBridgedEngine {
    bridge_impl: BridgedEngineImpl,
}

impl TabsBridgedEngine {
    pub fn new(bridge_impl: BridgedEngineImpl) -> Self {
        Self { bridge_impl }
    }

    pub fn last_sync(&self) -> ApiResult<i64> {
        self.bridge_impl.last_sync()
    }

    pub fn set_last_sync(&self, last_sync: i64) -> ApiResult<()> {
        self.bridge_impl.set_last_sync(last_sync)
    }

    pub fn sync_id(&self) -> ApiResult<Option<String>> {
        self.bridge_impl.sync_id()
    }

    pub fn reset_sync_id(&self) -> ApiResult<String> {
        self.bridge_impl.reset_sync_id()
    }

    pub fn ensure_current_sync_id(&self, sync_id: &str) -> ApiResult<String> {
        self.bridge_impl.ensure_current_sync_id(sync_id)
    }

    pub fn prepare_for_sync(&self, client_data: &str) -> ApiResult<()> {
        self.bridge_impl.prepare_for_sync(client_data)
    }

    pub fn sync_started(&self) -> ApiResult<()> {
        self.bridge_impl.sync_started()
    }

    // Decode the JSON-encoded IncomingBso's that UniFFI passes to us
    #[handle_error]
    fn convert_incoming_bsos(&self, incoming: Vec<String>) -> ApiResult<Vec<IncomingBso>> {
        let mut bsos = Vec::with_capacity(incoming.len());
        for inc in incoming {
            bsos.push(serde_json::from_str::<IncomingBso>(&inc)?);
        }
        Ok(bsos)
    }

    // Encode OutgoingBso's into JSON for UniFFI
    #[handle_error]
    fn convert_outgoing_bsos(&self, outgoing: Vec<OutgoingBso>) -> ApiResult<Vec<String>> {
        let mut bsos = Vec::with_capacity(outgoing.len());
        for e in outgoing {
            bsos.push(serde_json::to_string(&e)?);
        }
        Ok(bsos)
    }

    pub fn store_incoming(&self, incoming: Vec<String>) -> ApiResult<()> {
        self.bridge_impl
            .store_incoming(self.convert_incoming_bsos(incoming)?)
    }

    pub fn apply(&self) -> ApiResult<Vec<String>> {
        let apply_results = self.bridge_impl.apply()?;
        self.convert_outgoing_bsos(apply_results.records)
    }

    pub fn set_uploaded(&self, server_modified_millis: i64, guids: Vec<SyncGuid>) -> ApiResult<()> {
        self.bridge_impl
            .set_uploaded(server_modified_millis, &guids)
    }

    pub fn sync_finished(&self) -> ApiResult<()> {
        self.bridge_impl.sync_finished()
    }

    pub fn reset(&self) -> ApiResult<()> {
        self.bridge_impl.reset()
    }

    pub fn wipe(&self) -> ApiResult<()> {
        self.bridge_impl.wipe()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::storage::{RemoteTab, TABS_CLIENT_TTL};
    use crate::sync::record::TabsRecordTab;
    use serde_json::json;
    use std::collections::HashMap;
    use sync15::{ClientData, DeviceType, RemoteClient};

    // A copy of the normal "engine" tests but which go via the bridge
    #[test]
    fn test_sync_via_bridge() {
        env_logger::try_init().ok();

        let store = Arc::new(TabsStore::new_with_mem_path("test-bridge_incoming"));

        // Set some local tabs for our device.
        let my_tabs = vec![
            RemoteTab {
                title: "my first tab".to_string(),
                url_history: vec!["http://1.com".to_string()],
                icon: None,
                last_used: 0,
            },
            RemoteTab {
                title: "my second tab".to_string(),
                url_history: vec!["http://2.com".to_string()],
                icon: None,
                last_used: 1,
            },
        ];
        store.set_local_tabs(my_tabs.clone());

        let bridge = store.bridged_engine();

        let client_data = ClientData {
            local_client_id: "my-device".to_string(),
            recent_clients: HashMap::from([
                (
                    "my-device".to_string(),
                    RemoteClient {
                        fxa_device_id: None,
                        device_name: "my device".to_string(),
                        device_type: sync15::DeviceType::Unknown,
                    },
                ),
                (
                    "device-no-tabs".to_string(),
                    RemoteClient {
                        fxa_device_id: None,
                        device_name: "device with no tabs".to_string(),
                        device_type: DeviceType::Unknown,
                    },
                ),
                (
                    "device-with-a-tab".to_string(),
                    RemoteClient {
                        fxa_device_id: None,
                        device_name: "device with a tab".to_string(),
                        device_type: DeviceType::Unknown,
                    },
                ),
            ]),
        };
        bridge
            .prepare_for_sync(&serde_json::to_string(&client_data).unwrap())
            .expect("should work");

        let records = vec![
            // my-device should be ignored by sync - here it is acting as what our engine last
            // wrote, but the actual tabs in our store we set above are what should be used.
            json!({
                "id": "my-device",
                "clientName": "my device",
                "tabs": [{
                    "title": "the title",
                    "urlHistory": [
                        "https://mozilla.org/"
                    ],
                    "icon": "https://mozilla.org/icon",
                    "lastUsed": 1643764207
                }]
            }),
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

        let mut incoming = Vec::new();
        for record in records {
            // Annoyingly we can't use `IncomingEnvelope` directly as it intentionally doesn't
            // support Serialize - so need to use explicit json.
            let envelope = json!({
                "id": record.get("id"),
                "modified": 0,
                "payload": serde_json::to_string(&record).unwrap(),
            });
            incoming.push(serde_json::to_string(&envelope).unwrap());
        }

        bridge.store_incoming(incoming).expect("should store");

        let out = bridge.apply().expect("should apply");

        assert_eq!(out.len(), 1);
        let ours = serde_json::from_str::<serde_json::Value>(&out[0]).unwrap();
        // As above, can't use `OutgoingEnvelope` as it doesn't Deserialize.
        // First, convert my_tabs from the local `RemoteTab` to the Sync specific `TabsRecord`
        let expected_tabs: Vec<TabsRecordTab> =
            my_tabs.into_iter().map(|t| t.to_record_tab()).collect();
        let expected = json!({
            "id": "my-device".to_string(),
            "payload": json!({
                "id": "my-device".to_string(),
                "clientName": "my device",
                "tabs": serde_json::to_value(expected_tabs).unwrap(),
            }).to_string(),
            "ttl": TABS_CLIENT_TTL,
        });

        assert_eq!(ours, expected);
        bridge.set_uploaded(1234, vec![]).unwrap();
        assert_eq!(bridge.last_sync().unwrap(), 1234);
    }

    #[test]
    fn test_sync_meta() {
        env_logger::try_init().ok();

        let store = Arc::new(TabsStore::new_with_mem_path("test-meta"));
        let bridge = store.bridged_engine();

        // Should not error or panic
        assert_eq!(bridge.last_sync().unwrap(), 0);
        bridge.set_last_sync(3).unwrap();
        assert_eq!(bridge.last_sync().unwrap(), 3);

        assert!(bridge.sync_id().unwrap().is_none());

        bridge.ensure_current_sync_id("some_guid").unwrap();
        assert_eq!(bridge.sync_id().unwrap(), Some("some_guid".to_string()));
        // changing the sync ID should reset the timestamp
        assert_eq!(bridge.last_sync().unwrap(), 0);
        bridge.set_last_sync(3).unwrap();

        bridge.reset_sync_id().unwrap();
        // should now be a random guid.
        assert_ne!(bridge.sync_id().unwrap(), Some("some_guid".to_string()));
        // should have reset the last sync timestamp.
        assert_eq!(bridge.last_sync().unwrap(), 0);
        bridge.set_last_sync(3).unwrap();

        // `reset` clears the guid and the timestamp
        bridge.reset().unwrap();
        assert_eq!(bridge.last_sync().unwrap(), 0);
        assert!(bridge.sync_id().unwrap().is_none());
    }
}
