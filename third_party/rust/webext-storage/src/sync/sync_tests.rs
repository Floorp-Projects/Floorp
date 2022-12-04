/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tries to simulate "full syncs" - ie, from local state changing, to
// fetching incoming items, generating items to upload, then updating the local
// state (including the mirror) as a result.

use crate::api::{clear, get, set};
use crate::error::*;
use crate::schema::create_empty_sync_temp_tables;
use crate::sync::incoming::{apply_actions, get_incoming, plan_incoming, stage_incoming};
use crate::sync::outgoing::{get_outgoing, record_uploaded, stage_outgoing};
use crate::sync::test::new_syncable_mem_db;
use crate::sync::WebextRecord;
use interrupt_support::NeverInterrupts;
use rusqlite::{Connection, Row, Transaction};
use serde_json::json;
use sql_support::ConnExt;
use sync15::bso::{IncomingBso, IncomingContent, OutgoingBso};
use sync_guid::Guid;

// Here we try and simulate everything done by a "full sync", just minus the
// engine. Returns the records we uploaded.
fn do_sync(
    tx: &Transaction<'_>,
    incoming_payloads: &[IncomingContent<WebextRecord>],
) -> Result<Vec<OutgoingBso>> {
    log::info!("test do_sync() starting");
    // First we stage the incoming in the temp tables.
    stage_incoming(tx, incoming_payloads, &NeverInterrupts)?;
    // Then we process them getting a Vec of (item, state), which we turn into
    // a Vec of (item, action)
    let actions = get_incoming(tx)?
        .into_iter()
        .map(|(item, state)| (item, plan_incoming(state)))
        .collect();
    log::debug!("do_sync applying {:?}", actions);
    apply_actions(tx, actions, &NeverInterrupts)?;
    // So we've done incoming - do outgoing.
    stage_outgoing(tx)?;
    let outgoing = get_outgoing(tx, &NeverInterrupts)?;
    log::debug!("do_sync has outgoing {:?}", outgoing);
    record_uploaded(
        tx,
        outgoing
            .iter()
            .map(|p| p.envelope.id.clone())
            .collect::<Vec<Guid>>()
            .as_slice(),
        &NeverInterrupts,
    )?;
    create_empty_sync_temp_tables(tx)?;
    log::info!("test do_sync() complete");
    Ok(outgoing)
}

// Check *both* the mirror and local API have ended up with the specified data.
fn check_finished_with(conn: &Connection, ext_id: &str, val: serde_json::Value) -> Result<()> {
    let local = get(conn, ext_id, serde_json::Value::Null)?;
    assert_eq!(local, val);
    let guid = get_mirror_guid(conn, ext_id)?;
    let mirror = get_mirror_data(conn, &guid);
    assert_eq!(mirror, DbData::Data(val.to_string()));
    // and there should be zero items with a change counter.
    let count = conn.query_row_and_then(
        "SELECT COUNT(*) FROM storage_sync_data WHERE sync_change_counter != 0;",
        [],
        |row| row.get::<_, u32>(0),
    )?;
    assert_eq!(count, 0);
    Ok(())
}

fn get_mirror_guid(conn: &Connection, extid: &str) -> Result<Guid> {
    let guid = conn.query_row_and_then(
        "SELECT m.guid FROM storage_sync_mirror m WHERE m.ext_id = ?;",
        [extid],
        |row| row.get::<_, Guid>(0),
    )?;
    Ok(guid)
}

#[derive(Debug, PartialEq)]
enum DbData {
    NoRow,
    NullRow,
    Data(String),
}

impl DbData {
    fn has_data(&self) -> bool {
        matches!(self, DbData::Data(_))
    }
}

fn _get(conn: &Connection, id_name: &str, expected_extid: &str, table: &str) -> DbData {
    let sql = format!("SELECT {} as id, data FROM {}", id_name, table);

    fn from_row(row: &Row<'_>) -> Result<(String, Option<String>)> {
        Ok((row.get("id")?, row.get("data")?))
    }
    let mut items = conn
        .conn()
        .query_rows_and_then(&sql, [], from_row)
        .expect("should work");
    if items.is_empty() {
        DbData::NoRow
    } else {
        let item = items.pop().expect("it exists");
        assert_eq!(Guid::new(&item.0), expected_extid);
        match item.1 {
            None => DbData::NullRow,
            Some(v) => DbData::Data(v),
        }
    }
}

fn get_mirror_data(conn: &Connection, expected_extid: &str) -> DbData {
    _get(conn, "guid", expected_extid, "storage_sync_mirror")
}

