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
use crate::sync::Record;
use interrupt_support::NeverInterrupts;
use rusqlite::{Connection, Row, Transaction};
use serde_json::json;
use sql_support::ConnExt;
use sync15_traits::Payload;
use sync_guid::Guid;

// Here we try and simulate everything done by a "full sync", just minus the
// engine. Returns the records we uploaded.
fn do_sync(tx: &Transaction<'_>, incoming_payloads: Vec<Payload>) -> Result<Vec<Payload>> {
    // First we stage the incoming in the temp tables.
    stage_incoming(tx, incoming_payloads, &NeverInterrupts)?;
    // Then we process them getting a Vec of (item, state), which we turn into
    // a Vec of (item, action)
    let actions = get_incoming(tx)?
        .into_iter()
        .map(|(item, state)| (item, plan_incoming(state)))
        .collect();
    apply_actions(&tx, actions, &NeverInterrupts)?;
    // So we've done incoming - do outgoing.
    stage_outgoing(tx)?;
    let outgoing = get_outgoing(tx, &NeverInterrupts)?;
    record_uploaded(
        tx,
        outgoing
            .iter()
            .map(|p| p.id.clone())
            .collect::<Vec<Guid>>()
            .as_slice(),
        &NeverInterrupts,
    )?;
    create_empty_sync_temp_tables(tx)?;
    Ok(outgoing)
}

// Check *both* the mirror and local API have ended up with the specified data.
fn check_finished_with(conn: &Connection, ext_id: &str, val: serde_json::Value) -> Result<()> {
    let local = get(conn, &ext_id, serde_json::Value::Null)?;
    assert_eq!(local, val);
    let mirror = get_mirror_data(conn, ext_id);
    assert_eq!(mirror, DbData::Data(val.to_string()));
    // and there should be zero items with a change counter.
    let count = conn.query_row_and_then(
        "SELECT COUNT(*) FROM storage_sync_data WHERE sync_change_counter != 0;",
        rusqlite::NO_PARAMS,
        |row| row.get::<_, u32>(0),
    )?;
    assert_eq!(count, 0);
    Ok(())
}

#[derive(Debug, PartialEq)]
enum DbData {
    NoRow,
    NullRow,
    Data(String),
}

impl DbData {
    fn has_data(&self) -> bool {
        if let DbData::Data(_) = self {
            true
        } else {
            false
        }
    }
}

fn _get(conn: &Connection, expected_extid: &str, table: &str) -> DbData {
    let sql = format!("SELECT ext_id, data FROM {}", table);

    fn from_row(row: &Row<'_>) -> Result<(String, Option<String>)> {
        Ok((row.get("ext_id")?, row.get("data")?))
    }
    let mut items = conn
        .conn()
        .query_rows_and_then_named(&sql, &[], from_row)
        .expect("should work");
    if items.is_empty() {
        DbData::NoRow
    } else {
        let item = items.pop().expect("it exists");
        assert_eq!(item.0, expected_extid);
        match item.1 {
            None => DbData::NullRow,
            Some(v) => DbData::Data(v),
        }
    }
}

fn get_mirror_data(conn: &Connection, expected_extid: &str) -> DbData {
    _get(conn, expected_extid, "storage_sync_mirror")
}

fn get_local_data(conn: &Connection, expected_extid: &str) -> DbData {
    _get(conn, expected_extid, "storage_sync_data")
}

#[test]
fn test_simple_outgoing_sync() -> Result<()> {
    // So we are starting with an empty local store and empty server store.
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data.clone())?;
    assert_eq!(do_sync(&tx, vec![])?.len(), 1);
    check_finished_with(&tx, "ext-id", data)?;
    Ok(())
}

#[test]
fn test_simple_tombstone() -> Result<()> {
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
    assert_eq!(do_sync(&tx, vec![])?.len(), 1);
    assert!(get_local_data(&tx, "ext-id").has_data());
    assert!(get_mirror_data(&tx, "ext-id").has_data());
    clear(&tx, "ext-id")?;
    assert_eq!(get_local_data(&tx, "ext-id"), DbData::NullRow);
    // then after syncing, the tombstone will be in the mirror but the local row
    // has been removed.
    assert_eq!(do_sync(&tx, vec![])?.len(), 1);
    assert_eq!(get_local_data(&tx, "ext-id"), DbData::NoRow);
    assert_eq!(get_mirror_data(&tx, "ext-id"), DbData::NullRow);
    Ok(())
}

#[test]
fn test_merged() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value"});
    set(&tx, "ext-id", data)?;
    // Incoming payload without 'key1' and conflicting for 'key2'
    let payload = Payload::from_record(Record {
        guid: Guid::from("guid"),
        ext_id: "ext-id".to_string(),
        data: Some(json!({"key2": "key2-value"}).to_string()),
    })?;
    assert_eq!(do_sync(&tx, vec![payload])?.len(), 1);
    check_finished_with(
        &tx,
        "ext-id",
        json!({"key1": "key1-value", "key2": "key2-value"}),
    )?;
    Ok(())
}

#[test]
fn test_reconciled() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value"});
    set(&tx, "ext-id", data)?;
    // Incoming payload without 'key1' and conflicting for 'key2'
    let payload = Payload::from_record(Record {
        guid: Guid::from("guid"),
        ext_id: "ext-id".to_string(),
        data: Some(json!({"key1": "key1-value"}).to_string()),
    })?;
    // Should be no outgoing records as we reconciled.
    assert_eq!(do_sync(&tx, vec![payload])?.len(), 0);
    check_finished_with(&tx, "ext-id", json!({"key1": "key1-value"}))?;
    Ok(())
}

#[test]
fn test_conflicting_incoming() -> Result<()> {
    let mut db = new_syncable_mem_db();
    let tx = db.transaction()?;
    let data = json!({"key1": "key1-value", "key2": "key2-value"});
    set(&tx, "ext-id", data)?;
    // Incoming payload without 'key1' and conflicting for 'key2'
    let payload = Payload::from_record(Record {
        guid: Guid::from("guid"),
        ext_id: "ext-id".to_string(),
        data: Some(json!({"key2": "key2-incoming"}).to_string()),
    })?;
    assert_eq!(do_sync(&tx, vec![payload])?.len(), 1);
    check_finished_with(
        &tx,
        "ext-id",
        json!({"key1": "key1-value", "key2": "key2-incoming"}),
    )?;
    Ok(())
}

// There are lots more we could add here, particularly around the resolution of
// deletion of keys and deletions of the entire value.
