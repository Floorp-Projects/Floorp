/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod bridge;
mod incoming;
mod outgoing;

#[cfg(test)]
mod sync_tests;

use crate::api::{StorageChanges, StorageValueChange};
use crate::db::StorageDb;
use crate::error::*;
use serde::Deserialize;
use serde_derive::*;
use sql_support::ConnExt;
use sync_guid::Guid as SyncGuid;

pub use bridge::BridgedEngine;
use incoming::IncomingAction;

type JsonMap = serde_json::Map<String, serde_json::Value>;

pub const STORAGE_VERSION: usize = 1;

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct WebextRecord {
    #[serde(rename = "id")]
    guid: SyncGuid,
    #[serde(rename = "extId")]
    ext_id: String,
    data: String,
}

// Perform a 2-way or 3-way merge, where the incoming value wins on conflict.
fn merge(
    ext_id: String,
    mut other: JsonMap,
    mut ours: JsonMap,
    parent: Option<JsonMap>,
) -> IncomingAction {
    if other == ours {
        return IncomingAction::Same { ext_id };
    }
    let old_incoming = other.clone();
    // worst case is keys in each are unique.
    let mut changes = StorageChanges::with_capacity(other.len() + ours.len());
    if let Some(parent) = parent {
        // Perform 3-way merge. First, for every key in parent,
        // compare the parent value with the incoming value to compute
        // an implicit "diff".
        for (key, parent_value) in parent.into_iter() {
            if let Some(incoming_value) = other.remove(&key) {
                if incoming_value != parent_value {
                    log::trace!(
                        "merge: key {} was updated in incoming - copying value locally",
                        key
                    );
                    let old_value = ours.remove(&key);
                    let new_value = Some(incoming_value.clone());
                    if old_value != new_value {
                        changes.push(StorageValueChange {
                            key: key.clone(),
                            old_value,
                            new_value,
                        });
                    }
                    ours.insert(key, incoming_value);
                }
            } else {
                // Key was not present in incoming value.
                // Another client must have deleted it.
                log::trace!(
                    "merge: key {} no longer present in incoming - removing it locally",
                    key
                );
                if let Some(old_value) = ours.remove(&key) {
                    changes.push(StorageValueChange {
                        key,
                        old_value: Some(old_value),
                        new_value: None,
                    });
                }
            }
        }

        // Then, go through every remaining key in incoming. These are
        // the ones where a corresponding key does not exist in
        // parent, so it is a new key, and we need to add it.
        for (key, incoming_value) in other.into_iter() {
            log::trace!(
                "merge: key {} doesn't occur in parent - copying from incoming",
                key
            );
            changes.push(StorageValueChange {
                key: key.clone(),
                old_value: None,
                new_value: Some(incoming_value.clone()),
            });
            ours.insert(key, incoming_value);
        }
    } else {
        // No parent. Server wins. Overwrite every key in ours with
        // the corresponding value in other.
        log::trace!("merge: no parent - copying all keys from incoming");
        for (key, incoming_value) in other.into_iter() {
            let old_value = ours.remove(&key);
            let new_value = Some(incoming_value.clone());
            if old_value != new_value {
                changes.push(StorageValueChange {
                    key: key.clone(),
                    old_value,
                    new_value,
                });
            }
            ours.insert(key, incoming_value);
        }
    }

    if ours == old_incoming {
        IncomingAction::TakeRemote {
            ext_id,
            data: old_incoming,
            changes,
        }
    } else {
        IncomingAction::Merge {
            ext_id,
            data: ours,
            changes,
        }
    }
}

fn remove_matching_keys(mut ours: JsonMap, keys_to_remove: &JsonMap) -> (JsonMap, StorageChanges) {
    let mut changes = StorageChanges::with_capacity(keys_to_remove.len());
    for key in keys_to_remove.keys() {
        if let Some(old_value) = ours.remove(key) {
            changes.push(StorageValueChange {
                key: key.clone(),
                old_value: Some(old_value),
                new_value: None,
            });
        }
    }
    (ours, changes)
}