fn get_local_data(conn: &Connection, expected_extid: &str) -> DbData {
    _get(conn, "ext_id", expected_extid, "storage_sync_data")
}

fn make_incoming(
    guid: &Guid,
    ext_id: &str,
    data: &serde_json::Value,
) -> IncomingContent<WebextRecord> {
    let content = json!({"id": guid, "extId": ext_id, "data": data.to_string()});
    IncomingBso::from_test_content(content).into_content()
}

fn make_incoming_tombstone(guid: &Guid) -> IncomingContent<WebextRecord> {
    IncomingBso::new_test_tombstone(guid.clone()).into_content()
}

#[test]
fn test_simple_outgoing_sync() -> Result<()> {
    // So we are starting with an empty local store and empty server store.
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data.clone())?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    check_finished_with(&tx, "ext-id", data)?;
    Ok(())
}

#[test]
fn test_simple_incoming_sync() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    let bridge_record = make_incoming(&Guid::new("guid"), "ext-id", &data);
    assert_eq!(do_sync(&tx, &[bridge_record])?.len(), 0);
    let key1_from_api = get(&tx, "ext-id", json!("key1"))?;
    assert_eq!(key1_from_api, json!({"key1": "key1-value"}));
    check_finished_with(&tx, "ext-id", data)?;
    Ok(())
}

#[test]
fn test_outgoing_tombstone() -> Result<()> {
    // Tombstones are only kept when the mirror has that record - so first
    // test that, then arrange for the mirror to have the record.
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data.clone())?;
    assert_eq!(
        get_local_data(&tx, "ext-id"),
        DbData::Data(data.to_string())
    );
    // hasn't synced yet, so clearing shouldn't write a tombstone.
    clear(&tx, "ext-id")?;
    assert_eq!(get_local_data(&tx, "ext-id"), DbData::NoRow);
    // now set data again and sync and *then* remove.
    set(&tx, "ext-id", data)?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    assert!(get_local_data(&tx, "ext-id").has_data());
    let guid = get_mirror_guid(&tx, "ext-id")?;
    assert!(get_mirror_data(&tx, &guid).has_data());
    clear(&tx, "ext-id")?;
    assert_eq!(get_local_data(&tx, "ext-id"), DbData::NullRow);
    // then after syncing, the tombstone will be in the mirror but the local row
    // has been removed.
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    assert_eq!(get_local_data(&tx, "ext-id"), DbData::NoRow);
    assert_eq!(get_mirror_data(&tx, &guid), DbData::NullRow);
    Ok(())
}

#[test]
fn test_incoming_tombstone_exists() -> Result<()> {
    // An incoming tombstone for a record we've previously synced (and thus
    // have data for)
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data.clone())?;
    assert_eq!(
        get_local_data(&tx, "ext-id"),
        DbData::Data(data.to_string())
    );
    // sync to get data in our mirror.
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    assert!(get_local_data(&tx, "ext-id").has_data());
    let guid = get_mirror_guid(&tx, "ext-id")?;
    assert!(get_mirror_data(&tx, &guid).has_data());
    // Now an incoming tombstone for it.
    let tombstone = make_incoming_tombstone(&guid);
    assert_eq!(
        do_sync(&tx, &[tombstone])?.len(),
        0,
        "expect no outgoing records"
    );
    assert_eq!(get_local_data(&tx, "ext-id"), DbData::NoRow);
    assert_eq!(get_mirror_data(&tx, &guid), DbData::NullRow);
    Ok(())
}

#[test]
fn test_incoming_tombstone_not_exists() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    // An incoming tombstone for something that's not anywhere locally.
    let guid = Guid::new("guid");
    let tombstone = make_incoming_tombstone(&guid);
    assert_eq!(
        do_sync(&tx, &[tombstone])?.len(),
        0,
        "expect no outgoing records"
    );
    // But we still keep the tombstone in the mirror.
    assert_eq!(get_local_data(&tx, "ext-id"), DbData::NoRow);
    assert_eq!(get_mirror_data(&tx, &guid), DbData::NullRow);
    Ok(())
}

#[test]
fn test_reconciled() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value"});
    set(&tx, "ext-id", data)?;
    // Incoming payload with the same data
    let record = make_incoming(&Guid::new("guid"), "ext-id", &json!({"key1": "key1-value"}));
    // Should be no outgoing records as we reconciled.
    assert_eq!(do_sync(&tx, &[record])?.len(), 0);
    check_finished_with(&tx, "ext-id", json!({"key1": "key1-value"}))?;
    Ok(())
}

