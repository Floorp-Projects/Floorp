/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use rusqlite::Transaction;
use sync15_traits::{self, ApplyResults, IncomingEnvelope, OutgoingEnvelope};
use sync_guid::Guid as SyncGuid;

use crate::db::{delete_meta, get_meta, put_meta, StorageDb};
use crate::error::{Error, Result};
use crate::schema;
use crate::sync::incoming::{apply_actions, get_incoming, plan_incoming, stage_incoming};
use crate::sync::outgoing::{get_outgoing, record_uploaded, stage_outgoing};

const LAST_SYNC_META_KEY: &str = "last_sync_time";
const SYNC_ID_META_KEY: &str = "sync_id";

/// A bridged engine implements all the methods needed to make the
/// `storage.sync` store work with Desktop's Sync implementation.
/// Conceptually, it's similar to `sync15_traits::Store`, which we
/// should eventually rename and unify with this trait (#2841).
pub struct BridgedEngine<'a> {
    db: &'a StorageDb,
}

impl<'a> BridgedEngine<'a> {
    /// Creates a bridged engine for syncing.
    pub fn new(db: &'a StorageDb) -> Self {
        BridgedEngine { db }
    }

    fn do_reset(&self, tx: &Transaction<'_>) -> Result<()> {
        tx.execute_batch(
            "DELETE FROM storage_sync_mirror;
             UPDATE storage_sync_data SET sync_change_counter = 1;",
        )?;
        delete_meta(tx, LAST_SYNC_META_KEY)?;
        Ok(())
    }
}

impl<'a> sync15_traits::BridgedEngine for BridgedEngine<'a> {
    type Error = Error;

    fn last_sync(&self) -> Result<i64> {
        Ok(get_meta(self.db, LAST_SYNC_META_KEY)?.unwrap_or(0))
    }

    fn set_last_sync(&self, last_sync_millis: i64) -> Result<()> {
        put_meta(self.db, LAST_SYNC_META_KEY, &last_sync_millis)?;
        Ok(())
    }

    fn sync_id(&self) -> Result<Option<String>> {
        get_meta(self.db, SYNC_ID_META_KEY)
    }

    fn reset_sync_id(&self) -> Result<String> {
        let tx = self.db.unchecked_transaction()?;
        let new_id = SyncGuid::random().to_string();
        self.do_reset(&tx)?;
        put_meta(self.db, SYNC_ID_META_KEY, &new_id)?;
        tx.commit()?;
        Ok(new_id)
    }

    fn ensure_current_sync_id(&self, sync_id: &str) -> Result<String> {
        let current: Option<String> = get_meta(self.db, SYNC_ID_META_KEY)?;
        Ok(match current {
            Some(current) if current == sync_id => current,
            _ => {
                let tx = self.db.unchecked_transaction()?;
                self.do_reset(&tx)?;
                let result = sync_id.to_string();
                put_meta(self.db, SYNC_ID_META_KEY, &result)?;
                tx.commit()?;
                result
            }
        })
    }

    fn sync_started(&self) -> Result<()> {
        schema::create_empty_sync_temp_tables(self.db)?;
        Ok(())
    }

    fn store_incoming(&self, incoming_envelopes: &[IncomingEnvelope]) -> Result<()> {
        let signal = self.db.begin_interrupt_scope()?;

        let mut incoming_payloads = Vec::with_capacity(incoming_envelopes.len());
        for envelope in incoming_envelopes {
            signal.err_if_interrupted()?;
            incoming_payloads.push(envelope.payload()?);
        }

        let tx = self.db.unchecked_transaction()?;
        stage_incoming(&tx, incoming_payloads, &signal)?;
        tx.commit()?;
        Ok(())
    }

    fn apply(&self) -> Result<ApplyResults> {
        let signal = self.db.begin_interrupt_scope()?;

        let tx = self.db.unchecked_transaction()?;
        let incoming = get_incoming(&tx)?;
        let actions = incoming
            .into_iter()
            .map(|(item, state)| (item, plan_incoming(state)))
            .collect();
        apply_actions(&tx, actions, &signal)?;
        stage_outgoing(&tx)?;
        tx.commit()?;

        let outgoing = get_outgoing(self.db, &signal)?
            .into_iter()
            .map(OutgoingEnvelope::from)
            .collect::<Vec<_>>();
        Ok(outgoing.into())
    }

    fn set_uploaded(&self, _server_modified_millis: i64, ids: &[SyncGuid]) -> Result<()> {
        let signal = self.db.begin_interrupt_scope()?;
        let tx = self.db.unchecked_transaction()?;
        record_uploaded(&tx, ids, &signal)?;
        tx.commit()?;

        Ok(())
    }

    fn sync_finished(&self) -> Result<()> {
        schema::create_empty_sync_temp_tables(self.db)?;
        Ok(())
    }

    fn reset(&self) -> Result<()> {
        let tx = self.db.unchecked_transaction()?;
        self.do_reset(&tx)?;
        delete_meta(&tx, SYNC_ID_META_KEY)?;
        tx.commit()?;
        Ok(())
    }

