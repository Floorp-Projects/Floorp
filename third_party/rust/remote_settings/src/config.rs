/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This module defines the custom configurations that consumers can set.
//! Those configurations override default values and can be used to set a custom server,
//! collection name, and bucket name.
//! The purpose of the configuration parameters are to allow consumers an easy debugging option,
//! and the ability to be explicit about the server.

use url::Url;

use crate::Result;

/// Custom configuration for the client.
/// Currently includes the following:
/// - `server`: The Remote Settings server to use. If not specified, defaults to the production server (`RemoteSettingsServer::Prod`).
/// - `server_url`: An optional custom Remote Settings server URL. Deprecated; please use `server` instead.
/// - `bucket_name`: The optional name of the bucket containing the collection on the server. If not specified, the standard bucket will be used.
/// - `collection_name`: The name of the collection for the settings server.
#[derive(Debug, Clone)]
pub struct RemoteSettingsConfig {
    pub server: Option<RemoteSettingsServer>,
    pub server_url: Option<String>,
    pub bucket_name: Option<String>,
    pub collection_name: String,
}

/// The Remote Settings server that the client should use.
#[derive(Debug, Clone)]
pub enum RemoteSettingsServer {
    Prod,
    Stage,
    Dev,
    Custom { url: String },
}

impl RemoteSettingsServer {
    pub fn url(&self) -> Result<Url> {
        Ok(match self {
            Self::Prod => Url::parse("https://firefox.settings.services.mozilla.com").unwrap(),
            Self::Stage => Url::parse("https://firefox.settings.services.allizom.org").unwrap(),
            Self::Dev => Url::parse("https://remote-settings-dev.allizom.org").unwrap(),
            Self::Custom { url } => Url::parse(url)?,
        })
    }
}
