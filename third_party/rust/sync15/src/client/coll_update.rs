/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::{
    request::{NormalResponseHandler, UploadInfo},
    CollState, Sync15ClientResponse, Sync15StorageClient,
};
use crate::bso::{IncomingBso, OutgoingBso, OutgoingEncryptedBso};
use crate::engine::CollectionRequest;
use crate::error::{self, Error, Result};
use crate::{CollectionName, KeyBundle, ServerTimestamp};

fn encrypt_outgoing(o: Vec<OutgoingBso>, key: &KeyBundle) -> Result<Vec<OutgoingEncryptedBso>> {
    o.into_iter()
        .map(|change| change.into_encrypted(key))
        .collect()
}

pub fn fetch_incoming(
    client: &Sync15StorageClient,
    state: &CollState,
    collection_request: CollectionRequest,
) -> Result<Vec<IncomingBso>> {
    let (records, _timestamp) = match client.get_encrypted_records(collection_request)? {
        Sync15ClientResponse::Success {
            record,
            last_modified,
            ..
        } => (record, last_modified),
        other => return Err(other.create_storage_error()),
    };
    let mut result = Vec::with_capacity(records.len());
    for record in records {
        // if we see a HMAC error, we've made an explicit decision to
        // NOT handle it here, but restart the global state machine.
        // That should cause us to re-read crypto/keys and things should
        // work (although if for some reason crypto/keys was updated but
        // not all storage was wiped we are probably screwed.)
        result.push(record.into_decrypted(&state.key)?);
    }
    Ok(result)
}

pub struct CollectionUpdate<'a> {
    client: &'a Sync15StorageClient,
    state: &'a CollState,
    collection: CollectionName,
    xius: ServerTimestamp,
    to_update: Vec<OutgoingEncryptedBso>,
    fully_atomic: bool,
}

impl<'a> CollectionUpdate<'a> {
    pub fn new(
        client: &'a Sync15StorageClient,
        state: &'a CollState,
        collection: CollectionName,
        xius: ServerTimestamp,
        records: Vec<OutgoingEncryptedBso>,
        fully_atomic: bool,
    ) -> CollectionUpdate<'a> {
        CollectionUpdate {
            client,
            state,
            collection,
            xius,
            to_update: records,
            fully_atomic,
        }
    }

    pub fn new_from_changeset(
        client: &'a Sync15StorageClient,
        state: &'a CollState,
        collection: CollectionName,
        changeset: Vec<OutgoingBso>,
        fully_atomic: bool,
    ) -> Result<CollectionUpdate<'a>> {
        let to_update = encrypt_outgoing(changeset, &state.key)?;
        Ok(CollectionUpdate::new(
            client,
            state,
            collection,
            state.last_modified,
            to_update,
            fully_atomic,
        ))
    }

    /// Returns a list of the IDs that failed if allowed_dropped_records is true, otherwise
    /// returns an empty vec.
    pub fn upload(self) -> error::Result<UploadInfo> {
        let mut failed = vec![];
        let mut q = self.client.new_post_queue(
            &self.collection,
            &self.state.config,
            self.xius,
            NormalResponseHandler::new(!self.fully_atomic),
        )?;

        for record in self.to_update.into_iter() {
            let enqueued = q.enqueue(&record)?;
            if !enqueued && self.fully_atomic {
                return Err(Error::RecordTooLargeError);
            }
        }

        q.flush(true)?;
        let mut info = q.completed_upload_info();
        info.failed_ids.append(&mut failed);
        if self.fully_atomic {
            assert_eq!(
                info.failed_ids.len(),
                0,
                "Bug: Should have failed by now if we aren't allowing dropped records"
            );
        }
        Ok(info)
    }
}
