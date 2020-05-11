/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

mod bridge;
mod incoming;
mod outgoing;

#[cfg(test)]
mod sync_tests;

use serde_derive::*;
use sync_guid::Guid as SyncGuid;

pub use bridge::BridgedEngine;
use incoming::IncomingAction;

type JsonMap = serde_json::Map<String, serde_json::Value>;

pub const STORAGE_VERSION: usize = 1;

/// For use with `#[serde(skip_serializing_if = )]`
#[inline]
pub fn is_default<T: PartialEq + Default>(v: &T) -> bool {
    *v == T::default()
}

#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Record {
    #[serde(rename = "id")]
    guid: SyncGuid,
    ext_id: String,
    #[serde(default, skip_serializing_if = "is_default")]
    data: Option<String>,
}

// Perform a 2-way or 3-way merge, where the incoming value wins on confict.
fn merge(mut other: JsonMap, mut ours: JsonMap, parent: Option<JsonMap>) -> IncomingAction {
    if other == ours {
        return IncomingAction::Same;
    }
    let old_incoming = other.clone();
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
                    ours.insert(key, incoming_value);
                }
            } else {
                // Key was not present in incoming value.
                // Another client must have deleted it.
                log::trace!(
                    "merge: key {} no longer present in incoming - removing it locally",
                    key
                );
                ours.remove(&key);
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
            ours.insert(key, incoming_value);
        }
    } else {
        // No parent. Server wins. Overwrite every key in ours with
        // the corresponding value in other.
        log::trace!("merge: no parent - copying all keys from incoming");
        for (key, incoming_value) in other.into_iter() {
            ours.insert(key, incoming_value);
        }
    }

    if ours == old_incoming {
        IncomingAction::TakeRemote { data: old_incoming }
    } else {
        IncomingAction::Merge { data: ours }
    }
}

fn remove_matching_keys(mut ours: JsonMap, blacklist: &JsonMap) -> JsonMap {
    for key in blacklist.keys() {
        ours.remove(key);
    }
    ours
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
    use super::*;
    use crate::error::*;
    use serde_json::json;

    // a macro for these tests - constructs a serde_json::Value::Object
    macro_rules! map {
        ($($map:tt)+) => {
            json!($($map)+).as_object().unwrap().clone()
        };
    }

    #[test]
    fn test_3way_merging() -> Result<()> {
        // No conflict - identical local and remote.
        assert_eq!(
            merge(
                map!({"one": "one", "two": "two"}),
                map!({"two": "two", "one": "one"}),
                Some(map!({"parent_only": "parent"})),
            ),
            IncomingAction::Same
        );
        assert_eq!(
            merge(
                map!({"other_only": "other", "common": "common"}),
                map!({"ours_only": "ours", "common": "common"}),
                Some(map!({"parent_only": "parent", "common": "old_common"})),
            ),
            IncomingAction::Merge {
                data: map!({"other_only": "other", "ours_only": "ours", "common": "common"})
            }
        );
        // Simple conflict - parent value is neither local nor incoming. incoming wins.
        assert_eq!(
            merge(
                map!({"other_only": "other", "common": "incoming"}),
                map!({"ours_only": "ours", "common": "local"}),
                Some(map!({"parent_only": "parent", "common": "parent"})),
            ),
            IncomingAction::Merge {
                data: map!({"other_only": "other", "ours_only": "ours", "common": "incoming"})
            }
        );
        // Local change, no conflict.
        assert_eq!(
            merge(
                map!({"other_only": "other", "common": "old_value"}),
                map!({"ours_only": "ours", "common": "new_value"}),
                Some(map!({"parent_only": "parent", "common": "old_value"})),
            ),
            IncomingAction::Merge {
                data: map!({"other_only": "other", "ours_only": "ours", "common": "new_value"})
            }
        );
        // Field was removed remotely.
        assert_eq!(
            merge(
                map!({"other_only": "other"}),
                map!({"common": "old_value"}),
                Some(map!({"common": "old_value"})),
            ),
            IncomingAction::TakeRemote {
                data: map!({"other_only": "other"}),
            }
        );
        // Field was removed remotely but we added another one.
        assert_eq!(
            merge(
                map!({"other_only": "other"}),
                map!({"common": "old_value", "new_key": "new_value"}),
                Some(map!({"common": "old_value"})),
            ),
            IncomingAction::Merge {
                data: map!({"other_only": "other", "new_key": "new_value"}),
            }
        );
        // Field was removed both remotely and locally.
        assert_eq!(
            merge(
                map!({}),
                map!({"new_key": "new_value"}),
                Some(map!({"common": "old_value"})),
            ),
            IncomingAction::Merge {
                data: map!({"new_key": "new_value"}),
            }
        );
        Ok(())
    }

    #[test]
    fn test_remove_matching_keys() -> Result<()> {
        assert_eq!(
            remove_matching_keys(
                map!({"key1": "value1", "key2": "value2"}),
                &map!({"key1": "ignored", "key3": "ignored"})
            ),
            map!({"key2": "value2"})
        );
        Ok(())
    }
}
