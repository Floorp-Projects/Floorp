// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! Ping request representation.

use std::collections::HashMap;

use chrono::prelude::{DateTime, Utc};
use flate2::{read::GzDecoder, write::GzEncoder, Compression};
use serde_json::{self, Value as JsonValue};
use std::io::prelude::*;

use crate::error::{ErrorKind, Result};
use crate::system;

/// A representation for request headers.
pub type HeaderMap = HashMap<String, String>;

/// Creates a formatted date string that can be used with Date headers.
fn create_date_header_value(current_time: DateTime<Utc>) -> String {
    // Date headers are required to be in the following format:
    //
    // <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT
    //
    // as documented here:
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Date
    // Unfortunately we can't use `current_time.to_rfc2822()` as it
    // formats as "Mon, 22 Jun 2020 10:40:34 +0000", with an ending
    // "+0000" instead of "GMT". That's why we need to go with manual
    // formatting.
    current_time.format("%a, %d %b %Y %T GMT").to_string()
}

fn create_user_agent_header_value(
    version: &str,
    language_binding_name: &str,
    system: &str,
) -> String {
    format!(
        "Glean/{} ({} on {})",
        version, language_binding_name, system
    )
}

/// Attempt to gzip the contents of a ping.
fn gzip_content(path: &str, content: &[u8]) -> Option<Vec<u8>> {
    let mut gzipper = GzEncoder::new(Vec::new(), Compression::default());

    // Attempt to add the content to the gzipper.
    if let Err(e) = gzipper.write_all(content) {
        log::warn!("Failed to write to the gzipper: {} - {:?}", path, e);
        return None;
    }

    gzipper.finish().ok()
}

pub struct Builder {
    document_id: Option<String>,
    path: Option<String>,
    body: Option<Vec<u8>>,
    headers: HeaderMap,
    body_max_size: usize,
}

impl Builder {
    /// Creates a new builder for a PingRequest.
    pub fn new(language_binding_name: &str, body_max_size: usize) -> Self {
        let mut headers = HashMap::new();
        headers.insert("Date".to_string(), create_date_header_value(Utc::now()));
        headers.insert(
            "User-Agent".to_string(),
            create_user_agent_header_value(crate::GLEAN_VERSION, language_binding_name, system::OS),
        );
        headers.insert(
            "Content-Type".to_string(),
            "application/json; charset=utf-8".to_string(),
        );
        headers.insert("X-Client-Type".to_string(), "Glean".to_string());
        headers.insert(
            "X-Client-Version".to_string(),
            crate::GLEAN_VERSION.to_string(),
        );

        Self {
            document_id: None,
            path: None,
            body: None,
            headers,
            body_max_size,
        }
    }

    /// Sets the document_id for this request.
    pub fn document_id<S: Into<String>>(mut self, value: S) -> Self {
        self.document_id = Some(value.into());
        self
    }

    /// Sets the path for this request.
    pub fn path<S: Into<String>>(mut self, value: S) -> Self {
        self.path = Some(value.into());
        self
    }

    /// Sets the body for this request.
    ///
    /// This method will also attempt to gzip the body contents
    /// and add headers related to the body that was just added.
    ///
    /// Namely these headers are the "Content-Length" with the length of the body
    /// and in case we are successfull on gzipping the contents, the "Content-Encoding"="gzip".
    ///
    /// **Important**
    /// If we are unable to gzip we don't panic and instead just set the uncompressed body.
    ///
    /// # Panics
    ///
    /// This method will panic in case we try to set the body before setting the path.
    pub fn body<S: Into<String>>(mut self, value: S) -> Self {
        // Attempt to gzip the body contents.
        let original_as_string = value.into();
        let gzipped_content = gzip_content(
            self.path
                .as_ref()
                .expect("Path must be set before attempting to set the body"),
            original_as_string.as_bytes(),
        );
        let add_gzip_header = gzipped_content.is_some();
        let body = gzipped_content.unwrap_or_else(|| original_as_string.into_bytes());

        // Include headers related to body
        self = self.header("Content-Length", &body.len().to_string());
        if add_gzip_header {
            self = self.header("Content-Encoding", "gzip");
        }

        self.body = Some(body);
        self
    }

    /// Sets a header for this request.
    pub fn header<S: Into<String>>(mut self, key: S, value: S) -> Self {
        self.headers.insert(key.into(), value.into());
        self
    }

    /// Sets multiple headers for this request at once.
    pub fn headers(mut self, values: HeaderMap) -> Self {
        self.headers.extend(values);
        self
    }

    /// Consumes the builder and create a PingRequest.
    ///
    /// # Panics
    ///
    /// This method will panic if any of the required fields are missing:
    /// `document_id`, `path` and `body`.
    pub fn build(self) -> Result<PingRequest> {
        let body = self
            .body
            .expect("body must be set before attempting to build PingRequest");

        if body.len() > self.body_max_size {
            return Err(ErrorKind::PingBodyOverflow(body.len()).into());
        }

        Ok(PingRequest {
            document_id: self
                .document_id
                .expect("document_id must be set before attempting to build PingRequest"),
            path: self
                .path
                .expect("path must be set before attempting to build PingRequest"),
            body,
            headers: self.headers,
        })
    }
}

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
    pub headers: HeaderMap,
}

impl PingRequest {
    /// Creates a new builder-style structure to help build a PingRequest.
    ///
    /// # Arguments
    ///
    /// * `language_binding_name` - The name of the language used by the binding that instantiated this Glean instance.
    ///                             This is used to build the User-Agent header value.
    /// * `body_max_size` - The maximum size in bytes the compressed ping body may have to be eligible for upload.
    pub fn builder(language_binding_name: &str, body_max_size: usize) -> Builder {
        Builder::new(language_binding_name, body_max_size)
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

    /// Decompresses and pretty-format the ping payload
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
}

#[cfg(test)]
mod test {
    use super::*;
    use chrono::offset::TimeZone;

    #[test]
    fn date_header_resolution() {
        let date: DateTime<Utc> = Utc.ymd(2018, 2, 25).and_hms(11, 10, 37);
        let test_value = create_date_header_value(date);
        assert_eq!("Sun, 25 Feb 2018 11:10:37 GMT", test_value);
    }

    #[test]
    fn user_agent_header_resolution() {
        let test_value = create_user_agent_header_value("0.0.0", "Rust", "Windows");
        assert_eq!("Glean/0.0.0 (Rust on Windows)", test_value);
    }

    #[test]
    fn correctly_builds_ping_request() {
        let request = PingRequest::builder(/* language_binding_name */ "Rust", 1024 * 1024)
            .document_id("woop")
            .path("/random/path/doesnt/matter")
            .body("{}")
            .build()
            .unwrap();

        assert_eq!(request.document_id, "woop");
        assert_eq!(request.path, "/random/path/doesnt/matter");

        // Make sure all the expected headers were added.
        assert!(request.headers.contains_key("Date"));
        assert!(request.headers.contains_key("User-Agent"));
        assert!(request.headers.contains_key("Content-Type"));
        assert!(request.headers.contains_key("X-Client-Type"));
        assert!(request.headers.contains_key("X-Client-Version"));
        assert!(request.headers.contains_key("Content-Length"));
    }

    #[test]
    fn errors_when_request_body_exceeds_max_size() {
        // Create a new builder with an arbitrarily small value,
        // se we can test that the builder errors when body max size exceeds the expected.
        let request = Builder::new(
            /* language_binding_name */ "Rust", /* body_max_size */ 1,
        )
        .document_id("woop")
        .path("/random/path/doesnt/matter")
        .body("{}")
        .build();

        assert!(request.is_err());
    }
}
