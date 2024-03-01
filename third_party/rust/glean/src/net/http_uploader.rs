// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use crate::net::{PingUploadRequest, PingUploader, UploadResult};

/// A simple mechanism to upload pings over HTTPS.
#[derive(Debug)]
pub struct HttpUploader;

impl PingUploader for HttpUploader {
    /// Uploads a ping to a server.
    ///
    /// # Arguments
    ///
    /// * `upload_request` - the requested upload.
    fn upload(&self, upload_request: PingUploadRequest) -> UploadResult {
        log::debug!("TODO bug 1675468: submitting to {:?}", upload_request.url);
        UploadResult::http_status(200)
    }
}
