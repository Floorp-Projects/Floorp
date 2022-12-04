/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The "incoming" part of syncing - handling the incoming rows, staging them,
// working out a plan for them, updating the local data and mirror, etc.

use interrupt_support::Interruptee;
use rusqlite::{Connection, Row, Transaction};
use sql_support::ConnExt;
use sync15::bso::{IncomingContent, IncomingKind};
use sync_guid::Guid as SyncGuid;

use crate::api::{StorageChanges, StorageValueChange};
use crate::error::*;

use super::{merge, remove_matching_keys, JsonMap, WebextRecord};

/// The state data can be in. Could be represented as Option<JsonMap>, but this
/// is clearer and independent of how the data is stored.
#[derive(Debug, PartialEq, Eq)]
pub enum DataState {
    /// The data was deleted.
    Deleted,
    /// Data exists, as stored in the map.
    Exists(JsonMap),
}

// A little helper to create a StorageChanges object when we are creating
// a new value with multiple keys that doesn't exist locally.
fn changes_for_new_incoming(new: &JsonMap) -> StorageChanges {
    let mut result = StorageChanges::with_capacity(new.len());
    for (key, val) in new.iter() {
        result.push(StorageValueChange {
            key: key.clone(),
            old_value: None,
            new_value: Some(val.clone()),
        });
    }
    result
}

// This module deals exclusively with the Map inside a JsonValue::Object().
// This helper reads such a Map from a SQL row, ignoring anything which is
// either invalid JSON or a different JSON type.
fn json_map_from_row(row: &Row<'_>, col: &str) -> Result<DataState> {
    let s = row.get::<_, Option<String>>(col)?;
    Ok(match s {
        None => DataState::Deleted,
        Some(s) => match serde_json::from_str(&s) {
            Ok(serde_json::Value::Object(m)) => DataState::Exists(m),
            _ => {
                // We don't want invalid json or wrong types to kill syncing.
                // It should be impossible as we never write anything which
                // could cause it, but we can't really log the bad data as there
                // might be PII. Logging just a message without any additional
                // clues is going to be unhelpfully noisy, so, silently None.
                // XXX - Maybe record telemetry?
                DataState::Deleted
            }
        },
    })
}

/// The first thing we do with incoming items is to "stage" them in a temp table.
/// The actual processing is done via this table.
pub fn stage_incoming(
    tx: &Transaction<'_>,
    incoming_records: &[IncomingContent<WebextRecord>],
    signal: &dyn Interruptee,
) -> Result<()> {
    sql_support::each_sized_chunk(
        incoming_records,
        // We bind 3 params per chunk.
        sql_support::default_max_variable_number() / 3,
        |chunk, _| -> Result<()> {
            let mut params = Vec::with_capacity(chunk.len() * 3);
            for record in chunk {
                signal.err_if_interrupted()?;
                match &record.kind {
                    IncomingKind::Content(r) => {
                        params.push(Some(record.envelope.id.to_string()));
                        params.push(Some(r.ext_id.to_string()));
                        params.push(Some(r.data.clone()));
                    }
                    IncomingKind::Tombstone => {
                        params.push(Some(record.envelope.id.to_string()));
                        params.push(None);
                        params.push(None);
                    }
                    IncomingKind::Malformed => {
                        log::error!("Ignoring incoming malformed record: {}", record.envelope.id);
                    }
                }
            }
            // we might have skipped records
            let actual_len = params.len() / 3;
            if actual_len != 0 {
                let sql = format!(
                    "INSERT OR REPLACE INTO temp.storage_sync_staging
                    (guid, ext_id, data)
                    VALUES {}",
                    sql_support::repeat_multi_values(actual_len, 3)
                );
                tx.execute(&sql, rusqlite::params_from_iter(params))?;
            }
            Ok(())
        },
    )?;
    Ok(())
}