/// Tests that we handle things correctly if we get a payload that is
/// identical to what is in the mirrored table.
#[test]
fn test_reconcile_with_null_payload() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value"});
    set(&tx, "ext-id", data.clone())?;
    // We try to push this change on the next sync.
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    let guid = get_mirror_guid(&tx, "ext-id")?;
    assert_eq!(get_mirror_data(&tx, &guid), DbData::Data(data.to_string()));
    // Incoming payload with the same data.
    // This could happen if, for example, another client changed the
    // key and then put it back the way it was.
    let record = make_incoming(&guid, "ext-id", &data);
    // Should be no outgoing records as we reconciled.
    assert_eq!(do_sync(&tx, &[record])?.len(), 0);
    check_finished_with(&tx, "ext-id", data)?;
    Ok(())
}

#[test]
fn test_accept_incoming_when_local_is_deleted() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    // We only record an extension as deleted locally if it has been
    // uploaded before being deleted.
    let data = json!({"key1": "key1-value"});
    set(&tx, "ext-id", data)?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    let guid = get_mirror_guid(&tx, "ext-id")?;
    clear(&tx, "ext-id")?;
    // Incoming payload without 'key1'. Because we previously uploaded
    // key1, this means another client deleted it.
    let record = make_incoming(&guid, "ext-id", &json!({"key2": "key2-value"}));

    // We completely accept the incoming record.
    assert_eq!(do_sync(&tx, &[record])?.len(), 0);
    check_finished_with(&tx, "ext-id", json!({"key2": "key2-value"}))?;
    Ok(())
}

#[test]
fn test_accept_incoming_when_local_is_deleted_no_mirror() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value"});
    set(&tx, "ext-id", data)?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    clear(&tx, "ext-id")?;

    // Use a random guid so that we don't find the mirrored data.
    // This test is somewhat bad because deduping might obviate
    // the need for it.
    let record = make_incoming(&Guid::new("guid"), "ext-id", &json!({"key2": "key2-value"}));

    // We completely accept the incoming record.
    assert_eq!(do_sync(&tx, &[record])?.len(), 0);
    check_finished_with(&tx, "ext-id", json!({"key2": "key2-value"}))?;
    Ok(())
}

#[test]
fn test_accept_deleted_key_mirrored() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data)?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    let guid = get_mirror_guid(&tx, "ext-id")?;
    // Incoming payload without 'key1'. Because we previously uploaded
    // key1, this means another client deleted it.
    let record = make_incoming(&guid, "ext-id", &json!({"key2": "key2-value"}));
    // We completely accept the incoming record.
    assert_eq!(do_sync(&tx, &[record])?.len(), 0);
    check_finished_with(&tx, "ext-id", json!({"key2": "key2-value"}))?;
    Ok(())
}

#[test]
fn test_merged_no_mirror() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value"});
    set(&tx, "ext-id", data)?;
    // Incoming payload without 'key1' and some data for 'key2'.
    // Because we never uploaded 'key1', we merge our local values
    // with the remote.
    let record = make_incoming(&Guid::new("guid"), "ext-id", &json!({"key2": "key2-value"}));
    assert_eq!(do_sync(&tx, &[record])?.len(), 1);
    check_finished_with(
        &tx,
        "ext-id",
        json!({"key1": "key1-value", "key2": "key2-value"}),
    )?;
    Ok(())
}

#[test]
fn test_merged_incoming() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let old_data = json!({"key1": "key1-value", "key2": "key2-value", "doomed_key": "deletable"});
    set(&tx, "ext-id", old_data)?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    let guid = get_mirror_guid(&tx, "ext-id")?;
    // We update 'key1' locally.
    let local_data = json!({"key1": "key1-new", "key2": "key2-value", "doomed_key": "deletable"});
    set(&tx, "ext-id", local_data)?;
    // Incoming payload where another client set 'key2' and removed
    // the 'doomed_key'.
    // Because we never uploaded our data, we'll merge our
    // key1 in, but otherwise keep the server's changes.
    let record = make_incoming(
        &guid,
        "ext-id",
        &json!({"key1": "key1-value", "key2": "key2-incoming"}),
    );
    // We should send our 'key1'
    assert_eq!(do_sync(&tx, &[record])?.len(), 1);
    check_finished_with(
        &tx,
        "ext-id",
        json!({"key1": "key1-new", "key2": "key2-incoming"}),
    )?;
    Ok(())
}

