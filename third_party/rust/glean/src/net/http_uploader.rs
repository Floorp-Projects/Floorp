// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::net::{PingUploader, UploadResult};

/// A simple mechanism to upload pings over HTTPS.
#[derive(Debug)]
pub struct HttpUploader;

impl PingUploader for HttpUploader {
    /// Uploads a ping to a server.
    ///
    /// # Arguments
    ///
    /// * `url` - the URL path to upload the data to.
    /// * `body` - the serialized text data to send.
    /// * `headers` - a vector of tuples containing the headers to send with
    ///   the request, i.e. (Name, Value).
    fn upload(&self, url: String, _body: Vec<u8>, _headers: Vec<(String, String)>) -> UploadResult {
        log::debug!("TODO bug 1675468: submitting to {:?}", url);
        UploadResult::HttpStatus(200)
    }
}
