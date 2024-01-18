/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde_derive::{Deserialize, Serialize};

// copy/pasta...
fn skip_if_default<T: PartialEq + Default>(v: &T) -> bool {
    *v == T::default()
}

#[derive(Debug, Clone, Hash, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct TabsRecordTab {
    pub title: String,
    pub url_history: Vec<String>,
    pub icon: Option<String>,
    pub last_used: i64, // Seconds since epoch!
    #[serde(default, skip_serializing_if = "skip_if_default")]
    pub inactive: bool,
}

#[derive(Debug, Clone, Hash, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
// This struct mirrors what is stored on the server
pub struct TabsRecord {
    // `String` instead of `SyncGuid` because some IDs are FxA device ID (XXX - that doesn't
    // matter though - this could easily be a Guid!)
    pub id: String,
    pub client_name: String,
    pub tabs: Vec<TabsRecordTab>,
}

#[cfg(test)]
pub mod test {
    use super::*;
    use serde_json::json;

    #[test]
    fn test_payload() {
        let payload = json!({
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
        });
        let record: TabsRecord = serde_json::from_value(payload).expect("should work");
        assert_eq!(record.id, "JkeBPC50ZI0m");
        assert_eq!(record.client_name, "client name");
        assert_eq!(record.tabs.len(), 1);
        let tab = &record.tabs[0];
        assert_eq!(tab.title, "the title");
        assert_eq!(tab.icon, Some("https://mozilla.org/icon".to_string()));
        assert_eq!(tab.last_used, 1643764207);
        assert!(!tab.inactive);
    }

    #[test]
    fn test_roundtrip() {
        let tab = TabsRecord {
            id: "JkeBPC50ZI0m".into(),
            client_name: "client name".into(),
            tabs: vec![TabsRecordTab {
                title: "the title".into(),
                url_history: vec!["https://mozilla.org/".into()],
                icon: Some("https://mozilla.org/icon".into()),
                last_used: 1643764207,
                inactive: true,
            }],
        };
        let round_tripped =
            serde_json::from_value(serde_json::to_value(tab.clone()).unwrap()).unwrap();
        assert_eq!(tab, round_tripped);
    }

    #[test]
    fn test_extra_fields() {
        let payload = json!({
            "id": "JkeBPC50ZI0m",
            // Let's say we agree on new tabs to record, we want old versions to
            // ignore them!
            "ignoredField": "??",
            "clientName": "client name",
            "tabs": [{
                "title": "the title",
                "urlHistory": [
                    "https://mozilla.org/"
                ],
                "icon": "https://mozilla.org/icon",
                "lastUsed": 1643764207,
                // Ditto - make sure we ignore unexpected fields in each tab.
                "ignoredField": "??",
            }]
        });
        let record: TabsRecord = serde_json::from_value(payload).unwrap();
        // The point of this test is really just to ensure the deser worked, so
        // just check the ID.
        assert_eq!(record.id, "JkeBPC50ZI0m");
    }
}