/// Holds a JSON-serialized map of all synced changes for an extension.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SyncedExtensionChange {
    /// The extension ID.
    pub ext_id: String,
    /// The contents of a `StorageChanges` struct, in JSON format. We don't
    /// deserialize these because they need to be passed back to the browser
    /// as strings anyway.
    pub changes: String,
}

// Fetches the applied changes we stashed in the storage_sync_applied table.
pub fn get_synced_changes(db: &StorageDb) -> Result<Vec<SyncedExtensionChange>> {
    let signal = db.begin_interrupt_scope()?;
    let sql = "SELECT ext_id, changes FROM temp.storage_sync_applied";
    db.conn().query_rows_and_then(sql, [], |row| -> Result<_> {
        signal.err_if_interrupted()?;
        Ok(SyncedExtensionChange {
            ext_id: row.get("ext_id")?,
            changes: row.get("changes")?,
        })
    })
}

// Helpers for tests
#[cfg(test)]
pub mod test {
    use crate::db::{test::new_mem_db, StorageDb};
    use crate::schema::create_empty_sync_temp_tables;

    pub fn new_syncable_mem_db() -> StorageDb {
        let _ = env_logger::try_init();
        let db = new_mem_db();
        create_empty_sync_temp_tables(&db).expect("should work");
        db
    }
}

#[cfg(test)]
mod tests {
    use super::test::new_syncable_mem_db;
    use super::*;
    use serde_json::json;

    #[test]
    fn test_serde_record_ser() {
        assert_eq!(
            serde_json::to_string(&WebextRecord {
                guid: "guid".into(),
                ext_id: "ext_id".to_string(),
                data: "data".to_string()
            })
            .unwrap(),
            r#"{"id":"guid","extId":"ext_id","data":"data"}"#
        );
    }

    // a macro for these tests - constructs a serde_json::Value::Object
    macro_rules! map {
        ($($map:tt)+) => {
            json!($($map)+).as_object().unwrap().clone()
        };
    }

    macro_rules! change {
        ($key:literal, None, None) => {
            StorageValueChange {
                key: $key.to_string(),
                old_value: None,
                new_value: None,
            };
        };
        ($key:literal, $old:tt, None) => {
            StorageValueChange {
                key: $key.to_string(),
                old_value: Some(json!($old)),
                new_value: None,
            }
        };
        ($key:literal, None, $new:tt) => {
            StorageValueChange {
                key: $key.to_string(),
                old_value: None,
                new_value: Some(json!($new)),
            }
        };
        ($key:literal, $old:tt, $new:tt) => {
            StorageValueChange {
                key: $key.to_string(),
                old_value: Some(json!($old)),
                new_value: Some(json!($new)),
            }
        };
    }
    macro_rules! changes {
        ( ) => {
            StorageChanges::new()
        };
        ( $( $change:expr ),* ) => {
            {
                let mut changes = StorageChanges::new();
                $(
                    changes.push($change);
                )*
                changes
            }
        };
    }

