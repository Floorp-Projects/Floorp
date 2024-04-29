/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{rs::SuggestRemoteSettingsClient, Result};
use parking_lot::Mutex;
use remote_settings::{Client, GetItemsOptions, RemoteSettingsConfig, RemoteSettingsResponse};
use std::collections::HashMap;

/// Remotes settings client that runs during the benchmark warm-up phase.
///
/// This should be used to run a full ingestion.
/// Then it can be converted into a [RemoteSettingsBenchmarkClient], which allows benchmark code to exclude the network request time.
/// [RemoteSettingsBenchmarkClient] implements [SuggestRemoteSettingsClient] by getting data from a HashMap rather than hitting the network.
pub struct RemoteSettingsWarmUpClient {
    client: Client,
    pub get_records_responses: Mutex<HashMap<GetItemsOptions, RemoteSettingsResponse>>,
    pub get_attachment_responses: Mutex<HashMap<String, Vec<u8>>>,
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
            get_attachment_responses: Mutex::new(HashMap::new()),
        }
    }
}

impl Default for RemoteSettingsWarmUpClient {
    fn default() -> Self {
        Self::new()
    }
}

impl SuggestRemoteSettingsClient for RemoteSettingsWarmUpClient {
    fn get_records_with_options(
        &self,
        options: &GetItemsOptions,
    ) -> Result<RemoteSettingsResponse> {
        let response = self.client.get_records_with_options(options)?;
        self.get_records_responses
            .lock()
            .insert(options.clone(), response.clone());
        Ok(response)
    }

    fn get_attachment(&self, location: &str) -> Result<Vec<u8>> {
        let response = self.client.get_attachment(location)?;
        self.get_attachment_responses
            .lock()
            .insert(location.to_string(), response.clone());
        Ok(response)
    }
}

#[derive(Clone)]
pub struct RemoteSettingsBenchmarkClient {
    pub get_records_responses: HashMap<GetItemsOptions, RemoteSettingsResponse>,
    pub get_attachment_responses: HashMap<String, Vec<u8>>,
}

impl SuggestRemoteSettingsClient for RemoteSettingsBenchmarkClient {
    fn get_records_with_options(
        &self,
        options: &GetItemsOptions,
    ) -> Result<RemoteSettingsResponse> {
        Ok(self
            .get_records_responses
            .get(options)
            .unwrap_or_else(|| panic!("options not found: {options:?}"))
            .clone())
    }

    fn get_attachment(&self, location: &str) -> Result<Vec<u8>> {
        Ok(self
            .get_attachment_responses
            .get(location)
            .unwrap_or_else(|| panic!("location not found: {location:?}"))
            .clone())
    }
}

impl From<RemoteSettingsWarmUpClient> for RemoteSettingsBenchmarkClient {
    fn from(warm_up_client: RemoteSettingsWarmUpClient) -> Self {
        Self {
            get_records_responses: warm_up_client.get_records_responses.into_inner(),
            get_attachment_responses: warm_up_client.get_attachment_responses.into_inner(),
        }
    }
}