/// The "state" we find ourselves in when considering an incoming/staging
/// record. This "state" is the input to calculating the IncomingAction and
/// carries all the data we need to make the required local changes.
#[derive(Debug, PartialEq, Eq)]
pub enum IncomingState {
    /// There's an incoming item, but data for that extension doesn't exist
    /// either in our local data store or in the local mirror. IOW, this is the
    /// very first time we've seen this extension.
    IncomingOnlyData { ext_id: String, data: JsonMap },

    /// An incoming tombstone that doesn't exist locally. Because tombstones
    /// don't carry the ext-id, it means it's not in our mirror. We are just
    /// going to ignore it, but we track the state for consistency.
    IncomingOnlyTombstone,

    /// There's an incoming item and we have data for the same extension in
    /// our local store - but not in our mirror. This should be relatively
    /// uncommon as it means:
    /// * Some other profile has recently installed an extension and synced.
    /// * This profile has recently installed the same extension.
    /// * This is the first sync for this profile since both those events
    ///   happened.
    HasLocal {
        ext_id: String,
        incoming: DataState,
        local: DataState,
    },
    /// There's an incoming item and there's an item for the same extension in
    /// the mirror. The addon probably doesn't exist locally, or if it does,
    /// the last time we synced we synced the deletion of all data.
    NotLocal {
        ext_id: String,
        incoming: DataState,
        mirror: DataState,
    },
    /// This will be the most common "incoming" case - there's data incoming,
    /// in the mirror and in the local store for an addon.
    Everywhere {
        ext_id: String,
        incoming: DataState,
        mirror: DataState,
        local: DataState,
    },
}

/// Get the items we need to process from the staging table. Return details about
/// the item and the state of that item, ready for processing.
pub fn get_incoming(conn: &Connection) -> Result<Vec<(SyncGuid, IncomingState)>> {
    let sql = "
        SELECT
            s.guid as guid,
            l.ext_id as l_ext_id,
            m.ext_id as m_ext_id,
            s.ext_id as s_ext_id,
            s.data as s_data, m.data as m_data, l.data as l_data,
            l.sync_change_counter
        FROM temp.storage_sync_staging s
        LEFT JOIN storage_sync_mirror m ON m.guid = s.guid
        LEFT JOIN storage_sync_data l on l.ext_id IN (m.ext_id, s.ext_id);";

    fn from_row(row: &Row<'_>) -> Result<(SyncGuid, IncomingState)> {
        let guid = row.get("guid")?;
        // This is complicated because the staging row doesn't hold the ext_id.
        // However, both the local table and the mirror do.
        let mirror_ext_id: Option<String> = row.get("m_ext_id")?;
        let local_ext_id: Option<String> = row.get("l_ext_id")?;
        let staged_ext_id: Option<String> = row.get("s_ext_id")?;
        let incoming = json_map_from_row(row, "s_data")?;

        // We find the state by examining which tables the ext-id exists in,
        // using whether that column is null as a proxy for that.
        let state = match (local_ext_id, mirror_ext_id) {
            (None, None) => {
                match staged_ext_id {
                    Some(ext_id) => {
                        let data = match incoming {
                            // incoming record with missing data that's not a
                            // tombstone shouldn't happen, but we can cope by
                            // pretending it was an empty json map.
                            DataState::Deleted => JsonMap::new(),
                            DataState::Exists(data) => data,
                        };
                        IncomingState::IncomingOnlyData { ext_id, data }
                    }
                    None => IncomingState::IncomingOnlyTombstone,
                }
            }
            (Some(ext_id), None) => IncomingState::HasLocal {
                ext_id,
                incoming,
                local: json_map_from_row(row, "l_data")?,
            },
            (None, Some(ext_id)) => IncomingState::NotLocal {
                ext_id,
                incoming,
                mirror: json_map_from_row(row, "m_data")?,
            },
            (Some(ext_id), Some(_)) => IncomingState::Everywhere {
                ext_id,
                incoming,
                mirror: json_map_from_row(row, "m_data")?,
                local: json_map_from_row(row, "l_data")?,
            },
        };
        Ok((guid, state))
    }

    conn.conn().query_rows_and_then(sql, [], from_row)
}

