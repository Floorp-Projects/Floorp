/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{rs, Result};
use parking_lot::Mutex;
use remote_settings::{Client, RemoteSettingsConfig};
use std::collections::HashMap;

/// Remotes settings client that runs during the benchmark warm-up phase.
///
/// This should be used to run a full ingestion.
/// Then it can be converted into a [RemoteSettingsBenchmarkClient], which allows benchmark code to exclude the network request time.
/// [RemoteSettingsBenchmarkClient] implements [rs::Client] by getting data from a HashMap rather than hitting the network.
pub struct RemoteSettingsWarmUpClient {
    client: Client,
    pub get_records_responses: Mutex<HashMap<rs::RecordRequest, Vec<rs::Record>>>,
}

impl RemoteSettingsWarmUpClient {
    pub fn new() -> Self {
        Self {
            client: Client::new(RemoteSettingsConfig {
                server: None,
                server_url: None,
                bucket_name: None,
                collection_name: crate::rs::REMOTE_SETTINGS_COLLECTION.into(),
            })
            .unwrap(),
            get_records_responses: Mutex::new(HashMap::new()),
        }
    }
}

impl Default for RemoteSettingsWarmUpClient {
    fn default() -> Self {
        Self::new()
    }
}

impl rs::Client for RemoteSettingsWarmUpClient {
    fn get_records(&self, request: rs::RecordRequest) -> Result<Vec<rs::Record>> {
        let response = <Client as rs::Client>::get_records(&self.client, request.clone())?;
        self.get_records_responses
            .lock()
            .insert(request, response.clone());
        Ok(response)
    }
}

#[derive(Clone)]
pub struct RemoteSettingsBenchmarkClient {
    pub get_records_responses: HashMap<rs::RecordRequest, Vec<rs::Record>>,
}

impl rs::Client for RemoteSettingsBenchmarkClient {
    fn get_records(&self, request: rs::RecordRequest) -> Result<Vec<rs::Record>> {
        Ok(self
            .get_records_responses
            .get(&request)
            .unwrap_or_else(|| panic!("options not found: {request:?}"))
            .clone())
    }
}

impl From<RemoteSettingsWarmUpClient> for RemoteSettingsBenchmarkClient {
    fn from(warm_up_client: RemoteSettingsWarmUpClient) -> Self {
        Self {
            get_records_responses: warm_up_client.get_records_responses.into_inner(),
        }
    }
}