    fn wipe(&self) -> Result<()> {
        let tx = self.db.unchecked_transaction()?;
        // We assume the meta table is only used by sync.
        tx.execute_batch(
            "DELETE FROM storage_sync_data; DELETE FROM storage_sync_mirror; DELETE FROM meta;",
        )?;
        tx.commit()?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::db::test::new_mem_db;
    use sync15_traits::bridged_engine::BridgedEngine;

    fn query_count(conn: &StorageDb, table: &str) -> u32 {
        conn.query_row_and_then(&format!("SELECT COUNT(*) FROM {};", table), [], |row| {
            row.get::<_, u32>(0)
        })
        .expect("should work")
    }

    // Sets up mock data for the tests here.
    fn setup_mock_data(engine: &super::BridgedEngine<'_>) -> Result<()> {
        engine.db.execute(
            "INSERT INTO storage_sync_data (ext_id, data, sync_change_counter)
                 VALUES ('ext-a', 'invalid-json', 2)",
            [],
        )?;
        engine.db.execute(
            "INSERT INTO storage_sync_mirror (guid, ext_id, data)
                 VALUES ('guid', 'ext-a', '3')",
            [],
        )?;
        engine.set_last_sync(1)?;

        // and assert we wrote what we think we did.
        assert_eq!(query_count(engine.db, "storage_sync_data"), 1);
        assert_eq!(query_count(engine.db, "storage_sync_mirror"), 1);
        assert_eq!(query_count(engine.db, "meta"), 1);
        Ok(())
    }

    // Assuming a DB setup with setup_mock_data, assert it was correctly reset.
    fn assert_reset(engine: &super::BridgedEngine<'_>) -> Result<()> {
        // A reset never wipes data...
        assert_eq!(query_count(engine.db, "storage_sync_data"), 1);

        // But did reset the change counter.
        let cc = engine.db.query_row_and_then(
            "SELECT sync_change_counter FROM storage_sync_data WHERE ext_id = 'ext-a';",
            [],
            |row| row.get::<_, u32>(0),
        )?;
        assert_eq!(cc, 1);
        // But did wipe the mirror...
        assert_eq!(query_count(engine.db, "storage_sync_mirror"), 0);
        // And the last_sync should have been wiped.
        assert!(get_meta::<i64>(engine.db, LAST_SYNC_META_KEY)?.is_none());
        Ok(())
    }

    // Assuming a DB setup with setup_mock_data, assert it has not been reset.
    fn assert_not_reset(engine: &super::BridgedEngine<'_>) -> Result<()> {
        assert_eq!(query_count(engine.db, "storage_sync_data"), 1);
        let cc = engine.db.query_row_and_then(
            "SELECT sync_change_counter FROM storage_sync_data WHERE ext_id = 'ext-a';",
            [],
            |row| row.get::<_, u32>(0),
        )?;
        assert_eq!(cc, 2);
        assert_eq!(query_count(engine.db, "storage_sync_mirror"), 1);
        // And the last_sync should remain.
        assert!(get_meta::<i64>(engine.db, LAST_SYNC_META_KEY)?.is_some());
        Ok(())
    }

    #[test]
    fn test_wipe() -> Result<()> {
        let db = new_mem_db();
        let engine = super::BridgedEngine::new(&db);

        setup_mock_data(&engine)?;

        engine.wipe()?;
        assert_eq!(query_count(engine.db, "storage_sync_data"), 0);
        assert_eq!(query_count(engine.db, "storage_sync_mirror"), 0);
        assert_eq!(query_count(engine.db, "meta"), 0);
        Ok(())
    }

    #[test]
    fn test_reset() -> Result<()> {
        let db = new_mem_db();
        let engine = super::BridgedEngine::new(&db);

        setup_mock_data(&engine)?;
        put_meta(engine.db, SYNC_ID_META_KEY, &"sync-id".to_string())?;

        engine.reset()?;
        assert_reset(&engine)?;
        // Only an explicit reset kills the sync-id, so check that here.
        assert_eq!(get_meta::<String>(engine.db, SYNC_ID_META_KEY)?, None);

        Ok(())
    }

    #[test]
    fn test_ensure_missing_sync_id() -> Result<()> {
        let db = new_mem_db();
        let engine = super::BridgedEngine::new(&db);

        setup_mock_data(&engine)?;

        assert_eq!(engine.sync_id()?, None);
        // We don't have a sync ID - so setting one should reset.
        engine.ensure_current_sync_id("new-id")?;
        // should have cause a reset.
        assert_reset(&engine)?;
        Ok(())
    }

    #[test]
    fn test_ensure_new_sync_id() -> Result<()> {
        let db = new_mem_db();
        let engine = super::BridgedEngine::new(&db);

        setup_mock_data(&engine)?;

        put_meta(engine.db, SYNC_ID_META_KEY, &"old-id".to_string())?;
        assert_not_reset(&engine)?;
        assert_eq!(engine.sync_id()?, Some("old-id".to_string()));

        engine.ensure_current_sync_id("new-id")?;
        // should have cause a reset.
        assert_reset(&engine)?;
        // should have the new id.
        assert_eq!(engine.sync_id()?, Some("new-id".to_string()));
        Ok(())
    }

    #[test]
    fn test_ensure_same_sync_id() -> Result<()> {
        let db = new_mem_db();
        let engine = super::BridgedEngine::new(&db);

        setup_mock_data(&engine)?;
        assert_not_reset(&engine)?;

        put_meta(engine.db, SYNC_ID_META_KEY, &"sync-id".to_string())?;

        engine.ensure_current_sync_id("sync-id")?;
        // should not have reset.
        assert_not_reset(&engine)?;
        Ok(())
    }

    #[test]
    fn test_reset_sync_id() -> Result<()> {
        let db = new_mem_db();
        let engine = super::BridgedEngine::new(&db);

        setup_mock_data(&engine)?;
        put_meta(engine.db, SYNC_ID_META_KEY, &"sync-id".to_string())?;

        assert_eq!(engine.sync_id()?, Some("sync-id".to_string()));
        let new_id = engine.reset_sync_id()?;
        // should have cause a reset.
        assert_reset(&engine)?;
        assert_eq!(engine.sync_id()?, Some(new_id));
        Ok(())
    }
}