/// This is the set of actions we know how to take *locally* for incoming
/// records. Which one depends on the IncomingState.
/// Every state which updates also records the set of changes we should notify
#[derive(Debug, PartialEq, Eq)]
pub enum IncomingAction {
    /// We should locally delete the data for this record
    DeleteLocally {
        ext_id: String,
        changes: StorageChanges,
    },
    /// We will take the remote.
    TakeRemote {
        ext_id: String,
        data: JsonMap,
        changes: StorageChanges,
    },
    /// We merged this data - this is what we came up with.
    Merge {
        ext_id: String,
        data: JsonMap,
        changes: StorageChanges,
    },
    /// Entry exists locally and it's the same as the incoming record.
    Same { ext_id: String },
    /// Incoming tombstone for an item we've never seen.
    Nothing,
}

/// Takes the state of an item and returns the action we should take for it.
pub fn plan_incoming(s: IncomingState) -> IncomingAction {
    match s {
        IncomingState::Everywhere {
            ext_id,
            incoming,
            local,
            mirror,
        } => {
            // All records exist - but do they all have data?
            match (incoming, local, mirror) {
                (
                    DataState::Exists(incoming_data),
                    DataState::Exists(local_data),
                    DataState::Exists(mirror_data),
                ) => {
                    // all records have data - 3-way merge.
                    merge(ext_id, incoming_data, local_data, Some(mirror_data))
                }
                (
                    DataState::Exists(incoming_data),
                    DataState::Exists(local_data),
                    DataState::Deleted,
                ) => {
                    // No parent, so first time seeing this remotely - 2-way merge
                    merge(ext_id, incoming_data, local_data, None)
                }
                (DataState::Exists(incoming_data), DataState::Deleted, _) => {
                    // Incoming data, removed locally. Server wins.
                    IncomingAction::TakeRemote {
                        ext_id,
                        changes: changes_for_new_incoming(&incoming_data),
                        data: incoming_data,
                    }
                }
                (DataState::Deleted, DataState::Exists(local_data), DataState::Exists(mirror)) => {
                    // Deleted remotely.
                    // Treat this as a delete of every key that we
                    // know was present at the time.
                    let (result, changes) = remove_matching_keys(local_data, &mirror);
                    if result.is_empty() {
                        // If there were no more keys left, we can
                        // delete our version too.
                        IncomingAction::DeleteLocally { ext_id, changes }
                    } else {
                        IncomingAction::Merge {
                            ext_id,
                            data: result,
                            changes,
                        }
                    }
                }
                (DataState::Deleted, DataState::Exists(local_data), DataState::Deleted) => {
                    // Perhaps another client created and then deleted
                    // the whole object for this extension since the
                    // last time we synced.
                    // Treat this as a delete of every key that we
                    // knew was present. Unfortunately, we don't know
                    // any keys that were present, so we delete no keys.
                    IncomingAction::Merge {
                        ext_id,
                        data: local_data,
                        changes: StorageChanges::new(),
                    }
                }
                (DataState::Deleted, DataState::Deleted, _) => {
                    // We agree with the remote (regardless of what we
                    // have mirrored).
                    IncomingAction::Same { ext_id }
                }
            }
        }
        IncomingState::HasLocal {
            ext_id,
            incoming,
            local,
        } => {
            // So we have a local record and an incoming/staging record, but *not* a
            // mirror record. This means some other device has synced this for
            // the first time and we are yet to do the same.
            match (incoming, local) {
                (DataState::Exists(incoming_data), DataState::Exists(local_data)) => {
                    // This means the extension exists locally and remotely
                    // but this is the first time we've synced it. That's no problem, it's
                    // just a 2-way merge...
                    merge(ext_id, incoming_data, local_data, None)
                }
                (DataState::Deleted, DataState::Exists(local_data)) => {
                    // We've data locally, but there's an incoming deletion.
                    // We would normally remove keys that we knew were
                    // present on the server, but we don't know what
                    // was on the server, so we don't remove anything.
                    IncomingAction::Merge {
                        ext_id,
                        data: local_data,
                        changes: StorageChanges::new(),
                    }
                }
                (DataState::Exists(incoming_data), DataState::Deleted) => {
                    // No data locally, but some is incoming - take it.
                    IncomingAction::TakeRemote {
                        ext_id,
                        changes: changes_for_new_incoming(&incoming_data),
                        data: incoming_data,
                    }
                }
                (DataState::Deleted, DataState::Deleted) => {
                    // Nothing anywhere - odd, but OK.
                    IncomingAction::Same { ext_id }
                }
            }
        }
        IncomingState::NotLocal {
            ext_id, incoming, ..
        } => {
            // No local data but there's mirror and an incoming record.
            // This means a local deletion is being replaced by, or just re-doing
            // the incoming record.
            match incoming {
                DataState::Exists(data) => IncomingAction::TakeRemote {
                    ext_id,
                    changes: changes_for_new_incoming(&data),
                    data,
                },
                DataState::Deleted => IncomingAction::Same { ext_id },
            }
        }
        IncomingState::IncomingOnlyData { ext_id, data } => {
            // Only the staging record exists and it's not a tombstone.
            // This means it's the first time we've ever seen it. No
            // conflict possible, just take the remote.
            IncomingAction::TakeRemote {
                ext_id,
                changes: changes_for_new_incoming(&data),
                data,
            }
        }
        IncomingState::IncomingOnlyTombstone => {
            // Only the staging record exists and it is a tombstone - nothing to do.
            IncomingAction::Nothing
        }
    }
}