    #[test]
    fn test_3way_merging() {
        // No conflict - identical local and remote.
        assert_eq!(
            merge(
                "ext-id".to_string(),
                map!({"one": "one", "two": "two"}),
                map!({"two": "two", "one": "one"}),
                Some(map!({"parent_only": "parent"})),
            ),
            IncomingAction::Same {
                ext_id: "ext-id".to_string()
            }
        );
        assert_eq!(
            merge(
                "ext-id".to_string(),
                map!({"other_only": "other", "common": "common"}),
                map!({"ours_only": "ours", "common": "common"}),
                Some(map!({"parent_only": "parent", "common": "old_common"})),
            ),
            IncomingAction::Merge {
                ext_id: "ext-id".to_string(),
                data: map!({"other_only": "other", "ours_only": "ours", "common": "common"}),
                changes: changes![change!("other_only", None, "other")],
            }
        );
        // Simple conflict - parent value is neither local nor incoming. incoming wins.
        assert_eq!(
            merge(
                "ext-id".to_string(),
                map!({"other_only": "other", "common": "incoming"}),
                map!({"ours_only": "ours", "common": "local"}),
                Some(map!({"parent_only": "parent", "common": "parent"})),
            ),
            IncomingAction::Merge {
                ext_id: "ext-id".to_string(),
                data: map!({"other_only": "other", "ours_only": "ours", "common": "incoming"}),
                changes: changes![
                    change!("common", "local", "incoming"),
                    change!("other_only", None, "other")
                ],
            }
        );
        // Local change, no conflict.
        assert_eq!(
            merge(
                "ext-id".to_string(),
                map!({"other_only": "other", "common": "old_value"}),
                map!({"ours_only": "ours", "common": "new_value"}),
                Some(map!({"parent_only": "parent", "common": "old_value"})),
            ),
            IncomingAction::Merge {
                ext_id: "ext-id".to_string(),
                data: map!({"other_only": "other", "ours_only": "ours", "common": "new_value"}),
                changes: changes![change!("other_only", None, "other")],
            }
        );
        // Field was removed remotely.
        assert_eq!(
            merge(
                "ext-id".to_string(),
                map!({"other_only": "other"}),
                map!({"common": "old_value"}),
                Some(map!({"common": "old_value"})),
            ),
            IncomingAction::TakeRemote {
                ext_id: "ext-id".to_string(),
                data: map!({"other_only": "other"}),
                changes: changes![
                    change!("common", "old_value", None),
                    change!("other_only", None, "other")
                ],
            }
        );
        // Field was removed remotely but we added another one.
        assert_eq!(
            merge(
                "ext-id".to_string(),
                map!({"other_only": "other"}),
                map!({"common": "old_value", "new_key": "new_value"}),
                Some(map!({"common": "old_value"})),
            ),
            IncomingAction::Merge {
                ext_id: "ext-id".to_string(),
                data: map!({"other_only": "other", "new_key": "new_value"}),
                changes: changes![
                    change!("common", "old_value", None),
                    change!("other_only", None, "other")
                ],
            }
        );
        // Field was removed both remotely and locally.
        assert_eq!(
            merge(
                "ext-id".to_string(),
                map!({}),
                map!({"new_key": "new_value"}),
                Some(map!({"common": "old_value"})),
            ),
            IncomingAction::Merge {
                ext_id: "ext-id".to_string(),
                data: map!({"new_key": "new_value"}),
                changes: changes![],
            }
        );
    }

    #[test]
    fn test_remove_matching_keys() {
        assert_eq!(
            remove_matching_keys(
                map!({"key1": "value1", "key2": "value2"}),
                &map!({"key1": "ignored", "key3": "ignored"})
            ),
            (
                map!({"key2": "value2"}),
                changes![change!("key1", "value1", None)]
            )
        );
    }

    #[test]
    fn test_get_synced_changes() -> Result<()> {
        let db = new_syncable_mem_db();
        db.execute_batch(&format!(
            r#"INSERT INTO temp.storage_sync_applied (ext_id, changes)
                VALUES
                ('an-extension', '{change1}'),
                ('ext"id', '{change2}')
            "#,
            change1 = serde_json::to_string(&changes![change!("key1", "old-val", None)])?,
            change2 = serde_json::to_string(&changes![change!("key-for-second", None, "new-val")])?
        ))?;
        let changes = get_synced_changes(&db)?;
        assert_eq!(changes[0].ext_id, "an-extension");
        // sanity check it's valid!
        let c1: JsonMap =
            serde_json::from_str(&changes[0].changes).expect("changes must be an object");
        assert_eq!(
            c1.get("key1")
                .expect("must exist")
                .as_object()
                .expect("must be an object")
                .get("oldValue"),
            Some(&json!("old-val"))
        );

        // phew - do it again to check the string got escaped.
        assert_eq!(
            changes[1],
            SyncedExtensionChange {
                ext_id: "ext\"id".into(),
                changes: r#"{"key-for-second":{"newValue":"new-val"}}"#.into(),
            }
        );
        assert_eq!(changes[1].ext_id, "ext\"id");
        let c2: JsonMap =
            serde_json::from_str(&changes[1].changes).expect("changes must be an object");
        assert_eq!(
            c2.get("key-for-second")
                .expect("must exist")
                .as_object()
                .expect("must be an object")
                .get("newValue"),
            Some(&json!("new-val"))
        );
        Ok(())
    }
}