#[test]
fn test_merged_with_null_payload() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let old_data = json!({"key1": "key1-value"});
    set(&tx, "ext-id", old_data.clone())?;
    // Push this change remotely.
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    let guid = get_mirror_guid(&tx, "ext-id")?;
    assert_eq!(
        get_mirror_data(&tx, &guid),
        DbData::Data(old_data.to_string())
    );
    let local_data = json!({"key1": "key1-new", "key2": "key2-value"});
    set(&tx, "ext-id", local_data.clone())?;
    // Incoming payload with the same old data.
    let record = make_incoming(&guid, "ext-id", &old_data);
    // Three-way-merge will not detect any change in key1, so we
    // should keep our entire new value.
    assert_eq!(do_sync(&tx, &[record])?.len(), 1);
    check_finished_with(&tx, "ext-id", local_data)?;
    Ok(())
}

#[test]
fn test_deleted_mirrored_object_accept() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data)?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    let guid = get_mirror_guid(&tx, "ext-id")?;
    // Incoming payload with data deleted.
    // We synchronize this deletion by deleting the keys we think
    // were on the server.
    let record = make_incoming_tombstone(&guid);
    assert_eq!(do_sync(&tx, &[record])?.len(), 0);
    assert_eq!(get_local_data(&tx, "ext-id"), DbData::NoRow);
    assert_eq!(get_mirror_data(&tx, &guid), DbData::NullRow);
    Ok(())
}

#[test]
fn test_deleted_mirrored_object_merged() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    set(&tx, "ext-id", json!({"key1": "key1-value"}))?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    let guid = get_mirror_guid(&tx, "ext-id")?;
    set(
        &tx,
        "ext-id",
        json!({"key1": "key1-new", "key2": "key2-value"}),
    )?;
    // Incoming payload with data deleted.
    // We synchronize this deletion by deleting the keys we think
    // were on the server.
    let record = make_incoming_tombstone(&guid);
    // This overrides the change to 'key1', but we still upload 'key2'.
    assert_eq!(do_sync(&tx, &[record])?.len(), 1);
    check_finished_with(&tx, "ext-id", json!({"key2": "key2-value"}))?;
    Ok(())
}

/// Like the above test, but with a mirrored tombstone.
#[test]
fn test_deleted_mirrored_tombstone_merged() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    // Sync some data so we can get the guid for this extension.
    set(&tx, "ext-id", json!({"key1": "key1-value"}))?;
    assert_eq!(do_sync(&tx, &[])?.len(), 1);
    let guid = get_mirror_guid(&tx, "ext-id")?;
    // Sync a delete for this data so we have a tombstone in the mirror.
    let record = make_incoming_tombstone(&guid);
    assert_eq!(do_sync(&tx, &[record])?.len(), 0);
    assert_eq!(get_mirror_data(&tx, &guid), DbData::NullRow);

    // Set some data and sync it simultaneously with another incoming delete.
    set(&tx, "ext-id", json!({"key2": "key2-value"}))?;
    let record = make_incoming_tombstone(&guid);
    // We cannot delete any matching keys because there are no
    // matching keys. Instead we push our data.
    assert_eq!(do_sync(&tx, &[record])?.len(), 1);
    check_finished_with(&tx, "ext-id", json!({"key2": "key2-value"}))?;
    Ok(())
}

#[test]
fn test_deleted_not_mirrored_object_merged() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data)?;
    // Incoming payload with data deleted.
    let record = make_incoming_tombstone(&Guid::new("guid"));
    // We normally delete the keys we think were on the server, but
    // here we have no information about what was on the server, so we
    // don't delete anything. We merge in all undeleted keys.
    assert_eq!(do_sync(&tx, &[record])?.len(), 1);
    check_finished_with(
        &tx,
        "ext-id",
        json!({"key1": "key1-value", "key2": "key2-value"}),
    )?;
    Ok(())
}

#[test]
fn test_conflicting_incoming() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data)?;
    // Incoming payload without 'key1' and conflicting for 'key2'.
    // Because we never uploaded either of our keys, we'll merge our
    // key1 in, but the server key2 wins.
    let record = make_incoming(
        &Guid::new("guid"),
        "ext-id",
        &json!({"key2": "key2-incoming"}),
    );
    // We should send our 'key1'
    assert_eq!(do_sync(&tx, &[record])?.len(), 1);
    check_finished_with(
        &tx,
        "ext-id",
        json!({"key1": "key1-value", "key2": "key2-incoming"}),
    )?;
    Ok(())
}

#[test]
fn test_invalid_incoming() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let json = json!({"id": "id", "payload": json!("").to_string()});
    let bso = serde_json::from_value::<IncomingBso>(json).unwrap();
    let record = bso.into_content();

    // Should do nothing.
    assert_eq!(do_sync(&tx, &[record])?.len(), 0);
    Ok(())
}