fn insert_changes(tx: &Transaction<'_>, ext_id: &str, changes: &StorageChanges) -> Result<()> {
    tx.execute_cached(
        "INSERT INTO temp.storage_sync_applied (ext_id, changes)
            VALUES (:ext_id, :changes)",
        rusqlite::named_params! {
            ":ext_id": ext_id,
            ":changes": &serde_json::to_string(&changes)?,
        },
    )?;
    Ok(())
}

// Apply the actions necessary to fully process the incoming items.
pub fn apply_actions(
    tx: &Transaction<'_>,
    actions: Vec<(SyncGuid, IncomingAction)>,
    signal: &dyn Interruptee,
) -> Result<()> {
    for (item, action) in actions {
        signal.err_if_interrupted()?;

        log::trace!("action for '{:?}': {:?}", item, action);
        match action {
            IncomingAction::DeleteLocally { ext_id, changes } => {
                // Can just nuke it entirely.
                tx.execute_cached(
                    "DELETE FROM storage_sync_data WHERE ext_id = :ext_id",
                    &[(":ext_id", &ext_id)],
                )?;
                insert_changes(tx, &ext_id, &changes)?;
            }
            // We want to update the local record with 'data' and after this update the item no longer is considered dirty.
            IncomingAction::TakeRemote {
                ext_id,
                data,
                changes,
            } => {
                tx.execute_cached(
                    "INSERT OR REPLACE INTO storage_sync_data(ext_id, data, sync_change_counter)
                        VALUES (:ext_id, :data, 0)",
                    rusqlite::named_params! {
                        ":ext_id": ext_id,
                        ":data": serde_json::Value::Object(data),
                    },
                )?;
                insert_changes(tx, &ext_id, &changes)?;
            }

            // We merged this data, so need to update locally but still consider
            // it dirty because the merged data must be uploaded.
            IncomingAction::Merge {
                ext_id,
                data,
                changes,
            } => {
                tx.execute_cached(
                    "UPDATE storage_sync_data SET data = :data, sync_change_counter = sync_change_counter + 1 WHERE ext_id = :ext_id",
                    rusqlite::named_params! {
                        ":ext_id": ext_id,
                        ":data": serde_json::Value::Object(data),
                    },
                )?;
                insert_changes(tx, &ext_id, &changes)?;
            }

            // Both local and remote ended up the same - only need to nuke the
            // change counter.
            IncomingAction::Same { ext_id } => {
                tx.execute_cached(
                    "UPDATE storage_sync_data SET sync_change_counter = 0 WHERE ext_id = :ext_id",
                    &[(":ext_id", &ext_id)],
                )?;
                // no changes to write
            }
            // Literally nothing to do!
            IncomingAction::Nothing => {}
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::super::test::new_syncable_mem_db;
    use super::*;
    use crate::api;
    use interrupt_support::NeverInterrupts;
    use serde_json::{json, Value};
    use sync15::bso::IncomingBso;

    // select simple int
    fn ssi(conn: &Connection, stmt: &str) -> u32 {
        conn.try_query_one(stmt, [], true)
            .expect("must work")
            .unwrap_or_default()
    }

    fn array_to_incoming(mut array: Value) -> Vec<IncomingContent<WebextRecord>> {
        let jv = array.as_array_mut().expect("you must pass a json array");
        let mut result = Vec::with_capacity(jv.len());
        for elt in jv {
            result.push(IncomingBso::from_test_content(elt.take()).into_content());
        }
        result
    }

    // Can't find a way to import these from crate::sync::tests...
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
            };
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
    fn test_incoming_populates_staging() -> Result<()> {
        let mut db = new_syncable_mem_db();
        let tx = db.transaction()?;

        let incoming = json! {[
            {
                "id": "guidAAAAAAAA",
                "extId": "ext1@example.com",
                "data": json!({"foo": "bar"}).to_string(),
            }
        ]};

        stage_incoming(&tx, &array_to_incoming(incoming), &NeverInterrupts)?;
        // check staging table
        assert_eq!(
            ssi(&tx, "SELECT count(*) FROM temp.storage_sync_staging"),
            1
        );
        Ok(())
    }

    #[test]
    fn test_fetch_incoming_state() -> Result<()> {
        let mut db = new_syncable_mem_db();
        let tx = db.transaction()?;

        // Start with an item just in staging.
        tx.execute(
            r#"
            INSERT INTO temp.storage_sync_staging (guid, ext_id, data)
            VALUES ('guid', 'ext_id', '{"foo":"bar"}')
        "#,
            [],
        )?;

        let incoming = get_incoming(&tx)?;
        assert_eq!(incoming.len(), 1);
        assert_eq!(incoming[0].0, SyncGuid::new("guid"),);
        assert_eq!(
            incoming[0].1,
            IncomingState::IncomingOnlyData {
                ext_id: "ext_id".to_string(),
                data: map!({"foo": "bar"}),
            }
        );

        // Add the same item to the mirror.
        tx.execute(
            r#"
            INSERT INTO storage_sync_mirror (guid, ext_id, data)
            VALUES ('guid', 'ext_id', '{"foo":"new"}')
        "#,
            [],
        )?;
        let incoming = get_incoming(&tx)?;
        assert_eq!(incoming.len(), 1);
        assert_eq!(
            incoming[0].1,
            IncomingState::NotLocal {
                ext_id: "ext_id".to_string(),
                incoming: DataState::Exists(map!({"foo": "bar"})),
                mirror: DataState::Exists(map!({"foo": "new"})),
            }
        );

        // and finally the data itself - might as use the API here!
        api::set(&tx, "ext_id", json!({"foo": "local"}))?;
        let incoming = get_incoming(&tx)?;
        assert_eq!(incoming.len(), 1);
        assert_eq!(
            incoming[0].1,
            IncomingState::Everywhere {
                ext_id: "ext_id".to_string(),
                incoming: DataState::Exists(map!({"foo": "bar"})),
                local: DataState::Exists(map!({"foo": "local"})),
                mirror: DataState::Exists(map!({"foo": "new"})),
            }
        );
        Ok(())
    }

    // Like test_fetch_incoming_state, but check NULLs are handled correctly.
    #[test]
    fn test_fetch_incoming_state_nulls() -> Result<()> {
        let mut db = new_syncable_mem_db();
        let tx = db.transaction()?;

        // Start with a tombstone just in staging.
        tx.execute(
            r#"
            INSERT INTO temp.storage_sync_staging (guid, ext_id, data)
            VALUES ('guid', NULL, NULL)
        "#,
            [],
        )?;

        let incoming = get_incoming(&tx)?;
        assert_eq!(incoming.len(), 1);
        assert_eq!(incoming[0].1, IncomingState::IncomingOnlyTombstone,);

        // Add the same item to the mirror (can't store an ext_id for a
        // tombstone in the mirror as incoming tombstones never have it)
        tx.execute(
            r#"
            INSERT INTO storage_sync_mirror (guid, ext_id, data)
            VALUES ('guid', NULL, NULL)
        "#,
            [],
        )?;
        let incoming = get_incoming(&tx)?;
        assert_eq!(incoming.len(), 1);
        assert_eq!(incoming[0].1, IncomingState::IncomingOnlyTombstone);

        tx.execute(
            r#"
            INSERT INTO storage_sync_data (ext_id, data)
            VALUES ('ext_id', NULL)
        "#,
            [],
        )?;
        let incoming = get_incoming(&tx)?;
        assert_eq!(incoming.len(), 1);
        assert_eq!(
            incoming[0].1,
            // IncomingOnly* seems a little odd, but it is because we can't
            // tie the tombstones together due to the lack of any ext-id/guid
            // mapping in this case.
            IncomingState::IncomingOnlyTombstone
        );
        Ok(())
    }

    // apply_action tests.
    #[derive(Debug, PartialEq)]
    struct LocalItem {
        data: DataState,
        sync_change_counter: i32,
    }

    fn get_local_item(conn: &Connection) -> Option<LocalItem> {
        conn.try_query_row::<_, Error, _, _>(
            "SELECT data, sync_change_counter FROM storage_sync_data WHERE ext_id = 'ext_id'",
            [],
            |row| {
                let data = json_map_from_row(row, "data")?;
                let sync_change_counter = row.get::<_, i32>(1)?;
                Ok(LocalItem {
                    data,
                    sync_change_counter,
                })
            },
            true,
        )
        .expect("query should work")
    }

    fn get_applied_item_changes(conn: &Connection) -> Option<StorageChanges> {
        // no custom deserialize for storagechanges and we only need it for
        // tests, so do it manually.
        conn.try_query_row::<_, Error, _, _>(
            "SELECT changes FROM temp.storage_sync_applied WHERE ext_id = 'ext_id'",
            [],
            |row| Ok(serde_json::from_str(&row.get::<_, String>("changes")?)?),
            true,
        )
        .expect("query should work")
        .map(|val: serde_json::Value| {
            let ob = val.as_object().expect("should be an object of items");
            let mut result = StorageChanges::with_capacity(ob.len());
            for (key, val) in ob.into_iter() {
                let details = val.as_object().expect("elts should be objects");
                result.push(StorageValueChange {
                    key: key.to_string(),
                    old_value: details.get("oldValue").cloned(),
                    new_value: details.get("newValue").cloned(),
                });
            }
            result
        })
    }

    fn do_apply_action(tx: &Transaction<'_>, action: IncomingAction) {
        let guid = SyncGuid::new("guid");
        apply_actions(tx, vec![(guid, action)], &NeverInterrupts).expect("should apply");
    }

    #[test]
    fn test_apply_actions() -> Result<()> {
        let mut db = new_syncable_mem_db();

        // DeleteLocally - row should be entirely removed.
        let tx = db.transaction().expect("transaction should work");
        api::set(&tx, "ext_id", json!({"foo": "local"}))?;
        assert_eq!(
            api::get(&tx, "ext_id", json!(null))?,
            json!({"foo": "local"})
        );
        let changes = changes![change!("foo", "local", None)];
        do_apply_action(
            &tx,
            IncomingAction::DeleteLocally {
                ext_id: "ext_id".to_string(),
                changes: changes.clone(),
            },
        );
        assert_eq!(api::get(&tx, "ext_id", json!(null))?, json!({}));
        // and there should not be a local record at all.
        assert!(get_local_item(&tx).is_none());
        assert_eq!(get_applied_item_changes(&tx), Some(changes));
        tx.rollback()?;

        // TakeRemote - replace local data with remote and marked as not dirty.
        let tx = db.transaction().expect("transaction should work");
        api::set(&tx, "ext_id", json!({"foo": "local"}))?;
        assert_eq!(
            api::get(&tx, "ext_id", json!(null))?,
            json!({"foo": "local"})
        );
        // data should exist locally with a change recorded.
        assert_eq!(
            get_local_item(&tx),
            Some(LocalItem {
                data: DataState::Exists(map!({"foo": "local"})),
                sync_change_counter: 1
            })
        );
        let changes = changes![change!("foo", "local", "remote")];
        do_apply_action(
            &tx,
            IncomingAction::TakeRemote {
                ext_id: "ext_id".to_string(),
                data: map!({"foo": "remote"}),
                changes: changes.clone(),
            },
        );
        // data should exist locally with the remote data and not be dirty.
        assert_eq!(
            get_local_item(&tx),
            Some(LocalItem {
                data: DataState::Exists(map!({"foo": "remote"})),
                sync_change_counter: 0
            })
        );
        assert_eq!(get_applied_item_changes(&tx), Some(changes));
        tx.rollback()?;

        // Merge - like ::TakeRemote, but data remains dirty.
        let tx = db.transaction().expect("transaction should work");
        api::set(&tx, "ext_id", json!({"foo": "local"}))?;
        assert_eq!(
            api::get(&tx, "ext_id", json!(null))?,
            json!({"foo": "local"})
        );
        // data should exist locally with a change recorded.
        assert_eq!(
            get_local_item(&tx),
            Some(LocalItem {
                data: DataState::Exists(map!({"foo": "local"})),
                sync_change_counter: 1
            })
        );
        let changes = changes![change!("foo", "local", "remote")];
        do_apply_action(
            &tx,
            IncomingAction::Merge {
                ext_id: "ext_id".to_string(),
                data: map!({"foo": "remote"}),
                changes: changes.clone(),
            },
        );
        assert_eq!(
            get_local_item(&tx),
            Some(LocalItem {
                data: DataState::Exists(map!({"foo": "remote"})),
                sync_change_counter: 2
            })
        );
        assert_eq!(get_applied_item_changes(&tx), Some(changes));
        tx.rollback()?;

        // Same - data stays the same but is marked not dirty.
        let tx = db.transaction().expect("transaction should work");
        api::set(&tx, "ext_id", json!({"foo": "local"}))?;
        assert_eq!(
            api::get(&tx, "ext_id", json!(null))?,
            json!({"foo": "local"})
        );
        // data should exist locally with a change recorded.
        assert_eq!(
            get_local_item(&tx),
            Some(LocalItem {
                data: DataState::Exists(map!({"foo": "local"})),
                sync_change_counter: 1
            })
        );
        do_apply_action(
            &tx,
            IncomingAction::Same {
                ext_id: "ext_id".to_string(),
            },
        );
        assert_eq!(
            get_local_item(&tx),
            Some(LocalItem {
                data: DataState::Exists(map!({"foo": "local"})),
                sync_change_counter: 0
            })
        );
        assert_eq!(get_applied_item_changes(&tx), None);
        tx.rollback()?;

        Ok(())
    }
}
