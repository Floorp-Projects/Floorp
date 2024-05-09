/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

use crate::Result;
use remote_settings::RemoteSettingsResponse;
use serde::Deserialize;
/// The Remote Settings collection name.
pub(crate) const REMOTE_SETTINGS_COLLECTION: &str = "content-relevance";

/// A trait for a client that downloads records from Remote Settings.
///
/// This trait lets tests use a mock client.
pub(crate) trait RelevancyRemoteSettingsClient {
    /// Fetches records from the Suggest Remote Settings collection.
    fn get_records(&self) -> Result<RemoteSettingsResponse>;

    /// Fetches a record's attachment from the Suggest Remote Settings
    /// collection.
    fn get_attachment(&self, location: &str) -> Result<Vec<u8>>;
}

impl RelevancyRemoteSettingsClient for remote_settings::Client {
    fn get_records(&self) -> Result<RemoteSettingsResponse> {
        Ok(remote_settings::Client::get_records(self)?)
    }

    fn get_attachment(&self, location: &str) -> Result<Vec<u8>> {
        Ok(remote_settings::Client::get_attachment(self, location)?)
    }
}

/// A record in the Relevancy Remote Settings collection.
#[derive(Clone, Debug, Deserialize)]
pub struct RelevancyRecord {
    #[serde(rename = "type")]
    pub record_type: String,
    pub record_custom_details: RecordCustomDetails,
}

// Custom details related to category of the record.
#[derive(Clone, Debug, Deserialize)]
pub struct RecordCustomDetails {
    pub category_to_domains: CategoryToDomains,
}

/// Category information related to the record.
#[derive(Clone, Debug, Deserialize)]
pub struct CategoryToDomains {
    pub version: i32,
    pub category: String,
    pub category_code: i32,
}

/// A downloaded Remote Settings attachment that contains domain data.
#[derive(Clone, Debug, Deserialize)]
pub struct RelevancyAttachmentData {
    pub domain: String,
}
