/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde_derive::{Deserialize, Serialize};
use sync15::Payload;

#[derive(Debug, Clone, Hash, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct TabsRecordTab {
    pub title: String,
    pub url_history: Vec<String>,
    pub icon: Option<String>,
    pub last_used: i64, // Seconds since epoch!
}

#[derive(Debug, Clone, Hash, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct TabsRecord {
    // `String` instead of `SyncGuid` because some IDs are FxA device ID (XXX - that doesn't
    // matter though - this could easily be a Guid!)
    pub id: String,
    pub client_name: String,
    pub tabs: Vec<TabsRecordTab>,
    #[serde(default)]
    pub ttl: u32,
}

impl TabsRecord {
    #[inline]
    pub fn from_payload(payload: Payload) -> crate::error::Result<Self> {
        let record: TabsRecord = payload.into_record()?;
        Ok(record)
    }
}

#[cfg(test)]
pub mod test {
    use super::*;
    use serde_json::json;

    #[test]
    fn test_simple() {
        let payload = Payload::from_json(json!({
            "id": "JkeBPC50ZI0m",
            "clientName": "client name",
            "tabs": [{
                "title": "the title",
                "urlHistory": [
                    "https://mozilla.org/"
                ],
                "icon": "https://mozilla.org/icon",
                "lastUsed": 1643764207
            }]
        }))
        .expect("json is valid");
        let record = TabsRecord::from_payload(payload).expect("payload is valid");
        assert_eq!(record.id, "JkeBPC50ZI0m");
        assert_eq!(record.client_name, "client name");
        assert_eq!(record.tabs.len(), 1);
        let tab = &record.tabs[0];
        assert_eq!(tab.title, "the title");
        assert_eq!(tab.icon, Some("https://mozilla.org/icon".to_string()));
        assert_eq!(tab.last_used, 1643764207);
    }

    #[test]
    fn test_extra_fields() {
        let payload = Payload::from_json(json!({
            "id": "JkeBPC50ZI0m",
            "clientName": "client name",
            "tabs": [],
            // Let's say we agree on new tabs to record, we want old versions to
            // ignore them!
            "recentlyClosed": [],
        }))
        .expect("json is valid");
        let record = TabsRecord::from_payload(payload).expect("payload is valid");
        assert_eq!(record.id, "JkeBPC50ZI0m");
    }
}
