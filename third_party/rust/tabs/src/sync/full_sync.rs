/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{sync::engine::TabsSyncImpl, ApiResult, TabsEngine, TabsStore};
use error_support::handle_error;
use interrupt_support::NeverInterrupts;
use std::sync::Arc;
use sync15::client::{sync_multiple, MemoryCachedState, Sync15StorageClientInit};
use sync15::engine::EngineSyncAssociation;
use sync15::KeyBundle;

impl TabsStore {
    #[handle_error(crate::Error)]
    pub fn reset(self: Arc<Self>) -> ApiResult<()> {
        let mut sync_impl = TabsSyncImpl::new(Arc::clone(&self));
        sync_impl.reset(&EngineSyncAssociation::Disconnected)?;
        Ok(())
    }

    /// A convenience wrapper around sync_multiple.
    #[handle_error(crate::Error)]
    pub fn sync(
        self: Arc<Self>,
        key_id: String,
        access_token: String,
        sync_key: String,
        tokenserver_url: String,
        local_id: String,
    ) -> ApiResult<String> {
        let mut mem_cached_state = MemoryCachedState::default();
        let engine = TabsEngine::new(Arc::clone(&self));

        // Since we are syncing without the sync manager, there's no
        // command processor, therefore no clients engine, and in
        // consequence `TabsStore::prepare_for_sync` is never called
        // which means our `local_id` will never be set.
        // Do it here.
        engine.sync_impl.lock().unwrap().local_id = local_id;

        let storage_init = &Sync15StorageClientInit {
            key_id,
            access_token,
            tokenserver_url: url::Url::parse(tokenserver_url.as_str())?,
        };
        let root_sync_key = &KeyBundle::from_ksync_base64(sync_key.as_str())?;

        let mut result = sync_multiple(
            &[&engine],
            &mut None,
            &mut mem_cached_state,
            storage_init,
            root_sync_key,
            &NeverInterrupts,
            None,
        );

        // for b/w compat reasons, we do some dances with the result.
        // XXX - note that this means telemetry isn't going to be reported back
        // to the app - we need to check with lockwise about whether they really
        // need these failures to be reported or whether we can loosen this.
        if let Err(e) = result.result {
            return Err(e.into());
        }
        match result.engine_results.remove("tabs") {
            None | Some(Ok(())) => Ok(serde_json::to_string(&result.telemetry)?),
            Some(Err(e)) => Err(e.into()),
        }
    }
}
