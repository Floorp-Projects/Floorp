/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use sync15_traits::{self, ApplyResults, IncomingEnvelope, OutgoingEnvelope};
use sync_guid::Guid as SyncGuid;

use crate::api;
use crate::db::StorageDb;
use crate::error::{Error, ErrorKind, Result};
use crate::schema;
use crate::sync::incoming::{apply_actions, get_incoming, plan_incoming, stage_incoming};
use crate::sync::outgoing::{get_outgoing, record_uploaded, stage_outgoing};

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
}

impl<'a> sync15_traits::BridgedEngine for BridgedEngine<'a> {
    type Error = Error;

    fn last_sync(&self) -> Result<i64> {
        Err(ErrorKind::NotImplemented.into())
    }

    fn set_last_sync(&self, _last_sync_millis: i64) -> Result<()> {
        Err(ErrorKind::NotImplemented.into())
    }

    fn sync_id(&self) -> Result<Option<String>> {
        Err(ErrorKind::NotImplemented.into())
    }

    fn reset_sync_id(&self) -> Result<String> {
        Err(ErrorKind::NotImplemented.into())
    }

    fn ensure_current_sync_id(&self, _new_sync_id: &str) -> Result<String> {
        Err(ErrorKind::NotImplemented.into())
    }

    fn sync_started(&self) -> Result<()> {
        schema::create_empty_sync_temp_tables(&self.db)?;
        Ok(())
    }

    fn store_incoming(&self, incoming_envelopes: &[IncomingEnvelope]) -> Result<()> {
        let signal = self.db.begin_interrupt_scope();

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
        let signal = self.db.begin_interrupt_scope();

        let tx = self.db.unchecked_transaction()?;
        let incoming = get_incoming(&tx)?;
        let actions = incoming
            .into_iter()
            .map(|(item, state)| (item, plan_incoming(state)))
            .collect();
        apply_actions(&tx, actions, &signal)?;
        stage_outgoing(&tx)?;
        tx.commit()?;

        let outgoing = get_outgoing(&self.db, &signal)?
            .into_iter()
            .map(OutgoingEnvelope::from)
            .collect::<Vec<_>>();
        Ok(outgoing.into())
    }

    fn set_uploaded(&self, _server_modified_millis: i64, ids: &[SyncGuid]) -> Result<()> {
        let signal = self.db.begin_interrupt_scope();
        let tx = self.db.unchecked_transaction()?;
        record_uploaded(&tx, ids, &signal)?;
        tx.commit()?;

        Ok(())
    }

    fn sync_finished(&self) -> Result<()> {
        schema::create_empty_sync_temp_tables(&self.db)?;
        Ok(())
    }

    fn reset(&self) -> Result<()> {
        Err(ErrorKind::NotImplemented.into())
    }

    fn wipe(&self) -> Result<()> {
        let tx = self.db.unchecked_transaction()?;
        api::wipe_all(&tx)?;
        tx.commit()?;
        Ok(())
    }
}
