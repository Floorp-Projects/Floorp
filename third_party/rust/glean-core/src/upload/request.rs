// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Ping request representation.

use std::collections::HashMap;

use chrono::prelude::{DateTime, Utc};
use flate2::{read::GzDecoder, write::GzEncoder, Compression};
use serde_json::{self, Value as JsonValue};
use std::io::prelude::*;

/// Represents a request to upload a ping.
#[derive(PartialEq, Debug, Clone)]
pub struct PingRequest {
    /// The Job ID to identify this request,
    /// this is the same as the ping UUID.
    pub document_id: String,
    /// The path for the server to upload the ping to.
    pub path: String,
    /// The body of the request, as a byte array. If gzip encoded, then
    /// the `headers` list will contain a `Content-Encoding` header with
    /// the value `gzip`.
    pub body: Vec<u8>,
    /// A map with all the headers to be sent with the request.
    pub headers: HashMap<&'static str, String>,
}

impl PingRequest {
    /// Creates a new PingRequest.
    ///
    /// Automatically creates the default request headers.
    /// Clients may add more headers such as `userAgent` to this list.
    pub fn new(document_id: &str, path: &str, body: JsonValue) -> Self {
        // We want uploads to be gzip'd. Instead of doing this for each platform
        // we have language bindings for, apply compression here.
        let original_as_string = body.to_string();
        let gzipped_content = Self::gzip_content(path, original_as_string.as_bytes());
        let add_gzip_header = gzipped_content.is_some();
        let body = gzipped_content.unwrap_or_else(|| original_as_string.into_bytes());
        let body_len = body.len();

        Self {
            document_id: document_id.into(),
            path: path.into(),
            body,
            headers: Self::create_request_headers(add_gzip_header, body_len),
        }
    }

    /// Verifies if current request is for a deletion-request ping.
    pub fn is_deletion_request(&self) -> bool {
        // The path format should be `/submit/<app_id>/<ping_name>/<schema_version/<doc_id>`
        self.path
            .split('/')
            .nth(3)
            .map(|url| url == "deletion-request")
            .unwrap_or(false)
    }

    /// Decompress and pretty-format the ping payload
    ///
    /// Should be used for logging when required.
    /// This decompresses the payload in memory.
    pub fn pretty_body(&self) -> Option<String> {
        let mut gz = GzDecoder::new(&self.body[..]);
        let mut s = String::with_capacity(self.body.len());

        gz.read_to_string(&mut s)
            .ok()
            .map(|_| &s[..])
            .or_else(|| std::str::from_utf8(&self.body).ok())
            .and_then(|payload| serde_json::from_str::<JsonValue>(payload).ok())
            .and_then(|json| serde_json::to_string_pretty(&json).ok())
    }

    /// Attempt to gzip the provided ping content.
    fn gzip_content(path: &str, content: &[u8]) -> Option<Vec<u8>> {
        let mut gzipper = GzEncoder::new(Vec::new(), Compression::default());

        // Attempt to add the content to the gzipper.
        if let Err(e) = gzipper.write_all(content) {
            log::error!("Failed to write to the gzipper: {} - {:?}", path, e);
            return None;
        }

        gzipper.finish().ok()
    }

    /// Creates the default request headers.
    fn create_request_headers(is_gzipped: bool, body_len: usize) -> HashMap<&'static str, String> {
        let mut headers = HashMap::new();
        let date: DateTime<Utc> = Utc::now();
        headers.insert("Date", date.to_string());
        headers.insert("X-Client-Type", "Glean".to_string());
        headers.insert(
            "Content-Type",
            "application/json; charset=utf-8".to_string(),
        );
        headers.insert("Content-Length", body_len.to_string());
        if is_gzipped {
            headers.insert("Content-Encoding", "gzip".to_string());
        }
        headers.insert("X-Client-Version", crate::GLEAN_VERSION.to_string());
        headers
    }
}
