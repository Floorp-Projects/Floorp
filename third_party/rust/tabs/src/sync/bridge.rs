/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::sync::engine::TabsEngine;
use crate::TabsStore;
use anyhow::Result;
use std::sync::Arc;
use sync15::bso::{IncomingBso, OutgoingBso};
use sync15::engine::{BridgedEngine, BridgedEngineAdaptor};
use sync15::ServerTimestamp;
use sync_guid::Guid as SyncGuid;

impl TabsStore {
    // Returns a bridged sync engine for Desktop for this store.
    pub fn bridged_engine(self: Arc<Self>) -> Arc<TabsBridgedEngine> {
        let engine = TabsEngine::new(self);
        let bridged_engine = TabsBridgedEngineAdaptor { engine };
        Arc::new(TabsBridgedEngine::new(Box::new(bridged_engine)))
    }
}

/// A bridged engine implements all the methods needed to make the
/// `storage.sync` store work with Desktop's Sync implementation.
/// Conceptually it's very similar to our SyncEngine and there's a BridgedEngineAdaptor
/// trait we can implement to get a `BridgedEngine` from a `SyncEngine`, so that's
/// what we do. See also #2841, which will finally unify them completely.
struct TabsBridgedEngineAdaptor {
    engine: TabsEngine,
}

impl BridgedEngineAdaptor for TabsBridgedEngineAdaptor {
    fn last_sync(&self) -> Result<i64> {
        Ok(self.engine.get_last_sync()?.unwrap_or_default().as_millis())
    }

    fn set_last_sync(&self, last_sync_millis: i64) -> Result<()> {
        self.engine
            .set_last_sync(ServerTimestamp::from_millis(last_sync_millis))
    }

    fn engine(&self) -> &dyn sync15::engine::SyncEngine {
        &self.engine
    }
}

// This is for uniffi to expose, and does nothing than delegate back to the trait.
pub struct TabsBridgedEngine {
    bridge_impl: Box<dyn BridgedEngine>,
}

impl TabsBridgedEngine {
    pub fn new(bridge_impl: Box<dyn BridgedEngine>) -> Self {
        Self { bridge_impl }
    }

    pub fn last_sync(&self) -> Result<i64> {
        self.bridge_impl.last_sync()
    }

    pub fn set_last_sync(&self, last_sync: i64) -> Result<()> {
        self.bridge_impl.set_last_sync(last_sync)
    }

    pub fn sync_id(&self) -> Result<Option<String>> {
        self.bridge_impl.sync_id()
    }

    pub fn reset_sync_id(&self) -> Result<String> {
        self.bridge_impl.reset_sync_id()
    }

    pub fn ensure_current_sync_id(&self, sync_id: &str) -> Result<String> {
        self.bridge_impl.ensure_current_sync_id(sync_id)
    }

    pub fn prepare_for_sync(&self, client_data: &str) -> Result<()> {
        self.bridge_impl.prepare_for_sync(client_data)
    }

    pub fn sync_started(&self) -> Result<()> {
        self.bridge_impl.sync_started()
    }

    // Decode the JSON-encoded IncomingBso's that UniFFI passes to us
    fn convert_incoming_bsos(&self, incoming: Vec<String>) -> Result<Vec<IncomingBso>> {
        let mut bsos = Vec::with_capacity(incoming.len());
        for inc in incoming {
            bsos.push(serde_json::from_str::<IncomingBso>(&inc)?);
        }
        Ok(bsos)
    }

    // Encode OutgoingBso's into JSON for UniFFI
    fn convert_outgoing_bsos(&self, outgoing: Vec<OutgoingBso>) -> Result<Vec<String>> {
        let mut bsos = Vec::with_capacity(outgoing.len());
        for e in outgoing {
            bsos.push(serde_json::to_string(&e)?);
        }
        Ok(bsos)
    }

    pub fn store_incoming(&self, incoming: Vec<String>) -> Result<()> {
        self.bridge_impl
            .store_incoming(self.convert_incoming_bsos(incoming)?)
    }

    pub fn apply(&self) -> Result<Vec<String>> {
        let apply_results = self.bridge_impl.apply()?;
        self.convert_outgoing_bsos(apply_results.records)
    }

    pub fn set_uploaded(&self, server_modified_millis: i64, guids: Vec<SyncGuid>) -> Result<()> {
        self.bridge_impl
            .set_uploaded(server_modified_millis, &guids)
    }

    pub fn sync_finished(&self) -> Result<()> {
        self.bridge_impl.sync_finished()
    }

    pub fn reset(&self) -> Result<()> {
        self.bridge_impl.reset()
    }

    pub fn wipe(&self) -> Result<()> {
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
                last_used: 2,
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
