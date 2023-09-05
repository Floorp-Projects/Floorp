/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines the custom configurations that consumers can set.
//! Those configurations override default values and can be used to set a custom server url,
//! collection name, and bucket name.
//! The purpose of the configuration parameters are to allow consumers an easy debugging option,
//! and the ability to be explicit about the server.

/// Custom configuration for the client.
/// Currently includes the following:
/// - `server_url`: The optional url for the settings server. If not specified, the standard server will be used.
/// - `bucket_name`: The optional name of the bucket containing the collection on the server. If not specified, the standard bucket will be used.
/// - `collection_name`: The name of the collection for the settings server.
#[derive(Debug, Clone)]
pub struct RemoteSettingsConfig {
    pub server_url: Option<String>,
    pub bucket_name: Option<String>,
    pub collection_name: String,
}
